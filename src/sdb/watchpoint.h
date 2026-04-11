#ifndef NEMU_SDB_WATCHPOINT_H_
#define NEMU_SDB_WATCHPOINT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nemu {

class Monitor;

struct Watchpoint {
  int id;
  std::string expr;
  uint64_t last_value;
};

class WatchpointPool {
 public:
  WatchpointPool() = default;

  std::optional<int> add(const std::string &expr, const Monitor &monitor);
  bool remove(int id);

  bool check(const Monitor &monitor, std::string &out);
  void list(std::string &out) const;

 private:
  std::vector<Watchpoint> watchpoints_;
  int next_id_ = 1;
};

}  // namespace nemu

#endif  // NEMU_SDB_WATCHPOINT_H_
