// Firmware do nó da rede LoRa (Heltec WiFi LoRa 32 V3).
// Ver docs/firmware_no.md.

// Arduino.h precisa vir antes da biblioteca da Heltec, senão as macros de pinos
// dela conflitam com a definição da placa no PlatformIO.
#include <Arduino.h>
#include <heltec_unofficial.h>

#include "ble_gatt.h"
#include "config.h"
#include "no.h"
#include "painel.h"
#include "persistencia.h"
#include "radio_lora.h"
#include "servidor_ble.h"

// A biblioteca da Heltec define `radio`, `display` e `button` no próprio header;
// por isso ela só é incluída aqui e os módulos recebem referências.
static char titulo[24];
static Painel* painel;
static RadioLoRa radioLora(radio);
static ServidorBle ble;
#ifdef USAR_CARTAO_SD
static PersistenciaSD persistencia;
#else
static PersistenciaRAM persistencia;
#endif
static No* no;

void setup() {
  // Na V3.1/V3.2 o OLED é alimentado pela saída Vext, que a biblioteca da Heltec
  // só liga na Wireless Stick: sem isto o display fica apagado.
  heltec_ve(true);
  delay(50);
  heltec_setup();
  uint64_t mac = ESP.getEfuseMac();
  uint16_t idCurto = ((mac >> 32) & 0xFF) << 8 | ((mac >> 40) & 0xFF);
  snprintf(titulo, sizeof titulo, "No %04X", idCurto);
  painel = new Painel(display, titulo);
  painel->log("Iniciando %s", titulo);
  Wire.beginTransmission(0x3c);
  painel->log(Wire.endTransmission() == 0 ? "Display ok" : "Display nao responde");

  int s = radioLora.iniciar();
  painel->log(s == RADIOLIB_ERR_NONE ? "Radio ok %.1fMHz" : "Radio ERRO", cfg::FREQUENCIA_MHZ);
  if (s != RADIOLIB_ERR_NONE) painel->log("codigo %d", s);

  if (!persistencia.iniciar()) painel->log("Cartao SD ERRO");

  char nomeBle[24];
  snprintf(nomeBle, sizeof nomeBle, "%s%04X", redelora::gatt::PREFIXO_NOME, idCurto);
  painel->log(ble.iniciar(nomeBle) ? "BLE %s" : "BLE ERRO %s", nomeBle);

  no = new No(radioLora, ble, persistencia, *painel);
  painel->definirProvedores([](std::vector<String>& l) { no->linhasStatus(l); },
                            [](std::vector<String>& l) { no->linhasTabela(l); });
  no->iniciar();
}

// Comandos de teste pela serial: "perder <n>" descarta os próximos n blocos recebidos.
static void lerSerial() {
  static String linha;
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') {
      linha += c;
      continue;
    }
    if (linha.startsWith("perder ")) {
      int n = linha.substring(7).toInt();
      no->descartarProximosBlocos(n);
      painel->log("TESTE: perder %d blocos", n);
    }
    linha = "";
  }
}

void loop() {
  heltec_loop();
  lerSerial();
  if (button.isSingleClick()) painel->proximaPagina();
  no->loop();
  painel->atualizar();
}
