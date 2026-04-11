#include "monitor/monitor.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "common.h"
#include "disasm.h"
#include "encoding.h"
#include "isa_parser.h"
#include "processor.h"
#include "sim.h"
#include "sdb/sdb.h"

namespace nnemu {

Monitor::Monitor(const Config &config) : config_(config) {}

Monitor::~Monitor() = default;

processor_t *Monitor::core() const {
  return sim_->get_core(0);
}

uint64_t Monitor::pc() const {
  return core()->get_state()->pc;
}

std::optional<uint64_t> Monitor::gpr(int index) const {
  if (index < 0 || index >= 32) return std::nullopt;
  return core()->get_state()->XPR[index];
}

std::optional<uint64_t> Monitor::csr(int which) const {
  try {
    return core()->get_csr(which);
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<uint64_t> Monitor::mem_load(uint64_t addr, uint8_t width) const {
  char *host = const_cast<Monitor *>(this)->AddrToHost(addr);
  if (!host) return std::nullopt;
  uint64_t val = 0;
  std::memcpy(&val, host, width);
  return val;
}

std::optional<uint64_t> Monitor::reg_value(const std::string &name) const {
  if (name == "pc") return pc();

  if (name.size() >= 2 && name[0] == 'x') {
    int idx = 0;
    bool valid = true;
    for (size_t i = 1; i < name.size(); ++i) {
      if (name[i] < '0' || name[i] > '9') { valid = false; break; }
      idx = idx * 10 + (name[i] - '0');
    }
    if (valid && idx >= 0 && idx < 32) return gpr(idx);
    return std::nullopt;
  }

  for (int i = 0; i < 32; ++i) {
    if (name == kGprNames[i]) return gpr(i);
  }
  if (name == "fp") return gpr(8);
  if (name == "zero") return gpr(0);

  if (name == "mstatus") return csr(0x300);
  if (name == "mtvec") return csr(0x305);
  if (name == "mepc") return csr(0x341);
  if (name == "mcause") return csr(0x342);
  if (name == "mtval") return csr(0x343);
  if (name == "mie") return csr(0x304);
  if (name == "mip") return csr(0x344);
  if (name == "sstatus") return csr(0x100);
  if (name == "stvec") return csr(0x105);
  if (name == "sepc") return csr(0x141);
  if (name == "scause") return csr(0x142);
  if (name == "stval") return csr(0x143);
  if (name == "satp") return csr(0x180);

  return std::nullopt;
}

int Monitor::Run() {
  if (config_.nvboard) {
    board_ = nvboard::Board::Create();
    LOG(INFO) << "NVBoard initialized.";
  }

  nvboard::Board *bp = board_.get();
  uart_device_.set_board(bp);
  gpio_device_.set_board(bp);
  keyboard_device_.set_board(bp);
  vga_device_.set_board(bp);

  InitSpike();

  if (!config_.elf_path.empty()) {
    LoadElf();
  } else {
    LoadImage();
  }

  if (!config_.elf_path.empty()) {
    ftrace_ = std::make_unique<FuncTracer>(config_.elf_path);
  }

  sdb_ = std::make_unique<Sdb>(*this);

  MainLoop();
  return exit_code_;
}

void Monitor::InitSpike() {
  constexpr size_t kAlignMask = 0xFFF;
  auto align = [](size_t sz) -> size_t {
    return (sz + kAlignMask) & ~static_cast<size_t>(kAlignMask);
  };

  flash_ = new mem_t(align(kFlashSize));
  sdram_ = new mem_t(align(kSdramSize));

  std::vector<std::pair<reg_t, mem_t *>> mems;
  mems.emplace_back(reg_t(kFlashBase), flash_);
  mems.emplace_back(reg_t(kSdramBase), sdram_);

  std::vector<std::pair<reg_t, abstract_device_t *>> plugin_devices;
  plugin_devices.emplace_back(reg_t(kClintBase), &clint_device_);
  plugin_devices.emplace_back(reg_t(kUartBase), &uart_device_);
  plugin_devices.emplace_back(reg_t(kGpioBase), &gpio_device_);
  plugin_devices.emplace_back(reg_t(kKeyboardBase), &keyboard_device_);
  plugin_devices.emplace_back(reg_t(kVgaBase), &vga_device_);
  plugin_devices.emplace_back(reg_t(kPlicBase), &plic_device_);

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

  const char *isa_str = "RV64IMAFDC_zicsr_sstc";

  cfg_t cfg = {/*default_initrd_bounds=*/std::make_pair(reg_t(0), reg_t(0)),
               /*default_bootargs=*/nullptr,
               /*default_isa=*/isa_str,
               /*default_priv=*/DEFAULT_PRIV,
               /*default_varch=*/DEFAULT_VARCH,
               /*default_misaligned=*/false,
               /*default_endianness=*/endianness_little,
               /*default_pmpregions=*/16,
               /*default_mem_layout=*/std::vector<mem_cfg_t>(),
               /*default_hartids=*/std::vector<size_t>(1),
               /*default_real_time_clint=*/false,
               /*default_trigger_count=*/4};

  std::vector<std::string> htif_args{""};

  sim_ = std::make_unique<sim_t>(&cfg, /*halted=*/false, mems, plugin_devices,
                                 htif_args, dm_config, /*log_path=*/nullptr,
                                 /*dtb_enabled=*/false, /*dtb_file=*/nullptr,
                                 /*socket_enabled=*/false, /*cmd_file=*/nullptr,
                                 /*is_diff_ref=*/true);

  uint64_t reset_vec =
      config_.elf_path.empty() ? kResetVector : uint64_t(0x80000000);
  sim_->get_core(0)->get_state()->pc = reset_vec;

  isa_parser_ =
      std::make_unique<isa_parser_t>(isa_str, DEFAULT_PRIV);
  disasm_ = std::make_unique<disassembler_t>(isa_parser_.get());

  std::ostringstream oss;
  oss << "Spike initialized (" << isa_str << "), reset PC = 0x" << std::hex
      << std::setfill('0') << std::setw(16) << reset_vec;
  LOG(INFO) << oss.str();
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

void Monitor::LoadElf() {
  std::ifstream file(config_.elf_path, std::ios::binary | std::ios::ate);
  if (!file) {
    LOG(ERROR) << "Failed to open ELF: " << config_.elf_path;
    std::exit(1);
  }

  auto file_size = file.tellg();
  file.seekg(0);

  std::vector<uint8_t> buf(file_size);
  file.read(reinterpret_cast<char *>(buf.data()), file_size);

  if (file_size < 64 || buf[0] != 0x7f || buf[1] != 'E' || buf[2] != 'L' ||
      buf[3] != 'F') {
    LOG(ERROR) << "Not a valid ELF file: " << config_.elf_path;
    std::exit(1);
  }

  // ELF64 header parsing
  uint16_t e_phnum = 0;
  uint64_t e_phoff = 0;
  uint16_t e_phentsize = 0;
  uint64_t e_entry = 0;
  std::memcpy(&e_phoff, buf.data() + 32, 8);
  std::memcpy(&e_phentsize, buf.data() + 54, 2);
  std::memcpy(&e_phnum, buf.data() + 56, 2);
  std::memcpy(&e_entry, buf.data() + 24, 8);

  size_t total_loaded = 0;
  for (int i = 0; i < e_phnum; ++i) {
    const uint8_t *ph = buf.data() + e_phoff + i * e_phentsize;
    uint32_t p_type = 0;
    std::memcpy(&p_type, ph, 4);
    if (p_type != 1) continue;  // PT_LOAD = 1

    uint64_t p_offset = 0, p_vaddr = 0, p_filesz = 0, p_memsz = 0;
    std::memcpy(&p_offset, ph + 8, 8);
    std::memcpy(&p_vaddr, ph + 16, 8);
    std::memcpy(&p_filesz, ph + 32, 8);
    std::memcpy(&p_memsz, ph + 40, 8);

    char *host = AddrToHost(p_vaddr);
    if (!host) {
      LOG(ERROR) << "ELF segment vaddr 0x" << std::hex << p_vaddr
                 << " out of range";
      std::exit(1);
    }

    if (p_filesz > 0) {
      std::memcpy(host, buf.data() + p_offset, p_filesz);
    }
    if (p_memsz > p_filesz) {
      std::memset(host + p_filesz, 0, p_memsz - p_filesz);
    }
    total_loaded += p_memsz;

    LOG(INFO) << "ELF LOAD: vaddr=0x" << std::hex << p_vaddr << " filesz=0x"
              << p_filesz << " memsz=0x" << p_memsz;
  }

  sim_->get_core(0)->get_state()->pc = e_entry;
  LOG(INFO) << "Loaded ELF: " << config_.elf_path << " (entry=0x" << std::hex
            << e_entry << ", " << std::dec << total_loaded << " bytes)";
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

void Monitor::StepOne() {
  if (state_ != CpuState::kRunning) return;

  uint64_t pc = core()->get_state()->pc;

  char *host_pc = AddrToHost(pc);
  if (!host_pc) {
    std::ostringstream oss;
    oss << "PC out of memory range: 0x" << std::hex << std::setfill('0')
        << std::setw(16) << pc;
    LOG(ERROR) << oss.str();
    state_ = CpuState::kAborted;
    exit_code_ = 2;
    return;
  }

  uint32_t insn = 0;
  std::memcpy(&insn, host_pc, sizeof(insn));

  // ebreak detection
  if (insn == kEbreakInsn) {
    uint64_t a0 = core()->get_state()->XPR[10];
    exit_code_ = (a0 == 0) ? 0 : 1;
    state_ = CpuState::kStopped;
    std::ostringstream oss;
    oss << "ebreak at PC = 0x" << std::hex << std::setfill('0') << std::setw(16)
        << pc << ", a0 = " << std::dec << a0;
    LOG(INFO) << oss.str();
    return;
  }

  // Disassemble for trace
  std::string disasm_str = disasm_->disassemble(insn_t(insn));
  itrace_.push(ITraceEntry(pc, insn, disasm_str));

  try {
    core()->step(1);
  } catch (const std::exception &e) {
    std::ostringstream err;
    err << "Exception during step at PC = 0x" << std::hex << std::setfill('0')
        << std::setw(16) << pc << ": " << e.what();
    LOG(ERROR) << err.str();
    state_ = CpuState::kAborted;
    exit_code_ = 2;
    return;
  } catch (...) {
    std::ostringstream err;
    err << "Unknown exception during step at PC = 0x" << std::hex
        << std::setfill('0') << std::setw(16) << pc;
    LOG(ERROR) << err.str();
    state_ = CpuState::kAborted;
    exit_code_ = 2;
    return;
  }

  // FTrace: detect jal/jalr for call/ret
  if (ftrace_) {
    uint32_t opcode = insn & 0x7f;
    uint32_t rd = (insn >> 7) & 0x1f;
    uint64_t dnpc = core()->get_state()->pc;

    if (opcode == 0x6f && rd == 1) {
      // jal ra, ...  => call
      ftrace_->push_call(pc, dnpc, disasm_str);
    } else if (opcode == 0x67) {
      // jalr
      uint32_t rs1 = (insn >> 15) & 0x1f;
      if (rd == 1) {
        ftrace_->push_call(pc, dnpc, disasm_str);
      } else if (rd == 0 && rs1 == 1) {
        // jalr x0, 0(ra) => ret
        ftrace_->push_ret(pc, dnpc, disasm_str);
      }
    }
  }

  // Check device interrupts -> PLIC
  if (uart_device_.wants_interrupt()) {
    plic_device_.raise_irq(kUartIrq);
  }

  if (board_) {
    board_->Update();
    if (keyboard_device_.has_pending_data()) {
      plic_device_.raise_irq(kKeyboardIrq);
    }
  }

  // PLIC -> CPU external interrupt
  set_external_interrupt(plic_device_.has_pending());
}

void Monitor::Step(uint64_t n) {
  for (uint64_t i = 0; i < n && state_ == CpuState::kRunning; ++i) {
    StepOne();
  }
}

void Monitor::DumpTraces() const {
  LOG(INFO) << "=== ITrace (last " << itrace_.size() << " instructions) ===\n"
            << itrace_.dump();
  LOG(INFO) << "=== DTrace (last " << dtrace_.size() << " accesses) ===\n"
            << dtrace_.dump();
  if (ftrace_) {
    LOG(INFO) << "=== FTrace (last " << ftrace_->ring_buf.size()
              << " calls) ===\n"
              << ftrace_->ring_buf.dump();
  }
}

void Monitor::set_external_interrupt(bool val) {
  auto mip = core()->get_state()->mip;
  if (val) {
    mip->backdoor_write_with_mask(MIP_SEIP, MIP_SEIP);
  } else {
    mip->backdoor_write_with_mask(MIP_SEIP, 0);
  }
}

void Monitor::MainLoop() {
  if (config_.batch) {
    while (state_ == CpuState::kRunning) {
      StepOne();
    }
  } else {
    sdb_->MainLoop();
  }

  if (state_ == CpuState::kStopped) {
    if (exit_code_ == 0) {
      LOG(INFO) << "Program exited normally (HIT GOOD TRAP).";
    } else {
      LOG(ERROR) << "Program exited with code " << exit_code_
                 << " (HIT BAD TRAP).";
    }
  } else if (state_ == CpuState::kAborted) {
    std::ostringstream oss;
    oss << "Program aborted at PC = 0x" << std::hex << std::setfill('0')
        << std::setw(16) << core()->get_state()->pc;
    LOG(ERROR) << oss.str();
    DumpTraces();
  }
}

}  // namespace nnemu
