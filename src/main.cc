#include "monitor/monitor.h"

#include <cstdlib>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"

ABSL_FLAG(bool, batch, false, "run with batch mode");
ABSL_FLAG(std::string, log, "", "write log to specified file");
ABSL_FLAG(std::string, image, "", "RISC-V raw binary image file path (.bin)");
ABSL_FLAG(std::string, elf, "", "RISC-V ELF file path (loads to vaddr)");

int main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage(
      "nemu -- RISC-V simulator (spike + nvboard)\n"
      "  --image <path>   RISC-V raw binary image (.bin) [loaded to flash]\n"
      "  --elf <path>     RISC-V ELF file [loaded to vaddr, e.g. xv6]\n"
      "  --batch          run with batch mode (no SDB)\n"
      "  --log <path>     write log to specified file");

  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  std::string image_path = absl::GetFlag(FLAGS_image);
  std::string elf_path = absl::GetFlag(FLAGS_elf);

  if (image_path.empty() && elf_path.empty()) {
    LOG(ERROR) << "No image file specified. Use --image or --elf.";
    return 1;
  }

  nemu::Monitor::Config config{
      .batch = absl::GetFlag(FLAGS_batch),
      .log_file = absl::GetFlag(FLAGS_log),
      .image_path = std::move(image_path),
      .elf_path = std::move(elf_path),
  };

  nemu::Monitor monitor(config);
  return monitor.Run();
}
