#include "sdb/expr.h"

#include <string>

extern uint64_t expr_eval(const char* expr_str,
                          const nemu::Monitor* monitor, bool* success);
extern const char* expr_parse_error_msg;

namespace nemu {

std::optional<uint64_t> ExprEval(const std::string &expr,
                                 const Monitor &monitor,
                                 std::string *err_msg) {
  bool success = false;
  uint64_t result = expr_eval(expr.c_str(), &monitor, &success);
  if (!success) {
    if (err_msg) {
      *err_msg = expr_parse_error_msg ? expr_parse_error_msg : "parse error";
    }
    return std::nullopt;
  }
  return result;
}

}  // namespace nemu
