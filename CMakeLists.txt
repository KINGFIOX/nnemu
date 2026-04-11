cmake_minimum_required(VERSION 3.20)

project(spike VERSION 2026.03.04 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(softfloat_pa "Use minimal softfloat for PA course" ON)
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

find_package(Boost 1.53 QUIET COMPONENTS system regex)
if(Boost_FOUND)
  set(HAVE_BOOST_ASIO_DEFINE "#define HAVE_BOOST_ASIO 1")
  set(HAVE_BOOST_REGEX_DEFINE "#define HAVE_BOOST_REGEX 1")
  set(BOOST_LINK_LIBS Boost::system Boost::regex)
else()
  set(HAVE_BOOST_ASIO_DEFINE "/* #undef HAVE_BOOST_ASIO */")
  set(HAVE_BOOST_REGEX_DEFINE "/* #undef HAVE_BOOST_REGEX */")
  set(BOOST_LINK_LIBS "")
endif()

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
set(SPIKE_DASM_ENABLED_DEFINE "#define SPIKE_DASM_ENABLED 1")
set(SPIKE_MAIN_ENABLED_DEFINE "#define SPIKE_MAIN_ENABLED 1")

find_program(DTC_EXECUTABLE dtc REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

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
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/fesvr>
  PRIVATE
    "${CMAKE_SOURCE_DIR}/fesvr"
    "${CMAKE_SOURCE_DIR}/riscv"
)
apply_common_flags(fesvr)

add_executable(elf2hex fesvr/elf2hex.cc)
target_include_directories(elf2hex PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_SOURCE_DIR}/fesvr"
  "${CMAKE_SOURCE_DIR}/riscv"
)
target_link_libraries(elf2hex PRIVATE fesvr)
apply_common_flags(elf2hex)

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
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/fdt>
  PRIVATE
    "${CMAKE_SOURCE_DIR}/fdt"
)
apply_common_flags(fdt)

if(softfloat_pa)
  set(SOFTFLOAT_SRCS softfloat/dummy_for_pa.c)
else()
  set(SOFTFLOAT_SRCS
    softfloat/f128_add.c
    softfloat/f128_classify.c
    softfloat/f128_div.c
    softfloat/f128_eq.c
    softfloat/f128_eq_signaling.c
    softfloat/f128_isSignalingNaN.c
    softfloat/f128_le.c
    softfloat/f128_le_quiet.c
    softfloat/f128_lt.c
    softfloat/f128_lt_quiet.c
    softfloat/f128_mulAdd.c
    softfloat/f128_mul.c
    softfloat/f128_rem.c
    softfloat/f128_roundToInt.c
    softfloat/f128_sqrt.c
    softfloat/f128_sub.c
    softfloat/f128_to_f16.c
    softfloat/f128_to_f32.c
    softfloat/f128_to_f64.c
    softfloat/f128_to_i32.c
    softfloat/f128_to_i32_r_minMag.c
    softfloat/f128_to_i64.c
    softfloat/f128_to_i64_r_minMag.c
    softfloat/f128_to_ui32.c
    softfloat/f128_to_ui32_r_minMag.c
    softfloat/f128_to_ui64.c
    softfloat/f128_to_ui64_r_minMag.c
    softfloat/f16_add.c
    softfloat/f16_classify.c
    softfloat/f16_div.c
    softfloat/f16_eq.c
    softfloat/f16_eq_signaling.c
    softfloat/f16_isSignalingNaN.c
    softfloat/f16_le.c
    softfloat/f16_le_quiet.c
    softfloat/f16_lt.c
    softfloat/f16_lt_quiet.c
    softfloat/f16_mulAdd.c
    softfloat/f16_mul.c
    softfloat/f16_rem.c
    softfloat/f16_roundToInt.c
    softfloat/f16_sqrt.c
    softfloat/f16_sub.c
    softfloat/f16_to_f128.c
    softfloat/f16_to_f32.c
    softfloat/f16_to_f64.c
    softfloat/f16_to_i8.c
    softfloat/f16_to_i16.c
    softfloat/f16_to_i32.c
    softfloat/f16_to_i32_r_minMag.c
    softfloat/f16_to_i64.c
    softfloat/f16_to_i64_r_minMag.c
    softfloat/f16_to_ui8.c
    softfloat/f16_to_ui16.c
    softfloat/f16_to_ui32.c
    softfloat/f16_to_ui32_r_minMag.c
    softfloat/f16_to_ui64.c
    softfloat/f16_to_ui64_r_minMag.c
    softfloat/f32_add.c
    softfloat/f32_classify.c
    softfloat/f32_div.c
    softfloat/f32_eq.c
    softfloat/f32_eq_signaling.c
    softfloat/f32_isSignalingNaN.c
    softfloat/f32_le.c
    softfloat/f32_le_quiet.c
    softfloat/f32_lt.c
    softfloat/f32_lt_quiet.c
    softfloat/f32_mulAdd.c
    softfloat/f32_mul.c
    softfloat/f32_rem.c
    softfloat/f32_roundToInt.c
    softfloat/f32_sqrt.c
    softfloat/f32_sub.c
    softfloat/f32_to_f128.c
    softfloat/f32_to_f16.c
    softfloat/f32_to_f64.c
    softfloat/f32_to_i16.c
    softfloat/f32_to_i32.c
    softfloat/f32_to_i32_r_minMag.c
    softfloat/f32_to_i64.c
    softfloat/f32_to_i64_r_minMag.c
    softfloat/f32_to_ui16.c
    softfloat/f32_to_ui32.c
    softfloat/f32_to_ui32_r_minMag.c
    softfloat/f32_to_ui64.c
    softfloat/f32_to_ui64_r_minMag.c
    softfloat/f64_add.c
    softfloat/f64_classify.c
    softfloat/f64_div.c
    softfloat/f64_eq.c
    softfloat/f64_eq_signaling.c
    softfloat/f64_isSignalingNaN.c
    softfloat/f64_le.c
    softfloat/f64_le_quiet.c
    softfloat/f64_lt.c
    softfloat/f64_lt_quiet.c
    softfloat/f64_mulAdd.c
    softfloat/f64_mul.c
    softfloat/f64_rem.c
    softfloat/f64_roundToInt.c
    softfloat/f64_sqrt.c
    softfloat/f64_sub.c
    softfloat/f64_to_f128.c
    softfloat/f64_to_f16.c
    softfloat/f64_to_f32.c
    softfloat/f64_to_i32.c
    softfloat/f64_to_i32_r_minMag.c
    softfloat/f64_to_i64.c
    softfloat/f64_to_i64_r_minMag.c
    softfloat/f64_to_ui32.c
    softfloat/f64_to_ui32_r_minMag.c
    softfloat/f64_to_ui64.c
    softfloat/f64_to_ui64_r_minMag.c
    softfloat/fall_maxmin.c
    softfloat/fall_reciprocal.c
    softfloat/i32_to_f128.c
    softfloat/i32_to_f16.c
    softfloat/i32_to_f32.c
    softfloat/i32_to_f64.c
    softfloat/i64_to_f128.c
    softfloat/i64_to_f16.c
    softfloat/i64_to_f32.c
    softfloat/i64_to_f64.c
    softfloat/s_add128.c
    softfloat/s_add256M.c
    softfloat/s_addCarryM.c
    softfloat/s_addComplCarryM.c
    softfloat/s_addMagsF128.c
    softfloat/s_addMagsF16.c
    softfloat/s_addMagsF32.c
    softfloat/s_addMagsF64.c
    softfloat/s_addM.c
    softfloat/s_approxRecip_1Ks.c
    softfloat/s_approxRecip32_1.c
    softfloat/s_approxRecipSqrt_1Ks.c
    softfloat/s_approxRecipSqrt32_1.c
    softfloat/s_commonNaNToF32UI.c
    softfloat/s_commonNaNToF64UI.c
    softfloat/s_compare128M.c
    softfloat/s_compare96M.c
    softfloat/s_countLeadingZeros16.c
    softfloat/s_countLeadingZeros32.c
    softfloat/s_countLeadingZeros64.c
    softfloat/s_countLeadingZeros8.c
    softfloat/s_eq128.c
    softfloat/s_f32UIToCommonNaN.c
    softfloat/s_f64UIToCommonNaN.c
    softfloat/s_le128.c
    softfloat/s_lt128.c
    softfloat/s_mul128By32.c
    softfloat/s_mul128MTo256M.c
    softfloat/s_mul128To256M.c
    softfloat/s_mul64ByShifted32To128.c
    softfloat/s_mul64To128.c
    softfloat/s_mul64To128M.c
    softfloat/s_mulAddF128.c
    softfloat/s_mulAddF16.c
    softfloat/s_mulAddF32.c
    softfloat/s_mulAddF64.c
    softfloat/s_negXM.c
    softfloat/s_normRoundPackToF128.c
    softfloat/s_normRoundPackToF16.c
    softfloat/s_normRoundPackToF32.c
    softfloat/s_normRoundPackToF64.c
    softfloat/s_normSubnormalF128Sig.c
    softfloat/s_normSubnormalF16Sig.c
    softfloat/s_normSubnormalF32Sig.c
    softfloat/s_normSubnormalF64Sig.c
    softfloat/softfloat_raiseFlags.c
    softfloat/softfloat_state.c
    softfloat/s_propagateNaNF16UI.c
    softfloat/s_propagateNaNF32UI.c
    softfloat/s_propagateNaNF64UI.c
    softfloat/s_propagateNaNF128UI.c
    softfloat/s_remStepMBy32.c
    softfloat/s_roundMToI64.c
    softfloat/s_roundMToUI64.c
    softfloat/s_roundPackMToI64.c
    softfloat/s_roundPackMToUI64.c
    softfloat/s_roundPackToF128.c
    softfloat/s_roundPackToF16.c
    softfloat/s_roundPackToF32.c
    softfloat/s_roundPackToF64.c
    softfloat/s_roundPackToI32.c
    softfloat/s_roundPackToI64.c
    softfloat/s_roundPackToUI32.c
    softfloat/s_roundPackToUI64.c
    softfloat/s_roundToI32.c
    softfloat/s_roundToI64.c
    softfloat/s_roundToUI32.c
    softfloat/s_roundToUI64.c
    softfloat/s_shiftRightJam128.c
    softfloat/s_shiftRightJam128Extra.c
    softfloat/s_shiftRightJam256M.c
    softfloat/s_shiftRightJam32.c
    softfloat/s_shiftRightJam64.c
    softfloat/s_shiftRightJam64Extra.c
    softfloat/s_shortShiftLeft128.c
    softfloat/s_shortShiftLeft64To96M.c
    softfloat/s_shortShiftRight128.c
    softfloat/s_shortShiftRightExtendM.c
    softfloat/s_shortShiftRightJam128.c
    softfloat/s_shortShiftRightJam128Extra.c
    softfloat/s_shortShiftRightJam64.c
    softfloat/s_shortShiftRightJam64Extra.c
    softfloat/s_shortShiftRightM.c
    softfloat/s_sub128.c
    softfloat/s_sub1XM.c
    softfloat/s_sub256M.c
    softfloat/s_subMagsF128.c
    softfloat/s_subMagsF16.c
    softfloat/s_subMagsF32.c
    softfloat/s_subMagsF64.c
    softfloat/s_subM.c
    softfloat/ui32_to_f128.c
    softfloat/ui32_to_f16.c
    softfloat/ui32_to_f32.c
    softfloat/ui32_to_f64.c
    softfloat/ui64_to_f128.c
    softfloat/ui64_to_f16.c
    softfloat/ui64_to_f32.c
    softfloat/ui64_to_f64.c
  )
endif()

add_library(softfloat STATIC ${SOFTFLOAT_SRCS})
target_include_directories(softfloat
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/softfloat>
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
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/disasm>
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
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/riscv>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/riscv_gen>
  PRIVATE
    "${CMAKE_SOURCE_DIR}/fesvr"
    "${CMAKE_SOURCE_DIR}/fdt"
    "${CMAKE_SOURCE_DIR}/softfloat"
    "${CMAKE_SOURCE_DIR}/disasm"
    "${CMAKE_SOURCE_DIR}/riscv"
)
target_link_libraries(riscv PUBLIC fesvr fdt softfloat disasm)
if(Boost_FOUND)
  target_link_libraries(riscv PUBLIC ${BOOST_LINK_LIBS})
endif()
if(HAVE_DLOPEN AND DL_LIB)
  target_link_libraries(riscv PUBLIC "${DL_LIB}")
endif()
apply_common_flags(riscv)

add_library(customext SHARED
  customext/dummy_rocc.cc
  customext/cflush.cc
)
target_include_directories(customext PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_SOURCE_DIR}/spike_main"
  "${CMAKE_SOURCE_DIR}/riscv"
  "${CMAKE_SOURCE_DIR}/disasm"
  "${CMAKE_SOURCE_DIR}/softfloat"
)
target_link_libraries(customext PRIVATE riscv disasm softfloat)
apply_common_flags(customext)

set(SPIKE_DEPS fesvr fdt softfloat disasm riscv)
set(SPIKE_INC
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_SOURCE_DIR}/fesvr"
  "${CMAKE_SOURCE_DIR}/fdt"
  "${CMAKE_SOURCE_DIR}/softfloat"
  "${CMAKE_SOURCE_DIR}/disasm"
  "${CMAKE_SOURCE_DIR}/riscv"
  "${CMAKE_SOURCE_DIR}/spike_main"
)

add_executable(spike spike_main/spike.cc)
target_include_directories(spike PRIVATE ${SPIKE_INC})
target_link_libraries(spike PRIVATE ${SPIKE_DEPS})
target_compile_definitions(spike PRIVATE SPIKE_VERSION="${PROJECT_VERSION}")
if(Boost_FOUND)
  target_link_libraries(spike PRIVATE ${BOOST_LINK_LIBS})
endif()
if(HAVE_DLOPEN AND DL_LIB)
  target_link_libraries(spike PRIVATE "${DL_LIB}")
endif()
apply_common_flags(spike)

add_executable(spike-log-parser spike_main/spike-log-parser.cc)
target_include_directories(spike-log-parser PRIVATE ${SPIKE_INC})
target_link_libraries(spike-log-parser PRIVATE ${SPIKE_DEPS})
apply_common_flags(spike-log-parser)

add_executable(xspike spike_main/xspike.cc)
target_include_directories(xspike PRIVATE ${SPIKE_INC})
target_link_libraries(xspike PRIVATE ${SPIKE_DEPS})
if(Boost_FOUND)
  target_link_libraries(xspike PRIVATE ${BOOST_LINK_LIBS})
endif()
if(HAVE_DLOPEN AND DL_LIB)
  target_link_libraries(xspike PRIVATE "${DL_LIB}")
endif()
apply_common_flags(xspike)

add_executable(termios-xspike spike_main/termios-xspike.cc)

add_executable(spike-dasm
  spike_dasm/spike-dasm.cc
  spike_dasm/spike_dasm_option_parser.cc
)
target_include_directories(spike-dasm PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_SOURCE_DIR}/fesvr"
  "${CMAKE_SOURCE_DIR}/disasm"
  "${CMAKE_SOURCE_DIR}/softfloat"
  "${CMAKE_SOURCE_DIR}/riscv"
  "${CMAKE_SOURCE_DIR}/spike_dasm"
)
target_link_libraries(spike-dasm PRIVATE disasm softfloat riscv)
apply_common_flags(spike-dasm)

install(FILES "${CMAKE_BINARY_DIR}/config.h"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY fesvr fdt softfloat disasm riscv spike_main customext spike_dasm
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY "${RISCV_GEN_DIR}/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/riscv_gen
  FILES_MATCHING PATTERN "*.h"
)

install(TARGETS fesvr fdt softfloat disasm riscv
  EXPORT spikeTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(TARGETS customext
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(TARGETS elf2hex spike spike-log-parser xspike termios-xspike spike-dasm
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT spikeTargets
  FILE spikeTargets.cmake
  NAMESPACE spike::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/spike
)

set(spike_BOOST_FOUND ${Boost_FOUND})
if(HAVE_DLOPEN AND DL_LIB)
  set(spike_DLOPEN_FOUND TRUE)
else()
  set(spike_DLOPEN_FOUND FALSE)
endif()

configure_package_config_file(
  "${CMAKE_SOURCE_DIR}/cmake/spikeConfig.cmake.in"
  "${CMAKE_BINARY_DIR}/spikeConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/spike
)

write_basic_package_version_file(
  "${CMAKE_BINARY_DIR}/spikeConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_BINARY_DIR}/spikeConfig.cmake"
  "${CMAKE_BINARY_DIR}/spikeConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/spike
)
