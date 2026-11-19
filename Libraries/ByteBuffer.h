#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Buffer simples (RAII) usado pela biblioteca. Usa malloc/free; no ESP32 com
// PSRAM habilitada, alocacoes grandes vao para a PSRAM automaticamente.
class ByteBuffer {
 public:
  ByteBuffer() : _data(nullptr), _size(0), _cap(0) {}
  ~ByteBuffer() { clear(); }
  ByteBuffer(const ByteBuffer&) = delete;
  ByteBuffer& operator=(const ByteBuffer&) = delete;

  // Reserva 'capacity' bytes (descarta o conteudo anterior). size() passa a ser 0.
  bool alloc(size_t capacity) {
    clear();
    _data = (uint8_t*)malloc(capacity ? capacity : 1);
    if (!_data) return false;
    _cap = capacity;
    _size = 0;
    return true;
  }

  // Copia 'n' bytes e deixa um '\0' extra no fim (util para textos).
  bool assign(const uint8_t* src, size_t n) {
    if (!alloc(n + 1)) return false;
    if (n) memcpy(_data, src, n);
    _data[n] = 0;
    _size = n;
    return true;
  }

  bool setSize(size_t n) {
    if (n > _cap) return false;
    _size = n;
    return true;
  }

  void clear() {
    if (_data) free(_data);
    _data = nullptr;
    _size = _cap = 0;
  }

  uint8_t* data() { return _data; }
  const uint8_t* data() const { return _data; }
  size_t size() const { return _size; }
  size_t capacity() const { return _cap; }
  bool empty() const { return _size == 0; }

 private:
  uint8_t* _data;
  size_t _size;
  size_t _cap;
};
