#ifndef NNEMU_MONITOR_MONITOR_H_
#define NNEMU_MONITOR_MONITOR_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "nvboard/nvboard.h"

#include "device/device.h"
#include "tracer/dtrace.h"
#include "tracer/ftrace.h"
#include "tracer/itrace.h"
#include "tracer/ring_buf.h"

class sim_t;
class processor_t;
class mem_t;
class disassembler_t;
class isa_parser_t;
struct cfg_t;

namespace nnemu {

class Sdb;

enum class CpuState {
  kRunning,
  kStopped,
  kAborted,
};

inline constexpr size_t kITraceCapacity = 16;
inline constexpr size_t kDTraceCapacity = 16;

class Monitor {
 public:
  struct Config {
    bool batch = false;
    bool nvboard = false;
    std::string log_file;
    std::string image_path;
    std::string elf_path;
  };

  explicit Monitor(const Config &config);
  ~Monitor();

  Monitor(const Monitor &) = delete;
  Monitor &operator=(const Monitor &) = delete;

  int Run();

  CpuState state() const { return state_; }
  int exit_code() const { return exit_code_; }
  processor_t *core() const;
  sim_t *sim() const { return sim_.get(); }
  char *AddrToHost(uint64_t paddr);

  // Direct Spike accessors (no abstraction layer needed)
  uint64_t pc() const;
  std::optional<uint64_t> gpr(int index) const;
  std::optional<uint64_t> csr(int which) const;
  std::optional<uint64_t> mem_load(uint64_t addr, uint8_t width) const;
  std::optional<uint64_t> reg_value(const std::string &name) const;

  void StepOne();
  void DumpTraces() const;

  void set_external_interrupt(bool val);

 private:
  void InitSpike();
  void LoadImage();
  void LoadElf();
  void MainLoop();
  void Step(uint64_t n);

  Config config_;

  // Spike instances (owned)
  std::unique_ptr<sim_t> sim_;
  std::unique_ptr<isa_parser_t> isa_parser_;
  std::unique_ptr<disassembler_t> disasm_;

  // Memory regions
  mem_t *flash_ = nullptr;
  mem_t *sdram_ = nullptr;

  // NVBoard
  std::unique_ptr<nvboard::Board> board_;

  // Devices
  UartDevice uart_device_;
  GpioDevice gpio_device_;
  KeyboardDevice keyboard_device_;
  VgaDevice vga_device_;
  ClintDevice clint_device_;
  PlicDevice plic_device_;

  // Trace
  RingBuf<ITraceEntry> itrace_{kITraceCapacity};
  RingBuf<DTraceEntry> dtrace_{kDTraceCapacity};
  std::unique_ptr<FuncTracer> ftrace_;

  // SDB
  std::unique_ptr<Sdb> sdb_;

  // State
  CpuState state_ = CpuState::kRunning;
  // 0: GOOD TRAP
  // 1: BAD TRAP
  // 2: ABORT
  int exit_code_ = 0;
};

}  // namespace nnemu

#endif  // NNEMU_MONITOR_MONITOR_H_
