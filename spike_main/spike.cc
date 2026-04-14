#include "config.h"
#include "cfg.h"
#include "sim.h"
#include <nvboard/nvboard.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <memory>
#include <fstream>

static void help(int exit_code = 1)
{
  fprintf(stderr,
      "nemu -- RISC-V simulator (spike + nvboard)\n"
      "  --image <path>   RISC-V raw binary image (.bin) [loaded to flash]\n"
      "  --batch          run with batch mode (no SDB)\n"
      "  --log <path>     write log to specified file\n");
  exit(exit_code);
}

int main(int argc, char** argv)
{
  const char *image_path = nullptr;
  const char *log_path = nullptr;
  bool batch = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--image") == 0 && i + 1 < argc) {
      image_path = argv[++i];
    } else if (strcmp(argv[i], "--batch") == 0) {
      batch = true;
    } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
      log_path = argv[++i];
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      help(0);
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      help(1);
    }
  }

  if (!image_path) {
    fprintf(stderr, "Error: --image is required\n");
    help(1);
  }

  std::vector<mem_cfg_t> mem_layout = {
    mem_cfg_t(DEFAULT_RSTVEC, 0x10000000),
    mem_cfg_t(DRAM_BASE, DRAM_SIZE),
  };

  std::vector<std::pair<reg_t, mem_t*>> mems;
  for (const auto &cfg_mem : mem_layout)
    mems.emplace_back(cfg_mem.get_base(), new mem_t(cfg_mem.get_size()));

  {
    std::ifstream in(image_path, std::ios::binary | std::ios::ate);
    if (!in.good()) {
      fprintf(stderr, "Cannot open image file '%s'\n", image_path);
      exit(1);
    }
    auto size = in.tellg();
    in.seekg(0, std::ios::beg);
    if (static_cast<size_t>(size) > 0x10000000) {
      fprintf(stderr, "Image too large (%ld bytes, max 256 MiB)\n", (long)size);
      exit(1);
    }
    std::vector<char> buf(size);
    in.read(buf.data(), size);
    mems[0].second->store(0, size, reinterpret_cast<const uint8_t*>(buf.data()));
    fprintf(stderr, "Loaded image '%s' (%ld bytes) to 0x%x\n",
            image_path, (long)size, DEFAULT_RSTVEC);
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
  cfg.start_pc = DEFAULT_RSTVEC;

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
