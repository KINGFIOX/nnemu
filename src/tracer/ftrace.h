#ifndef NEMU_TRACER_FTRACE_H_
#define NEMU_TRACER_FTRACE_H_

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <ostream>
#include <string>
#include <unordered_map>

#include "tracer/ring_buf.h"

namespace nemu {

inline constexpr size_t kFtraceCapacity = 32;

enum class FuncType { kCall, kRet };

struct FTraceEntry {
  uint64_t pc = 0;
  uint64_t dnpc = 0;
  uint32_t depth = 0;
  FuncType func_type = FuncType::kCall;
  std::string func_name;
  std::string disasm;

  FTraceEntry() = default;
  FTraceEntry(uint64_t pc, uint64_t dnpc, uint32_t depth, FuncType func_type,
              std::string func_name, std::string disasm)
      : pc(pc),
        dnpc(dnpc),
        depth(depth),
        func_type(func_type),
        func_name(std::move(func_name)),
        disasm(std::move(disasm)) {}

  friend std::ostream &operator<<(std::ostream &os, const FTraceEntry &e) {
    auto flags = os.flags();
    std::string indent(e.depth * 2, ' ');
    if (e.func_type == FuncType::kCall) {
      os << "0x" << std::hex << std::setfill('0') << std::setw(16) << e.pc
         << ": " << indent << "call [" << e.func_name << "@0x" << std::setw(16)
         << e.dnpc << "] (" << e.disasm << ")";
    } else {
      os << "0x" << std::hex << std::setfill('0') << std::setw(16) << e.pc
         << ": " << indent << "ret (" << e.disasm << ")";
    }
    os.flags(flags);
    return os;
  }
};

class FuncTracer {
 public:
  explicit FuncTracer(const std::filesystem::path &elf_path);

  void push_call(uint64_t pc, uint64_t dnpc, const std::string &disasm);
  void push_ret(uint64_t pc, uint64_t dnpc, const std::string &disasm);

  RingBuf<FTraceEntry> ring_buf;
  std::unordered_map<uint64_t, std::string> symtab;

 private:
  uint32_t depth_ = 0;
};

}  // namespace nemu

#endif  // NEMU_TRACER_FTRACE_H_
