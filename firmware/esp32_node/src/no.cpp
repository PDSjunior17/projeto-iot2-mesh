#include "no.h"

#include <algorithm>

#include "ble_gatt.h"
#include "config.h"

using namespace redelora;

namespace {

uint32_t aleatorio(uint32_t min, uint32_t max) { return min + esp_random() % (max - min + 1); }

// a >= b, tolerando a virada do millis() (49 dias)
bool passou(uint32_t agora, uint32_t instante) { return (int32_t)(agora - instante) >= 0; }

// Intervalo com variação aleatória de ±VARIACAO_REENVIO_PCT.
uint32_t comVariacao(uint32_t ms) {
  uint32_t v = ms / 100 * cfg::VARIACAO_REENVIO_PCT;
  return ms - v + esp_random() % (2 * v + 1);
}

uint32_t lerU32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

bool contem(const std::vector<uint32_t>& v, uint32_t x) { return std::find(v.begin(), v.end(), x) != v.end(); }

Bytes serializar(const Pacote& p) {
  uint8_t buf[MAX_PACOTE];
  size_t n = codificar(p, buf, sizeof buf);
  return Bytes(buf, buf + n);
}

Bytes pacoteCabecalho(const Mensagem& m, uint8_t ttl) {
  Pacote p;
  p.idi = Idi::Cabecalho;
  p.flags = m.env.flags;
  p.ttl = ttl;
  p.msgId = m.env.msgId;
  p.destino = m.env.destino;
  p.remetente = m.env.remetente;
  p.nBlocos = m.nBlocos;
  return serializar(p);
}

Bytes pacoteBloco(const Mensagem& m, uint8_t i, uint8_t ttl) {
  Pacote p;
  p.idi = Idi::Bloco;
  p.flags = m.env.flags;
  p.ttl = ttl;
  p.msgId = m.env.msgId;
  p.bloco = i;
  p.dados = m.blocos[i].data();
  p.tamDados = m.blocos[i].size();
  return serializar(p);
}

char letraEstado(Estado e) { return e == Estado::Incompleta ? 'I' : e == Estado::Pendente ? 'P' : 'E'; }

}  // namespace

void No::iniciar() {
  size_t n = pers_.carregar(tabela_, millis());
  painel_.log("Tabela (%s): %u msgs", pers_.nome(), (unsigned)n);
}

void No::loop() {
  processarRadio();
  processarBle();
  processarFilaTx();
  processarFilaBle();
  uint32_t agora = millis();
  if (agora - ultimaManutencaoMs_ >= cfg::INTERVALO_MANUTENCAO_MS) {
    ultimaManutencaoMs_ = agora;
    manutencao();
  }
}

// ---------------------------------------------------------------- recepção

void No::processarRadio() {
  uint8_t buf[MAX_PACOTE + 8];
  size_t tam;
  bool erroCrc;
  if (!radio_.receber(buf, sizeof buf, tam, erroCrc)) {
    if (erroCrc) {
      contCrc_++;
      painel_.log("RX CRC invalido (%.0fdBm)", radio_.ultimoRssi());
    }
    return;
  }
  contRx_++;
  Pacote p;
  if (!decodificar(buf, tam, p)) {
    painel_.log("RX invalido %uB", (unsigned)tam);
    return;
  }
  if (p.idi == Idi::Bloco && descartarBlocos_) {
    descartarBlocos_--;
    painel_.log("TESTE descartei BLK b%u", p.bloco);
    return;
  }
  if (p.idi == Idi::Bloco)
    painel_.log("RX BLK %04X b%u %.0fdB", (unsigned)(p.msgId & 0xFFFF), p.bloco, radio_.ultimoRssi());
  else
    painel_.log("RX %s %04X t%u %.0fdB", nomeIdi(p.idi), (unsigned)(p.msgId & 0xFFFF), p.ttl,
                radio_.ultimoRssi());
  tratarPacote(p, -1);
}

void No::tratarPacote(Pacote p, int conn) {
  if (conn >= 0) {
    Usuario& u = usuarios_[conn];
    bool temRemetente = p.mensagemUnica() || p.idi == Idi::Cabecalho;
    if (!u.registrado) {
      enviarEvento(conn, gatt::EVT_ERRO, p.msgId, gatt::ERRO_NAO_REGISTRADO);
      return;
    }
    if (p.idi == Idi::Ack || p.idi == Idi::Nack || (temRemetente && p.remetente != u.id)) {
      enviarEvento(conn, gatt::EVT_ERRO, p.msgId, gatt::ERRO_PACOTE_INVALIDO);
      return;
    }
    p.ttl = TTL_PADRAO;
  } else {
    // Alguém já respondeu a este NACK: não precisamos repetir.
    if (p.idi == Idi::Bloco) suprimirResposta(p.msgId, p.bloco);
    if (p.idi == Idi::Cabecalho) suprimirResposta(p.msgId, -1);
  }

  switch (p.idi) {
    case Idi::Mensagem:
    case Idi::Ipfs: receberMensagemUnica(p, conn); break;
    case Idi::Cabecalho: receberCabecalho(p, conn); break;
    case Idi::Bloco: receberBloco(p, conn); break;
    case Idi::Ack: receberAck(p); break;
    case Idi::Nack: receberNack(p); break;
  }
}

void No::receberMensagemUnica(const Pacote& p, int conn) {
  if (Mensagem* m = tabela_.buscar(p.msgId)) {
    if (m->estado == Estado::Entregue && conn < 0) repetirAck(*m);
    if (m->estado == Estado::Pendente && conn < 0) adiarReenvio(*m);
    return;  // já conhecida
  }
  Mensagem& m = tabela_.criar(p.msgId, millis());
  m.env = {p.msgId, p.destino, p.remetente, p.flags, p.ttl};
  m.idi = p.idi;
  m.blocos.emplace_back(p.dados, p.dados + p.tamDados);
  m.origemLocal = conn >= 0;
  m.estado = Estado::Pendente;
  mensagemCompleta(m, conn);
}

void No::receberCabecalho(const Pacote& p, int conn) {
  Mensagem* m = tabela_.buscar(p.msgId);
  if (m) {
    if (m->estado == Estado::Entregue) {
      if (conn < 0) repetirAck(*m);
      return;
    }
    if (m->temCabecalho) {  // duplicado
      if (m->estado == Estado::Pendente && conn < 0) adiarReenvio(*m);
      return;
    }
  } else {
    m = &tabela_.criar(p.msgId, millis());
  }
  m->env = {p.msgId, p.destino, p.remetente, p.flags, p.ttl};
  m->idi = Idi::Cabecalho;
  m->temCabecalho = true;
  m->nBlocos = p.nBlocos;
  m->blocos.resize(p.nBlocos);
  m->origemLocal = conn >= 0;
  m->ultimoRxMs = millis();
  m->nacks = 0;
  verificarCompleta(*m, conn);
}

void No::receberBloco(const Pacote& p, int conn) {
  Mensagem* m = tabela_.buscar(p.msgId);
  if (m && m->estado == Estado::Entregue) return;
  if (!m) {
    // Bloco chegou antes do cabeçalho: guarda e espera (ou pede por NACK).
    m = &tabela_.criar(p.msgId, millis());
    m->idi = Idi::Cabecalho;
    m->env.ttl = p.ttl;
    m->env.flags = p.flags;
    m->origemLocal = conn >= 0;
  }
  if (m->temCabecalho && p.bloco >= m->nBlocos) return;
  if (m->temBloco(p.bloco)) return;  // duplicado
  if (m->blocos.size() <= p.bloco) m->blocos.resize(p.bloco + 1);
  m->blocos[p.bloco].assign(p.dados, p.dados + p.tamDados);
  m->ultimoRxMs = millis();
  m->nacks = 0;
  tabela_.liberarEspaco(p.msgId);
  verificarCompleta(*m, conn);
}

void No::verificarCompleta(Mensagem& m, int conn) {
  if (m.estado == Estado::Incompleta && m.completa()) {
    m.estado = Estado::Pendente;
    mensagemCompleta(m, conn);
  } else {
    salvar(m);
  }
}

void No::mensagemCompleta(Mensagem& m, int conn) {
  uint32_t agora = millis();
  painel_.log("OK %04X %lX>%lX %uB", (unsigned)(m.env.msgId & 0xFFFF), (unsigned long)m.env.remetente,
              (unsigned long)m.env.destino, (unsigned)m.bytes());
  m.proximoReenvioMs = agora + comVariacao(cfg::REENVIOS_MS[0]);
  m.reenvios = 0;
  salvar(m);
  if (conn >= 0) enviarEvento(conn, gatt::EVT_ACEITA, m.env.msgId);

  bool destinoAqui = entregarLocal(m);
  // Destinatário conectado aqui: não precisa espalhar (o ACK vem em seguida).
  if (destinoAqui && !(m.env.flags & flag::Grupo)) return;
  uint8_t ttl = conn >= 0 ? TTL_PADRAO : (m.env.ttl > 0 ? m.env.ttl - 1 : 0);
  if (ttl > 0) enfileirarMensagem(m, ttl, aleatorio(cfg::ATRASO_TX_MIN_MS, cfg::ATRASO_TX_MAX_MS));
}

void No::marcarEntregue(Mensagem& m, bool comAck) {
  m.estado = Estado::Entregue;
  m.blocos.clear();
  m.blocos.shrink_to_fit();
  m.criadaMs = millis();  // o registro de entregue conta 7 dias a partir daqui
  removerDaFila(m.env.msgId);
  tabela_.recalcularBytes();
  salvar(m);
  painel_.log("ENTREGUE %04X", (unsigned)(m.env.msgId & 0xFFFF));
  if (comAck) enviarAck(m.env.msgId, TTL_PADRAO), m.ultimoAckMs = millis();
  for (auto& kv : usuarios_)
    if (kv.second.registrado && kv.second.id == m.env.remetente) enviarEvento(kv.first, gatt::EVT_ENTREGUE, m.env.msgId);
}

void No::receberAck(const Pacote& p) {
  Mensagem* m = tabela_.buscar(p.msgId);
  if (m && m->estado == Estado::Entregue) return;  // ACK já processado
  if (!m) {
    // Não conhecíamos a mensagem: guarda o registro para ignorar cópias futuras.
    Mensagem& r = tabela_.criar(p.msgId, millis());
    r.env.msgId = p.msgId;
    m = &r;
  }
  marcarEntregue(*m, false);
  if (p.ttl > 1) enviarAck(p.msgId, p.ttl - 1);
}

void No::receberNack(const Pacote& p) {
  Mensagem* m = tabela_.buscar(p.msgId);
  if (!m) return;
  if (m->estado == Estado::Entregue) {
    repetirAck(*m);
    return;
  }
  if (!m->emBlocos()) return;
  unsigned n = 0;
  if (bitmapTem(p.bitmap, BIT_NACK_CABECALHO) && m->temCabecalho) {
    enfileirarTx(pacoteCabecalho(*m, 1), m->env.msgId, -1, true, false,
                 aleatorio(100, cfg::RESPOSTA_NACK_MAX_MS));
    n++;
  }
  for (size_t i = 0; i < m->blocos.size() && i < MAX_BLOCOS; i++) {
    if (bitmapTem(p.bitmap, i) && m->temBloco(i)) {
      enfileirarTx(pacoteBloco(*m, i, 1), m->env.msgId, i, true, false, aleatorio(100, cfg::RESPOSTA_NACK_MAX_MS));
      n++;
    }
  }
  if (n) painel_.log("NACK %04X: envio %u", (unsigned)(p.msgId & 0xFFFF), n);
}

// Um vizinho acabou de (re)transmitir esta mensagem: o nosso reenvio periódico
// pode esperar o próximo ciclo, evitando que todos reenviem a mesma coisa.
void No::adiarReenvio(Mensagem& m) {
  uint32_t agora = millis();
  constexpr size_t N = sizeof(cfg::REENVIOS_MS) / sizeof(cfg::REENVIOS_MS[0]);
  uint32_t intervalo = m.reenvios < N ? cfg::REENVIOS_MS[m.reenvios] : cfg::REENVIO_DEPOIS_MS;
  if ((int32_t)(m.proximoReenvioMs - agora) < (int32_t)(intervalo / 2))
    m.proximoReenvioMs = agora + comVariacao(intervalo);
}

void No::repetirAck(Mensagem& m) {
  if (millis() - m.ultimoAckMs < cfg::ACK_REPETIDO_MIN_MS && m.ultimoAckMs) return;
  m.ultimoAckMs = millis();
  enviarAck(m.env.msgId, TTL_PADRAO);
}

// ---------------------------------------------------------------- transmissão

void No::enfileirarMensagem(const Mensagem& m, uint8_t ttl, uint32_t atrasoMs) {
  std::vector<Bytes> pacotes = m.pacotes(ttl);
  for (size_t i = 0; i < pacotes.size(); i++)
    enfileirarTx(pacotes[i], m.env.msgId, m.emBlocos() ? (int16_t)i - 1 : -1, false, false, atrasoMs);
}

void No::enfileirarTx(const Bytes& dados, uint32_t msgId, int16_t bloco, bool resposta, bool controle,
                      uint32_t atrasoMs) {
  if (dados.empty()) return;
  if (resposta) {
    for (auto& it : filaTx_)
      if (it.msgId == msgId && it.bloco == bloco && !it.controle) return;  // já vai sair
  }
  filaTx_.push_back({dados, millis() + atrasoMs, msgId, bloco, resposta, controle, 0});
}

void No::enviarAck(uint32_t msgId, uint8_t ttl) {
  Pacote p;
  p.idi = Idi::Ack;
  p.ttl = ttl;
  p.msgId = msgId;
  enfileirarTx(serializar(p), msgId, -1, false, true, aleatorio(cfg::ATRASO_TX_MIN_MS, cfg::ATRASO_TX_MAX_MS));
}

void No::enviarNack(Mensagem& m) {
  Pacote p;
  p.idi = Idi::Nack;
  p.ttl = 1;  // só os vizinhos respondem; eles pedem aos vizinhos deles se também faltar
  p.msgId = m.env.msgId;
  if (!m.temCabecalho) bitmapMarcar(p.bitmap, BIT_NACK_CABECALHO);
  size_t limite = m.temCabecalho ? m.nBlocos : m.blocos.size();
  unsigned faltando = 0;
  for (size_t i = 0; i < limite; i++)
    if (!m.temBloco(i)) bitmapMarcar(p.bitmap, i), faltando++;
  m.nacks++;
  m.ultimoNackMs = millis();
  painel_.log("NACK %04X falta %u%s", (unsigned)(m.env.msgId & 0xFFFF), faltando, m.temCabecalho ? "" : "+cab");
  enfileirarTx(serializar(p), m.env.msgId, -1, false, true, aleatorio(cfg::ATRASO_TX_MIN_MS, cfg::ATRASO_TX_MAX_MS));
}

void No::removerDaFila(uint32_t msgId) {
  filaTx_.erase(std::remove_if(filaTx_.begin(), filaTx_.end(),
                               [&](const ItemTx& it) { return it.msgId == msgId && !it.controle; }),
                filaTx_.end());
}

void No::suprimirResposta(uint32_t msgId, int16_t bloco) {
  filaTx_.erase(std::remove_if(filaTx_.begin(), filaTx_.end(),
                               [&](const ItemTx& it) { return it.resposta && it.msgId == msgId && it.bloco == bloco; }),
                filaTx_.end());
}

bool No::naFila(uint32_t msgId) const {
  for (auto& it : filaTx_)
    if (it.msgId == msgId && !it.controle) return true;
  return false;
}

void No::processarFilaTx() {
  if (filaTx_.empty()) return;
  uint32_t agora = millis();
  ItemTx& it = filaTx_.front();
  if (!passou(agora, it.prontoEm)) return;
  if (agora - ultimoTxMs_ < cfg::INTERVALO_ENTRE_TX_MS) return;
  if (radio_.ultimoRxMs() && agora - radio_.ultimoRxMs() < cfg::SILENCIO_APOS_RX_MS) return;
  if (!radio_.canalLivre() && ++it.tentativas < cfg::MAX_TENTATIVAS_CAD) {
    it.prontoEm = agora + aleatorio(100, 500);
    return;
  }
  ItemTx item = std::move(it);
  filaTx_.pop_front();
  if (!filaTx_.empty() && filaTx_.front().msgId != item.msgId && !item.controle) {
    uint32_t pausa = millis() + aleatorio(cfg::PAUSA_ENTRE_MSGS_MIN_MS, cfg::PAUSA_ENTRE_MSGS_MAX_MS);
    if (!passou(filaTx_.front().prontoEm, pausa)) filaTx_.front().prontoEm = pausa;
  }
  uint32_t duracao;
  bool ok = radio_.transmitir(item.dados.data(), item.dados.size(), duracao);
  ultimoTxMs_ = millis();
  contTx_++;
  Pacote p;
  decodificar(item.dados.data(), item.dados.size(), p);
  if (p.idi == Idi::Bloco)
    painel_.log("TX BLK %04X b%u %lums%s", (unsigned)(p.msgId & 0xFFFF), p.bloco, (unsigned long)duracao, ok ? "" : " FALHA");
  else
    painel_.log("TX %s %04X t%u %lums%s", nomeIdi(p.idi), (unsigned)(p.msgId & 0xFFFF), p.ttl, (unsigned long)duracao,
                ok ? "" : " FALHA");
}

// ---------------------------------------------------------------- BLE

void No::processarBle() {
  EventoBle e;
  while (ble_.proximoEvento(e)) {
    switch (e.tipo) {
      case EventoBle::Conectou:
        usuarios_[e.conn] = Usuario();
        painel_.log("BLE conectou (%u)", (unsigned)ble_.conexoes());
        break;
      case EventoBle::Desconectou:
        painel_.log("BLE saiu %lX", (unsigned long)usuarios_[e.conn].id);
        usuarios_.erase(e.conn);
        filaBle_.erase(std::remove_if(filaBle_.begin(), filaBle_.end(),
                                      [&](const ItemBle& it) { return it.conn == e.conn; }),
                       filaBle_.end());
        break;
      case EventoBle::Pacote: {
        Pacote p;
        if (!decodificar(e.dados, e.tam, p)) {
          enviarEvento(e.conn, gatt::EVT_ERRO, 0, gatt::ERRO_PACOTE_INVALIDO);
          break;
        }
        if (p.idi != Idi::Bloco)
          painel_.log("BLE> %s %04X", nomeIdi(p.idi), (unsigned)(p.msgId & 0xFFFF));
        tratarPacote(p, e.conn);
        break;
      }
      case EventoBle::Controle:
        tratarControle(e.conn, e.dados, e.tam);
        break;
    }
  }
}

void No::tratarControle(uint16_t conn, const uint8_t* d, size_t tam) {
  if (tam < 1) return;
  Usuario& u = usuarios_[conn];
  if (d[0] == gatt::CMD_REGISTRAR && tam >= 6) {
    uint8_t n = d[5];
    if (n > gatt::MAX_GRUPOS || tam < 6u + 4u * n || lerU32(d + 1) == 0) {
      enviarEvento(conn, gatt::EVT_ERRO, 0, gatt::ERRO_PACOTE_INVALIDO);
      return;
    }
    u.id = lerU32(d + 1);
    u.grupos.clear();
    for (uint8_t i = 0; i < n; i++) u.grupos.push_back(lerU32(d + 6 + 4 * i));
    u.registrado = true;

    // Entrega o que estava guardado para este usuário (ou para os grupos dele).
    std::vector<Mensagem*> pendentes;
    for (auto& kv : tabela_.todas()) {
      Mensagem& m = kv.second;
      if (m.estado != Estado::Pendente) continue;
      bool grupo = m.env.flags & flag::Grupo;
      if ((!grupo && m.env.destino == u.id) ||
          (grupo && contem(u.grupos, m.env.destino) && m.env.remetente != u.id && !contem(m.entreguesGrupo, u.id)))
        pendentes.push_back(&m);
    }
    painel_.log("REG %lX g%u pend %u", (unsigned long)u.id, n, (unsigned)pendentes.size());
    enviarEvento(conn, gatt::EVT_REGISTRADO, 0, pendentes.size());
    for (Mensagem* m : pendentes) {
      enviarMensagemBle(conn, *m);
      if (m->env.flags & flag::Grupo) m->entreguesGrupo.push_back(u.id), salvar(*m);
    }
  } else if (d[0] == gatt::CMD_CONFIRMAR && tam >= 5) {
    uint32_t id = lerU32(d + 1);
    Mensagem* m = tabela_.buscar(id);
    if (!m || !u.registrado) return;
    // Mensagem de grupo não tem ACK (vários destinatários): expira por tempo.
    if (!(m->env.flags & flag::Grupo) && m->env.destino == u.id && m->estado != Estado::Entregue)
      marcarEntregue(*m, true);
  }
}

bool No::entregarLocal(Mensagem& m) {
  bool destinoAqui = false;
  bool grupo = m.env.flags & flag::Grupo;
  for (auto& kv : usuarios_) {
    Usuario& u = kv.second;
    if (!u.registrado) continue;
    if (!grupo && u.id == m.env.destino) {
      enviarMensagemBle(kv.first, m);
      destinoAqui = true;
    } else if (grupo && contem(u.grupos, m.env.destino) && u.id != m.env.remetente && !contem(m.entreguesGrupo, u.id)) {
      enviarMensagemBle(kv.first, m);
      m.entreguesGrupo.push_back(u.id);
      salvar(m);
    }
  }
  return destinoAqui;
}

void No::enviarMensagemBle(uint16_t conn, const Mensagem& m) {
  painel_.log("BLE< %04X para %lX", (unsigned)(m.env.msgId & 0xFFFF), (unsigned long)usuarios_[conn].id);
  for (auto& pkt : m.pacotes(m.env.ttl)) enviarBle(conn, false, pkt);
}

void No::enviarEvento(uint16_t conn, uint8_t evt, uint32_t msgId, int extra) {
  Bytes b{evt};
  if (evt == gatt::EVT_REGISTRADO) {
    b.push_back(extra < 0 ? 0 : (extra > 255 ? 255 : extra));
  } else {
    if (evt == gatt::EVT_ERRO) b.push_back(extra);
    for (int i = 0; i < 4; i++) b.push_back(msgId >> (8 * i));
  }
  enviarBle(conn, true, b);
}

void No::enviarBle(uint16_t conn, bool controle, const Bytes& dados) { filaBle_.push_back({conn, controle, dados}); }

void No::processarFilaBle() {
  if (filaBle_.empty() || millis() - ultimaNotificacaoMs_ < cfg::INTERVALO_NOTIFICACAO_MS) return;
  ItemBle& it = filaBle_.front();
  if (!usuarios_.count(it.conn)) {
    filaBle_.pop_front();
    return;
  }
  bool ok = it.controle ? ble_.notificarControle(it.conn, it.dados.data(), it.dados.size())
                        : ble_.notificarPacote(it.conn, it.dados.data(), it.dados.size());
  ultimaNotificacaoMs_ = millis();
  if (ok) filaBle_.pop_front();  // sem memória na pilha BLE: tenta de novo no próximo ciclo
}

// ---------------------------------------------------------------- manutenção

void No::manutencao() {
  uint32_t agora = millis();
  std::vector<uint32_t> remover;
  for (auto& kv : tabela_.todas()) {
    Mensagem& m = kv.second;
    uint32_t idade = agora - m.criadaMs;
    if (m.estado == Estado::Entregue) {
      if (idade > cfg::EXPIRACAO_ENTREGUE_MS) remover.push_back(kv.first);
      continue;
    }
    if (idade > cfg::EXPIRACAO_PENDENTE_MS) {
      painel_.log("EXPIROU %04X", (unsigned)(kv.first & 0xFFFF));
      remover.push_back(kv.first);
      continue;
    }
    if (m.estado == Estado::Incompleta) {
      if (agora - m.ultimoRxMs > cfg::NACK_ESPERA_MS && m.nacks < cfg::NACK_MAX &&
          (m.ultimoNackMs == 0 || agora - m.ultimoNackMs > cfg::NACK_ESPERA_MS))
        enviarNack(m);
    } else if (passou(agora, m.proximoReenvioMs)) {
      // Reenvio periódico com TTL cheio para alcançar quem perdeu a mensagem.
      if (!naFila(m.env.msgId)) {
        painel_.log("REENVIO %04X #%u", (unsigned)(kv.first & 0xFFFF), m.reenvios + 1);
        enfileirarMensagem(m, TTL_PADRAO, aleatorio(cfg::ATRASO_TX_MIN_MS, cfg::ATRASO_TX_MAX_MS));
      }
      m.reenvios++;
      constexpr size_t N = sizeof(cfg::REENVIOS_MS) / sizeof(cfg::REENVIOS_MS[0]);
      m.proximoReenvioMs = agora + comVariacao(m.reenvios < N ? cfg::REENVIOS_MS[m.reenvios] : cfg::REENVIO_DEPOIS_MS);
    }
  }
  for (uint32_t id : remover) {
    tabela_.remover(id);
    pers_.remover(id);
    removerDaFila(id);
  }
}

// ---------------------------------------------------------------- display

void No::linhasStatus(std::vector<String>& l) {
  char b[48];
  snprintf(b, sizeof b, "%.0fMHz SF%u BW%.0f %ddBm", cfg::FREQUENCIA_MHZ, cfg::SPREADING_FACTOR,
           cfg::LARGURA_BANDA_KHZ, cfg::POTENCIA_DBM);
  l.push_back(b);
  snprintf(b, sizeof b, "Msg P%u I%u E%u  %uB", (unsigned)tabela_.contar(Estado::Pendente),
           (unsigned)tabela_.contar(Estado::Incompleta), (unsigned)tabela_.contar(Estado::Entregue),
           (unsigned)tabela_.bytes());
  l.push_back(b);
  snprintf(b, sizeof b, "RX%lu TX%lu CRC%lu fila%u", (unsigned long)contRx_, (unsigned long)contTx_,
           (unsigned long)contCrc_, (unsigned)filaTx_.size());
  l.push_back(b);
  snprintf(b, sizeof b, "RSSI %.0f SNR %.1f", radio_.ultimoRssi(), radio_.ultimoSnr());
  l.push_back(b);
  String ble = "BLE " + String((unsigned)ble_.conexoes()) + ":";
  for (auto& kv : usuarios_)
    if (kv.second.registrado) ble += " " + String(kv.second.id, HEX);
  l.push_back(ble);
}

void No::linhasTabela(std::vector<String>& l) {
  if (tabela_.todas().empty()) {
    l.push_back("(vazia)");
    return;
  }
  // As mais recentes primeiro.
  std::vector<const Mensagem*> ms;
  for (auto& kv : tabela_.todas()) ms.push_back(&kv.second);
  std::sort(ms.begin(), ms.end(), [](const Mensagem* a, const Mensagem* b) { return (int32_t)(a->criadaMs - b->criadaMs) > 0; });
  char b[48];
  for (const Mensagem* m : ms) {
    if (l.size() >= 5) break;
    if (m->emBlocos())
      snprintf(b, sizeof b, "%04X %c %lX>%lX %u/%u", (unsigned)(m->env.msgId & 0xFFFF), letraEstado(m->estado),
               (unsigned long)m->env.remetente, (unsigned long)m->env.destino, (unsigned)m->blocosRecebidos(),
               m->nBlocos);
    else
      snprintf(b, sizeof b, "%04X %c %lX>%lX", (unsigned)(m->env.msgId & 0xFFFF), letraEstado(m->estado),
               (unsigned long)m->env.remetente, (unsigned long)m->env.destino);
    l.push_back(b);
  }
}
