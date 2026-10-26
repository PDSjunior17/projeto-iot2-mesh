// Testes nativos (PC) do protocolo, fragmentação e compressão.
// Os vetores vêm de test_vectors/vetores.json (gerados por tools/gerar_vetores.py),
// os mesmos usados pelos testes do Dart.
#include <unity.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "compressao.h"
#include "fragmentacao.h"
#include "protocolo.h"
#include "test_vectors.h"

using namespace redelora;

static Bytes deHex(const char* h) {
  Bytes b;
  for (size_t i = 0; h[i] && h[i + 1]; i += 2) {
    char t[3] = {h[i], h[i + 1], 0};
    b.push_back(strtoul(t, nullptr, 16));
  }
  return b;
}

void setUp() {}
void tearDown() {}

void test_vetores_validos_decodificam_e_recodificam() {
  for (const auto& v : VETORES_VALIDOS) {
    Bytes b = deHex(v.hex);
    Pacote p;
    TEST_ASSERT_TRUE_MESSAGE(decodificar(b.data(), b.size(), p), v.nome);
    TEST_ASSERT_EQUAL_MESSAGE(v.idi, (int)p.idi, v.nome);
    TEST_ASSERT_EQUAL_MESSAGE(v.flags, p.flags, v.nome);
    TEST_ASSERT_EQUAL_MESSAGE(v.ttl, p.ttl, v.nome);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(v.msgId, p.msgId, v.nome);
    if (p.idi == Idi::Mensagem || p.idi == Idi::Ipfs || p.idi == Idi::Cabecalho) {
      TEST_ASSERT_EQUAL_HEX32_MESSAGE(v.destino, p.destino, v.nome);
      TEST_ASSERT_EQUAL_HEX32_MESSAGE(v.remetente, p.remetente, v.nome);
    }
    if (p.idi == Idi::Cabecalho) TEST_ASSERT_EQUAL(v.nBlocos, p.nBlocos);
    if (p.idi == Idi::Bloco) TEST_ASSERT_EQUAL(v.bloco, p.bloco);
    Bytes dados = deHex(v.dados);
    if (!dados.empty()) {
      TEST_ASSERT_EQUAL(dados.size(), p.tamDados);
      TEST_ASSERT_EQUAL_MEMORY(dados.data(), p.dados, dados.size());
    }
    uint8_t buf[MAX_PACOTE];
    size_t n = codificar(p, buf, sizeof buf);
    TEST_ASSERT_EQUAL_MESSAGE(b.size(), n, v.nome);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(b.data(), buf, n, v.nome);
  }
}

void test_nack_bitmap() {
  Bytes b = deHex(VETORES_VALIDOS[6].hex);
  Pacote p;
  TEST_ASSERT_TRUE(decodificar(b.data(), b.size(), p));
  for (int i = 0; i < 256; i++) {
    bool esperado = i == 0 || i == 9 || i == 254 || i == 255;
    TEST_ASSERT_EQUAL(esperado, bitmapTem(p.bitmap, i));
  }
}

void test_vetores_invalidos_rejeitados() {
  for (const char* h : VETORES_INVALIDOS) {
    Bytes b = deHex(h);
    Pacote p;
    TEST_ASSERT_FALSE_MESSAGE(decodificar(b.data(), b.size(), p), h);
  }
}

static void fragmentarERemontar(size_t tam) {
  Bytes msg(tam);
  for (size_t i = 0; i < tam; i++) msg[i] = (i * 7) ^ (i >> 3);
  Envelope env{0xDEADBEEF, 2, 1, 0, TTL_PADRAO};
  std::vector<Bytes> pacotes;
  TEST_ASSERT_TRUE(fragmentar(env, msg.data(), msg.size(), pacotes));
  size_t esperado = tam <= MAX_DADOS_MSG ? 1 : 1 + (tam + TAM_BLOCO - 1) / TAM_BLOCO;
  TEST_ASSERT_EQUAL(esperado, pacotes.size());

  // Entrega em ordem invertida (blocos antes do cabeçalho) e com duplicatas.
  Remontador r;
  MensagemCompleta saida;
  int completas = 0;
  for (size_t k = pacotes.size(); k-- > 0;) {
    for (int rep = 0; rep < 2; rep++) {
      TEST_ASSERT_TRUE(pacotes[k].size() <= MAX_PACOTE);
      Pacote p;
      TEST_ASSERT_TRUE(decodificar(pacotes[k].data(), pacotes[k].size(), p));
      if (r.alimentar(p, 0, saida)) completas++;
    }
  }
  // Mensagem única é "completa" a cada cópia (o nó deduplica pelo msgId).
  TEST_ASSERT_EQUAL(tam <= MAX_DADOS_MSG ? 2 : 1, completas);
  TEST_ASSERT_EQUAL(tam, saida.dados.size());
  if (tam) TEST_ASSERT_EQUAL_MEMORY(msg.data(), saida.dados.data(), tam);
  TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, saida.env.msgId);
  TEST_ASSERT_EQUAL(0, r.pendentes());
}

void test_fragmentacao() {
  for (size_t tam : std::initializer_list<size_t>{0, 1, 114, 115, 121, 242, 243, 1000, (size_t)MAX_MENSAGEM}) fragmentarERemontar(tam);
  Bytes grande(MAX_MENSAGEM + 1);
  std::vector<Bytes> pacotes;
  TEST_ASSERT_FALSE(fragmentar(Envelope{1, 2, 3, 0, 7}, grande.data(), grande.size(), pacotes));
}

static void idaEVolta(const Bytes& orig) {
  Bytes c, d;
  comprimir(orig.data(), orig.size(), c);
  TEST_ASSERT_TRUE(descomprimir(c.data(), c.size(), d, orig.size() + 16));
  TEST_ASSERT_EQUAL(orig.size(), d.size());
  if (!orig.empty()) TEST_ASSERT_EQUAL_MEMORY(orig.data(), d.data(), orig.size());
}

void test_compressao() {
  idaEVolta({});
  std::string texto =
      "A rede LoRa entrega mensagens assincronas. A rede LoRa guarda mensagens ate o destinatario "
      "conectar. Mensagens grandes viram blocos; blocos perdidos sao pedidos de novo com NACK. ";
  std::string longo;
  for (int i = 0; i < 300; i++) longo += texto + std::to_string(i);
  idaEVolta(Bytes(texto.begin(), texto.end()));
  idaEVolta(Bytes(longo.begin(), longo.end()));
  Bytes c;
  comprimir((const uint8_t*)longo.data(), longo.size(), c);
  TEST_ASSERT_TRUE(c.size() < longo.size() / 3);  // texto repetitivo comprime bem
  Bytes aleatorio(5000), zeros(70000, 0);
  srand(42);
  for (auto& x : aleatorio) x = rand();
  idaEVolta(aleatorio);
  Bytes d;
  comprimir(zeros.data(), zeros.size(), c);
  TEST_ASSERT_TRUE(descomprimir(c.data(), c.size(), d, 70000));
  TEST_ASSERT_FALSE(descomprimir(c.data(), c.size(), d, 1000));  // passa do limite
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_vetores_validos_decodificam_e_recodificam);
  RUN_TEST(test_nack_bitmap);
  RUN_TEST(test_vetores_invalidos_rejeitados);
  RUN_TEST(test_fragmentacao);
  RUN_TEST(test_compressao);
  return UNITY_END();
}
