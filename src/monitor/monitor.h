#ifndef NNEMU_MONITOR_MONITOR_H_
#define NNEMU_MONITOR_MONITOR_H_

#include <memory>
#include <string>

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

  // Returns a pointer into the appropriate mem_t for the given physical addr,
  // or nullptr if the address is not in any memory region.
  char *AddrToHost(uint64_t paddr);

  Config config_;

  // Spike instances (owned)
  sim_t *sim_ = nullptr;
  cfg_t *cfg_ = nullptr;
  processor_t *proc_ = nullptr;

  // Memory regions
  mem_t *flash_ = nullptr;
  mem_t *sram_ = nullptr;
  mem_t *sdram_ = nullptr;

  // Devices
  UartDevice uart_device_;
  GpioDevice gpio_device_;
  KeyboardDevice keyboard_device_;
  VgaDevice vga_device_;
  ClintDevice clint_device_;

  // State
  CpuState state_ = CpuState::kRunning;
  int exit_code_ = 0;
  bool nvboard_active_ = false;
};

} // namespace nnemu

#endif // NNEMU_MONITOR_MONITOR_H_
