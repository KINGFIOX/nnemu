#include "config.h"
#include "cfg.h"
#include "sim.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include <nvboard/nvboard.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <memory>
#include <fstream>

ABSL_FLAG(std::string, image, "", "RISC-V raw binary image (.bin) [loaded to flash]");
ABSL_FLAG(bool, batch, false, "Run with batch mode (no SDB)");
ABSL_FLAG(std::string, log, "", "Write log to specified file");
ABSL_FLAG(bool, nvboard, "", "Enableh NJU Virtual Board (stub, always enable)");

int main(int argc, char** argv)
{
  absl::SetProgramUsageMessage(
      "nemu -- RISC-V simulator (spike + nvboard)\n"
      "  --image=<path>  RISC-V raw binary image (.bin) [loaded to flash]\n"
      "  --batch         run with batch mode (no SDB)\n"
      "  --nvboard       Enableh NJU Virtual Board (stub, always enable)\n"
      "  --log=<path>    write log to specified file");
  auto positional_args = absl::ParseCommandLine(argc, argv);
  if (positional_args.size() > 1) {
    fprintf(stderr, "Error: unexpected positional arguments\n");
    return 1;
  }

  const auto image_path = absl::GetFlag(FLAGS_image);
  const auto log_path_flag = absl::GetFlag(FLAGS_log);
  const char *log_path = log_path_flag.empty() ? nullptr : log_path_flag.c_str();
  const bool batch = absl::GetFlag(FLAGS_batch);

  if (image_path.empty()) {
    fprintf(stderr, "Error: --image is required and cannot be empty\n");
    return 1;
  }

  std::vector<mem_cfg_t> mem_layout = {
    mem_cfg_t(FLASH_BASE, FLASH_SIZE),
    mem_cfg_t(DRAM_BASE, DRAM_SIZE),
  };

  std::vector<std::pair<reg_t, mem_t*>> mems;
  for (const auto &cfg_mem : mem_layout)
    mems.emplace_back(cfg_mem.get_base(), new mem_t(cfg_mem.get_size()));

  {
    std::ifstream in(image_path, std::ios::binary | std::ios::ate);
    if (!in.good()) {
      fprintf(stderr, "Cannot open image file '%s'\n", image_path.c_str());
      exit(1);
    }
    auto size = in.tellg();
    in.seekg(0, std::ios::beg);
    if (static_cast<size_t>(size) > FLASH_SIZE) {
      fprintf(stderr, "Image too large (%ld bytes, max 256 MiB)\n", (long)size);
      exit(1);
    }
    std::vector<char> buf(size);
    in.read(buf.data(), size);
    mems[0].second->store(0, size, reinterpret_cast<const uint8_t*>(buf.data()));
    fprintf(stderr, "Loaded image '%s' (%ld bytes) to 0x%x\n",
            image_path.c_str(), (long)size, (unsigned)FLASH_BASE);
    // #region agent log
    {FILE*f=fopen("/Users/wangfiox/Documents/ysyx/ysyx-workbench/.cursor/debug-cf6300.log","a");if(f){fprintf(f,"{\"sessionId\":\"cf6300\",\"location\":\"spike.cc:66\",\"message\":\"image_load\",\"data\":{\"mems0_base\":%lu,\"DEFAULT_RSTVEC\":%lu,\"FLASH_BASE\":%lu,\"image_size\":%ld},\"runId\":\"post-fix\",\"hypothesisId\":\"A\"}\n",(unsigned long)mem_layout[0].get_base(),(unsigned long)DEFAULT_RSTVEC,(unsigned long)FLASH_BASE,(long)size);fclose(f);}}
    // #endregion
  }

  cfg_t cfg(
      /*default_initrd_bounds=*/std::make_pair(reg_t(0), reg_t(0)),
      /*default_bootargs=*/nullptr,
      /*default_isa=*/DEFAULT_ISA,
      /*default_priv=*/DEFAULT_PRIV,
      /*default_varch=*/DEFAULT_VARCH,
      /*default_misaligned=*/false,
      /*default_endianness=*/endianness_little,
      /*default_pmpregions=*/16,
      /*default_mem_layout=*/mem_layout,
      /*default_hartids=*/std::vector<size_t>{0},
      /*default_real_time_clint=*/false,
      /*default_trigger_count=*/4);
  cfg.start_pc = FLASH_BASE;

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

  auto board = nvboard::Board::Create();

  std::vector<std::string> htif_args = {"none"};
  std::vector<std::pair<reg_t, abstract_device_t*>> plugin_devices;

  sim_t s(&cfg, false,
      mems, plugin_devices, htif_args, dm_config, log_path,
      true, nullptr, false, nullptr,
      std::move(board));

  s.set_debug(!batch);
  s.configure_log(log_path != nullptr, false);

  auto return_code = s.run();

  for (auto &m : mems)
    delete m.second;

  return return_code;
}
