// GERADO por tools/gerar_vetores.py - não edite à mão.
#pragma once

struct VetorValido { const char* nome; const char* hex; int idi; int flags; int ttl;
  unsigned long msgId; unsigned long destino; unsigned long remetente;
  int nBlocos; int bloco; const char* dados; };

static const VetorValido VETORES_VALIDOS[] = {
  {"mensagem_texto_nivel0", "00077856341202000000010000004f6c612072656465", 0, 0, 7, 0x12345678UL, 0x00000002UL, 0x00000001UL, 0, 0, "4f6c612072656465"},
  {"mensagem_grupo_comprimida", "0c03d4c3b2a1bebafecaefbe0000f348cd", 0, 12, 3, 0xA1B2C3D4UL, 0xCAFEBABEUL, 0x0000BEEFUL, 0, 0, "f348cd"},
  {"bloco", "20070403020102000102030405", 1, 0, 7, 0x01020304UL, 0x00000000UL, 0x00000000UL, 0, 2, "000102030405"},
  {"cabecalho_nivel1", "500704030201100000002000000003", 2, 16, 7, 0x01020304UL, 0x00000010UL, 0x00000020UL, 3, 0, ""},
  {"ipfs", "7007ffff000002000000010000001220abababababababababababababababababababababababababababababababab", 3, 16, 7, 0x0000FFFFUL, 0x00000002UL, 0x00000001UL, 0, 0, "1220abababababababababababababababababababababababababababababababab"},
  {"ack", "800778563412", 4, 0, 7, 0x12345678UL, 0x00000000UL, 0x00000000UL, 0, 0, ""},
  {"nack", "a0010403020101020000000000000000000000000000000000000000000000000000000000c0", 5, 0, 1, 0x01020304UL, 0x00000000UL, 0x00000000UL, 0, 0, ""},
};

static const char* const VETORES_INVALIDOS[] = {
  "",  // vazio
  "e00701000000",  // idi_7
  "8007010000",  // ack_curto
  "800700000000",  // ack_id_zero
  "400705000000010000000200000000",  // cabecalho_zero_blocos
  "20070500000000",  // bloco_sem_dados
  "000705000000010000000200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",  // pacote_grande
  "a0010500000002000000000000000000000000000000000000000000000000000000000000",  // nack_curto
};
