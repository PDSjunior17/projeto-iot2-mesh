// Parâmetros do nó. Ver docs/firmware_no.md para a justificativa de cada valor.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace cfg {

// ---- Rádio (Brasil: 915-928 MHz) ----
constexpr float FREQUENCIA_MHZ = 915.0;
constexpr float LARGURA_BANDA_KHZ = 250.0;
constexpr uint8_t SPREADING_FACTOR = 9;
constexpr uint8_t CODING_RATE = 5;          // 4/5
constexpr int8_t POTENCIA_DBM = 2;          // bancada; até 22 dBm com antena
constexpr uint8_t PALAVRA_SINCRONIA = 0x12; // rede privada (LoRaWAN usa 0x34)
constexpr uint16_t PREAMBULO = 8;

// ---- Acesso ao meio ----
constexpr uint32_t ATRASO_TX_MIN_MS = 100;    // atraso aleatório antes de transmitir/retransmitir
constexpr uint32_t ATRASO_TX_MAX_MS = 1200;
constexpr uint32_t INTERVALO_ENTRE_TX_MS = 30;  // folga entre pacotes da mesma mensagem
// Pausa entre mensagens diferentes: deixa os vizinhos retransmitirem a anterior.
constexpr uint32_t PAUSA_ENTRE_MSGS_MIN_MS = 2000;  // > silêncio + atraso máximo de retransmissão
constexpr uint32_t PAUSA_ENTRE_MSGS_MAX_MS = 3500;
constexpr uint32_t SILENCIO_APOS_RX_MS = 500;   // não transmite logo após ouvir alguém
constexpr uint8_t MAX_TENTATIVAS_CAD = 8;       // canal ocupado: tenta de novo depois
// RSSI instantâneo acima disso = alguém transmitindo (checado antes do CAD,
// porque o CAD interrompe uma recepção em andamento).
constexpr float LIMIAR_CANAL_OCUPADO_DBM = -100.0;

// ---- Mensagens ----
constexpr uint32_t MINUTO_MS = 60UL * 1000;
constexpr uint32_t HORA_MS = 60 * MINUTO_MS;
constexpr uint32_t EXPIRACAO_PENDENTE_MS = 72 * HORA_MS;  // não entregue: guarda 72 h
constexpr uint32_t EXPIRACAO_ENTREGUE_MS = 7 * 24 * HORA_MS;  // registro de entregue: 7 dias
// Reenvio periódico das pendentes: 2 min, 10 min, 30 min e depois a cada 2 h.
constexpr uint32_t REENVIOS_MS[] = {2 * MINUTO_MS, 10 * MINUTO_MS, 30 * MINUTO_MS};
constexpr uint32_t REENVIO_DEPOIS_MS = 2 * HORA_MS;
constexpr uint8_t VARIACAO_REENVIO_PCT = 20;  // ±20% para os nós não reenviarem juntos
constexpr uint32_t NACK_ESPERA_MS = 5000;   // sem blocos novos por 5 s -> NACK
constexpr uint8_t NACK_MAX = 3;
constexpr uint32_t RESPOSTA_NACK_MAX_MS = 1500;  // atraso aleatório da resposta (supressão)
constexpr uint32_t ACK_REPETIDO_MIN_MS = MINUTO_MS;  // reenvio de ACK para cópia atrasada
constexpr uint32_t INTERVALO_MANUTENCAO_MS = 1000;

// ---- Memória (sem cartão SD tudo fica na RAM) ----
constexpr size_t MAX_MENSAGENS = 96;              // inclui registros de entregues
constexpr size_t MAX_BYTES_DADOS = 48 * 1024;

// ---- BLE ----
constexpr uint8_t MAX_CONEXOES_BLE = 3;
constexpr uint32_t INTERVALO_NOTIFICACAO_MS = 15;

// ---- Cartão SD (2º barramento SPI; ver docs/hardware.md) ----
constexpr int SD_CS = 4;
constexpr int SD_SCK = 5;
constexpr int SD_MOSI = 6;
constexpr int SD_MISO = 7;
constexpr uint32_t SD_FREQUENCIA_HZ = 10000000;

}  // namespace cfg
