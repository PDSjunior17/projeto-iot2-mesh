#include "painel.h"

#include <stdarg.h>

Painel::Painel(SSD1306Wire& display, const char* titulo) : display_(display), titulo_(titulo) {}

void Painel::log(const char* fmt, ...) {
  char buf[96];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof buf, fmt, args);
  va_end(args);
  uint32_t s = millis() / 1000;
  Serial.printf("[%02lu:%02lu:%02lu] %s\n", s / 3600, (s / 60) % 60, s % 60, buf);
  linhas_.push_back(buf);
  while (linhas_.size() > LINHAS_LOG) linhas_.pop_front();
  if (pagina_ == LOG) sujo_ = true;
}

void Painel::definirProvedores(Provedor status, Provedor tabela) {
  status_ = status;
  tabela_ = tabela;
}

void Painel::proximaPagina() {
  pagina_ = (pagina_ + 1) % TOTAL;
  sujo_ = true;
}

void Painel::atualizar() {
  uint32_t agora = millis();
  // Status e tabela mudam sozinhos (tempos, contadores): redesenha a cada 1 s.
  if (pagina_ != LOG && agora - ultimoDesenho_ > 1000) sujo_ = true;
  if (!sujo_ || agora - ultimoDesenho_ < 250) return;
  ultimoDesenho_ = agora;
  sujo_ = false;

  std::vector<String> linhas;
  switch (pagina_) {
    case LOG:
      linhas.assign(linhas_.begin(), linhas_.end());
      desenhar("LOG", linhas);
      break;
    case STATUS:
      if (status_) status_(linhas);
      desenhar("STATUS", linhas);
      break;
    case TABELA:
      if (tabela_) tabela_(linhas);
      desenhar("TABELA", linhas);
      break;
  }
}

void Painel::desenhar(const char* nome, const std::vector<String>& linhas) {
  display_.clear();
  display_.setFont(ArialMT_Plain_10);
  display_.setTextAlignment(TEXT_ALIGN_LEFT);
  display_.drawString(0, 0, titulo_);
  display_.setTextAlignment(TEXT_ALIGN_RIGHT);
  display_.drawString(127, 0, nome);
  display_.setTextAlignment(TEXT_ALIGN_LEFT);
  display_.drawHorizontalLine(0, 12, 128);
  int y = 13;
  for (size_t i = 0; i < linhas.size() && i < LINHAS_LOG; i++, y += 10) display_.drawString(0, y, linhas[i]);
  display_.display();
}
