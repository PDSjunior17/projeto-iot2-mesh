#include "BleBlockSender.h"

#if defined(ARDUINO_ARCH_ESP32)

namespace {
class ScanCb : public BLEAdvertisedDeviceCallbacks {
 public:
  explicit ScanCb(BleBlockSender* o) : _o(o) {}
  void onResult(BLEAdvertisedDevice dev) override { _o->_onScanResult(dev); }

 private:
  BleBlockSender* _o;
};
}  // namespace

const char* sendResultName(SendResult r) {
  switch (r) {
    case SEND_OK:                 return "OK";
    case SEND_ERR_NOT_CONNECTED:  return "nao conectado";
    case SEND_ERR_INVALID:        return "splitter invalido";
    case SEND_ERR_BLOCK_TOO_LARGE:return "bloco maior que o MTU permite";
    case SEND_ERR_WRITE:          return "falha na escrita BLE";
    case SEND_ERR_REMOTE:         return "receptor nao confirmou";
  }
  return "?";
}

BleBlockSender::BleBlockSender()
    : _client(nullptr), _dataChar(nullptr), _statusChar(nullptr), _target(nullptr),
      _found(false), _started(false), _withResponse(true), _delayMs(0),
      _localMtu(FILELINK_DEFAULT_MTU), _mtuOverride(0) {
  memset(&_remote, 0, sizeof(_remote));
}

BleBlockSender::~BleBlockSender() {
  if (_target) delete _target;
}

bool BleBlockSender::begin(const char* deviceName, uint16_t localMtu) {
  if (_started) return true;
  _localMtu = localMtu;
  BLEDevice::init(deviceName);
  BLEDevice::setMTU(localMtu);
  _started = true;
  return true;
}

void BleBlockSender::_onScanResult(BLEAdvertisedDevice& dev) {
  if (_found) return;
  if (dev.haveServiceUUID() && dev.isAdvertisingService(BLEUUID(FILELINK_SERVICE_UUID))) {
    if (_target) delete _target;
    _target = new BLEAdvertisedDevice(dev);
    _found = true;
    BLEDevice::getScan()->stop();
  }
}

bool BleBlockSender::connect(uint32_t scanSeconds) {
  if (!_started) return false;
  if (isConnected()) return true;

  _found = false;
  _dataChar = _statusChar = nullptr;

  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new ScanCb(this));
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  scan->start(scanSeconds, false);  // bloqueia ate achar (stop() na callback) ou expirar
  scan->clearResults();

  if (!_found || !_target) return false;

  if (!_client) _client = BLEDevice::createClient();
  if (!_client->connect(_target)) return false;

  // Pede MTU maior e espera o evento de confirmacao (ate ~2 s).
  _client->setMTU(_localMtu);
  for (uint32_t t = 0; t < 2000 && _client->getMTU() <= 23; t += 50) delay(50);

  BLERemoteService* svc = _client->getService(BLEUUID(FILELINK_SERVICE_UUID));
  if (!svc) {
    _client->disconnect();
    return false;
  }
  _dataChar = svc->getCharacteristic(BLEUUID(FILELINK_DATA_UUID));
  _statusChar = svc->getCharacteristic(BLEUUID(FILELINK_STATUS_UUID));
  if (!_dataChar || !_statusChar) {
    _client->disconnect();
    return false;
  }
  return true;
}

void BleBlockSender::disconnect() {
  if (_client && _client->isConnected()) _client->disconnect();
  _dataChar = _statusChar = nullptr;
}

bool BleBlockSender::isConnected() { return _client && _client->isConnected(); }

uint16_t BleBlockSender::mtu() const {
  if (_mtuOverride) return _mtuOverride;
  return _client ? _client->getMTU() : 23;
}

size_t BleBlockSender::maxBlockPayload() const {
  size_t m = mtu();
  size_t overhead = FILELINK_ATT_OVERHEAD + BlockFormat::HEADER_SIZE;
  return m > overhead ? m - overhead : 0;
}

bool BleBlockSender::readRemoteStatus(RemoteStatus& out) {
  out.valid = false;
  if (!isConnected() || !_statusChar) return false;
  auto v = _statusChar->readValue();  // std::string (core 2.x) ou String (core 3.x)
  if (v.length() < 8) return false;
  uint8_t b[8];
  for (int i = 0; i < 8; i++) b[i] = (uint8_t)v[i];
  out.state = b[0];
  out.lastResult = b[1];
  out.transferId = (uint16_t)(b[2] | (b[3] << 8));
  out.received = (uint16_t)(b[4] | (b[5] << 8));
  out.total = (uint16_t)(b[6] | (b[7] << 8));
  out.valid = true;
  return true;
}

SendResult BleBlockSender::send(const BlockSplitter& s, ProgressCb cb, void* user) {
  if (!isConnected() || !_dataChar) return SEND_ERR_NOT_CONNECTED;
  if (!s.valid()) return SEND_ERR_INVALID;

  const size_t maxPacket = (size_t)mtu() - FILELINK_ATT_OVERHEAD;
  if (mtu() <= FILELINK_ATT_OVERHEAD || s.packetLength(0) > maxPacket)
    return SEND_ERR_BLOCK_TOO_LARGE;

  uint8_t* pkt = (uint8_t*)malloc(maxPacket);
  if (!pkt) return SEND_ERR_WRITE;

  SendResult result = SEND_OK;
  for (uint16_t i = 0; i < s.count(); i++) {
    size_t n = s.buildPacket(i, pkt, maxPacket);
    if (!n) {
      result = SEND_ERR_INVALID;
      break;
    }
    _dataChar->writeValue(pkt, n, _withResponse);
    if (!isConnected()) {
      result = SEND_ERR_WRITE;
      break;
    }
    if (_delayMs) delay(_delayMs);
    if (cb) cb((uint16_t)(i + 1), s.count(), user);
  }
  free(pkt);
  if (result != SEND_OK) return result;

  // Confirmacao: o receptor deve estar COMPLETE para este mesmo transferId.
  if (!readRemoteStatus(_remote)) return SEND_ERR_REMOTE;
  if (_remote.state != ASM_COMPLETE || _remote.transferId != s.transferId() ||
      _remote.received != s.count())
    return SEND_ERR_REMOTE;
  return SEND_OK;
}

#endif  // ARDUINO_ARCH_ESP32
