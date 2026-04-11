#include "device/device.h"

#include <cstdio>
#include <cstring>

namespace nemu {

enum Reg : uint8_t {
  kTHR = 0,
  kIER = 1,
  kFCR = 2,
  kLCR = 3,
  kMCR = 4,
  kLSR = 5,
  kMSR = 6,
  kSCR = 7,
};

static constexpr uint8_t kLCR_DLAB = 0x80;
static constexpr uint8_t kLSR_DR = 0x01;
static constexpr uint8_t kLSR_THRE = 0x20;
static constexpr uint8_t kLSR_TEMT = 0x40;
static constexpr uint8_t kIER_RDI = 0x01;

bool UartDevice::has_rx_data() const {
  return board_ && board_->uart().Available();
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
      } else if (board_ && board_->uart().Available()) {
        val = board_->uart().Getchar();
      }
      break;
    case kIER:
      val = (lcr_ & kLCR_DLAB) ? dlm_ : ier_;
      break;
    case kFCR:
      if (wants_interrupt()) {
        val = 0xC4;
      } else {
        val = 0xC1;
      }
      break;
    case kLCR:
      val = lcr_;
      break;
    case kLSR: {
      val = kLSR_THRE | kLSR_TEMT;
      if (has_rx_data()) val |= kLSR_DR;
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

}  // namespace nemu
