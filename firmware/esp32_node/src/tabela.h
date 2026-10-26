// Tabela de mensagens do nó: o que foi recebido, o que falta, o que foi entregue.
#pragma once

#include <map>
#include <vector>

#include "fragmentacao.h"

enum class Estado : uint8_t {
  Incompleta = 0,  // faltam blocos ou o cabeçalho
  Pendente = 1,    // completa, destinatário ainda não confirmou
  Entregue = 2,    // ACK recebido: só o registro fica (sem conteúdo)
};

struct Mensagem {
  redelora::Envelope env;
  redelora::Idi idi = redelora::Idi::Mensagem;  // Mensagem, Ipfs (únicas) ou Cabecalho (em blocos)
  Estado estado = Estado::Incompleta;
  bool temCabecalho = false;
  uint8_t nBlocos = 0;                    // 0 enquanto o cabeçalho não chega
  std::vector<redelora::Bytes> blocos;    // mensagem única: blocos[0]
  bool origemLocal = false;               // veio de um dispositivo BLE deste nó
  uint32_t criadaMs = 0;
  uint32_t ultimoRxMs = 0;
  uint32_t proximoReenvioMs = 0;
  uint8_t reenvios = 0;
  uint8_t nacks = 0;
  uint32_t ultimoNackMs = 0;
  uint32_t ultimoAckMs = 0;
  std::vector<uint32_t> entreguesGrupo;  // usuários do grupo que já receberam

  bool emBlocos() const { return idi == redelora::Idi::Cabecalho; }
  bool temBloco(size_t i) const { return i < blocos.size() && !blocos[i].empty(); }
  size_t blocosRecebidos() const;
  bool completa() const;
  size_t bytes() const;
  redelora::Bytes conteudo() const;  // blocos concatenados
  // Pacotes serializados da mensagem com o TTL indicado (vazio se incompleta).
  std::vector<redelora::Bytes> pacotes(uint8_t ttl) const;
};

class Tabela {
 public:
  Mensagem* buscar(uint32_t msgId);
  // Cria a entrada; libera espaço apagando as mais antigas se preciso.
  Mensagem& criar(uint32_t msgId, uint32_t agoraMs);
  void remover(uint32_t msgId);
  void recalcularBytes();
  void liberarEspaco(uint32_t protegido);

  std::map<uint32_t, Mensagem>& todas() { return msgs_; }
  size_t bytes() const { return bytes_; }
  size_t contar(Estado e) const;

 private:
  std::map<uint32_t, Mensagem> msgs_;
  size_t bytes_ = 0;
};
