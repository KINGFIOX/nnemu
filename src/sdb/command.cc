#include "sdb/command.h"

#include "absl/strings/ascii.h"

namespace nnemu {

std::optional<Command> ParseCommand(absl::string_view input) {
  absl::string_view trimmed = absl::StripAsciiWhitespace(input);
  if (trimmed.empty()) return std::nullopt;

  size_t space = trimmed.find(' ');
  if (space == absl::string_view::npos) {
    return Command{std::string(trimmed), ""};
  }

  std::string name(trimmed.substr(0, space));
  std::string args(
      absl::StripLeadingAsciiWhitespace(trimmed.substr(space + 1)));
  return Command{std::move(name), std::move(args)};
}

}  // namespace nnemu
