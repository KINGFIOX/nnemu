#include "device/device.h"

#include <cstring>

namespace nemu {

ClintDevice::ClintDevice() : boot_time_(std::chrono::steady_clock::now()) {}

bool ClintDevice::load(reg_t addr, size_t len, uint8_t *bytes) {
  std::memset(bytes, 0, len);

  if (addr == 0x0000 && len >= 4) {
    // msip
    std::memcpy(bytes, &msip_, 4);
    return true;
  }

  if (addr >= 0x4000 && addr < 0x4008) {
    // mtimecmp
    uint8_t buf[8];
    std::memcpy(buf, &mtimecmp_, 8);
    size_t off = addr - 0x4000;
    size_t copy = std::min(len, sizeof(buf) - static_cast<size_t>(off));
    std::memcpy(bytes, buf + off, copy);
    return true;
  }

  if (addr >= 0xBFF8 && addr < 0xC000) {
    // mtime: microseconds since boot
    auto now = std::chrono::steady_clock::now();
    uint64_t us =
        std::chrono::duration_cast<std::chrono::microseconds>(now - boot_time_)
            .count();
    uint8_t buf[8];
    std::memcpy(buf, &us, 8);
    size_t off = addr - 0xBFF8;
    size_t copy = std::min(len, sizeof(buf) - static_cast<size_t>(off));
    std::memcpy(bytes, buf + off, copy);
    return true;
  }

  return true;
}

bool ClintDevice::store(reg_t addr, size_t len, const uint8_t *bytes) {
  if (addr == 0x0000 && len >= 4) {
    std::memcpy(&msip_, bytes, 4);
    return true;
  }

  if (addr >= 0x4000 && addr < 0x4008) {
    uint8_t buf[8];
    std::memcpy(buf, &mtimecmp_, 8);
    size_t off = addr - 0x4000;
    size_t copy = std::min(len, sizeof(buf) - static_cast<size_t>(off));
    std::memcpy(buf + off, bytes, copy);
    std::memcpy(&mtimecmp_, buf, 8);
    return true;
  }

  return true;
}

} // namespace nemu
