#include "device/device.h"

#include <cstdio>
#include <cstring>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace nnemu {

// 16550 register offsets
enum Reg : uint8_t {
  kTHR = 0,  // Transmit Holding / Receive Buffer (DLAB=0)
  kIER = 1,  // Interrupt Enable (DLAB=0) / DLM (DLAB=1)
  kFCR = 2,  // FIFO Control (write) / IIR (read)
  kLCR = 3,  // Line Control
  kMCR = 4,  // Modem Control
  kLSR = 5,  // Line Status
  kMSR = 6,  // Modem Status
  kSCR = 7,  // Scratch
};

static constexpr uint8_t kLCR_DLAB = 0x80;
static constexpr uint8_t kLSR_DR = 0x01;
static constexpr uint8_t kLSR_THRE = 0x20;
static constexpr uint8_t kLSR_TEMT = 0x40;
static constexpr uint8_t kIER_RDI = 0x01;  // Received data available

bool UartDevice::has_rx_data() const {
  if (board_ && board_->uart().Available()) return true;
  if (has_stdin_char_) return true;

  // Non-blocking poll of stdin
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  char c;
  ssize_t n = read(STDIN_FILENO, &c, 1);
  fcntl(STDIN_FILENO, F_SETFL, flags);
  if (n == 1) {
    const_cast<UartDevice *>(this)->stdin_char_ = static_cast<uint8_t>(c);
    const_cast<UartDevice *>(this)->has_stdin_char_ = true;
    return true;
  }
  return false;
}

bool UartDevice::wants_interrupt() const {
  return (ier_ & kIER_RDI) && has_rx_data();
}

bool UartDevice::load(reg_t addr, size_t len, uint8_t *bytes) {
  std::memset(bytes, 0, len);
  if (len == 0) return true;

  uint8_t val = 0;
  switch (addr) {
    case kTHR:
      if (lcr_ & kLCR_DLAB) {
        val = dll_;
      } else {
        if (board_ && board_->uart().Available()) {
          val = board_->uart().Getchar();
        } else if (has_stdin_char_) {
          val = stdin_char_;
          has_stdin_char_ = false;
        }
      }
      break;
    case kIER:
      val = (lcr_ & kLCR_DLAB) ? dlm_ : ier_;
      break;
    case kFCR:
      if (wants_interrupt()) {
        val = 0xC4;  // IIR: FIFOs enabled, RDA interrupt pending
      } else {
        val = 0xC1;  // IIR: FIFOs enabled, no pending interrupt
      }
      break;
    case kLCR:
      val = lcr_;
      break;
    case kLSR: {
      val = kLSR_THRE | kLSR_TEMT;
      bool has_data = (board_ && board_->uart().Available()) || has_stdin_char_;
#ifndef _WIN32
      if (!has_data) {
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        fcntl(STDIN_FILENO, F_SETFL, flags);
        if (n == 1) {
          stdin_char_ = static_cast<uint8_t>(c);
          has_stdin_char_ = true;
          has_data = true;
        }
      }
#endif
      if (has_data) val |= kLSR_DR;
      break;
    }
    default:
      break;
  }

  bytes[0] = val;
  return true;
}

bool UartDevice::store(reg_t addr, size_t len, const uint8_t *bytes) {
  if (len == 0) return true;

  uint8_t val = bytes[0];
  switch (addr) {
    case kTHR:
      if (lcr_ & kLCR_DLAB) {
        dll_ = val;
      } else {
        if (board_) {
          board_->uart().Putchar(val);
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

}  // namespace nnemu
