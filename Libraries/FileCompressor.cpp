#include "FileCompressor.h"
#include <string.h>
#include "lz4.h"

static const uint8_t kVersion = 1;

static inline void put32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

static inline uint32_t get32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

FLResult FileCompressor::compress(const uint8_t* src, size_t len, ByteBuffer& out,
                                  int acceleration) {
  if (!src && len) return FL_ERR_ARG;
  if (len > (size_t)LZ4_MAX_INPUT_SIZE || (uint64_t)len > 0x7FFFFFFFULL)
    return FL_ERR_TOO_LARGE;
  if (acceleration < 1) acceleration = 1;

  int bound = LZ4_compressBound((int)len);
  if (bound <= 0) return FL_ERR_TOO_LARGE;

  if (!out.alloc(HEADER_SIZE + (size_t)bound)) return FL_ERR_NOMEM;
  uint8_t* dst = out.data();
  dst[0] = 'F';
  dst[1] = 'L';
  dst[2] = kVersion;
  dst[3] = 0;
  put32(dst + 4, (uint32_t)len);

  int n = 0;
  if (len > 0) {
    // A tabela hash do LZ4 tem ~16 KB: fica no heap (nao na pilha da task).
    void* state = malloc((size_t)LZ4_sizeofState());
    if (!state) {
      out.clear();
      return FL_ERR_NOMEM;
    }
    n = LZ4_compress_fast_extState(state, (const char*)src, (char*)dst + HEADER_SIZE,
                                   (int)len, bound, acceleration);
    free(state);
    if (n <= 0) {
      out.clear();
      return FL_ERR_COMPRESS;
    }
  }

  if (len == 0 || (size_t)n >= len) {
    // Nao compensou comprimir: guarda os dados originais.
    dst[3] = 0;
    if (len) memcpy(dst + HEADER_SIZE, src, len);
    out.setSize(HEADER_SIZE + len);
  } else {
    dst[3] = 1;
    out.setSize(HEADER_SIZE + (size_t)n);
  }
  return FL_OK;
}

FLResult FileCompressor::compressText(const char* text, ByteBuffer& out,
                                      int acceleration) {
  if (!text) return FL_ERR_ARG;
  return compress((const uint8_t*)text, strlen(text), out, acceleration);
}

bool FileCompressor::originalSize(const uint8_t* src, size_t len, size_t* outSize) {
  if (!src || len < HEADER_SIZE) return false;
  if (src[0] != 'F' || src[1] != 'L' || src[2] != kVersion || src[3] > 1) return false;
  if (outSize) *outSize = get32(src + 4);
  return true;
}

FLResult FileCompressor::decompress(const uint8_t* src, size_t len, ByteBuffer& out,
                                    size_t maxOriginalSize) {
  size_t orig = 0;
  if (!originalSize(src, len, &orig)) return FL_ERR_FORMAT;
  if (orig > maxOriginalSize || orig > 0x7FFFFFFEUL) return FL_ERR_TOO_LARGE;

  const uint8_t method = src[3];
  const uint8_t* payload = src + HEADER_SIZE;
  const size_t payloadLen = len - HEADER_SIZE;

  if (!out.alloc(orig + 1)) return FL_ERR_NOMEM;

  if (method == 0) {
    if (payloadLen != orig) {
      out.clear();
      return FL_ERR_DECOMPRESS;
    }
    if (orig) memcpy(out.data(), payload, orig);
  } else {
    if (payloadLen == 0 || orig == 0 || payloadLen > 0x7FFFFFFFUL) {
      out.clear();
      return FL_ERR_DECOMPRESS;
    }
    int n = LZ4_decompress_safe((const char*)payload, (char*)out.data(),
                                (int)payloadLen, (int)orig);
    if (n < 0 || (size_t)n != orig) {
      out.clear();
      return FL_ERR_DECOMPRESS;
    }
  }
  out.data()[orig] = 0;
  out.setSize(orig);
  return FL_OK;
}
