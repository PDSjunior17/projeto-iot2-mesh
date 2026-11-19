#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLEUtils.h>

#include "BlockCodec.h"
#include "FileLinkConfig.h"

enum SendResult : uint8_t {
  SEND_OK = 0,
  SEND_ERR_NOT_CONNECTED,
  SEND_ERR_INVALID,         // splitter invalido
  SEND_ERR_BLOCK_TOO_LARGE, // cabecalho + blockSize nao cabem no MTU negociado
  SEND_ERR_WRITE,           // conexao caiu durante o envio
  SEND_ERR_REMOTE           // receptor nao confirmou o arquivo completo (veja remoteStatus())
};

const char* sendResultName(SendResult r);

// Status lido do receptor (characteristic FILELINK_STATUS_UUID).
struct RemoteStatus {
  uint8_t state;       // AssemblerState
  uint8_t lastResult;  // BlockStatus do ultimo pacote
  uint16_t transferId;
  uint16_t received;
  uint16_t total;
  bool valid;
};

// Lado EMISSOR: cliente GATT (central). Procura o receptor pelo UUID do servico,
// conecta, negocia o MTU e escreve os blocos um a um.
class BleBlockSender {
 public:
  typedef void (*ProgressCb)(uint16_t sent, uint16_t total, void* user);

  BleBlockSender();
  ~BleBlockSender();

  bool begin(const char* deviceName = "FileLink-TX",
             uint16_t localMtu = FILELINK_DEFAULT_MTU);

  // Procura (bloqueante, ate scanSeconds) um receptor e conecta. true se conectou.
  bool connect(uint32_t scanSeconds = 10);
  void disconnect();
  bool isConnected();

  // MTU negociado e maior payload por bloco que cabe em um unico pacote (m maximo).
  uint16_t mtu() const;
  size_t maxBlockPayload() const;
  // Caso o core nao informe o MTU negociado, force um valor (ex.: 247). 0 = automatico.
  void setMtuOverride(uint16_t mtu) { _mtuOverride = mtu; }

  // true (padrao): escrita com resposta (confiavel, controle de fluxo automatico).
  // false: sem resposta (mais rapido; use junto com setInterBlockDelay).
  void setWriteWithResponse(bool on) { _withResponse = on; }
  void setInterBlockDelay(uint32_t ms) { _delayMs = ms; }

  // Envia todos os blocos e, no fim, le o status do receptor para confirmar.
  SendResult send(const BlockSplitter& splitter, ProgressCb cb = nullptr,
                  void* user = nullptr);

  bool readRemoteStatus(RemoteStatus& out);
  const RemoteStatus& remoteStatus() const { return _remote; }

  // --- uso interno ---
  void _onScanResult(BLEAdvertisedDevice& dev);

 private:
  BLEClient* _client;
  BLERemoteCharacteristic* _dataChar;
  BLERemoteCharacteristic* _statusChar;
  BLEAdvertisedDevice* _target;
  volatile bool _found;
  bool _started;
  bool _withResponse;
  uint32_t _delayMs;
  uint16_t _localMtu;
  uint16_t _mtuOverride;
  RemoteStatus _remote;
};
