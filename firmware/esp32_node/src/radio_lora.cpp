#include "radio_lora.h"

#include "config.h"

volatile bool RadioLoRa::sinal_ = false;

void IRAM_ATTR RadioLoRa::aoInterromper() { sinal_ = true; }

int RadioLoRa::iniciar() {
  int s = radio_.begin(cfg::FREQUENCIA_MHZ, cfg::LARGURA_BANDA_KHZ, cfg::SPREADING_FACTOR,
                       cfg::CODING_RATE, cfg::PALAVRA_SINCRONIA, cfg::POTENCIA_DBM, cfg::PREAMBULO);
  if (s != RADIOLIB_ERR_NONE) return s;
  // CRC-16 do LoRa (já é o padrão do begin(); deixado explícito). Com cabeçalho
  // explícito o receptor sabe que há CRC e o readData() devolve
  // RADIOLIB_ERR_CRC_MISMATCH quando o pacote chega corrompido.
  s = radio_.setCRC(2);
  if (s != RADIOLIB_ERR_NONE) return s;
  voltarARecepcao();
  return RADIOLIB_ERR_NONE;
}

void RadioLoRa::voltarARecepcao() {
  sinal_ = false;
  radio_.setDio1Action(aoInterromper);
  radio_.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF);
}

bool RadioLoRa::receber(uint8_t* buf, size_t cap, size_t& tam, bool& erroCrc) {
  erroCrc = false;
  if (!sinal_) return false;
  sinal_ = false;
  size_t n = radio_.getPacketLength();
  if (n == 0 || n > cap) {
    radio_.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF);
    return false;
  }
  int s = radio_.readData(buf, n);
  ultimoRxMs_ = millis();
  rssi_ = radio_.getRSSI();
  snr_ = radio_.getSNR();
  radio_.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF);
  if (s == RADIOLIB_ERR_CRC_MISMATCH) {
    erroCrc = true;
    return false;
  }
  if (s != RADIOLIB_ERR_NONE) return false;
  tam = n;
  return true;
}

bool RadioLoRa::canalLivre() {
  if (sinal_) return false;  // chegou algo: deixa o loop ler primeiro
  // Primeiro o RSSI instantâneo (não atrapalha a recepção). Só se o canal
  // parecer quieto fazemos o CAD, que detecta preâmbulos abaixo do ruído.
  if (radio_.getRSSI(false) > cfg::LIMIAR_CANAL_OCUPADO_DBM) return false;
  radio_.clearDio1Action();
  int s = radio_.scanChannel();
  voltarARecepcao();
  return s == RADIOLIB_CHANNEL_FREE;
}

bool RadioLoRa::transmitir(const uint8_t* buf, size_t tam, uint32_t& duracaoMs) {
  radio_.clearDio1Action();
  uint32_t t = millis();
  int s = radio_.transmit(buf, tam);
  duracaoMs = millis() - t;
  voltarARecepcao();
  return s == RADIOLIB_ERR_NONE;
}
