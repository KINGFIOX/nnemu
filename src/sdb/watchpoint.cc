#include "sdb/watchpoint.h"

#include <sstream>

#include "sdb/expr.h"

namespace nemu {

std::optional<int> WatchpointPool::add(const std::string &expr,
                                       const Monitor &monitor) {
  auto val = ExprEval(expr, monitor);
  if (!val) return std::nullopt;

  int id = next_id_++;
  watchpoints_.push_back({id, expr, *val});
  return id;
}

bool WatchpointPool::remove(int id) {
  for (auto it = watchpoints_.begin(); it != watchpoints_.end(); ++it) {
    if (it->id == id) {
      watchpoints_.erase(it);
      return true;
    }
  }
  return false;
}

bool WatchpointPool::check(const Monitor &monitor, std::string &out) {
  bool triggered = false;
  for (auto &wp : watchpoints_) {
    auto val = ExprEval(wp.expr, monitor);
    if (!val) continue;
    if (*val != wp.last_value) {
      std::ostringstream oss;
      oss << "watchpoint #" << wp.id << " (" << wp.expr << "): 0x" << std::hex
          << wp.last_value << " -> 0x" << *val << "\n";
      out += oss.str();
      wp.last_value = *val;
      triggered = true;
    }
  }
  return triggered;
}

void WatchpointPool::list(std::string &out) const {
  if (watchpoints_.empty()) {
    out += "no watchpoints\n";
    return;
  }
  for (const auto &wp : watchpoints_) {
    std::ostringstream oss;
    oss << "  #" << wp.id << ": " << wp.expr << " = 0x" << std::hex
        << wp.last_value << "\n";
    out += oss.str();
  }
}

}  // namespace nemu
