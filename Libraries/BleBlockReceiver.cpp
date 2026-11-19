#include "BleBlockReceiver.h"

#if defined(ARDUINO_ARCH_ESP32)

namespace {

class Lock {
 public:
  explicit Lock(SemaphoreHandle_t m) : _m(m) { xSemaphoreTake(_m, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(_m); }

 private:
  SemaphoreHandle_t _m;
};

class ServerCb : public BLEServerCallbacks {
 public:
  explicit ServerCb(BleBlockReceiver* o) : _o(o) {}
  void onConnect(BLEServer*) override { _o->_onConnect(); }
  void onDisconnect(BLEServer*) override { _o->_onDisconnect(); }

 private:
  BleBlockReceiver* _o;
};

class WriteCb : public BLECharacteristicCallbacks {
 public:
  explicit WriteCb(BleBlockReceiver* o) : _o(o) {}
  void onWrite(BLECharacteristic* c) override { _o->_onWrite(c->getData(), c->getLength()); }

 private:
  BleBlockReceiver* _o;
};

}  // namespace

BleBlockReceiver::BleBlockReceiver()
    : _mutex(xSemaphoreCreateMutex()), _statusChar(nullptr), _connected(false),
      _started(false), _last(BLOCK_ACCEPTED), _accepted(0), _rejected(0) {}

BleBlockReceiver::~BleBlockReceiver() {
  if (_mutex) vSemaphoreDelete(_mutex);
}

bool BleBlockReceiver::begin(const char* deviceName, uint16_t localMtu) {
  if (_started) return true;

  BLEDevice::init(deviceName);
  BLEDevice::setMTU(localMtu);

  // As callbacks sao alocadas com new porque, dependendo da versao do core
  // Arduino-ESP32, a propria biblioteca BLE assume a posse (e libera) delas.
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCb(this));

  BLEService* service = server->createService(FILELINK_SERVICE_UUID);

  BLECharacteristic* dataChar = service->createCharacteristic(
      FILELINK_DATA_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  dataChar->setCallbacks(new WriteCb(this));

  _statusChar = service->createCharacteristic(FILELINK_STATUS_UUID,
                                              BLECharacteristic::PROPERTY_READ);
  publishStatus();

  service->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(FILELINK_SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  _started = true;
  return true;
}

void BleBlockReceiver::end() {
  if (!_started) return;
  BLEDevice::deinit(true);
  _statusChar = nullptr;
  _connected = false;
  _started = false;
}

void BleBlockReceiver::publishStatus() {
  // Chamar com o mutex ja tomado (ou antes de o BLE iniciar).
  if (!_statusChar) return;
  uint8_t st[8];
  uint16_t id = _asm.transferId(), rec = _asm.receivedCount(), cnt = _asm.blockCount();
  st[0] = (uint8_t)_asm.state();
  st[1] = (uint8_t)_last;
  st[2] = (uint8_t)id;
  st[3] = (uint8_t)(id >> 8);
  st[4] = (uint8_t)rec;
  st[5] = (uint8_t)(rec >> 8);
  st[6] = (uint8_t)cnt;
  st[7] = (uint8_t)(cnt >> 8);
  _statusChar->setValue(st, sizeof(st));
}

void BleBlockReceiver::_onWrite(const uint8_t* data, size_t len) {
  Lock lock(_mutex);
  _last = _asm.addPacket(data, len);
  if (_last == BLOCK_ACCEPTED || _last == BLOCK_DUPLICATE || _last == BLOCK_COMPLETE)
    _accepted++;
  else
    _rejected++;
  publishStatus();
}

void BleBlockReceiver::_onConnect() { _connected = true; }

void BleBlockReceiver::_onDisconnect() {
  _connected = false;
  // O progresso parcial e mantido; volta a anunciar para aceitar nova conexao.
  BLEDevice::startAdvertising();
}

bool BleBlockReceiver::transferReady() {
  Lock lock(_mutex);
  return _asm.isComplete();
}

const uint8_t* BleBlockReceiver::data() { return _asm.data(); }
size_t BleBlockReceiver::size() { return _asm.size(); }

void BleBlockReceiver::release() {
  Lock lock(_mutex);
  _asm.reset();
  _last = BLOCK_ACCEPTED;
  publishStatus();
}

uint16_t BleBlockReceiver::transferId() {
  Lock lock(_mutex);
  return _asm.transferId();
}

uint16_t BleBlockReceiver::receivedBlocks() {
  Lock lock(_mutex);
  return _asm.receivedCount();
}

uint16_t BleBlockReceiver::totalBlocks() {
  Lock lock(_mutex);
  return _asm.blockCount();
}

void BleBlockReceiver::setMaxTransferSize(size_t bytes) {
  Lock lock(_mutex);
  _asm.setMaxTotalSize(bytes);
}

#endif  // ARDUINO_ARCH_ESP32
