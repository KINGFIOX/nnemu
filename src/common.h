#ifndef NNEMU_COMMON_H_
#define NNEMU_COMMON_H_

#include <cstddef>
#include <cstdint>

namespace nnemu {

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

inline constexpr int kDefaultScreenWidth = 640;
inline constexpr int kDefaultScreenHeight = 480;

inline constexpr uint32_t kEbreakInsn = 0x00100073;

} // namespace nnemu

#endif // NNEMU_COMMON_H_
