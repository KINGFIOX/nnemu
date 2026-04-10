#ifndef NNEMU_DEVICE_DEVICE_H_
#define NNEMU_DEVICE_DEVICE_H_

#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>

#include "abstract_device.h"
#include "common.h"

namespace nnemu {

// Serial port at kSerialPort (0x10000000).
// Store a byte → putchar to stdout.
class SerialDevice : public abstract_device_t {
 public:
  bool load(reg_t addr, size_t len, uint8_t* bytes) override;
  bool store(reg_t addr, size_t len, const uint8_t* bytes) override;
};

// Combined device for all MMIO at kDeviceBase (0xa0000000).
//
// Dispatch by offset:
//   0x048       RTC (timer, 8 bytes: lo32 + hi32 microseconds)
//   0x060       Keyboard (4 bytes: keycode | keydown<<15)
//   0x100       VGA control (4 bytes width<<16|height; +4 sync flag)
//   0x200       Audio registers (stub)
//   0x300       Disk registers (stub)
//   0x1000000   Framebuffer (kDefaultScreenWidth * kDefaultScreenHeight * 4)
//   0x1200000   Audio sample buffer (stub)
class SimDevice : public abstract_device_t {
 public:
  SimDevice();

  bool load(reg_t addr, size_t len, uint8_t* bytes) override;
  bool store(reg_t addr, size_t len, const uint8_t* bytes) override;

  // --- Keyboard ---
  void EnqueueKey(uint32_t keycode) { keycode_ = keycode; }

  // --- VGA ---
  uint32_t* framebuffer() { return framebuffer_.data(); }
  int screen_width() const { return screen_width_; }
  int screen_height() const { return screen_height_; }
  bool ConsumeSync() {
    bool s = sync_pending_;
    sync_pending_ = false;
    return s;
  }

 private:
  bool LoadTimer(reg_t offset, size_t len, uint8_t* bytes);
  bool LoadKeyboard(reg_t offset, size_t len, uint8_t* bytes);
  bool LoadVgaCtl(reg_t offset, size_t len, uint8_t* bytes);
  bool StoreVgaCtl(reg_t offset, size_t len, const uint8_t* bytes);
  bool LoadFb(reg_t offset, size_t len, uint8_t* bytes);
  bool StoreFb(reg_t offset, size_t len, const uint8_t* bytes);

  // Timer
  std::chrono::steady_clock::time_point boot_time_;

  // Keyboard
  uint32_t keycode_ = 0;

  // VGA
  int screen_width_ = kDefaultScreenWidth;
  int screen_height_ = kDefaultScreenHeight;
  bool sync_pending_ = false;
  std::vector<uint32_t> framebuffer_;
};

}  // namespace nnemu

#endif  // NNEMU_DEVICE_DEVICE_H_
