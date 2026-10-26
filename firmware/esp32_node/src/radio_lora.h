// Camada de rádio: configura o SX1262, recebe por interrupção, verifica o canal
// (CAD) e transmite. O CRC-16 é do próprio LoRa: pacotes corrompidos são descartados.
#pragma once

#include <RadioLib.h>

class RadioLoRa {
 public:
  explicit RadioLoRa(SX1262& radio) : radio_(radio) {}

  int iniciar();  // retorna o código do RadioLib (0 = ok)

  // Se chegou um pacote, copia para `buf` e retorna true.
  // `erroCrc` indica pacote descartado por CRC inválido.
  bool receber(uint8_t* buf, size_t cap, size_t& tam, bool& erroCrc);
  bool canalLivre();
  bool transmitir(const uint8_t* buf, size_t tam, uint32_t& duracaoMs);

  float ultimoRssi() const { return rssi_; }
  float ultimoSnr() const { return snr_; }
  uint32_t ultimoRxMs() const { return ultimoRxMs_; }
  uint32_t tempoNoAr(size_t tam) { return radio_.getTimeOnAir(tam) / 1000; }

 private:
  static void IRAM_ATTR aoInterromper();
  void voltarARecepcao();

  SX1262& radio_;
  float rssi_ = 0, snr_ = 0;
  uint32_t ultimoRxMs_ = 0;
  static volatile bool sinal_;
};
