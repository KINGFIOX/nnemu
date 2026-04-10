#include "monitor/monitor.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "common.h"
#include "sim.h"

namespace nnemu {

Monitor::Monitor(const Config &config) : config_(config) {}

Monitor::~Monitor() = default;

int Monitor::Run() {
  if (config_.nvboard) {
    board_ = nvboard::Board::Create();
    LOG(INFO) << "NVBoard initialized.";
  }

  // board
  nvboard::Board *bp = board_.get();
  uart_device_.set_board(bp);
  gpio_device_.set_board(bp);
  keyboard_device_.set_board(bp);
  vga_device_.set_board(bp);

  InitSpike();
  LoadImage();
  MainLoop();
  return exit_code_;
}

void Monitor::InitSpike() {
  constexpr size_t kAlignMask = 0xFFF;
  auto align = [](size_t sz) -> size_t {
    return (sz + kAlignMask) & ~static_cast<size_t>(kAlignMask);
  };

  // rom + ram
  flash_ = new mem_t(align(kFlashSize));
  sdram_ = new mem_t(align(kSdramSize));

  // mem
  std::vector<std::pair<reg_t, mem_t *>> mems;
  mems.emplace_back(reg_t(kFlashBase), flash_);
  mems.emplace_back(reg_t(kSdramBase), sdram_);

  // mmio bus
  std::vector<std::pair<reg_t, abstract_device_t *>> plugin_devices;
  plugin_devices.emplace_back(reg_t(kClintBase), &clint_device_);
  plugin_devices.emplace_back(reg_t(kUartBase), &uart_device_);
  plugin_devices.emplace_back(reg_t(kGpioBase), &gpio_device_);
  plugin_devices.emplace_back(reg_t(kKeyboardBase), &keyboard_device_);
  plugin_devices.emplace_back(reg_t(kVgaBase), &vga_device_);

  // for debugger
  debug_module_config_t dm_config = {
      .progbufsize = 2,
      .max_sba_data_width = 0,
      .require_authentication = false,
      .abstract_rti = 0,
      .support_hasel = true,
      .support_abstract_csr_access = true,
      .support_abstract_fpr_access = true,
      .support_haltgroups = true,
      .support_impebreak = true,
  };

  cfg_t cfg = {/*default_initrd_bounds=*/std::make_pair(reg_t(0), reg_t(0)),
               /*default_bootargs=*/nullptr,
               /*default_isa=*/"RV64IMAFDC",
               /*default_priv=*/DEFAULT_PRIV,
               /*default_varch=*/DEFAULT_VARCH,
               /*default_misaligned=*/false,
               /*default_endianness=*/endianness_little,
               /*default_pmpregions=*/16,
               /*default_mem_layout=*/std::vector<mem_cfg_t>(),
               /*default_hartids=*/std::vector<size_t>(1),
               /*default_real_time_clint=*/false,
               /*default_trigger_count=*/4};

  // host target interface
  std::vector<std::string> htif_args{""};

  sim_ = std::make_unique<sim_t>(&cfg, /*halted=*/false, mems, plugin_devices,
                                 htif_args, dm_config, /*log_path=*/nullptr,
                                 /*dtb_enabled=*/false, /*dtb_file=*/nullptr,
                                 /*socket_enabled=*/false, /*cmd_file=*/nullptr,
                                 /*is_diff_ref=*/true);

  sim_->get_core(0)->get_state()->pc = kResetVector;
  LOG(INFO) << "Spike initialized (rv64im_zicsr).";
}

void Monitor::LoadImage() {
  std::ifstream file(config_.image_path, std::ios::binary | std::ios::ate);
  if (!file) {
    LOG(ERROR) << "Failed to open image: " << config_.image_path;
    std::exit(1);
  }

  auto size = file.tellg();
  file.seekg(0);

  std::vector<uint8_t> buf(size);
  file.read(reinterpret_cast<char *>(buf.data()), size);

  constexpr size_t kPageSize = 4096;
  for (size_t offset = 0; offset < buf.size(); offset += kPageSize) {
    size_t chunk = std::min(kPageSize, buf.size() - offset);
    char *page = flash_->contents(offset);
    std::memcpy(page, buf.data() + offset, chunk);
  }

  LOG(INFO) << "Loaded image: " << config_.image_path << " (" << size
            << " bytes)";
}

char *Monitor::AddrToHost(uint64_t paddr) {
  if (paddr >= kFlashBase && paddr < kFlashBase + kFlashSize) {
    return flash_->contents(paddr - kFlashBase);
  }
  if (paddr >= kSdramBase && paddr < kSdramBase + kSdramSize) {
    return sdram_->contents(paddr - kSdramBase);
  }
  return nullptr;
}

void Monitor::Step(uint64_t n) {
  for (uint64_t i = 0; i < n && state_ == CpuState::kRunning; ++i) {
    uint64_t pc = sim_->get_core(0)->get_state()->pc;

    char *host_pc = AddrToHost(pc);
    if (!host_pc) {
      LOG(ERROR) << "PC out of memory range: 0x" << std::hex << pc;
      state_ = CpuState::kAborted;
      exit_code_ = 2;
      return;
    }

    // get inst
    uint32_t insn = 0;
    std::memcpy(&insn, host_pc, sizeof(insn));

    // ebreak
    if (insn == kEbreakInsn) {
      uint64_t a0 = sim_->get_core(0)->get_state()->XPR[10];
      exit_code_ = (a0 == 0) ? 0 : 1;
      state_ = CpuState::kStopped;
      LOG(INFO) << "ebreak at PC = 0x" << std::hex << pc << ", a0 = " << a0;
      return;
    }

    try {
      sim_->get_core(0)->step(1);
    } catch (const std::exception &e) {
      LOG(ERROR) << "Exception during step at PC = 0x" << std::hex << pc << ": "
                 << e.what();
      state_ = CpuState::kAborted;
      exit_code_ = 2;
      return;
    } catch (...) {
      LOG(ERROR) << "Unknown exception during step at PC = 0x" << std::hex
                 << pc;
      state_ = CpuState::kAborted;
      exit_code_ = 2;
      return;
    }
  }
}

void Monitor::MainLoop() {
  constexpr uint64_t kBatchSteps = -1;

  while (state_ == CpuState::kRunning) {
    Step(1);

    if (board_) {
      board_->Update();
    }
  }

  if (state_ == CpuState::kStopped) {
    if (exit_code_ == 0) {
      LOG(INFO) << "Program exited normally (HIT GOOD TRAP).";
    } else {
      LOG(ERROR) << "Program exited with code " << exit_code_
                 << " (HIT BAD TRAP).";
    }
  } else {
    LOG(ERROR) << "Program aborted at PC = 0x" << std::hex
               << sim_->get_core(0)->get_state()->pc;
  }
}

} // namespace nnemu
