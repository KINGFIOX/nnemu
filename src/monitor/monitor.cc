#include "monitor/monitor.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <nvboard.h>

#include "absl/log/log.h"
#include "common.h"
#include "sim.h"

namespace nnemu {

Monitor::Monitor(const Config& config) : config_(config) {}

Monitor::~Monitor() {
  if (nvboard_active_) {
    nvboard_quit();
  }
  delete sim_;
  delete cfg_;
}

int Monitor::Run() {
  if (config_.nvboard) {
    nvboard_init();
    nvboard_active_ = true;
    LOG(INFO) << "NVBoard initialized.";
  }

  InitSpike();
  LoadImage();
  MainLoop();
  return exit_code_;
}

void Monitor::InitSpike() {
  constexpr size_t kAlignMask = 0xFFF;
  size_t aligned = (kPmemSize + kAlignMask) & ~kAlignMask;
  pmem_ = new mem_t(aligned);

  std::vector<std::pair<reg_t, mem_t*>> mems;
  mems.emplace_back(reg_t(kPmemBase), pmem_);

  std::vector<std::pair<reg_t, abstract_device_t*>> plugin_devices;
  plugin_devices.emplace_back(reg_t(kSerialPort), &serial_device_);
  plugin_devices.emplace_back(reg_t(kDeviceBase), &sim_device_);

  static debug_module_config_t dm_config = {
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

  cfg_ = new cfg_t(
      /*default_initrd_bounds=*/std::make_pair(reg_t(0), reg_t(0)),
      /*default_bootargs=*/nullptr,
      /*default_isa=*/"rv64im_zicsr",
      /*default_priv=*/DEFAULT_PRIV,
      /*default_varch=*/DEFAULT_VARCH,
      /*default_misaligned=*/false,
      /*default_endianness=*/endianness_little,
      /*default_pmpregions=*/16,
      /*default_mem_layout=*/std::vector<mem_cfg_t>(),
      /*default_hartids=*/std::vector<size_t>(1),
      /*default_real_time_clint=*/false,
      /*default_trigger_count=*/4);

  std::vector<std::string> htif_args{""};

  sim_ = new sim_t(cfg_, /*halted=*/false, mems, plugin_devices, htif_args,
                    dm_config, /*log_path=*/nullptr,
                    /*dtb_enabled=*/false, /*dtb_file=*/nullptr,
                    /*socket_enabled=*/false, /*cmd_file=*/nullptr,
                    /*is_diff_ref=*/true);

  proc_ = sim_->get_core(0);
  proc_->get_state()->pc = kPmemBase;
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
  file.read(reinterpret_cast<char*>(buf.data()), size);

  // mem_t uses sparse page storage; must write page-by-page.
  constexpr size_t kPageSize = 4096;
  for (size_t offset = 0; offset < buf.size(); offset += kPageSize) {
    size_t chunk = std::min(kPageSize, buf.size() - offset);
    char* page = pmem_->contents(offset);
    std::memcpy(page, buf.data() + offset, chunk);
  }

  LOG(INFO) << "Loaded image: " << config_.image_path << " (" << size
            << " bytes)";
}

void Monitor::Step(uint64_t n) {
  for (uint64_t i = 0; i < n && state_ == CpuState::kRunning; ++i) {
    uint64_t pc = proc_->get_state()->pc;

    if (pc < kPmemBase || pc >= kPmemBase + pmem_->size()) {
      LOG(ERROR) << "PC out of pmem range: 0x" << std::hex << pc;
      state_ = CpuState::kAborted;
      return;
    }

    uint32_t insn = 0;
    std::memcpy(&insn, pmem_->contents(pc - kPmemBase), sizeof(insn));

    if (insn == kEbreakInsn) {
      exit_code_ = static_cast<int>(proc_->get_state()->XPR[10]);  // a0
      state_ = CpuState::kStopped;
      return;
    }

    try {
      proc_->step(1);
    } catch (...) {
      LOG(ERROR) << "Exception during step at PC = 0x" << std::hex << pc;
      state_ = CpuState::kAborted;
      return;
    }
  }
}

void Monitor::MainLoop() {
  constexpr uint64_t kBatchSteps = 65536;

  while (state_ == CpuState::kRunning) {
    Step(config_.batch ? kBatchSteps : 1);

    if (nvboard_active_) {
      nvboard_update();
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
               << proc_->get_state()->pc;
  }
}

}  // namespace nnemu
