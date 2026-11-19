#pragma once

// UUIDs do servico GATT usado pelo FileLinkBLE. Receptor e emissor precisam
// usar os mesmos valores (troque por UUIDs proprios se quiser isolar sua rede).
#define FILELINK_SERVICE_UUID "b2a30001-5e1d-4f6a-9c3b-7d2a1e0f4c51"
// Emissor -> receptor: cada escrita carrega UM pacote de bloco.
#define FILELINK_DATA_UUID "b2a30002-5e1d-4f6a-9c3b-7d2a1e0f4c51"
// Receptor -> emissor (leitura): status da transferencia, 8 bytes:
//   [0] estado (AssemblerState)   [1] ultimo BlockStatus
//   [2-3] transferId  [4-5] blocos recebidos  [6-7] total de blocos  (little-endian)
#define FILELINK_STATUS_UUID "b2a30003-5e1d-4f6a-9c3b-7d2a1e0f4c51"

// MTU local que ambos pedem ao negociar (o maximo do BLE e 517).
#define FILELINK_DEFAULT_MTU 517

// Bytes de overhead do ATT numa escrita: payload maximo por pacote = MTU - 3.
#define FILELINK_ATT_OVERHEAD 3
