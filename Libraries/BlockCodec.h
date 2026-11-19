#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ByteBuffer.h"

// ---------------------------------------------------------------------------
// Formato do pacote (little-endian). Cada bloco e autocontido:
//
//  off  tam  campo
//   0    1   magic (0xB7)
//   1    1   versao (1)
//   2    2   transferId   identifica a transferencia
//   4    2   index        numero do bloco (0..count-1)
//   6    2   count        total de blocos
//   8    2   blockSize    tamanho "m" do payload (o ultimo bloco pode ser menor)
//  10    2   payloadLen   bytes de payload neste bloco
//  12    4   totalSize    tamanho do arquivo comprimido
//  16    4   totalCrc     CRC32 do arquivo comprimido inteiro
//  20    2   blockCrc     CRC16-CCITT do payload deste bloco
//  22    N   payload
// ---------------------------------------------------------------------------
struct BlockFormat {
  enum : uint8_t { MAGIC = 0xB7, VERSION = 1 };
  enum : size_t { HEADER_SIZE = 22 };
};

struct BlockHeader {
  uint16_t transferId;
  uint16_t index;
  uint16_t count;
  uint16_t blockSize;
  uint16_t payloadLen;
  uint32_t totalSize;
  uint32_t totalCrc;
  uint16_t blockCrc;
};

// CRCs usados pelo protocolo. crc32 e o CRC-32 padrao (igual ao zlib) e pode
// ser calculado de forma incremental passando o valor anterior em 'crc'.
uint32_t flCrc32(const uint8_t* data, size_t len, uint32_t crc = 0);
uint16_t flCrc16(const uint8_t* data, size_t len);

// Le e valida o cabecalho de um pacote (magic, versao, coerencia dos campos e
// comprimento). Retorna false se o pacote for invalido.
bool parseBlockHeader(const uint8_t* pkt, size_t len, BlockHeader& h);

// ---------------------------------------------------------------------------
// DIVIDIR: fatia um buffer (o arquivo comprimido) em n blocos de tamanho m.
// Nao copia os dados: guarda um ponteiro, entao o buffer original precisa
// continuar vivo enquanto o splitter for usado. Cada pacote e montado sob demanda.
// ---------------------------------------------------------------------------
class BlockSplitter {
 public:
  // transferId = 0 gera um id automatico (sequencial).
  BlockSplitter(const uint8_t* data, size_t size, uint16_t blockSize,
                uint16_t transferId = 0);

  bool valid() const { return _valid; }
  uint16_t count() const { return _count; }          // n
  uint16_t blockSize() const { return _blockSize; }  // m
  size_t totalSize() const { return _size; }
  uint16_t transferId() const { return _id; }

  size_t payloadLength(uint16_t index) const;
  size_t packetLength(uint16_t index) const;  // cabecalho + payload
  const uint8_t* payload(uint16_t index) const;

  // Monta o pacote 'index' em 'out'. Retorna o numero de bytes (0 em caso de erro).
  size_t buildPacket(uint16_t index, uint8_t* out, size_t cap) const;

 private:
  const uint8_t* _data;
  size_t _size;
  uint16_t _blockSize;
  uint16_t _count;
  uint16_t _id;
  uint32_t _crc;
  bool _valid;
};

// ---------------------------------------------------------------------------
// MESCLAR: recebe pacotes em qualquer ordem (aceita duplicados) e reconstroi
// o arquivo comprimido. Ao receber o ultimo bloco confere o CRC32 total.
// Nao e thread-safe: proteja com mutex se usar de varias tasks.
// ---------------------------------------------------------------------------
enum BlockStatus : uint8_t {
  BLOCK_ACCEPTED = 0,    // bloco novo guardado
  BLOCK_DUPLICATE,       // bloco ja recebido (ignorado)
  BLOCK_COMPLETE,        // era o ultimo: arquivo completo e verificado
  BLOCK_ERR_FORMAT,      // pacote invalido
  BLOCK_ERR_CRC,         // CRC16 do bloco nao confere
  BLOCK_ERR_TOTAL_CRC,   // todos os blocos chegaram, mas o CRC32 total falhou
  BLOCK_ERR_TOO_LARGE,   // arquivo maior que o limite configurado
  BLOCK_ERR_NOMEM,       // sem memoria
  BLOCK_ERR_LOCKED       // ha um arquivo completo aguardando release()
};

enum AssemblerState : uint8_t {
  ASM_IDLE = 0,
  ASM_RECEIVING = 1,
  ASM_COMPLETE = 2,
  ASM_ERROR = 3
};

const char* blockStatusName(BlockStatus s);

class BlockAssembler {
 public:
  BlockAssembler();
  ~BlockAssembler();
  BlockAssembler(const BlockAssembler&) = delete;
  BlockAssembler& operator=(const BlockAssembler&) = delete;

  // Limite do arquivo comprimido aceito (protege a RAM). Padrao: 128 KB.
  void setMaxTotalSize(size_t bytes) { _maxTotal = bytes; }

  // Um pacote com parametros diferentes dos da transferencia em andamento
  // (outro transferId/tamanho/CRC) descarta o progresso e inicia uma nova.
  BlockStatus addPacket(const uint8_t* pkt, size_t len);

  // Libera tudo e volta ao estado ocioso (chame depois de usar data()).
  void reset();

  AssemblerState state() const { return _state; }
  bool isComplete() const { return _state == ASM_COMPLETE; }
  uint16_t transferId() const { return _id; }
  uint16_t blockCount() const { return _count; }
  uint16_t receivedCount() const { return _received; }
  bool hasBlock(uint16_t index) const;

  // Arquivo comprimido mesclado (so valido quando isComplete()).
  const uint8_t* data() const { return isComplete() ? _buf.data() : nullptr; }
  size_t size() const { return isComplete() ? _total : 0; }

 private:
  bool matches(const BlockHeader& h) const;

  ByteBuffer _buf;
  uint8_t* _got;      // bitmap de blocos recebidos
  size_t _maxTotal;
  size_t _total;
  uint32_t _crc;
  uint16_t _id, _count, _blockSize, _received;
  AssemblerState _state;
};
