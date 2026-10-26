// Persistência no cartão SD. DESATIVADA até o módulo ser ligado:
// habilite com -DUSAR_CARTAO_SD em platformio.ini.
#ifdef USAR_CARTAO_SD

#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include "config.h"
#include "persistencia.h"

using namespace redelora;

namespace {

// O rádio usa o barramento SPI padrão (FSPI); o cartão fica no HSPI.
SPIClass spiSD(HSPI);
constexpr const char* DIRETORIO = "/rede";
constexpr uint32_t MAGICO = 0x314D4C52;  // "RLM1"

String caminho(uint32_t msgId) {
  char nome[24];
  snprintf(nome, sizeof nome, "%s/%08lX.msg", DIRETORIO, (unsigned long)msgId);
  return nome;
}

template <typename T>
void escrever(File& f, const T& v) {
  f.write((const uint8_t*)&v, sizeof v);
}

template <typename T>
bool ler(File& f, T& v) {
  return f.read((uint8_t*)&v, sizeof v) == sizeof v;
}

}  // namespace

bool PersistenciaSD::iniciar() {
  spiSD.begin(cfg::SD_SCK, cfg::SD_MISO, cfg::SD_MOSI, cfg::SD_CS);
  ok_ = SD.begin(cfg::SD_CS, spiSD, cfg::SD_FREQUENCIA_HZ);
  if (ok_ && !SD.exists(DIRETORIO)) ok_ = SD.mkdir(DIRETORIO);
  return ok_;
}

void PersistenciaSD::salvar(const Mensagem& m) {
  if (!ok_) return;
  String tmp = caminho(m.env.msgId) + ".tmp";
  File f = SD.open(tmp, FILE_WRITE);
  if (!f) return;
  escrever(f, MAGICO);
  escrever(f, (uint8_t)m.estado);
  escrever(f, (uint8_t)m.idi);
  escrever(f, m.env.flags);
  escrever(f, m.env.ttl);
  escrever(f, m.env.msgId);
  escrever(f, m.env.destino);
  escrever(f, m.env.remetente);
  escrever(f, (uint8_t)m.temCabecalho);
  escrever(f, m.nBlocos);
  escrever(f, (uint8_t)m.origemLocal);
  escrever(f, (uint8_t)m.entreguesGrupo.size());
  for (uint32_t u : m.entreguesGrupo) escrever(f, u);
  escrever(f, (uint16_t)m.blocos.size());
  for (auto& b : m.blocos) {
    escrever(f, (uint8_t)b.size());  // 0 = bloco ainda não recebido
    if (!b.empty()) f.write(b.data(), b.size());
  }
  f.close();
  // Troca atômica: um desligamento no meio da escrita não corrompe a versão anterior.
  SD.remove(caminho(m.env.msgId));
  SD.rename(tmp, caminho(m.env.msgId));
}

void PersistenciaSD::remover(uint32_t msgId) {
  if (ok_) SD.remove(caminho(msgId));
}

size_t PersistenciaSD::carregar(Tabela& t, uint32_t agoraMs) {
  if (!ok_) return 0;
  File dir = SD.open(DIRETORIO);
  if (!dir) return 0;
  size_t n = 0;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (String(f.name()).endsWith(".tmp")) {  // escrita interrompida
      f.close();
      continue;
    }
    uint32_t magico = 0, msgId = 0, dest = 0, rem = 0;
    uint8_t estado, idi, flags, ttl, temCab, nBlocos, origem, nGrupo;
    uint16_t qtdBlocos;
    bool ok = ler(f, magico) && magico == MAGICO && ler(f, estado) && ler(f, idi) && ler(f, flags) &&
              ler(f, ttl) && ler(f, msgId) && ler(f, dest) && ler(f, rem) && ler(f, temCab) &&
              ler(f, nBlocos) && ler(f, origem) && ler(f, nGrupo);
    if (!ok || msgId == 0) {
      f.close();
      continue;
    }
    // Sem relógio de tempo real: a idade recomeça a contar do boot.
    Mensagem& m = t.criar(msgId, agoraMs);
    m.estado = (Estado)estado;
    m.idi = (Idi)idi;
    m.env = {msgId, dest, rem, flags, ttl};
    m.temCabecalho = temCab;
    m.nBlocos = nBlocos;
    m.origemLocal = origem;
    for (uint8_t i = 0; i < nGrupo; i++) {
      uint32_t u;
      if (ler(f, u)) m.entreguesGrupo.push_back(u);
    }
    if (ler(f, qtdBlocos)) {
      m.blocos.resize(qtdBlocos);
      for (auto& b : m.blocos) {
        uint8_t tam;
        if (!ler(f, tam)) break;
        b.resize(tam);
        if (tam) f.read(b.data(), tam);
      }
    }
    m.proximoReenvioMs = agoraMs + cfg::REENVIOS_MS[0];
    f.close();
    n++;
  }
  dir.close();
  return n;
}

#endif  // USAR_CARTAO_SD
