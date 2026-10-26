// Lógica do nó: recebe por BLE e LoRa, guarda, retransmite (flooding com TTL),
// recupera blocos perdidos (NACK), entrega por BLE e propaga a confirmação (ACK).
#pragma once

#include <Arduino.h>

#include <deque>
#include <map>
#include <vector>

#include "painel.h"
#include "persistencia.h"
#include "radio_lora.h"
#include "servidor_ble.h"
#include "tabela.h"

class No {
 public:
  No(RadioLoRa& radio, ServidorBle& ble, Persistencia& pers, Painel& painel)
      : radio_(radio), ble_(ble), pers_(pers), painel_(painel) {}

  void iniciar();
  void loop();

  // Conteúdo das páginas STATUS e TABELA do display.
  void linhasStatus(std::vector<String>& linhas);
  void linhasTabela(std::vector<String>& linhas);

  // Teste: descarta os próximos `n` blocos recebidos pelo rádio (força o NACK).
  void descartarProximosBlocos(uint8_t n) { descartarBlocos_ = n; }

 private:
  struct Usuario {
    uint32_t id = 0;
    std::vector<uint32_t> grupos;
    bool registrado = false;
  };
  struct ItemTx {
    redelora::Bytes dados;
    uint32_t prontoEm;
    uint32_t msgId;
    int16_t bloco;     // -1 = cabeçalho ou mensagem única
    bool resposta;     // resposta a NACK (pode ser suprimida)
    bool controle;     // ACK/NACK
    uint8_t tentativas;
  };
  struct ItemBle {
    uint16_t conn;
    bool controle;
    redelora::Bytes dados;
  };

  // Recepção (conn < 0: veio pelo rádio; conn >= 0: veio do dispositivo BLE)
  void processarRadio();
  void tratarPacote(redelora::Pacote p, int conn);
  void receberMensagemUnica(const redelora::Pacote& p, int conn);
  void receberCabecalho(const redelora::Pacote& p, int conn);
  void receberBloco(const redelora::Pacote& p, int conn);
  void receberAck(const redelora::Pacote& p);
  void receberNack(const redelora::Pacote& p);
  void verificarCompleta(Mensagem& m, int conn);
  void mensagemCompleta(Mensagem& m, int conn);
  void marcarEntregue(Mensagem& m, bool comAck);
  void repetirAck(Mensagem& m);
  void adiarReenvio(Mensagem& m);

  // Transmissão LoRa
  void enfileirarMensagem(const Mensagem& m, uint8_t ttl, uint32_t atrasoMs);
  void enfileirarTx(const redelora::Bytes& dados, uint32_t msgId, int16_t bloco, bool resposta,
                    bool controle, uint32_t atrasoMs);
  void enviarAck(uint32_t msgId, uint8_t ttl);
  void enviarNack(Mensagem& m);
  void removerDaFila(uint32_t msgId);
  void suprimirResposta(uint32_t msgId, int16_t bloco);
  bool naFila(uint32_t msgId) const;
  void processarFilaTx();

  // BLE
  void processarBle();
  void tratarControle(uint16_t conn, const uint8_t* d, size_t tam);
  bool entregarLocal(Mensagem& m);
  void enviarMensagemBle(uint16_t conn, const Mensagem& m);
  void enviarEvento(uint16_t conn, uint8_t evt, uint32_t msgId, int extra = -1);
  void enviarBle(uint16_t conn, bool controle, const redelora::Bytes& dados);
  void processarFilaBle();

  void manutencao();
  void salvar(const Mensagem& m) { pers_.salvar(m); }

  RadioLoRa& radio_;
  ServidorBle& ble_;
  Persistencia& pers_;
  Painel& painel_;
  Tabela tabela_;
  std::map<uint16_t, Usuario> usuarios_;
  std::deque<ItemTx> filaTx_;
  std::deque<ItemBle> filaBle_;
  uint32_t ultimoTxMs_ = 0, ultimaNotificacaoMs_ = 0, ultimaManutencaoMs_ = 0;
  uint32_t contRx_ = 0, contTx_ = 0, contCrc_ = 0;
  uint8_t descartarBlocos_ = 0;
};
