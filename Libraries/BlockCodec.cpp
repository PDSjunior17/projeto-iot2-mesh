#include "BlockCodec.h"
#include <string.h>

// ------------------------------------------------------------------ CRCs ----

uint32_t flCrc32(const uint8_t* data, size_t len, uint32_t crc) {
  // CRC-32 (poly 0xEDB88320) com tabela de 16 entradas: pouca flash, boa velocidade.
  static const uint32_t tbl[16] = {
      0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4,
      0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
      0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C};
  crc = ~crc;
  for (size_t i = 0; i < len; i++) {
    crc = tbl[(crc ^ data[i]) & 0x0F] ^ (crc >> 4);
    crc = tbl[(crc ^ (data[i] >> 4)) & 0x0F] ^ (crc >> 4);
  }
  return ~crc;
}

uint16_t flCrc16(const uint8_t* data, size_t len) {
  // CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF)
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

// ------------------------------------------------------- serializacao LE ----

static inline void put16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
static inline void put32(uint8_t* p, uint32_t v) {
  put16(p, (uint16_t)v);
  put16(p + 2, (uint16_t)(v >> 16));
}
static inline uint16_t get16(const uint8_t* p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}
static inline uint32_t get32(const uint8_t* p) {
  return (uint32_t)get16(p) | ((uint32_t)get16(p + 2) << 16);
}

bool parseBlockHeader(const uint8_t* pkt, size_t len, BlockHeader& h) {
  if (!pkt || len < BlockFormat::HEADER_SIZE) return false;
  if (pkt[0] != BlockFormat::MAGIC || pkt[1] != BlockFormat::VERSION) return false;

  h.transferId = get16(pkt + 2);
  h.index = get16(pkt + 4);
  h.count = get16(pkt + 6);
  h.blockSize = get16(pkt + 8);
  h.payloadLen = get16(pkt + 10);
  h.totalSize = get32(pkt + 12);
  h.totalCrc = get32(pkt + 16);
  h.blockCrc = get16(pkt + 20);

  if (h.count == 0 || h.index >= h.count || h.blockSize == 0 || h.totalSize == 0)
    return false;

  // O numero de blocos e o tamanho do payload precisam bater com totalSize/blockSize.
  uint64_t expectedCount = ((uint64_t)h.totalSize + h.blockSize - 1) / h.blockSize;
  if (expectedCount != h.count) return false;
  uint64_t expectedLen = (h.index + 1u < h.count)
                             ? h.blockSize
                             : (uint64_t)h.totalSize - (uint64_t)(h.count - 1) * h.blockSize;
  if (h.payloadLen != expectedLen) return false;

  return len >= BlockFormat::HEADER_SIZE + (size_t)h.payloadLen;
}

// -------------------------------------------------------------- Splitter ----

BlockSplitter::BlockSplitter(const uint8_t* data, size_t size, uint16_t blockSize,
                             uint16_t transferId)
    : _data(data), _size(size), _blockSize(blockSize), _count(0), _id(transferId),
      _crc(0), _valid(false) {
  if (!data || size == 0 || blockSize == 0) return;
  if ((uint64_t)size > 0xFFFFFFFFULL) return;
  size_t cnt = (size + blockSize - 1) / blockSize;
  if (cnt > 0xFFFF) return;  // aumente blockSize
  _count = (uint16_t)cnt;
  if (_id == 0) {
    static uint16_t next = 0;
    if (++next == 0) next = 1;
    _id = next;
  }
  _crc = flCrc32(data, size);
  _valid = true;
}

size_t BlockSplitter::payloadLength(uint16_t index) const {
  if (!_valid || index >= _count) return 0;
  if (index + 1u < _count) return _blockSize;
  return _size - (size_t)(_count - 1) * _blockSize;
}

size_t BlockSplitter::packetLength(uint16_t index) const {
  size_t p = payloadLength(index);
  return p ? BlockFormat::HEADER_SIZE + p : 0;
}

const uint8_t* BlockSplitter::payload(uint16_t index) const {
  if (!_valid || index >= _count) return nullptr;
  return _data + (size_t)index * _blockSize;
}

size_t BlockSplitter::buildPacket(uint16_t index, uint8_t* out, size_t cap) const {
  size_t plen = payloadLength(index);
  if (!plen || !out || cap < BlockFormat::HEADER_SIZE + plen) return 0;
  const uint8_t* pl = payload(index);

  out[0] = BlockFormat::MAGIC;
  out[1] = BlockFormat::VERSION;
  put16(out + 2, _id);
  put16(out + 4, index);
  put16(out + 6, _count);
  put16(out + 8, _blockSize);
  put16(out + 10, (uint16_t)plen);
  put32(out + 12, (uint32_t)_size);
  put32(out + 16, _crc);
  put16(out + 20, flCrc16(pl, plen));
  memcpy(out + BlockFormat::HEADER_SIZE, pl, plen);
  return BlockFormat::HEADER_SIZE + plen;
}

// ------------------------------------------------------------- Assembler ----

const char* blockStatusName(BlockStatus s) {
  switch (s) {
    case BLOCK_ACCEPTED:      return "aceito";
    case BLOCK_DUPLICATE:     return "duplicado";
    case BLOCK_COMPLETE:      return "completo";
    case BLOCK_ERR_FORMAT:    return "pacote invalido";
    case BLOCK_ERR_CRC:       return "CRC do bloco invalido";
    case BLOCK_ERR_TOTAL_CRC: return "CRC total invalido";
    case BLOCK_ERR_TOO_LARGE: return "arquivo grande demais";
    case BLOCK_ERR_NOMEM:     return "sem memoria";
    case BLOCK_ERR_LOCKED:    return "aguardando release()";
  }
  return "?";
}

BlockAssembler::BlockAssembler()
    : _got(nullptr), _maxTotal(128 * 1024), _total(0), _crc(0), _id(0), _count(0),
      _blockSize(0), _received(0), _state(ASM_IDLE) {}

BlockAssembler::~BlockAssembler() { reset(); }

void BlockAssembler::reset() {
  _buf.clear();
  if (_got) free(_got);
  _got = nullptr;
  _total = 0;
  _crc = 0;
  _id = _count = _blockSize = _received = 0;
  _state = ASM_IDLE;
}

bool BlockAssembler::hasBlock(uint16_t index) const {
  if (!_got || index >= _count) return false;
  return _got[index >> 3] & (1u << (index & 7));
}

bool BlockAssembler::matches(const BlockHeader& h) const {
  return _state == ASM_RECEIVING && h.transferId == _id && h.count == _count &&
         h.blockSize == _blockSize && h.totalSize == _total && h.totalCrc == _crc;
}

BlockStatus BlockAssembler::addPacket(const uint8_t* pkt, size_t len) {
  BlockHeader h;
  if (!parseBlockHeader(pkt, len, h)) return BLOCK_ERR_FORMAT;
  if (_state == ASM_COMPLETE) return BLOCK_ERR_LOCKED;

  const uint8_t* payload = pkt + BlockFormat::HEADER_SIZE;
  if (flCrc16(payload, h.payloadLen) != h.blockCrc) return BLOCK_ERR_CRC;

  if (!matches(h)) {
    // Transferencia nova: descarta o progresso anterior.
    if (h.totalSize > _maxTotal) return BLOCK_ERR_TOO_LARGE;
    reset();
    if (!_buf.alloc(h.totalSize)) {
      reset();
      return BLOCK_ERR_NOMEM;
    }
    _got = (uint8_t*)calloc(((size_t)h.count + 7) / 8, 1);
    if (!_got) {
      reset();
      return BLOCK_ERR_NOMEM;
    }
    _id = h.transferId;
    _count = h.count;
    _blockSize = h.blockSize;
    _total = h.totalSize;
    _crc = h.totalCrc;
    _received = 0;
    _state = ASM_RECEIVING;
  }

  if (hasBlock(h.index)) return BLOCK_DUPLICATE;

  memcpy(_buf.data() + (size_t)h.index * _blockSize, payload, h.payloadLen);
  _got[h.index >> 3] |= (uint8_t)(1u << (h.index & 7));
  _received++;

  if (_received < _count) return BLOCK_ACCEPTED;

  // Todos os blocos chegaram: confere o arquivo inteiro.
  if (flCrc32(_buf.data(), _total) != _crc) {
    reset();
    _state = ASM_ERROR;
    return BLOCK_ERR_TOTAL_CRC;
  }
  _buf.setSize(_total);
  _state = ASM_COMPLETE;
  return BLOCK_COMPLETE;
}
