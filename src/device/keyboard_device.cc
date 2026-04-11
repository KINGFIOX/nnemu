#include "device/device.h"

#include <cstring>

namespace nemu {

bool KeyboardDevice::load(reg_t addr, size_t len, uint8_t *bytes) {
  std::memset(bytes, 0, len);
  if (len == 0 || addr >= kKeyboardSize)
    return true;

  uint8_t sc = 0;
  if (board_ && board_->kbd().Available()) {
    sc = board_->kbd().Dequeue();
  }
  bytes[0] = sc;
  return true;
}

// ignore
bool KeyboardDevice::store(reg_t /*addr*/, size_t /*len*/,
                           const uint8_t * /*bytes*/) {
  return true;
}

} // namespace nemu
