#pragma once
#include <stdint.h>

// Resultado das operacoes de compressao/descompressao.
enum FLResult : uint8_t {
  FL_OK = 0,
  FL_ERR_ARG,         // argumento invalido
  FL_ERR_NOMEM,       // sem memoria
  FL_ERR_TOO_LARGE,   // entrada/saida acima do limite
  FL_ERR_COMPRESS,    // falha na compressao
  FL_ERR_FORMAT,      // dados nao tem o cabecalho esperado
  FL_ERR_DECOMPRESS   // dados corrompidos / tamanho final divergente
};

inline const char* flResultName(FLResult r) {
  switch (r) {
    case FL_OK:             return "OK";
    case FL_ERR_ARG:        return "argumento invalido";
    case FL_ERR_NOMEM:      return "sem memoria";
    case FL_ERR_TOO_LARGE:  return "tamanho acima do limite";
    case FL_ERR_COMPRESS:   return "falha na compressao";
    case FL_ERR_FORMAT:     return "formato invalido";
    case FL_ERR_DECOMPRESS: return "dados corrompidos";
  }
  return "?";
}
