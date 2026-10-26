// Servidor GATT do nó (NimBLE). Os callbacks do BLE rodam em outra tarefa do
// FreeRTOS, então eles só colocam eventos numa fila; o loop() processa.
#pragma once

#include <NimBLEDevice.h>

#include <stddef.h>
#include <stdint.h>

struct EventoBle {
  enum Tipo : uint8_t { Conectou, Desconectou, Pacote, Controle };
  Tipo tipo;
  uint16_t conn;
  uint8_t tam;
  uint8_t dados[160];
};

class ServidorBle {
 public:
  bool iniciar(const char* nome);
  bool proximoEvento(EventoBle& e);
  // Notificação para uma conexão específica. Retorna false se a pilha BLE
  // estiver sem memória (tentar de novo depois).
  bool notificarPacote(uint16_t conn, const uint8_t* dados, size_t tam);
  bool notificarControle(uint16_t conn, const uint8_t* dados, size_t tam);
  size_t conexoes() const;
  uint16_t mtu(uint16_t conn) const;
  void desconectar(uint16_t conn);

  // Usado pelos callbacks.
  void publicar(EventoBle::Tipo tipo, uint16_t conn, const uint8_t* dados, size_t tam);
  void conectou(uint16_t conn);

 private:
  bool notificar(NimBLECharacteristic* c, uint16_t conn, const uint8_t* dados, size_t tam);

  QueueHandle_t fila_ = nullptr;
  NimBLEServer* servidor_ = nullptr;
  NimBLECharacteristic* entrada_ = nullptr;
  NimBLECharacteristic* saida_ = nullptr;
  NimBLECharacteristic* controle_ = nullptr;
};
