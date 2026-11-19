#pragma once
// Cabecalho unico da biblioteca FileLinkBLE.
#include "ByteBuffer.h"
#include "FileLinkTypes.h"
#include "FileCompressor.h"  // comprimir / descomprimir (LZ4)
#include "BlockCodec.h"      // dividir (BlockSplitter) / mesclar (BlockAssembler)

#if defined(ARDUINO_ARCH_ESP32)
#include "BleBlockSender.h"    // emissor BLE (cliente GATT)
#include "BleBlockReceiver.h"  // receptor BLE (servidor GATT)
#endif
