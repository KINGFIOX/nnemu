// See LICENSE for license details.
#ifndef _RISCV_PLATFORM_H
#define _RISCV_PLATFORM_H

#define DEFAULT_KERNEL_BOOTARGS "console=ttyS0 earlycon"
#define DEFAULT_RSTVEC     0x00001000
#define FLASH_BASE         0x30000000
#define FLASH_SIZE         0x10000000
#define CLINT_BASE         0x02000000
#define CLINT_SIZE         0x00010000
#define PLIC_BASE          0x0c000000
#define PLIC_SIZE          0x00400000
#define PLIC_NDEV          31
#define PLIC_PRIO_BITS     4
#define NS16550_BASE       0x10000000
#define NS16550_SIZE       0x1000
#define NS16550_REG_SHIFT  0
#define NS16550_REG_IO_WIDTH 1
#define NS16550_INTERRUPT_ID 10
#define SYNC_DISK_BASE     0x10001000
#define SYNC_DISK_SIZE     0x1000
#define VGA_BASE           0x21000000
#define VGA_SIZE           0x200000
#define VGA_WIDTH          640
#define VGA_HEIGHT         480
#define EXT_IO_BASE        0x40000000
#define DRAM_BASE          0x80000000
#define DRAM_SIZE          0x10000000

#endif
