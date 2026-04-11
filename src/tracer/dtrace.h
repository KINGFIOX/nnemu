#ifndef NEMU_TRACER_DTRACE_H_
#define NEMU_TRACER_DTRACE_H_

#include <cstdint>
#include <iomanip>
#include <ostream>

namespace nemu {

enum class MemDir { kRead, kWrite };

struct DTraceEntry {
  MemDir dir = MemDir::kRead;
  uint64_t addr = 0;
  uint64_t data = 0;
  uint8_t width = 0;

  DTraceEntry() = default;
  DTraceEntry(MemDir dir, uint64_t addr, uint64_t data, uint8_t width)
      : dir(dir), addr(addr), data(data), width(width) {}

  friend std::ostream &operator<<(std::ostream &os, const DTraceEntry &e) {
    auto flags = os.flags();
    os << (e.dir == MemDir::kRead ? "R" : "W") << " [0x" << std::hex
       << std::setfill('0') << std::setw(16) << e.addr << "] w"
       << static_cast<int>(e.width) << " = 0x" << std::setw(16) << e.data;
    os.flags(flags);
    return os;
  }
};

}  // namespace nemu

#endif  // NEMU_TRACER_DTRACE_H_
