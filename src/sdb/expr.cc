#include "sdb/expr.h"

#include <cctype>
#include <cstdlib>

#include "monitor/monitor.h"

namespace nnemu {
namespace {

class ExprParser {
 public:
  ExprParser(const std::string &input, const Monitor &monitor)
      : input_(input), pos_(0), monitor_(monitor) {}

  std::optional<uint64_t> Parse() {
    skip_ws();
    auto val = parse_logic_or();
    if (!val) return std::nullopt;
    skip_ws();
    if (pos_ != input_.size()) {
      error_ = "unexpected trailing characters";
      return std::nullopt;
    }
    return val;
  }

  const std::string &error() const { return error_; }

 private:
  const std::string &input_;
  size_t pos_;
  const Monitor &monitor_;
  std::string error_;

  char peek() const { return pos_ < input_.size() ? input_[pos_] : '\0'; }
  char advance() { return pos_ < input_.size() ? input_[pos_++] : '\0'; }

  void skip_ws() {
    while (pos_ < input_.size() && std::isspace(input_[pos_])) ++pos_;
  }

  bool match(const char *s) {
    skip_ws();
    size_t len = 0;
    while (s[len]) ++len;
    if (pos_ + len > input_.size()) return false;
    if (input_.compare(pos_, len, s) == 0) {
      pos_ += len;
      return true;
    }
    return false;
  }

  std::optional<uint64_t> parse_logic_or() {
    auto left = parse_logic_and();
    if (!left) return std::nullopt;
    while (true) {
      skip_ws();
      if (match("||")) {
        auto right = parse_logic_and();
        if (!right) return std::nullopt;
        left = uint64_t((*left != 0) || (*right != 0));
      } else {
        break;
      }
    }
    return left;
  }

  std::optional<uint64_t> parse_logic_and() {
    auto left = parse_equality();
    if (!left) return std::nullopt;
    while (true) {
      skip_ws();
      if (match("&&")) {
        auto right = parse_equality();
        if (!right) return std::nullopt;
        left = uint64_t((*left != 0) && (*right != 0));
      } else {
        break;
      }
    }
    return left;
  }

  std::optional<uint64_t> parse_equality() {
    auto left = parse_compare();
    if (!left) return std::nullopt;
    while (true) {
      skip_ws();
      if (match("==")) {
        auto right = parse_compare();
        if (!right) return std::nullopt;
        left = uint64_t(int64_t(*left) == int64_t(*right));
      } else if (match("!=")) {
        auto right = parse_compare();
        if (!right) return std::nullopt;
        left = uint64_t(int64_t(*left) != int64_t(*right));
      } else {
        break;
      }
    }
    return left;
  }

  std::optional<uint64_t> parse_compare() {
    auto left = parse_term();
    if (!left) return std::nullopt;
    while (true) {
      skip_ws();
      if (match("<=")) {
        auto right = parse_term();
        if (!right) return std::nullopt;
        left = uint64_t(int64_t(*left) <= int64_t(*right));
      } else if (match(">=")) {
        auto right = parse_term();
        if (!right) return std::nullopt;
        left = uint64_t(int64_t(*left) >= int64_t(*right));
      } else if (match("<")) {
        auto right = parse_term();
        if (!right) return std::nullopt;
        left = uint64_t(int64_t(*left) < int64_t(*right));
      } else if (match(">")) {
        auto right = parse_term();
        if (!right) return std::nullopt;
        left = uint64_t(int64_t(*left) > int64_t(*right));
      } else {
        break;
      }
    }
    return left;
  }

  std::optional<uint64_t> parse_term() {
    auto left = parse_factor();
    if (!left) return std::nullopt;
    while (true) {
      skip_ws();
      if (match("+")) {
        auto right = parse_factor();
        if (!right) return std::nullopt;
        left = uint64_t(int64_t(*left) + int64_t(*right));
      } else if (peek() == '-') {
        size_t saved = pos_;
        advance();
        auto right = parse_factor();
        if (!right) {
          pos_ = saved;
          break;
        }
        left = uint64_t(int64_t(*left) - int64_t(*right));
      } else {
        break;
      }
    }
    return left;
  }

  std::optional<uint64_t> parse_factor() {
    auto left = parse_unary();
    if (!left) return std::nullopt;
    while (true) {
      skip_ws();
      if (peek() == '*') {
        size_t saved = pos_;
        advance();
        auto right = parse_unary();
        if (!right) {
          pos_ = saved;
          break;
        }
        left = uint64_t(int64_t(*left) * int64_t(*right));
      } else if (match("/")) {
        auto right = parse_unary();
        if (!right) return std::nullopt;
        if (*right == 0) {
          error_ = "division by zero";
          return std::nullopt;
        }
        left = uint64_t(int64_t(*left) / int64_t(*right));
      } else {
        break;
      }
    }
    return left;
  }

  std::optional<uint64_t> parse_unary() {
    skip_ws();
    if (peek() == '-') {
      advance();
      auto val = parse_unary();
      if (!val) return std::nullopt;
      return uint64_t(-int64_t(*val));
    }
    if (peek() == '*') {
      advance();
      auto val = parse_unary();
      if (!val) return std::nullopt;
      auto result = monitor_.mem_load(*val, 4);
      if (!result) {
        error_ = "cannot read memory";
        return std::nullopt;
      }
      return *result;
    }
    return parse_primary();
  }

  std::optional<uint64_t> parse_primary() {
    skip_ws();

    if (peek() == '(') {
      advance();
      auto val = parse_logic_or();
      if (!val) return std::nullopt;
      skip_ws();
      if (peek() != ')') {
        error_ = "expected ')'";
        return std::nullopt;
      }
      advance();
      return val;
    }

    if (peek() == '$') {
      advance();
      size_t start = pos_;
      while (pos_ < input_.size() &&
             (std::isalnum(input_[pos_]) || input_[pos_] == '_')) {
        ++pos_;
      }
      if (pos_ == start) {
        error_ = "expected register name after $";
        return std::nullopt;
      }
      std::string reg_name = input_.substr(start, pos_ - start);
      auto val = monitor_.reg_value(reg_name);
      if (!val) {
        error_ = "unknown register: " + reg_name;
        return std::nullopt;
      }
      return val;
    }

    if (pos_ + 1 < input_.size() && input_[pos_] == '0' &&
        (input_[pos_ + 1] == 'x' || input_[pos_ + 1] == 'X')) {
      pos_ += 2;
      size_t start = pos_;
      while (pos_ < input_.size() && std::isxdigit(input_[pos_])) ++pos_;
      if (pos_ == start) {
        error_ = "expected hex digits after 0x";
        return std::nullopt;
      }
      std::string hex_str = input_.substr(start, pos_ - start);
      return std::strtoull(hex_str.c_str(), nullptr, 16);
    }

    if (std::isdigit(peek())) {
      size_t start = pos_;
      while (pos_ < input_.size() && std::isdigit(input_[pos_])) ++pos_;
      std::string dec_str = input_.substr(start, pos_ - start);
      return std::strtoull(dec_str.c_str(), nullptr, 10);
    }

    error_ = "unexpected character";
    return std::nullopt;
  }
};

}  // namespace

std::optional<uint64_t> ExprEval(const std::string &expr,
                                 const Monitor &monitor,
                                 std::string *err_msg) {
  ExprParser parser(expr, monitor);
  auto result = parser.Parse();
  if (!result && err_msg) {
    *err_msg = parser.error();
  }
  return result;
}

}  // namespace nnemu
