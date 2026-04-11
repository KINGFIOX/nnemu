#ifndef NEMU_DEVICE_DEVICE_H_
#define NEMU_DEVICE_DEVICE_H_

#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>

#include "nvboard/nvboard.h"

#include "abstract_device.h"
#include "common.h"

namespace nemu {

// UART 16550 at kUartBase (0x10000000).
// Implements THR/RBR/LSR/LCR/DLL/DLM/FCR/IER registers.
// When board_ is set, output goes to nvboard UART terminal.
class UartDevice : public abstract_device_t {
 public:
  UartDevice() = default;
  void set_board(nvboard::Board *board) { board_ = board; }

  bool load(reg_t addr, size_t len, uint8_t *bytes) override;
  bool store(reg_t addr, size_t len, const uint8_t *bytes) override;

  bool has_rx_data() const;
  bool wants_interrupt() const;

 private:
  nvboard::Board *board_ = nullptr;
  uint8_t lcr_ = 0;
  uint8_t ier_ = 0;
  uint8_t dll_ = 1;
  uint8_t dlm_ = 0;
  uint8_t fcr_ = 0;
};

// GPIO at kGpioBase (0x10002000), 16 bytes.
// Offset 0x0: output (LED, 2 bytes)
// Offset 0x8: input (SW/BTN, 2+1 bytes)
class GpioDevice : public abstract_device_t {
public:
  GpioDevice() = default;
  void set_board(nvboard::Board *board) { board_ = board; }

  bool load(reg_t addr, size_t len, uint8_t *bytes) override;
  bool store(reg_t addr, size_t len, const uint8_t *bytes) override;

private:
  nvboard::Board *board_ = nullptr;
  uint16_t led_state_ = 0;
};

// PS/2 Keyboard at kKeyboardBase (0x10011000), 8 bytes.
// Read offset 0: next PS/2 scancode byte (0 if empty).
class KeyboardDevice : public abstract_device_t {
 public:
  KeyboardDevice() = default;
  void set_board(nvboard::Board *board) { board_ = board; }

  bool load(reg_t addr, size_t len, uint8_t *bytes) override;
  bool store(reg_t addr, size_t len, const uint8_t *bytes) override;

  bool has_pending_data() const {
    return board_ && board_->kbd().Available();
  }

 private:
  nvboard::Board *board_ = nullptr;
};

// VGA framebuffer at kVgaBase (0x21000000), kVgaSize (2 MiB).
// Layout:
//   [0x000000, 0x1FFF00): pixel data (640*480*4 = 0x12C000 bytes)
//   0x1FFF00: VGA control (width<<16|height)
//   0x1FFF04: sync flag (write non-zero to sync)
class VgaDevice : public abstract_device_t {
public:
  VgaDevice();
  void set_board(nvboard::Board *board);

  bool load(reg_t addr, size_t len, uint8_t *bytes) override;
  bool store(reg_t addr, size_t len, const uint8_t *bytes) override;

  uint32_t *framebuffer() { return framebuffer_.data(); }

private:
  static constexpr uint64_t kCtlOffset = 0x1FFF00;
  static constexpr uint64_t kSyncOffset = 0x1FFF04;

  nvboard::Board *board_ = nullptr;
  int screen_width_ = kDefaultScreenWidth;
  int screen_height_ = kDefaultScreenHeight;
  bool sync_pending_ = false;
  std::vector<uint32_t> framebuffer_;
};

// CLINT at kClintBase (0x02000000), kClintSize (64 KiB).
// Offsets:
//   0x0000: msip
//   0x4000: mtimecmp (lo32), 0x4004: mtimecmp (hi32)
//   0xBFF8: mtime (lo32), 0xBFFC: mtime (hi32) -- microseconds
class ClintDevice : public abstract_device_t {
 public:
  ClintDevice();

  bool load(reg_t addr, size_t len, uint8_t *bytes) override;
  bool store(reg_t addr, size_t len, const uint8_t *bytes) override;

 private:
  std::chrono::steady_clock::time_point boot_time_;
  uint32_t msip_ = 0;
  uint64_t mtimecmp_ = 0;
};

// PLIC at kPlicBase (0x0c000000), kPlicSize (0x400000).
// Implements S-mode external interrupt routing for up to 64 IRQ sources.
class PlicDevice : public abstract_device_t {
 public:
  PlicDevice() = default;

  bool load(reg_t addr, size_t len, uint8_t *bytes) override;
  bool store(reg_t addr, size_t len, const uint8_t *bytes) override;

  void raise_irq(int irq);
  void clear_irq(int irq);
  bool has_pending() const;

 private:
  static constexpr int kMaxIrqs = 64;
  static constexpr int kMaxHarts = 8;

  uint32_t priority_[kMaxIrqs] = {};
  uint64_t pending_ = 0;
  uint64_t claimed_ = 0;
  uint64_t s_enable_[kMaxHarts] = {};
  uint32_t s_threshold_[kMaxHarts] = {};

  int claim(int hart);
  void complete(int hart, int irq);
};

}  // namespace nemu

#endif  // NEMU_DEVICE_DEVICE_H_
