#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ByteBuffer.h"
#include "FileLinkTypes.h"

// Compressao/descompressao com LZ4 (rapido e leve).
//
// O resultado e um "container" autodescritivo:
//   [0]   'F'
//   [1]   'L'
//   [2]   versao (1)
//   [3]   metodo: 0 = armazenado (sem compressao), 1 = LZ4
//   [4-7] tamanho original (uint32, little-endian)
//   [8..] dados
// Se o LZ4 nao reduzir o tamanho (textos muito curtos, dados aleatorios), os
// dados sao armazenados sem compressao para nunca crescer mais que 8 bytes.
class FileCompressor {
 public:
  enum { HEADER_SIZE = 8 };

  // acceleration >= 1: valores maiores = mais rapido, comprime menos.
  static FLResult compress(const uint8_t* src, size_t len, ByteBuffer& out,
                           int acceleration = 1);

  // Atalho para texto C (sem o '\0' final).
  static FLResult compressText(const char* text, ByteBuffer& out,
                               int acceleration = 1);

  // Restaura o conteudo original. 'out' recebe um '\0' extra apos os dados
  // (nao contado em out.size()), entao pode ser usado como string C.
  // maxOriginalSize protege contra containers corrompidos que pedem memoria demais.
  static FLResult decompress(const uint8_t* src, size_t len, ByteBuffer& out,
                             size_t maxOriginalSize = 1024UL * 1024UL);

  // Le o tamanho original sem descomprimir. Retorna false se nao for um container valido.
  static bool originalSize(const uint8_t* src, size_t len, size_t* outSize);
};
