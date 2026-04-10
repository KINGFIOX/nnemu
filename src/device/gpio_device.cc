#include "device/device.h"

#include <cstring>

#include <nvboard.h>

namespace nnemu {

bool GpioDevice::load(reg_t addr, size_t len, uint8_t *bytes) {
  std::memset(bytes, 0, len);
  if (len == 0)
    return true;

  if (addr < 0x8) {
    // Output register (LED state) readback
    uint8_t buf[8] = {};
    std::memcpy(buf, &led_state_, sizeof(led_state_));
    size_t copy = std::min(len, sizeof(buf) - static_cast<size_t>(addr));
    std::memcpy(bytes, buf + addr, copy);
  } else {
    // Input register (switches + buttons)
    uint16_t sw = nvboard_ ? nvboard_sw_read() : 0;
    uint8_t btn = nvboard_ ? nvboard_btn_read() : 0;
    uint8_t buf[8] = {};
    std::memcpy(buf, &sw, sizeof(sw));
    buf[2] = btn;
    size_t off = addr - 0x8;
    size_t copy = std::min(len, sizeof(buf) - static_cast<size_t>(off));
    std::memcpy(bytes, buf + off, copy);
  }
  return true;
}

bool GpioDevice::store(reg_t addr, size_t len, const uint8_t *bytes) {
  if (addr < 0x8 && len >= 2) {
    std::memcpy(&led_state_, bytes, 2);
    if (nvboard_) {
      nvboard_led_write(led_state_);
    }
  }
  return true;
}

} // namespace nnemu
