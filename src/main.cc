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
ABSL_FLAG(bool, nvboard, false, "use NVBoard simulation board");
ABSL_FLAG(std::string, log, "", "write log to specified file");
ABSL_FLAG(std::string, image, "", "RISC-V image file path (.bin)");

int main(int argc, char* argv[]) {
  absl::SetProgramUsageMessage(
      "nnemu -- RISC-V simulator (spike + nvboard)\n"
      "  --image <path>   RISC-V image file path (.bin) [required]\n"
      "  --batch          run with batch mode\n"
      "  --nvboard        use NVBoard simulation board\n"
      "  --log <path>     write log to specified file");

  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  std::string image_path = absl::GetFlag(FLAGS_image);
  if (image_path.empty()) {
    LOG(ERROR) << "No image file specified. Use --image <path>.";
    return 1;
  }

  nnemu::Monitor::Config config{
      .batch = absl::GetFlag(FLAGS_batch),
      .nvboard = absl::GetFlag(FLAGS_nvboard),
      .log_file = absl::GetFlag(FLAGS_log),
      .image_path = std::move(image_path),
  };

  nnemu::Monitor monitor(config);
  return monitor.Run();
}
