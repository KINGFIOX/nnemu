# Meson build for RISC-V Spike ISA Simulator
# Replaces autotools + Makefile build system

project('spike', 'c', 'cpp',
  version: run_command('scripts/vcs-version.sh', meson.project_source_root(), check: false).stdout().strip(),
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
    'buildtype=release',
    'b_lto=false',
  ],
  meson_version: '>=0.60.0',
)

# Compiler checks
cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

# Check for __int128 support (used for RISC-V vector extension)
have_int128 = cc.compiles('__int128 x = 0;', name: '__int128 support')

# Check for dlopen (dynamic library loading)
dl_dep = cc.find_library('dl', required: false)
have_dlopen = cc.has_function('dlopen', dependencies: dl_dep)

# Optional: Boost (for xspike network support)
boost_dep = dependency('boost', required: false, version: '>=1.53')
have_boost_asio = boost_dep.found()
have_boost_regex = boost_dep.found()

# Build config for config.h
conf_data = configuration_data()
conf_data.set('PREFIX', get_option('prefix'))
conf_data.set('HAVE_INT128', have_int128 ? '#define HAVE_INT128 1' : '/* #undef HAVE_INT128 */')
conf_data.set('HAVE_DLOPEN', have_dlopen ? '#define HAVE_DLOPEN 1' : '/* #undef HAVE_DLOPEN */')
conf_data.set('HAVE_BOOST_ASIO', have_boost_asio ? '#define HAVE_BOOST_ASIO 1' : '/* #undef HAVE_BOOST_ASIO */')
conf_data.set('HAVE_BOOST_REGEX', have_boost_regex ? '#define HAVE_BOOST_REGEX 1' : '/* #undef HAVE_BOOST_REGEX */')
conf_data.set('CUSTOMEXT_ENABLED', '#define CUSTOMEXT_ENABLED 1')
conf_data.set('DISASM_ENABLED', '#define DISASM_ENABLED 1')
conf_data.set('FDT_ENABLED', '#define FDT_ENABLED 1')
conf_data.set('FESVR_ENABLED', '#define FESVR_ENABLED 1')
conf_data.set('RISCV_ENABLED', '#define RISCV_ENABLED 1')
conf_data.set('SOFTFLOAT_ENABLED', '#define SOFTFLOAT_ENABLED 1')
conf_data.set('SPIKE_DASM_ENABLED', '#define SPIKE_DASM_ENABLED 1')
conf_data.set('SPIKE_MAIN_ENABLED', '#define SPIKE_MAIN_ENABLED 1')
find_program('dtc', required: true)
conf_data.set('DEFAULT_ISA', get_option('default_isa'))
conf_data.set('DEFAULT_PRIV', get_option('default_priv'))
conf_data.set('DEFAULT_VARCH', get_option('default_varch'))

config_h = configure_file(
  input: 'config.h.meson.in',
  output: 'config.h',
  configuration: conf_data,
)

# Common include directories (include_directories('.') adds both source and build dir)
inc_root = include_directories('.')
inc_fesvr = include_directories('fesvr')
inc_fdt = include_directories('fdt')
inc_softfloat = include_directories('softfloat')
inc_disasm = include_directories('disasm')
inc_riscv = include_directories('riscv')
inc_spike_main = include_directories('spike_main')
inc_customext = include_directories('customext')
inc_spike_dasm = include_directories('spike_dasm')

# Common compile args
common_cpp_args = [
  '-DPREFIX="@0@"'.format(get_option('prefix')),
  '-Wall', '-Wno-unused',
  '-fPIC',
]

# --- fesvr ---
fesvr_srcs = [
  'fesvr/elfloader.cc', 'fesvr/htif.cc', 'fesvr/memif.cc', 'fesvr/dtm.cc',
  'fesvr/syscall.cc', 'fesvr/device.cc', 'fesvr/rfb.cc', 'fesvr/context.cc',
  'fesvr/htif_pthread.cc', 'fesvr/htif_hexwriter.cc', 'fesvr/dummy.cc',
  'fesvr/option_parser.cc', 'fesvr/term.cc', 'fesvr/tsi.cc',
]
libfesvr = static_library('fesvr', fesvr_srcs,
  include_directories: [inc_root, inc_fesvr, inc_riscv],
  cpp_args: common_cpp_args,
  install: true,
)

# elf2hex executable
elf2hex = executable('elf2hex', 'fesvr/elf2hex.cc',
  include_directories: [inc_root, inc_fesvr, inc_riscv],
  link_with: libfesvr,
  cpp_args: common_cpp_args,
  install: true,
)

# --- fdt ---
fdt_srcs = [
  'fdt/fdt.c', 'fdt/fdt_ro.c', 'fdt/fdt_wip.c', 'fdt/fdt_sw.c', 'fdt/fdt_rw.c',
  'fdt/fdt_strerror.c', 'fdt/fdt_empty_tree.c', 'fdt/fdt_addresses.c',
  'fdt/fdt_overlay.c',
]
libfdt = static_library('fdt', fdt_srcs,
  include_directories: [ inc_root, inc_fdt],
  c_args: common_cpp_args,
  install: true,
)

# --- softfloat ---
softfloat_pa = get_option('softfloat_pa')
if softfloat_pa
  softfloat_srcs = ['softfloat/dummy_for_pa.c']
else
  softfloat_srcs = ['softfloat/f128_add.c', 'softfloat/f128_classify.c', 'softfloat/f128_div.c',
    'softfloat/f128_eq.c', 'softfloat/f128_eq_signaling.c', 'softfloat/f128_isSignalingNaN.c',
    'softfloat/f128_le.c', 'softfloat/f128_le_quiet.c', 'softfloat/f128_lt.c',
    'softfloat/f128_lt_quiet.c', 'softfloat/f128_mulAdd.c', 'softfloat/f128_mul.c',
    'softfloat/f128_rem.c', 'softfloat/f128_roundToInt.c', 'softfloat/f128_sqrt.c',
    'softfloat/f128_sub.c', 'softfloat/f128_to_f16.c', 'softfloat/f128_to_f32.c',
    'softfloat/f128_to_f64.c', 'softfloat/f128_to_i32.c', 'softfloat/f128_to_i32_r_minMag.c',
    'softfloat/f128_to_i64.c', 'softfloat/f128_to_i64_r_minMag.c', 'softfloat/f128_to_ui32.c',
    'softfloat/f128_to_ui32_r_minMag.c', 'softfloat/f128_to_ui64.c', 'softfloat/f128_to_ui64_r_minMag.c',
    'softfloat/f16_add.c', 'softfloat/f16_classify.c', 'softfloat/f16_div.c',
    'softfloat/f16_eq.c', 'softfloat/f16_eq_signaling.c', 'softfloat/f16_isSignalingNaN.c',
    'softfloat/f16_le.c', 'softfloat/f16_le_quiet.c', 'softfloat/f16_lt.c',
    'softfloat/f16_lt_quiet.c', 'softfloat/f16_mulAdd.c', 'softfloat/f16_mul.c',
    'softfloat/f16_rem.c', 'softfloat/f16_roundToInt.c', 'softfloat/f16_sqrt.c',
    'softfloat/f16_sub.c', 'softfloat/f16_to_f128.c', 'softfloat/f16_to_f32.c',
    'softfloat/f16_to_f64.c', 'softfloat/f16_to_i8.c', 'softfloat/f16_to_i16.c',
    'softfloat/f16_to_i32.c', 'softfloat/f16_to_i32_r_minMag.c', 'softfloat/f16_to_i64.c',
    'softfloat/f16_to_i64_r_minMag.c', 'softfloat/f16_to_ui8.c', 'softfloat/f16_to_ui16.c',
    'softfloat/f16_to_ui32.c', 'softfloat/f16_to_ui32_r_minMag.c', 'softfloat/f16_to_ui64.c',
    'softfloat/f16_to_ui64_r_minMag.c', 'softfloat/f32_add.c', 'softfloat/f32_classify.c',
    'softfloat/f32_div.c', 'softfloat/f32_eq.c', 'softfloat/f32_eq_signaling.c',
    'softfloat/f32_isSignalingNaN.c', 'softfloat/f32_le.c', 'softfloat/f32_le_quiet.c',
    'softfloat/f32_lt.c', 'softfloat/f32_lt_quiet.c', 'softfloat/f32_mulAdd.c',
    'softfloat/f32_mul.c', 'softfloat/f32_rem.c', 'softfloat/f32_roundToInt.c',
    'softfloat/f32_sqrt.c', 'softfloat/f32_sub.c', 'softfloat/f32_to_f128.c',
    'softfloat/f32_to_f16.c', 'softfloat/f32_to_f64.c', 'softfloat/f32_to_i16.c',
    'softfloat/f32_to_i32.c', 'softfloat/f32_to_i32_r_minMag.c', 'softfloat/f32_to_i64.c',
    'softfloat/f32_to_i64_r_minMag.c', 'softfloat/f32_to_ui16.c', 'softfloat/f32_to_ui32.c',
    'softfloat/f32_to_ui32_r_minMag.c', 'softfloat/f32_to_ui64.c', 'softfloat/f32_to_ui64_r_minMag.c',
    'softfloat/f64_add.c', 'softfloat/f64_classify.c', 'softfloat/f64_div.c',
    'softfloat/f64_eq.c', 'softfloat/f64_eq_signaling.c', 'softfloat/f64_isSignalingNaN.c',
    'softfloat/f64_le.c', 'softfloat/f64_le_quiet.c', 'softfloat/f64_lt.c',
    'softfloat/f64_lt_quiet.c', 'softfloat/f64_mulAdd.c', 'softfloat/f64_mul.c',
    'softfloat/f64_rem.c', 'softfloat/f64_roundToInt.c', 'softfloat/f64_sqrt.c',
    'softfloat/f64_sub.c', 'softfloat/f64_to_f128.c', 'softfloat/f64_to_f16.c',
    'softfloat/f64_to_f32.c', 'softfloat/f64_to_i32.c', 'softfloat/f64_to_i32_r_minMag.c',
    'softfloat/f64_to_i64.c', 'softfloat/f64_to_i64_r_minMag.c', 'softfloat/f64_to_ui32.c',
    'softfloat/f64_to_ui32_r_minMag.c', 'softfloat/f64_to_ui64.c', 'softfloat/f64_to_ui64_r_minMag.c',
    'softfloat/fall_maxmin.c', 'softfloat/fall_reciprocal.c', 'softfloat/i32_to_f128.c',
    'softfloat/i32_to_f16.c', 'softfloat/i32_to_f32.c', 'softfloat/i32_to_f64.c',
    'softfloat/i64_to_f128.c', 'softfloat/i64_to_f16.c', 'softfloat/i64_to_f32.c',
    'softfloat/i64_to_f64.c', 'softfloat/s_add128.c', 'softfloat/s_add256M.c',
    'softfloat/s_addCarryM.c', 'softfloat/s_addComplCarryM.c', 'softfloat/s_addMagsF128.c',
    'softfloat/s_addMagsF16.c', 'softfloat/s_addMagsF32.c', 'softfloat/s_addMagsF64.c',
    'softfloat/s_addM.c', 'softfloat/s_approxRecip_1Ks.c', 'softfloat/s_approxRecip32_1.c',
    'softfloat/s_approxRecipSqrt_1Ks.c', 'softfloat/s_approxRecipSqrt32_1.c',
    'softfloat/s_commonNaNToF32UI.c', 'softfloat/s_commonNaNToF64UI.c',
    'softfloat/s_compare128M.c', 'softfloat/s_compare96M.c', 'softfloat/s_countLeadingZeros16.c',
    'softfloat/s_countLeadingZeros32.c', 'softfloat/s_countLeadingZeros64.c',
    'softfloat/s_countLeadingZeros8.c', 'softfloat/s_eq128.c', 'softfloat/s_f32UIToCommonNaN.c',
    'softfloat/s_f64UIToCommonNaN.c', 'softfloat/s_le128.c', 'softfloat/s_lt128.c',
    'softfloat/s_mul128By32.c', 'softfloat/s_mul128MTo256M.c', 'softfloat/s_mul128To256M.c',
    'softfloat/s_mul64ByShifted32To128.c', 'softfloat/s_mul64To128.c', 'softfloat/s_mul64To128M.c',
    'softfloat/s_mulAddF128.c', 'softfloat/s_mulAddF16.c', 'softfloat/s_mulAddF32.c',
    'softfloat/s_mulAddF64.c', 'softfloat/s_negXM.c', 'softfloat/s_normRoundPackToF128.c',
    'softfloat/s_normRoundPackToF16.c', 'softfloat/s_normRoundPackToF32.c',
    'softfloat/s_normRoundPackToF64.c', 'softfloat/s_normSubnormalF128Sig.c',
    'softfloat/s_normSubnormalF16Sig.c', 'softfloat/s_normSubnormalF32Sig.c',
    'softfloat/s_normSubnormalF64Sig.c', 'softfloat/softfloat_raiseFlags.c',
    'softfloat/softfloat_state.c', 'softfloat/s_propagateNaNF16UI.c', 'softfloat/s_propagateNaNF32UI.c',
    'softfloat/s_propagateNaNF64UI.c', 'softfloat/s_propagateNaNF128UI.c',
    'softfloat/s_remStepMBy32.c', 'softfloat/s_roundMToI64.c', 'softfloat/s_roundMToUI64.c',
    'softfloat/s_roundPackMToI64.c', 'softfloat/s_roundPackMToUI64.c', 'softfloat/s_roundPackToF128.c',
    'softfloat/s_roundPackToF16.c', 'softfloat/s_roundPackToF32.c', 'softfloat/s_roundPackToF64.c',
    'softfloat/s_roundPackToI32.c', 'softfloat/s_roundPackToI64.c', 'softfloat/s_roundPackToUI32.c',
    'softfloat/s_roundPackToUI64.c', 'softfloat/s_roundToI32.c', 'softfloat/s_roundToI64.c',
    'softfloat/s_roundToUI32.c', 'softfloat/s_roundToUI64.c', 'softfloat/s_shiftRightJam128.c',
    'softfloat/s_shiftRightJam128Extra.c', 'softfloat/s_shiftRightJam256M.c',
    'softfloat/s_shiftRightJam32.c', 'softfloat/s_shiftRightJam64.c', 'softfloat/s_shiftRightJam64Extra.c',
    'softfloat/s_shortShiftLeft128.c', 'softfloat/s_shortShiftLeft64To96M.c',
    'softfloat/s_shortShiftRight128.c', 'softfloat/s_shortShiftRightExtendM.c',
    'softfloat/s_shortShiftRightJam128.c', 'softfloat/s_shortShiftRightJam128Extra.c',
    'softfloat/s_shortShiftRightJam64.c', 'softfloat/s_shortShiftRightJam64Extra.c',
    'softfloat/s_shortShiftRightM.c', 'softfloat/s_sub128.c', 'softfloat/s_sub1XM.c',
    'softfloat/s_sub256M.c', 'softfloat/s_subMagsF128.c', 'softfloat/s_subMagsF16.c',
    'softfloat/s_subMagsF32.c', 'softfloat/s_subMagsF64.c', 'softfloat/s_subM.c',
    'softfloat/ui32_to_f128.c', 'softfloat/ui32_to_f16.c', 'softfloat/ui32_to_f32.c',
    'softfloat/ui32_to_f64.c', 'softfloat/ui64_to_f128.c', 'softfloat/ui64_to_f16.c',
    'softfloat/ui64_to_f32.c', 'softfloat/ui64_to_f64.c',
  ]
endif

libsoftfloat = static_library('softfloat', softfloat_srcs,
  include_directories: [ inc_root, inc_softfloat],
  c_args: common_cpp_args,
  install: true,
)

# --- disasm ---
libdisasm = static_library('disasm',
  'disasm/disasm.cc', 'disasm/regnames.cc',
  include_directories: [inc_root, inc_disasm, inc_riscv, inc_softfloat],
  cpp_args: common_cpp_args,
  install: true,
)

# --- riscv ---
# Generate instruction sources at configure time (output to source/riscv_gen for include path)
gen_dir = meson.project_source_root() / 'riscv_gen'
run_command('mkdir', '-p', gen_dir, check: true)
run_command(find_program('python3'),
  meson.project_source_root() / 'scripts/gen_riscv_insns.py',
  meson.project_source_root() / 'riscv',
  meson.project_source_root() / 'riscv' / 'encoding.h',
  meson.project_source_root() / 'riscv' / 'insn_template.cc',
  gen_dir,
  check: true,
)
gen_list = run_command('cat', gen_dir / 'generated_files.txt', check: true).stdout().strip().split('\n')
riscv_gen_srcs = []
foreach f : gen_list
  riscv_gen_srcs += gen_dir / f
endforeach

riscv_srcs = [
  'riscv/isa_parser.cc', 'riscv/processor.cc', 'riscv/execute.cc', 'riscv/dts.cc',
  'riscv/sim.cc', 'riscv/interactive.cc', 'riscv/cachesim.cc', 'riscv/mmu.cc',
  'riscv/extension.cc', 'riscv/extensions.cc', 'riscv/rocc.cc', 'riscv/devices.cc',
  'riscv/rom.cc', 'riscv/clint.cc', 'riscv/plic.cc', 'riscv/ns16550.cc',
  'riscv/debug_module.cc', 'riscv/remote_bitbang.cc', 'riscv/jtag_dtm.cc',
  'riscv/csrs.cc', 'riscv/triggers.cc', 'riscv/vector_unit.cc', 'riscv/socketif.cc',
  'riscv/cfg.cc',
] + riscv_gen_srcs

inc_riscv_gen = include_directories('riscv_gen')

libriscv = shared_library('riscv', riscv_srcs,
  include_directories: [ inc_root, inc_fesvr, inc_fdt, inc_softfloat, inc_disasm, inc_riscv, inc_riscv_gen],
  link_with: [libfesvr, libfdt, libsoftfloat, libdisasm],
  cpp_args: common_cpp_args + ['-fPIC'],
  dependencies: (boost_dep.found() ? [boost_dep] : []) + (have_dlopen ? [dl_dep] : []),
  install: true,
)

# --- customext ---
libcustomext = shared_library('customext',
  'customext/dummy_rocc.cc', 'customext/cflush.cc',
  include_directories: [ inc_root, inc_spike_main, inc_riscv, inc_disasm, inc_softfloat],
  link_with: [libriscv, libdisasm, libsoftfloat],
  cpp_args: common_cpp_args + ['-fPIC'],
  install: true,
)

# --- spike_main executables ---
spike_deps = [libfesvr, libfdt, libsoftfloat, libdisasm, libriscv]
spike_inc = [ inc_root, inc_fesvr, inc_fdt, inc_softfloat, inc_disasm, inc_riscv, inc_spike_main]

spike = executable('spike', 'spike_main/spike.cc',
  include_directories: spike_inc,
  link_with: spike_deps,
  cpp_args: common_cpp_args,
  dependencies: (boost_dep.found() ? [boost_dep] : []) + (have_dlopen ? [dl_dep] : []),
  install: true,
)

spike_log_parser = executable('spike-log-parser', 'spike_main/spike-log-parser.cc',
  include_directories: spike_inc,
  link_with: spike_deps,
  cpp_args: common_cpp_args,
  install: true,
)

xspike = executable('xspike', 'spike_main/xspike.cc',
  include_directories: spike_inc,
  link_with: spike_deps,
  cpp_args: common_cpp_args,
  dependencies: (boost_dep.found() ? [boost_dep] : []) + (have_dlopen ? [dl_dep] : []),
  install: true,
)

termios_xspike = executable('termios-xspike', 'spike_main/termios-xspike.cc',
  cpp_args: ['-std=c++17'],
  install: true,
)

# --- spike_dasm ---
spike_dasm = executable('spike-dasm', 'spike_dasm/spike-dasm.cc',
  'spike_dasm/spike_dasm_option_parser.cc',
  include_directories: [inc_root, inc_fesvr, inc_disasm, inc_softfloat, inc_riscv, inc_spike_dasm],
  link_with: [libdisasm, libsoftfloat, libriscv],
  cpp_args: common_cpp_args,
  dependencies: [],
  install: true,
)
