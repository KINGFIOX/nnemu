#include "device/device.h"

#include <cstdio>

namespace nnemu {

bool SerialDevice::load(reg_t /*addr*/, size_t len, uint8_t* bytes) {
  std::memset(bytes, 0, len);
  return true;
}

bool SerialDevice::store(reg_t /*addr*/, size_t /*len*/,
                         const uint8_t* bytes) {
  std::putchar(static_cast<char>(bytes[0]));
  std::fflush(stdout);
  return true;
}

}  // namespace nnemu
