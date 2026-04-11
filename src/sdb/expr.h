#ifndef NNEMU_SDB_EXPR_H_
#define NNEMU_SDB_EXPR_H_

#include <cstdint>
#include <optional>
#include <string>

namespace nnemu {

class Monitor;

std::optional<uint64_t> ExprEval(const std::string &expr,
                                 const Monitor &monitor,
                                 std::string *err_msg = nullptr);

}  // namespace nnemu

#endif  // NNEMU_SDB_EXPR_H_
