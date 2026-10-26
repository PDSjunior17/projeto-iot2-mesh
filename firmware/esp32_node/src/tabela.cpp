#include "tabela.h"

#include "config.h"

using namespace redelora;

size_t Mensagem::blocosRecebidos() const {
  size_t n = 0;
  for (auto& b : blocos) n += !b.empty();
  return n;
}

bool Mensagem::completa() const {
  if (!emBlocos()) return !blocos.empty();
  return temCabecalho && blocos.size() == nBlocos && blocosRecebidos() == nBlocos;
}

size_t Mensagem::bytes() const {
  size_t n = 0;
  for (auto& b : blocos) n += b.size();
  return n;
}

Bytes Mensagem::conteudo() const {
  Bytes c;
  for (auto& b : blocos) c.insert(c.end(), b.begin(), b.end());
  return c;
}

std::vector<Bytes> Mensagem::pacotes(uint8_t ttl) const {
  std::vector<Bytes> saida;
  if (!completa()) return saida;
  Envelope e = env;
  e.ttl = ttl;
  if (!emBlocos()) {
    // IDI 0 ou 3: um pacote só.
    Pacote p;
    p.idi = idi;
    p.flags = e.flags;
    p.ttl = ttl;
    p.msgId = e.msgId;
    p.destino = e.destino;
    p.remetente = e.remetente;
    p.dados = blocos[0].data();
    p.tamDados = blocos[0].size();
    uint8_t buf[MAX_PACOTE];
    size_t n = codificar(p, buf, sizeof buf);
    if (n) saida.emplace_back(buf, buf + n);
    return saida;
  }
  Bytes c = conteudo();
  fragmentar(e, c.data(), c.size(), saida);
  return saida;
}

Mensagem* Tabela::buscar(uint32_t msgId) {
  auto it = msgs_.find(msgId);
  return it == msgs_.end() ? nullptr : &it->second;
}

Mensagem& Tabela::criar(uint32_t msgId, uint32_t agoraMs) {
  liberarEspaco(0);
  Mensagem& m = msgs_[msgId];
  m = Mensagem();
  m.env.msgId = msgId;
  m.criadaMs = agoraMs;
  m.ultimoRxMs = agoraMs;
  return m;
}

void Tabela::remover(uint32_t msgId) {
  msgs_.erase(msgId);
  recalcularBytes();
}

void Tabela::recalcularBytes() {
  bytes_ = 0;
  for (auto& kv : msgs_) bytes_ += kv.second.bytes();
}

// Sem espaço: apaga primeiro registros de entregues, depois as mensagens mais antigas.
void Tabela::liberarEspaco(uint32_t protegido) {
  recalcularBytes();
  while (!msgs_.empty() && (msgs_.size() >= cfg::MAX_MENSAGENS || bytes_ > cfg::MAX_BYTES_DADOS)) {
    auto vitima = msgs_.end();
    for (auto it = msgs_.begin(); it != msgs_.end(); ++it) {
      if (it->first == protegido) continue;
      if (vitima == msgs_.end()) {
        vitima = it;
        continue;
      }
      bool eEntregue = it->second.estado == Estado::Entregue;
      bool vEntregue = vitima->second.estado == Estado::Entregue;
      if (eEntregue != vEntregue) {
        if (eEntregue) vitima = it;
      } else if ((int32_t)(it->second.criadaMs - vitima->second.criadaMs) < 0) {
        vitima = it;
      }
    }
    if (vitima == msgs_.end()) break;
    msgs_.erase(vitima);
    recalcularBytes();
  }
}

size_t Tabela::contar(Estado e) const {
  size_t n = 0;
  for (auto& kv : msgs_) n += kv.second.estado == e;
  return n;
}
