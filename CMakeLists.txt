cmake_minimum_required(VERSION 3.20)

project(nemu LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

set(default_isa "RV64IMAFDC_zicntr_zihpm" CACHE STRING "Default value for --isa switch")
set(default_priv "MSU" CACHE STRING "Default value for --priv switch")
set(default_varch "vlen:128,elen:64" CACHE STRING "Default value for --varch switch")

include(CheckCSourceCompiles)
include(CheckFunctionExists)

check_c_source_compiles("__int128 x = 0; int main(void) { (void)x; return 0; }" HAVE_INT128_C)

find_library(DL_LIB dl)
if(DL_LIB)
  set(CMAKE_REQUIRED_LIBRARIES "${DL_LIB}")
endif()
check_function_exists(dlopen HAVE_DLOPEN)
set(CMAKE_REQUIRED_LIBRARIES "")

set(HAVE_BOOST_ASIO_DEFINE "/* #undef HAVE_BOOST_ASIO */")
set(HAVE_BOOST_REGEX_DEFINE "/* #undef HAVE_BOOST_REGEX */")

if(HAVE_INT128_C)
  set(HAVE_INT128_DEFINE "#define HAVE_INT128 1")
else()
  set(HAVE_INT128_DEFINE "/* #undef HAVE_INT128 */")
endif()

if(HAVE_DLOPEN)
  set(HAVE_DLOPEN_DEFINE "#define HAVE_DLOPEN 1")
else()
  set(HAVE_DLOPEN_DEFINE "/* #undef HAVE_DLOPEN */")
endif()

set(CUSTOMEXT_ENABLED_DEFINE "#define CUSTOMEXT_ENABLED 1")
set(DISASM_ENABLED_DEFINE "#define DISASM_ENABLED 1")
set(FDT_ENABLED_DEFINE "#define FDT_ENABLED 1")
set(FESVR_ENABLED_DEFINE "#define FESVR_ENABLED 1")
set(RISCV_ENABLED_DEFINE "#define RISCV_ENABLED 1")
set(SOFTFLOAT_ENABLED_DEFINE "#define SOFTFLOAT_ENABLED 1")
set(SPIKE_DASM_ENABLED_DEFINE "/* #undef SPIKE_DASM_ENABLED */")
set(SPIKE_MAIN_ENABLED_DEFINE "#define SPIKE_MAIN_ENABLED 1")

find_program(DTC_EXECUTABLE dtc REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(nvboard REQUIRED)
find_package(absl REQUIRED)

configure_file(
  "${CMAKE_SOURCE_DIR}/config.h.in"
  "${CMAKE_BINARY_DIR}/config.h"
  @ONLY
)

set(RISCV_GEN_DIR "${CMAKE_BINARY_DIR}/riscv_gen")
file(MAKE_DIRECTORY "${RISCV_GEN_DIR}")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/gen_riscv_insns.py"
          "${CMAKE_SOURCE_DIR}/riscv"
          "${CMAKE_SOURCE_DIR}/riscv/encoding.h"
          "${CMAKE_SOURCE_DIR}/riscv/insn_template.cc"
          "${RISCV_GEN_DIR}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  RESULT_VARIABLE gen_riscv_result
)
if(NOT gen_riscv_result EQUAL 0)
  message(FATAL_ERROR "Failed to generate riscv instruction sources")
endif()

file(STRINGS "${RISCV_GEN_DIR}/generated_files.txt" RISCV_GEN_LIST)
set(RISCV_GEN_SRCS "")
foreach(gen_file IN LISTS RISCV_GEN_LIST)
  list(APPEND RISCV_GEN_SRCS "${RISCV_GEN_DIR}/${gen_file}")
endforeach()

set(COMMON_COMPILE_OPTIONS -Wno-unused)
set(COMMON_COMPILE_DEFINITIONS PREFIX="${CMAKE_INSTALL_PREFIX}")

function(apply_common_flags target_name)
  target_compile_options("${target_name}" PRIVATE ${COMMON_COMPILE_OPTIONS})
  target_compile_definitions("${target_name}" PRIVATE ${COMMON_COMPILE_DEFINITIONS})
endfunction()

set(FESVR_SRCS
  fesvr/elfloader.cc
  fesvr/htif.cc
  fesvr/memif.cc
  fesvr/dtm.cc
  fesvr/syscall.cc
  fesvr/device.cc
  fesvr/rfb.cc
  fesvr/context.cc
  fesvr/htif_pthread.cc
  fesvr/htif_hexwriter.cc
  fesvr/dummy.cc
  fesvr/option_parser.cc
  fesvr/term.cc
  fesvr/tsi.cc
)

add_library(fesvr STATIC ${FESVR_SRCS})
target_include_directories(fesvr
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  PRIVATE
    "${CMAKE_SOURCE_DIR}/fesvr"
    "${CMAKE_SOURCE_DIR}/riscv"
)
apply_common_flags(fesvr)

set(FDT_SRCS
  fdt/fdt.c
  fdt/fdt_ro.c
  fdt/fdt_wip.c
  fdt/fdt_sw.c
  fdt/fdt_rw.c
  fdt/fdt_strerror.c
  fdt/fdt_empty_tree.c
  fdt/fdt_addresses.c
  fdt/fdt_overlay.c
)

add_library(fdt STATIC ${FDT_SRCS})
target_include_directories(fdt
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  PRIVATE
    "${CMAKE_SOURCE_DIR}/fdt"
)
apply_common_flags(fdt)

set(SOFTFLOAT_SRCS softfloat/dummy_for_pa.c)

add_library(softfloat STATIC ${SOFTFLOAT_SRCS})
target_include_directories(softfloat
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  PRIVATE
    "${CMAKE_SOURCE_DIR}/softfloat"
)
apply_common_flags(softfloat)

add_library(disasm STATIC
  disasm/disasm.cc
  disasm/regnames.cc
)
target_include_directories(disasm
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  PRIVATE
    "${CMAKE_SOURCE_DIR}/disasm"
    "${CMAKE_SOURCE_DIR}/riscv"
    "${CMAKE_SOURCE_DIR}/softfloat"
)
apply_common_flags(disasm)

set(RISCV_SRCS
  riscv/isa_parser.cc
  riscv/processor.cc
  riscv/execute.cc
  riscv/dts.cc
  riscv/sim.cc
  riscv/interactive.cc
  riscv/cachesim.cc
  riscv/mmu.cc
  riscv/extension.cc
  riscv/extensions.cc
  riscv/rocc.cc
  riscv/devices.cc
  riscv/rom.cc
  riscv/clint.cc
  riscv/plic.cc
  riscv/ns16550.cc
  riscv/vga.cc
  riscv/debug_module.cc
  riscv/remote_bitbang.cc
  riscv/jtag_dtm.cc
  riscv/csrs.cc
  riscv/triggers.cc
  riscv/vector_unit.cc
  riscv/socketif.cc
  riscv/cfg.cc
  ${RISCV_GEN_SRCS}
)

add_library(riscv STATIC ${RISCV_SRCS})
target_include_directories(riscv
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    $<BUILD_INTERFACE:${RISCV_GEN_DIR}>
  PRIVATE
    "${CMAKE_SOURCE_DIR}/fesvr"
    "${CMAKE_SOURCE_DIR}/fdt"
    "${CMAKE_SOURCE_DIR}/softfloat"
    "${CMAKE_SOURCE_DIR}/disasm"
    "${CMAKE_SOURCE_DIR}/riscv"
)
target_link_libraries(riscv PUBLIC fesvr fdt softfloat disasm nvboard::nvboard)
if(HAVE_DLOPEN AND DL_LIB)
  target_link_libraries(riscv PUBLIC "${DL_LIB}")
endif()
apply_common_flags(riscv)

set(NEMU_DEPS fesvr fdt softfloat disasm riscv)
set(NEMU_INC
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_SOURCE_DIR}/fesvr"
  "${CMAKE_SOURCE_DIR}/fdt"
  "${CMAKE_SOURCE_DIR}/softfloat"
  "${CMAKE_SOURCE_DIR}/disasm"
  "${CMAKE_SOURCE_DIR}/riscv"
  "${CMAKE_SOURCE_DIR}/spike_main"
)

add_executable(nemu spike_main/spike.cc)
target_include_directories(nemu PRIVATE ${NEMU_INC})
target_link_libraries(nemu PRIVATE
  ${NEMU_DEPS}
  nvboard::nvboard
  absl::flags
  absl::flags_parse
)
target_compile_definitions(nemu PRIVATE NEMU_VERSION="${PROJECT_VERSION}")
if(HAVE_DLOPEN AND DL_LIB)
  target_link_libraries(nemu PRIVATE "${DL_LIB}")
endif()
apply_common_flags(nemu)

install(TARGETS nemu
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
