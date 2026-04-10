#include "device/device.h"

#include <chrono>
#include <cstring>

#include "absl/log/log.h"

namespace nnemu {

SimDevice::SimDevice()
    : boot_time_(std::chrono::steady_clock::now()),
      framebuffer_(screen_width_ * screen_height_, 0) {}

// ---- Top-level dispatch ----

bool SimDevice::load(reg_t addr, size_t len, uint8_t* bytes) {
  if (addr >= kFbOffset && addr < kFbOffset + framebuffer_.size() * 4) {
    return LoadFb(addr - kFbOffset, len, bytes);
  }

  switch (addr & ~0xFULL) {
    case kRtcOffset & ~0xFULL:
      return LoadTimer(addr - kRtcOffset, len, bytes);
    case kKbdOffset & ~0xFULL:
      return LoadKeyboard(addr - kKbdOffset, len, bytes);
    case kVgaCtlOffset & ~0xFULL:
      return LoadVgaCtl(addr - kVgaCtlOffset, len, bytes);
    default:
      std::memset(bytes, 0, len);
      return true;
  }
}

bool SimDevice::store(reg_t addr, size_t len, const uint8_t* bytes) {
  if (addr >= kFbOffset && addr < kFbOffset + framebuffer_.size() * 4) {
    return StoreFb(addr - kFbOffset, len, bytes);
  }

  if (addr >= kVgaCtlOffset && addr < kVgaCtlOffset + 8) {
    return StoreVgaCtl(addr - kVgaCtlOffset, len, bytes);
  }

  return true;
}

// ---- Timer (RTC_ADDR = kDeviceBase + 0x48) ----
// Layout: [lo32 microseconds] [hi32 microseconds]

bool SimDevice::LoadTimer(reg_t offset, size_t len, uint8_t* bytes) {
  auto now = std::chrono::steady_clock::now();
  uint64_t us =
      std::chrono::duration_cast<std::chrono::microseconds>(now - boot_time_)
          .count();

  uint32_t lo = static_cast<uint32_t>(us);
  uint32_t hi = static_cast<uint32_t>(us >> 32);

  uint8_t buf[8];
  std::memcpy(buf, &lo, 4);
  std::memcpy(buf + 4, &hi, 4);

  size_t copy_len = std::min(len, sizeof(buf) - static_cast<size_t>(offset));
  std::memcpy(bytes, buf + offset, copy_len);
  return true;
}

// ---- Keyboard (KBD_ADDR = kDeviceBase + 0x60) ----
// Read 32-bit: keycode | (keydown << 15), then clear.

bool SimDevice::LoadKeyboard(reg_t /*offset*/, size_t len, uint8_t* bytes) {
  uint32_t kc = keycode_;
  keycode_ = 0;

  size_t copy_len = std::min(len, sizeof(kc));
  std::memcpy(bytes, &kc, copy_len);
  return true;
}

// ---- VGA control (VGACTL_ADDR = kDeviceBase + 0x100) ----
// +0: (width << 16) | height (read-only)
// +4: sync flag (write 1 to sync)

bool SimDevice::LoadVgaCtl(reg_t offset, size_t len, uint8_t* bytes) {
  uint32_t wh = (static_cast<uint32_t>(screen_width_) << 16) |
                static_cast<uint32_t>(screen_height_);
  uint32_t sync = 0;

  uint8_t buf[8];
  std::memcpy(buf, &wh, 4);
  std::memcpy(buf + 4, &sync, 4);

  size_t copy_len = std::min(len, sizeof(buf) - static_cast<size_t>(offset));
  std::memcpy(bytes, buf + offset, copy_len);
  return true;
}

bool SimDevice::StoreVgaCtl(reg_t offset, size_t len, const uint8_t* bytes) {
  if (offset == 4 && len >= 4) {
    uint32_t val;
    std::memcpy(&val, bytes, 4);
    if (val != 0) {
      sync_pending_ = true;
    }
  }
  return true;
}

// ---- Framebuffer (FB_ADDR = kDeviceBase + 0x1000000) ----

bool SimDevice::LoadFb(reg_t offset, size_t len, uint8_t* bytes) {
  auto* base = reinterpret_cast<const uint8_t*>(framebuffer_.data());
  std::memcpy(bytes, base + offset, len);
  return true;
}

bool SimDevice::StoreFb(reg_t offset, size_t len, const uint8_t* bytes) {
  auto* base = reinterpret_cast<uint8_t*>(framebuffer_.data());
  std::memcpy(base + offset, bytes, len);
  return true;
}

}  // namespace nnemu
