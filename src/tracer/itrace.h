#ifndef NEMU_TRACER_ITRACE_H_
#define NEMU_TRACER_ITRACE_H_

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <string>

namespace nemu {

struct ITraceEntry {
  uint64_t pc = 0;
  uint32_t inst = 0;
  std::string disasm;

  ITraceEntry() = default;
  ITraceEntry(uint64_t pc, uint32_t inst, std::string disasm)
      : pc(pc), inst(inst), disasm(std::move(disasm)) {}

  friend std::ostream &operator<<(std::ostream &os, const ITraceEntry &e) {
    auto flags = os.flags();
    os << "0x" << std::hex << std::setfill('0') << std::setw(16) << e.pc
       << ": [" << std::setw(8) << e.inst << "] " << e.disasm;
    os.flags(flags);
    return os;
  }
};

}  // namespace nemu

#endif  // NEMU_TRACER_ITRACE_H_
