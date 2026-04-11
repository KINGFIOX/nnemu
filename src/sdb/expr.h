#ifndef NEMU_SDB_EXPR_H_
#define NEMU_SDB_EXPR_H_

#include <cstdint>
#include <optional>
#include <string>

namespace nemu {

class Monitor;

std::optional<uint64_t> ExprEval(const std::string &expr,
                                 const Monitor &monitor,
                                 std::string *err_msg = nullptr);

}  // namespace nemu

#endif  // NEMU_SDB_EXPR_H_
