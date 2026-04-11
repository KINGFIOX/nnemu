cmake_minimum_required(VERSION 3.20)
project(nemu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- abseil-cpp (provided by Nix devShell) ---
find_package(absl REQUIRED CONFIG)

# --- Spike (riscv-isa-sim, CMake config package) ---
find_package(spike REQUIRED CONFIG)

# --- NVBoard (CMake config package) ---
find_package(nvboard REQUIRED CONFIG)

# --- ELFIO (header-only) ---
find_path(ELFIO_INCLUDE_DIR elfio/elfio.hpp REQUIRED)

# --- readline ---
find_library(READLINE_LIB readline REQUIRED)
find_path(READLINE_INCLUDE_DIR readline/readline.h REQUIRED)

# --- Flex / Bison ---
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

set(EXPR_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/sdb")
file(MAKE_DIRECTORY ${EXPR_GEN_DIR})

bison_target(ExprParser "${CMAKE_CURRENT_SOURCE_DIR}/src/sdb/expr.y"
  "${EXPR_GEN_DIR}/expr.tab.cc"
  DEFINES_FILE "${EXPR_GEN_DIR}/expr.tab.h"
)
flex_target(ExprLexer "${CMAKE_CURRENT_SOURCE_DIR}/src/sdb/expr.l"
  "${EXPR_GEN_DIR}/expr.lex.cc"
)
add_flex_bison_dependency(ExprLexer ExprParser)

# --- Sources ---
file(GLOB_RECURSE SOURCES src/*.cc)
add_executable(nemu
  ${SOURCES}
  ${BISON_ExprParser_OUTPUTS}
  ${FLEX_ExprLexer_OUTPUTS}
)

target_include_directories(nemu PRIVATE
  src
  ${EXPR_GEN_DIR}
  ${ELFIO_INCLUDE_DIR}
  ${READLINE_INCLUDE_DIR}
)

target_link_libraries(nemu PRIVATE
  absl::flags
  absl::flags_parse
  absl::log
  absl::log_initialize
  absl::strings
  spike::riscv
  nvboard::nvboard
  ${READLINE_LIB}
)

if(APPLE)
  target_link_libraries(nemu PRIVATE c++)
else()
  target_link_libraries(nemu PRIVATE stdc++)
endif()
target_link_libraries(nemu PRIVATE pthread)
