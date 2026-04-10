#include "device/device.h"

#include <cstdio>
#include <cstring>

#include <nvboard.h>

namespace nnemu {

// 16550 register offsets
enum Reg : uint8_t {
  kTHR = 0, // Transmit Holding / Receive Buffer (DLAB=0)
  kIER = 1, // Interrupt Enable (DLAB=0) / DLM (DLAB=1)
  kFCR = 2, // FIFO Control (write) / IIR (read)
  kLCR = 3, // Line Control
  kMCR = 4, // Modem Control
  kLSR = 5, // Line Status
  kMSR = 6, // Modem Status
  kSCR = 7, // Scratch
};

static constexpr uint8_t kLCR_DLAB = 0x80;
static constexpr uint8_t kLSR_DR = 0x01;
static constexpr uint8_t kLSR_THRE = 0x20;
static constexpr uint8_t kLSR_TEMT = 0x40;

bool UartDevice::load(reg_t addr, size_t len, uint8_t *bytes) {
  std::memset(bytes, 0, len);
  if (len == 0)
    return true;

  uint8_t val = 0;
  switch (addr) {
  case kTHR:
    if (lcr_ & kLCR_DLAB) {
      val = dll_;
    } else {
      if (nvboard_ && nvboard_uart_available()) {
        val = nvboard_uart_getchar();
      }
    }
    break;
  case kIER:
    val = (lcr_ & kLCR_DLAB) ? dlm_ : ier_;
    break;
  case kFCR:
    val = 0xC1; // IIR: FIFOs enabled, no pending interrupt
    break;
  case kLCR:
    val = lcr_;
    break;
  case kLSR: {
    val = kLSR_THRE | kLSR_TEMT;
    if (nvboard_ && nvboard_uart_available()) {
      val |= kLSR_DR;
    }
    break;
  }
  default:
    break;
  }

  bytes[0] = val;
  return true;
}

bool UartDevice::store(reg_t addr, size_t len, const uint8_t *bytes) {
  if (len == 0)
    return true;

  uint8_t val = bytes[0];
  switch (addr) {
  case kTHR:
    if (lcr_ & kLCR_DLAB) {
      dll_ = val;
    } else {
      if (nvboard_) {
        nvboard_uart_putchar(val);
      }
      std::putchar(static_cast<char>(val));
      std::fflush(stdout);
    }
    break;
  case kIER:
    if (lcr_ & kLCR_DLAB) {
      dlm_ = val;
    } else {
      ier_ = val;
    }
    break;
  case kFCR:
    fcr_ = val;
    break;
  case kLCR:
    lcr_ = val;
    break;
  default:
    break;
  }
  return true;
}

} // namespace nnemu
