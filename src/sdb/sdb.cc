#include "sdb/sdb.h"

#include <readline/history.h>
#include <readline/readline.h>

#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>

#include "absl/log/log.h"
#include "common.h"
#include "monitor/monitor.h"
#include "sdb/command.h"
#include "sdb/expr.h"

namespace nemu {

const std::vector<Sdb::CommandDef> &Sdb::GetCommands() {
  static const auto *commands = new std::vector<CommandDef>{
      {{"help", "h"}, "show this help message", &Sdb::cmd_help},
      {{"quit", "q"}, "quit the debugger", &Sdb::cmd_quit},
      {{"continue", "c"}, "continue execution", &Sdb::cmd_continue},
      {{"step", "si", "s"}, "step N instructions (default 1)", &Sdb::cmd_step},
      {{"info"}, "info r(egisters) / info w(atchpoints)", &Sdb::cmd_info},
      {{"examine", "x"}, "x N EXPR - examine N words at EXPR",
       &Sdb::cmd_examine},
      {{"print", "p", "eval"}, "evaluate expression", &Sdb::cmd_print},
      {{"watch", "w"}, "add watchpoint on expression", &Sdb::cmd_watch},
      {{"delete", "d"}, "delete watchpoint by id", &Sdb::cmd_delete},
      {{"break", "b"}, "set breakpoint at address", &Sdb::cmd_break},
  };
  return *commands;
}

Sdb::Sdb(Monitor &monitor) : monitor_(monitor) {}

Sdb::~Sdb() = default;

void Sdb::MainLoop() {
  while (true) {
    char *line_raw = readline("(nemu) ");
    if (line_raw == nullptr) {
      return;
    }
    std::string line(line_raw);
    free(line_raw);

    while (!line.empty() &&
           (line.back() == ' ' || line.back() == '\t' || line.back() == '\n')) {
      line.pop_back();
    }

    std::string input;
    if (line.empty()) {
      if (last_cmd_.has_value()) {
        input = *last_cmd_;
      } else {
        continue;
      }
    } else {
      add_history(line.c_str());
      last_cmd_ = line;
      input = line;
    }

    auto r = execute_line(input);
    if (r.is_fatal()) {
      LOG(ERROR) << r.error_msg;
      monitor_.DumpTraces();
      return;
    }
    if (!r.ok && !r.is_fatal()) {
      LOG(WARNING) << r.error_msg;
    }
    if (r.action == Action::kQuit) {
      return;
    }
  }
}

Sdb::CmdResult Sdb::execute_line(const std::string &input) {
  auto cmd = ParseCommand(input);
  if (!cmd.has_value()) return CmdResult::Continue();

  for (const auto &def : GetCommands()) {
    for (const auto &name : def.names) {
      if (cmd->name == name) {
        return (this->*def.handler)(cmd->args);
      }
    }
  }

  return CmdResult::InputError("unknown command: " + cmd->name);
}

Sdb::CmdResult Sdb::execute_steps(size_t n) {
  for (size_t i = 0; i < n; ++i) {
    monitor_.StepOne();

    if (monitor_.state() == CpuState::kStopped) {
      if (monitor_.exit_code() == 0) {
        LOG(INFO) << "program exited successfully";
        return CmdResult::Quit();
      }
      return CmdResult::Fatal("program exited with failure (exit_code = " +
                              std::to_string(monitor_.exit_code()) + ")");
    }

    if (monitor_.state() == CpuState::kAborted) {
      return CmdResult::Fatal("program aborted");
    }

    if (check_breakpoints()) {
      return CmdResult::Continue();
    }
    std::string wp_buf;
    if (watchpoints_.check(monitor_, wp_buf)) {
      LOG(INFO) << wp_buf;
      return CmdResult::Continue();
    }
  }
  return CmdResult::Continue();
}

bool Sdb::check_breakpoints() const {
  uint64_t cur_pc = monitor_.pc();
  for (uint64_t bp : breakpoints_) {
    if (cur_pc == bp) {
      std::ostringstream oss;
      oss << "breakpoint hit at 0x" << std::hex << std::setfill('0')
          << std::setw(16) << cur_pc;
      LOG(INFO) << oss.str();
      return true;
    }
  }
  return false;
}

// ========================== Command Handlers ==========================

Sdb::CmdResult Sdb::cmd_help(const std::string &) {
  std::string buf = "Commands:\n";
  for (const auto &def : GetCommands()) {
    std::string names;
    for (size_t i = 0; i < def.names.size(); ++i) {
      if (i > 0) names += ", ";
      names += def.names[i];
    }
    buf += "  ";
    buf += names;
    buf += std::string(std::max(1, 20 - static_cast<int>(names.size())), ' ');
    buf += def.help;
    buf += "\n";
  }
  LOG(INFO) << buf;
  return CmdResult::Continue();
}

Sdb::CmdResult Sdb::cmd_quit(const std::string &) {
  return CmdResult::Quit();
}

Sdb::CmdResult Sdb::cmd_continue(const std::string &) {
  return execute_steps(SIZE_MAX);
}

Sdb::CmdResult Sdb::cmd_step(const std::string &args) {
  size_t n = 1;
  if (!args.empty()) {
    try {
      n = std::stoull(args);
    } catch (...) {
      return CmdResult::InputError("usage: step [N]");
    }
  }
  return execute_steps(n);
}

Sdb::CmdResult Sdb::cmd_info(const std::string &args) {
  std::string sub = args;
  while (!sub.empty() && sub.front() == ' ') sub.erase(sub.begin());
  while (!sub.empty() && sub.back() == ' ') sub.pop_back();

  if (sub == "r" || sub == "registers" || sub == "reg") {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << "pc  = 0x" << std::setw(16) << monitor_.pc() << "\n";
    for (int i = 0; i < 32; ++i) {
      std::string name(kGprNames[i]);
      oss << name << std::string(std::max(1, 4 - static_cast<int>(name.size())),
                                 ' ')
          << " = 0x" << std::setw(16) << monitor_.gpr(i).value_or(0) << "  ";
      if ((i + 1) % 4 == 0) oss << "\n";
    }
    LOG(INFO) << oss.str();
  } else if (sub == "w" || sub == "watchpoints" || sub == "wp") {
    std::string buf;
    watchpoints_.list(buf);
    LOG(INFO) << buf;
  } else if (sub == "b" || sub == "breakpoints" || sub == "bp") {
    if (breakpoints_.empty()) {
      LOG(INFO) << "no breakpoints";
    } else {
      std::ostringstream oss;
      oss << std::hex << std::setfill('0');
      for (size_t i = 0; i < breakpoints_.size(); ++i) {
        oss << "  #" << std::dec << (i + 1) << ": 0x" << std::hex
            << std::setw(16) << breakpoints_[i] << "\n";
      }
      LOG(INFO) << oss.str();
    }
  } else {
    return CmdResult::InputError("usage: info r|w|b");
  }
  return CmdResult::Continue();
}

Sdb::CmdResult Sdb::cmd_examine(const std::string &args) {
  size_t space = args.find(' ');
  if (space == std::string::npos) {
    return CmdResult::InputError("usage: x N EXPR");
  }
  std::string count_str = args.substr(0, space);
  std::string expr_str = args.substr(space + 1);
  while (!expr_str.empty() && expr_str.front() == ' ')
    expr_str.erase(expr_str.begin());

  size_t n = 0;
  try {
    n = std::stoull(count_str);
  } catch (...) {
    return CmdResult::InputError("bad count: " + count_str);
  }

  std::string err;
  auto addr = ExprEval(expr_str, monitor_, &err);
  if (!addr) {
    return CmdResult::InputError("expression error: " + err);
  }

  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < n; ++i) {
    uint64_t a = *addr + i * 4;
    if (i % 4 == 0) {
      oss << "0x" << std::setw(16) << a << ":";
    }
    auto val = monitor_.mem_load(a, 4);
    if (val) {
      oss << "  0x" << std::setw(8) << static_cast<uint32_t>(*val);
    } else {
      oss << "  ??????????";
    }
    if ((i + 1) % 4 == 0 || i + 1 == n) oss << "\n";
  }
  LOG(INFO) << oss.str();
  return CmdResult::Continue();
}

Sdb::CmdResult Sdb::cmd_print(const std::string &args) {
  std::string expr = args;
  while (!expr.empty() && expr.front() == ' ') expr.erase(expr.begin());
  while (!expr.empty() && expr.back() == ' ') expr.pop_back();

  if (expr.empty()) {
    return CmdResult::InputError("usage: p EXPR");
  }

  std::string err;
  auto val = ExprEval(expr, monitor_, &err);
  if (!val) {
    return CmdResult::InputError("expression error: " + err);
  }

  std::ostringstream oss;
  oss << "0x" << std::hex << std::setfill('0') << std::setw(16) << *val
      << " (" << std::dec << *val << ")";
  LOG(INFO) << oss.str();
  return CmdResult::Continue();
}

Sdb::CmdResult Sdb::cmd_watch(const std::string &args) {
  std::string expr = args;
  while (!expr.empty() && expr.front() == ' ') expr.erase(expr.begin());
  while (!expr.empty() && expr.back() == ' ') expr.pop_back();

  if (expr.empty()) {
    return CmdResult::InputError("usage: w EXPR");
  }

  auto id = watchpoints_.add(expr, monitor_);
  if (!id) {
    return CmdResult::InputError("expression error");
  }
  LOG(INFO) << "watchpoint #" << *id << ": " << expr;
  return CmdResult::Continue();
}

Sdb::CmdResult Sdb::cmd_delete(const std::string &args) {
  std::string trimmed = args;
  while (!trimmed.empty() && trimmed.front() == ' ')
    trimmed.erase(trimmed.begin());

  int id = 0;
  try {
    id = std::stoi(trimmed);
  } catch (...) {
    return CmdResult::InputError("usage: d N");
  }

  if (watchpoints_.remove(id)) {
    LOG(INFO) << "deleted watchpoint #" << id;
  } else {
    return CmdResult::InputError("watchpoint #" + std::to_string(id) +
                                 " not found");
  }
  return CmdResult::Continue();
}

Sdb::CmdResult Sdb::cmd_break(const std::string &args) {
  std::string expr = args;
  while (!expr.empty() && expr.front() == ' ') expr.erase(expr.begin());
  while (!expr.empty() && expr.back() == ' ') expr.pop_back();

  if (expr.empty()) {
    return CmdResult::InputError("usage: b ADDR");
  }

  // Sub-commands
  size_t sp = expr.find(' ');
  std::string first = (sp != std::string::npos) ? expr.substr(0, sp) : expr;

  if (first == "ls" || first == "list") {
    if (breakpoints_.empty()) {
      LOG(INFO) << "no breakpoints";
    } else {
      std::ostringstream oss;
      oss << std::hex << std::setfill('0');
      for (size_t i = 0; i < breakpoints_.size(); ++i) {
        oss << "  #" << std::dec << (i + 1) << ": 0x" << std::hex
            << std::setw(16) << breakpoints_[i] << "\n";
      }
      LOG(INFO) << oss.str();
    }
    return CmdResult::Continue();
  }

  if (first == "rm" || first == "remove") {
    std::string rest =
        (sp != std::string::npos) ? expr.substr(sp + 1) : "";
    while (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());

    size_t idx = 0;
    try {
      idx = std::stoull(rest);
    } catch (...) {
      return CmdResult::InputError("usage: b rm N");
    }
    if (idx < 1 || idx > breakpoints_.size()) {
      return CmdResult::InputError("usage: b rm N");
    }
    uint64_t addr = breakpoints_[idx - 1];
    breakpoints_.erase(breakpoints_.begin() + static_cast<ptrdiff_t>(idx - 1));
    std::ostringstream oss;
    oss << "deleted breakpoint #" << idx << " at 0x" << std::hex
        << std::setfill('0') << std::setw(16) << addr;
    LOG(INFO) << oss.str();
    return CmdResult::Continue();
  }

  std::string err;
  auto addr = ExprEval(expr, monitor_, &err);
  if (!addr) {
    return CmdResult::InputError("expression error: " + err);
  }
  breakpoints_.push_back(*addr);
  std::ostringstream oss;
  oss << "breakpoint #" << breakpoints_.size() << " at 0x" << std::hex
      << std::setfill('0') << std::setw(16) << *addr;
  LOG(INFO) << oss.str();
  return CmdResult::Continue();
}

}  // namespace nemu
