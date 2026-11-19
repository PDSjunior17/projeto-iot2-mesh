#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "BlockCodec.h"
#include "FileLinkConfig.h"

// Lado RECEPTOR: servidor GATT (periferico) que anuncia o servico, recebe os
// pacotes de bloco, mescla-os e avisa quando o arquivo comprimido esta completo.
//
//   receiver.begin();
//   loop() { if (receiver.transferReady()) { ...usa data()/size()...; receiver.release(); } }
//
// Enquanto um arquivo completo nao for liberado com release(), novos blocos sao
// recusados (o emissor recebe um erro) para o buffer nao mudar enquanto voce o le.
class BleBlockReceiver {
 public:
  BleBlockReceiver();
  ~BleBlockReceiver();

  bool begin(const char* deviceName = "FileLink-RX",
             uint16_t localMtu = FILELINK_DEFAULT_MTU);
  void end();

  bool isConnected() const { return _connected; }

  // true quando todos os blocos chegaram e o CRC32 total confere.
  bool transferReady();
  const uint8_t* data();  // arquivo comprimido mesclado (valido se transferReady())
  size_t size();
  void release();         // libera o buffer e aceita uma nova transferencia

  // Progresso da transferencia em andamento.
  uint16_t transferId();
  uint16_t receivedBlocks();
  uint16_t totalBlocks();
  uint32_t packetsAccepted() const { return _accepted; }
  uint32_t packetsRejected() const { return _rejected; }

  void setMaxTransferSize(size_t bytes);  // padrao 128 KB

  // --- uso interno (chamados pelas callbacks do BLE) ---
  void _onWrite(const uint8_t* data, size_t len);
  void _onConnect();
  void _onDisconnect();

 private:
  void publishStatus();

  BlockAssembler _asm;
  SemaphoreHandle_t _mutex;
  BLECharacteristic* _statusChar;
  volatile bool _connected;
  bool _started;
  BlockStatus _last;
  volatile uint32_t _accepted, _rejected;
};
