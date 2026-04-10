#include "device/device.h"

#include <cstring>

#include <nvboard.h>

namespace nnemu {

VgaDevice::VgaDevice()
    : framebuffer_(screen_width_ * screen_height_, 0) {}

void VgaDevice::set_nvboard(bool enabled) {
  nvboard_ = enabled;
  if (nvboard_) {
    nvboard_vga_set_framebuffer(framebuffer_.data(), screen_width_,
                                screen_height_);
  }
}

bool VgaDevice::load(reg_t addr, size_t len, uint8_t *bytes) {
  std::memset(bytes, 0, len);

  if (addr >= kCtlOffset && addr < kCtlOffset + 8) {
    uint32_t wh = (static_cast<uint32_t>(screen_width_) << 16) |
                  static_cast<uint32_t>(screen_height_);
    uint32_t sync = 0;
    uint8_t buf[8];
    std::memcpy(buf, &wh, 4);
    std::memcpy(buf + 4, &sync, 4);
    size_t off = addr - kCtlOffset;
    size_t copy = std::min(len, sizeof(buf) - static_cast<size_t>(off));
    std::memcpy(bytes, buf + off, copy);
    return true;
  }

  size_t fb_bytes = framebuffer_.size() * 4;
  if (addr < fb_bytes) {
    auto *base = reinterpret_cast<const uint8_t *>(framebuffer_.data());
    size_t copy = std::min(len, fb_bytes - static_cast<size_t>(addr));
    std::memcpy(bytes, base + addr, copy);
  }
  return true;
}

bool VgaDevice::store(reg_t addr, size_t len, const uint8_t *bytes) {
  if (addr >= kSyncOffset && addr < kSyncOffset + 4 && len >= 4) {
    uint32_t val;
    std::memcpy(&val, bytes, 4);
    if (val != 0) {
      sync_pending_ = true;
      if (nvboard_) {
        nvboard_vga_sync();
      }
    }
    return true;
  }

  if (addr >= kCtlOffset) {
    return true;
  }

  size_t fb_bytes = framebuffer_.size() * 4;
  if (addr < fb_bytes) {
    auto *base = reinterpret_cast<uint8_t *>(framebuffer_.data());
    size_t copy = std::min(len, fb_bytes - static_cast<size_t>(addr));
    std::memcpy(base + addr, bytes, copy);
  }
  return true;
}

} // namespace nnemu
