cmake_minimum_required(VERSION 3.20)
project(nemu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- abseil-cpp (provided by Nix devShell) ---
find_package(absl REQUIRED CONFIG)
# Make absl headers explicit in compile_commands.json for clangd.
# In Nix envs, wrapper-provided include paths may not be visible to clangd.
get_filename_component(ABSL_CMAKE_DIR "${absl_DIR}" DIRECTORY)      # .../lib/cmake
get_filename_component(ABSL_LIB_DIR "${ABSL_CMAKE_DIR}" DIRECTORY)  # .../lib
get_filename_component(ABSL_ROOT_DIR "${ABSL_LIB_DIR}" DIRECTORY)   # prefix
set(ABSL_INCLUDE_DIR "${ABSL_ROOT_DIR}/include")

# --- Spike (riscv-isa-sim) ---
if(NOT DEFINED ENV{SPIKE_HOME})
  message(FATAL_ERROR "SPIKE_HOME environment variable is not set")
endif()
set(SPIKE_HOME $ENV{SPIKE_HOME})

# --- NVBoard (CMake config package) ---
if(NOT DEFINED ENV{NVBOARD_HOME})
  message(FATAL_ERROR "NVBOARD_HOME environment variable is not set")
endif()
set(NVBOARD_HOME $ENV{NVBOARD_HOME})
find_package(nvboard REQUIRED CONFIG
  PATHS "${NVBOARD_HOME}/lib/cmake/nvboard"
  NO_DEFAULT_PATH
)

# --- Sources ---
file(GLOB_RECURSE SOURCES src/*.cc)
add_executable(nemu ${SOURCES})

target_include_directories(nemu PRIVATE
  src
  ${SPIKE_HOME}/include
  ${SPIKE_HOME}/include/riscv
  ${SPIKE_HOME}/include/softfloat
)
target_compile_options(nemu PRIVATE "-isystem${ABSL_INCLUDE_DIR}")

target_link_directories(nemu PRIVATE ${SPIKE_HOME}/lib)

target_link_libraries(nemu PRIVATE
  absl::flags
  absl::flags_parse
  absl::log
  absl::log_initialize
  absl::strings
  riscv
  fesvr
  disasm
  fdt
  softfloat
  nvboard::nvboard
)

if(APPLE)
  target_link_libraries(nemu PRIVATE c++)
else()
  target_link_libraries(nemu PRIVATE stdc++)
endif()
target_link_libraries(nemu PRIVATE pthread)
