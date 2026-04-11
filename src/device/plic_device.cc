#include <cstring>

#include "absl/log/log.h"
#include "device/device.h"

namespace nnemu {

// PLIC register layout (base = 0x0c000000):
//   0x000000 + irq*4      : priority[irq]       (1 word per IRQ)
//   0x001000              : pending bitmap[0..31] (word 0)
//   0x001004              : pending bitmap[32..63] (word 1)
//   0x002000 + hart*0x100 : M-mode enable bitmap (ignored, we only do S-mode)
//   0x002080 + hart*0x100 : S-mode enable bitmap word 0
//   0x002084 + hart*0x100 : S-mode enable bitmap word 1
//   0x200000 + hart*0x1000: M-mode priority threshold (ignored)
//   0x200004 + hart*0x1000: M-mode claim/complete (ignored)
//   0x201000 + hart*0x2000: S-mode priority threshold
//   0x201004 + hart*0x2000: S-mode claim/complete

bool PlicDevice::load(reg_t addr, size_t len, uint8_t *bytes) {
  uint32_t val = 0;

  if (addr < 0x1000) {
    // Priority registers
    int irq = addr / 4;
    if (irq >= 0 && irq < kMaxIrqs) {
      val = priority_[irq];
    }
  } else if (addr >= 0x1000 && addr < 0x1008) {
    // Pending bitmap
    if (addr == 0x1000) {
      val = static_cast<uint32_t>(pending_);
    } else {
      val = static_cast<uint32_t>(pending_ >> 32);
    }
  } else if (addr >= 0x2000 && addr < 0x2000 + kMaxHarts * 0x100) {
    // Enable bitmaps
    uint64_t offset = addr - 0x2000;
    int hart = offset / 0x100;
    uint64_t reg_off = offset % 0x100;
    if (hart < kMaxHarts) {
      if (reg_off == 0x80) {
        val = static_cast<uint32_t>(s_enable_[hart]);
      } else if (reg_off == 0x84) {
        val = static_cast<uint32_t>(s_enable_[hart] >> 32);
      }
    }
  } else if (addr >= 0x200000 && addr < 0x200000 + kMaxHarts * 0x2000) {
    // Context registers
    uint64_t offset = addr - 0x200000;
    int hart = offset / 0x2000;
    uint64_t reg_off = offset % 0x2000;
    if (hart < kMaxHarts) {
      if (reg_off == 0x1000) {
        val = s_threshold_[hart];
      } else if (reg_off == 0x1004) {
        val = claim(hart);
      }
    }
  }

  if (len == 4) {
    std::memcpy(bytes, &val, 4);
  } else if (len == 1) {
    bytes[0] = val & 0xff;
  } else if (len == 2) {
    uint16_t v16 = val & 0xffff;
    std::memcpy(bytes, &v16, 2);
  }
  return true;
}

bool PlicDevice::store(reg_t addr, size_t len, const uint8_t *bytes) {
  uint32_t val = 0;
  if (len == 4) {
    std::memcpy(&val, bytes, 4);
  } else if (len == 1) {
    val = bytes[0];
  } else if (len == 2) {
    uint16_t v16 = 0;
    std::memcpy(&v16, bytes, 2);
    val = v16;
  }

  if (addr < 0x1000) {
    int irq = addr / 4;
    if (irq >= 0 && irq < kMaxIrqs) {
      priority_[irq] = val;
    }
  } else if (addr >= 0x2000 && addr < 0x2000 + kMaxHarts * 0x100) {
    uint64_t offset = addr - 0x2000;
    int hart = offset / 0x100;
    uint64_t reg_off = offset % 0x100;
    if (hart < kMaxHarts) {
      if (reg_off == 0x80) {
        s_enable_[hart] =
            (s_enable_[hart] & 0xFFFFFFFF00000000ULL) | val;
      } else if (reg_off == 0x84) {
        s_enable_[hart] =
            (s_enable_[hart] & 0x00000000FFFFFFFFULL) | (uint64_t(val) << 32);
      }
    }
  } else if (addr >= 0x200000 && addr < 0x200000 + kMaxHarts * 0x2000) {
    uint64_t offset = addr - 0x200000;
    int hart = offset / 0x2000;
    uint64_t reg_off = offset % 0x2000;
    if (hart < kMaxHarts) {
      if (reg_off == 0x1000) {
        s_threshold_[hart] = val;
      } else if (reg_off == 0x1004) {
        complete(hart, val);
      }
    }
  }

  return true;
}

void PlicDevice::raise_irq(int irq) {
  if (irq > 0 && irq < kMaxIrqs) {
    pending_ |= (1ULL << irq);
  }
}

void PlicDevice::clear_irq(int irq) {
  if (irq > 0 && irq < kMaxIrqs) {
    pending_ &= ~(1ULL << irq);
  }
}

bool PlicDevice::has_pending() const {
  for (int hart = 0; hart < kMaxHarts; ++hart) {
    uint64_t effective = pending_ & s_enable_[hart] & ~claimed_;
    if (effective == 0) continue;
    for (int irq = 1; irq < kMaxIrqs; ++irq) {
      if ((effective & (1ULL << irq)) && priority_[irq] > s_threshold_[hart]) {
        return true;
      }
    }
  }
  return false;
}

int PlicDevice::claim(int hart) {
  if (hart < 0 || hart >= kMaxHarts) return 0;
  uint64_t effective = pending_ & s_enable_[hart] & ~claimed_;

  int best_irq = 0;
  uint32_t best_prio = 0;
  for (int irq = 1; irq < kMaxIrqs; ++irq) {
    if (!(effective & (1ULL << irq))) continue;
    if (priority_[irq] > best_prio && priority_[irq] > s_threshold_[hart]) {
      best_irq = irq;
      best_prio = priority_[irq];
    }
  }

  if (best_irq > 0) {
    claimed_ |= (1ULL << best_irq);
    pending_ &= ~(1ULL << best_irq);
  }
  return best_irq;
}

void PlicDevice::complete(int hart, int irq) {
  (void)hart;
  if (irq > 0 && irq < kMaxIrqs) {
    claimed_ &= ~(1ULL << irq);
  }
}

}  // namespace nnemu
