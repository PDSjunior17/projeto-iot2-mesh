// Persistência da tabela de mensagens.
//
// Hoje o nó roda SEM cartão SD: a tabela vive só na RAM (PersistenciaRAM) e
// se perde ao reiniciar. O código do cartão SD está pronto, mas desativado:
// para ligar, ligue o módulo nos pinos de config.h e descomente
// -DUSAR_CARTAO_SD em platformio.ini.
#pragma once

#include "tabela.h"

class Persistencia {
 public:
  virtual ~Persistencia() = default;
  virtual bool iniciar() = 0;
  virtual void salvar(const Mensagem& m) = 0;
  virtual void remover(uint32_t msgId) = 0;
  virtual size_t carregar(Tabela& t, uint32_t agoraMs) = 0;  // retorna quantas carregou
  virtual const char* nome() const = 0;
};

// Sem persistência: a tabela já está na RAM.
class PersistenciaRAM : public Persistencia {
 public:
  bool iniciar() override { return true; }
  void salvar(const Mensagem&) override {}
  void remover(uint32_t) override {}
  size_t carregar(Tabela&, uint32_t) override { return 0; }
  const char* nome() const override { return "RAM"; }
};

#ifdef USAR_CARTAO_SD
// Um arquivo por mensagem em /rede/<msgId>.msg (formato em docs/firmware_no.md).
class PersistenciaSD : public Persistencia {
 public:
  bool iniciar() override;
  void salvar(const Mensagem& m) override;
  void remover(uint32_t msgId) override;
  size_t carregar(Tabela& t, uint32_t agoraMs) override;
  const char* nome() const override { return "SD"; }

 private:
  bool ok_ = false;
};
#endif
