#ifndef NNEMU_MONITOR_MONITOR_H_
#define NNEMU_MONITOR_MONITOR_H_

#include <memory>
#include <string>

#include "nvboard/nvboard.h"

#include "device/device.h"

class sim_t;
class processor_t;
class mem_t;
struct cfg_t;

namespace nnemu {

enum class CpuState {
  kRunning,
  kStopped,
  kAborted,
};

class Monitor {
public:
  struct Config {
    bool batch = false;
    bool nvboard = false;
    std::string log_file;
    std::string image_path;
  };

  explicit Monitor(const Config &config);
  ~Monitor();

  Monitor(const Monitor &) = delete;
  Monitor &operator=(const Monitor &) = delete;

  int Run();

private:
  void InitSpike();
  void LoadImage();
  void MainLoop();
  void Step(uint64_t n);

  char *AddrToHost(uint64_t paddr);

  Config config_;

  // Spike instances (owned)
  std::unique_ptr<sim_t> sim_;

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

  // State
  CpuState state_ = CpuState::kRunning;
  // 0: GOOD TRAP
  // 1: BAD TRAP
  // 2: ABORT
  int exit_code_ = 0;
};

} // namespace nnemu

#endif // NNEMU_MONITOR_MONITOR_H_
