#include "device/device.h"

#include <cstring>

#include <nvboard.h>

namespace nnemu {

bool KeyboardDevice::load(reg_t addr, size_t len, uint8_t *bytes) {
  std::memset(bytes, 0, len);
  if (len == 0 || addr >= kKeyboardSize)
    return true;

  uint8_t sc = 0;
  if (nvboard_ && nvboard_kbd_available()) {
    sc = nvboard_kbd_dequeue();
  }
  bytes[0] = sc;
  return true;
}

bool KeyboardDevice::store(reg_t /*addr*/, size_t /*len*/,
                           const uint8_t * /*bytes*/) {
  return true;
}

} // namespace nnemu
