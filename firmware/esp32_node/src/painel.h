// Logs e telas de depuração no OLED da Heltec V3 (e na serial).
// Botão PRG: alterna entre as páginas LOG, STATUS e TABELA.
#pragma once

#include <Arduino.h>
#include <SSD1306Wire.h>

#include <deque>
#include <functional>
#include <vector>

class Painel {
 public:
  using Provedor = std::function<void(std::vector<String>&)>;

  Painel(SSD1306Wire& display, const char* titulo);

  // Escreve na serial (com tempo) e no buffer da página LOG.
  void log(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

  void definirProvedores(Provedor status, Provedor tabela);
  void proximaPagina();
  void atualizar();  // chamar no loop(); redesenha no máximo a cada 250 ms

 private:
  enum Pagina { LOG, STATUS, TABELA, TOTAL };
  void desenhar(const char* nome, const std::vector<String>& linhas);

  SSD1306Wire& display_;
  String titulo_;
  std::deque<String> linhas_;
  Provedor status_, tabela_;
  int pagina_ = LOG;
  bool sujo_ = true;
  uint32_t ultimoDesenho_ = 0;
  static constexpr size_t LINHAS_LOG = 5;
};
