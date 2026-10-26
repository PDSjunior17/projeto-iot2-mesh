#include "servidor_ble.h"

#include <string.h>

#include "ble_gatt.h"
#include "config.h"

using namespace redelora;

namespace {

class CbServidor : public NimBLEServerCallbacks {
 public:
  explicit CbServidor(ServidorBle* s) : s_(s) {}
  void onConnect(NimBLEServer*, ble_gap_conn_desc* d) override { s_->conectou(d->conn_handle); }
  void onDisconnect(NimBLEServer*, ble_gap_conn_desc* d) override {
    s_->publicar(EventoBle::Desconectou, d->conn_handle, nullptr, 0);
  }

 private:
  ServidorBle* s_;
};

class CbEscrita : public NimBLECharacteristicCallbacks {
 public:
  CbEscrita(ServidorBle* s, EventoBle::Tipo tipo) : s_(s), tipo_(tipo) {}
  void onWrite(NimBLECharacteristic* c, ble_gap_conn_desc* d) override {
    NimBLEAttValue v = c->getValue();
    s_->publicar(tipo_, d->conn_handle, v.data(), v.length());
  }

 private:
  ServidorBle* s_;
  EventoBle::Tipo tipo_;
};

}  // namespace

bool ServidorBle::iniciar(const char* nome) {
  fila_ = xQueueCreate(24, sizeof(EventoBle));
  if (!fila_) return false;
  NimBLEDevice::init(nome);
  NimBLEDevice::setMTU(gatt::MTU);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  servidor_ = NimBLEDevice::createServer();
  servidor_->setCallbacks(new CbServidor(this));
  servidor_->advertiseOnDisconnect(true);

  NimBLEService* svc = servidor_->createService(gatt::UUID_SERVICO);
  entrada_ = svc->createCharacteristic(gatt::UUID_PACOTE_ENTRADA, NIMBLE_PROPERTY::WRITE);
  entrada_->setCallbacks(new CbEscrita(this, EventoBle::Pacote));
  saida_ = svc->createCharacteristic(gatt::UUID_PACOTE_SAIDA, NIMBLE_PROPERTY::NOTIFY);
  controle_ = svc->createCharacteristic(gatt::UUID_CONTROLE, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  controle_->setCallbacks(new CbEscrita(this, EventoBle::Controle));
  svc->start();

  // Nome e UUID no próprio anúncio (3 + 18 + 9 = 30 de 31 bytes): o cliente não
  // depende da resposta ao scan, que às vezes se perde.
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData dados;
  dados.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  dados.setCompleteServices(NimBLEUUID(gatt::UUID_SERVICO));
  dados.setName(nome);
  adv->setAdvertisementData(dados);
  adv->setScanResponse(false);
  return adv->start();
}

void ServidorBle::conectou(uint16_t conn) {
  publicar(EventoBle::Conectou, conn, nullptr, 0);
  // Aceita vários dispositivos ao mesmo tempo: volta a anunciar.
  if (servidor_->getConnectedCount() < cfg::MAX_CONEXOES_BLE) NimBLEDevice::startAdvertising();
}

void ServidorBle::publicar(EventoBle::Tipo tipo, uint16_t conn, const uint8_t* dados, size_t tam) {
  EventoBle e;
  e.tipo = tipo;
  e.conn = conn;
  e.tam = tam > sizeof e.dados ? sizeof e.dados : tam;
  if (dados && e.tam) memcpy(e.dados, dados, e.tam);
  xQueueSend(fila_, &e, 0);
}

bool ServidorBle::proximoEvento(EventoBle& e) { return fila_ && xQueueReceive(fila_, &e, 0) == pdTRUE; }

bool ServidorBle::notificar(NimBLECharacteristic* c, uint16_t conn, const uint8_t* dados, size_t tam) {
  // NimBLE-Arduino 1.4 só notifica todos os clientes; para notificar um só,
  // chamamos a pilha NimBLE diretamente (é o que a biblioteca faz por dentro).
  os_mbuf* om = ble_hs_mbuf_from_flat(dados, tam);
  if (!om) return false;
  return ble_gattc_notify_custom(conn, c->getHandle(), om) == 0;
}

bool ServidorBle::notificarPacote(uint16_t conn, const uint8_t* d, size_t t) { return notificar(saida_, conn, d, t); }
bool ServidorBle::notificarControle(uint16_t conn, const uint8_t* d, size_t t) { return notificar(controle_, conn, d, t); }
size_t ServidorBle::conexoes() const { return servidor_ ? servidor_->getConnectedCount() : 0; }
uint16_t ServidorBle::mtu(uint16_t conn) const { return servidor_->getPeerMTU(conn); }
void ServidorBle::desconectar(uint16_t conn) { servidor_->disconnect(conn); }
