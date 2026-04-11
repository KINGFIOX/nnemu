#ifndef NNEMU_SDB_COMMAND_H_
#define NNEMU_SDB_COMMAND_H_

#include <optional>
#include <string>

#include "absl/strings/string_view.h"

namespace nnemu {

struct Command {
  std::string name;
  std::string args;
};

std::optional<Command> ParseCommand(absl::string_view input);

}  // namespace nnemu

#endif  // NNEMU_SDB_COMMAND_H_
