#ifndef NNEMU_COMMON_H_
#define NNEMU_COMMON_H_

#include <cstdint>
#include <cstddef>

namespace nnemu {

inline constexpr uint64_t kPmemBase = 0x80000000;
inline constexpr uint64_t kPmemSize = 128 * 1024 * 1024;  // 128 MiB

inline constexpr uint64_t kSerialPort = 0x10000000;
inline constexpr uint64_t kDeviceBase = 0xa0000000;
inline constexpr uint64_t kFbAddr = kDeviceBase + 0x1000000;

// Offsets within the device MMIO region (relative to kDeviceBase)
inline constexpr uint64_t kRtcOffset = 0x0000048;
inline constexpr uint64_t kKbdOffset = 0x0000060;
inline constexpr uint64_t kVgaCtlOffset = 0x0000100;
inline constexpr uint64_t kAudioOffset = 0x0000200;
inline constexpr uint64_t kDiskOffset = 0x0000300;
inline constexpr uint64_t kFbOffset = 0x1000000;
inline constexpr uint64_t kAudioSbufOffset = 0x1200000;

inline constexpr int kDefaultScreenWidth = 400;
inline constexpr int kDefaultScreenHeight = 300;

inline constexpr uint32_t kEbreakInsn = 0x00100073;

}  // namespace nnemu

#endif  // NNEMU_COMMON_H_
