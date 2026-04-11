#ifndef NNEMU_SDB_SDB_H_
#define NNEMU_SDB_SDB_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sdb/watchpoint.h"

namespace nnemu {

class Monitor;

class Sdb {
 public:
  explicit Sdb(Monitor &monitor);
  ~Sdb();

  Sdb(const Sdb &) = delete;
  Sdb &operator=(const Sdb &) = delete;

  void MainLoop();

 private:
  enum class Action { kContinue, kQuit };

  struct CmdResult {
    bool ok;
    Action action;
    std::string error_msg;

    static CmdResult Continue() { return {true, Action::kContinue, ""}; }
    static CmdResult Quit() { return {true, Action::kQuit, ""}; }
    static CmdResult InputError(std::string msg) {
      return {false, Action::kContinue, std::move(msg)};
    }
    static CmdResult Fatal(std::string msg) {
      return {false, Action::kQuit, std::move(msg)};
    }
    bool is_fatal() const { return !ok && action == Action::kQuit; }
  };

  using CmdHandler = CmdResult (Sdb::*)(const std::string &);

  struct CommandDef {
    std::vector<std::string> names;
    std::string help;
    CmdHandler handler;
  };

  static const std::vector<CommandDef> &GetCommands();

  CmdResult execute_line(const std::string &input);
  CmdResult execute_steps(size_t n);
  bool check_breakpoints() const;

  CmdResult cmd_help(const std::string &args);
  CmdResult cmd_quit(const std::string &args);
  CmdResult cmd_continue(const std::string &args);
  CmdResult cmd_step(const std::string &args);
  CmdResult cmd_info(const std::string &args);
  CmdResult cmd_examine(const std::string &args);
  CmdResult cmd_print(const std::string &args);
  CmdResult cmd_watch(const std::string &args);
  CmdResult cmd_delete(const std::string &args);
  CmdResult cmd_break(const std::string &args);

  Monitor &monitor_;
  std::vector<uint64_t> breakpoints_;
  WatchpointPool watchpoints_;
  std::optional<std::string> last_cmd_;
};

}  // namespace nnemu

#endif  // NNEMU_SDB_SDB_H_
