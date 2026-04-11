#ifndef NEMU_COMMON_H_
#define NEMU_COMMON_H_

#include <cstddef>
#include <cstdint>

namespace nemu {

// Memory regions (matching npc SoCConfig)
inline constexpr uint64_t kFlashBase = 0x30000000;
inline constexpr uint64_t kFlashSize = 0x10000000; // 256 MiB
inline constexpr uint64_t kSramBase = 0x0f000000;
inline constexpr uint64_t kSramSize = 0x2000; // 8 KiB
inline constexpr uint64_t kSdramBase = 0x80000000;
inline constexpr uint64_t kSdramSize = 0x10000000; // 256 MiB
inline constexpr uint64_t kResetVector = 0x30000000;

// Device MMIO (matching npc SoCConfig)
inline constexpr uint64_t kUartBase = 0x10000000;
inline constexpr uint64_t kUartSize = 0x1000;
inline constexpr uint64_t kGpioBase = 0x10002000;
inline constexpr uint64_t kGpioSize = 0x10;
inline constexpr uint64_t kKeyboardBase = 0x10011000;
inline constexpr uint64_t kKeyboardSize = 0x8;
inline constexpr uint64_t kVgaBase = 0x21000000;
inline constexpr uint64_t kVgaSize = 0x200000; // 2 MiB
inline constexpr uint64_t kClintBase = 0x02000000;
inline constexpr uint64_t kClintSize = 0x10000;

inline constexpr uint64_t kPlicBase = 0x0c000000;
inline constexpr uint64_t kPlicSize = 0x400000;

inline constexpr int kUartIrq = 10;
inline constexpr int kKeyboardIrq = 12;

inline constexpr int kDefaultScreenWidth = 640;
inline constexpr int kDefaultScreenHeight = 480;

inline constexpr uint32_t kEbreakInsn = 0x00100073;

inline constexpr const char *kGprNames[] = {
    "$0", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
    "a1", "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
    "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

} // namespace nemu

#endif // NEMU_COMMON_H_
