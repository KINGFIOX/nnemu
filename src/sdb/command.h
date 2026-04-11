#ifndef NEMU_SDB_COMMAND_H_
#define NEMU_SDB_COMMAND_H_

#include <optional>
#include <string>

#include "absl/strings/string_view.h"

namespace nemu {

struct Command {
  std::string name;
  std::string args;
};

std::optional<Command> ParseCommand(absl::string_view input);

}  // namespace nemu

#endif  // NEMU_SDB_COMMAND_H_
