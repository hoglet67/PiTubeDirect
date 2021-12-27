// tube-defs.h

#ifndef TUBE_DEFS_H
#define TUBE_DEFS_H

#define RELEASENAME "Hognose-rc1"

#define NDEBUG

#ifdef DEBUG
#define LOG_DEBUG(...) printf(__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#define LOG_INFO(...) printf(__VA_ARGS__)

#define LOG_WARN(...) printf(__VA_ARGS__)

// Pi 2/3 Multicore options
#if defined(RPI2) || defined(RPI3) || defined(RPI4)

// Indicate the platform has multiple cores
#define HAS_MULTICORE

#endif

// If defined, the doorbell is used rather then the mailbox
#define USE_DOORBELL

//
// tube_irq bit definitions
//
// bit 7 Selects if R7 is used to inform the copro of an interrupt event used for fast 6502
// bit 6 Selects if direct native arm irq are used
// bit 5 native arm irq lock
// bit 3 tube_enable
// bit 2 Reset event
// bit 1 NMI
// bit 0 IRQ
#define FAST6502_BIT 128
#define NATIVEARM_BIT 64
#define nativearmlock_bit 32
#define TUBE_ENABLE_BIT  8
#define RESET_BIT 4
#define NMI_BIT 2
#define IRQ_BIT 1

#include "rpi-base.h"

#define DOORBELL        (PERIPHERAL_BASE + 0x00B844) // Doorbell 1
#define DOORBELLDATA    (PERIPHERAL_BASE + 0x20C014) // Hijack PWM_DAT1 for Doorbell1 Data
#define MBOX0_READ      (PERIPHERAL_BASE + 0x00B880)
#define MBOX0_STATUS    (PERIPHERAL_BASE + 0x00B898)
#define MBOX0_CONFIG    (PERIPHERAL_BASE + 0x00B89C)
#define MBOX0_EMPTY     (0x40000000)
#define MBOX0_DATAIRQEN (0x00000001)
#define FIQCTRL         (PERIPHERAL_BASE + 0x00B20C)


#ifdef __ASSEMBLER__

#define GPFSEL0 (PERIPHERAL_BASE + 0x200000)  // controls GPIOs 0..9
#define GPFSEL1 (PERIPHERAL_BASE + 0x200004)  // controls GPIOs 10..19
#define GPFSEL2 (PERIPHERAL_BASE + 0x200008)  // controls GPIOs 20..29
#define GPSET0  (PERIPHERAL_BASE + 0x20001C)
#define GPCLR0  (PERIPHERAL_BASE + 0x200028)
#define GPLEV0  (PERIPHERAL_BASE + 0x200034)
#define GPEDS0  (PERIPHERAL_BASE + 0x200040)
#define FIQCTRL (PERIPHERAL_BASE + 0x00B20C)

#endif // __ASSEMBLER__

//    A2 – Green  - Pin 5  – GPIO1  (GPIO3 on later models)
//    A1 – Blue   - Pin 3  – GPIO0  (GPIO2 on later models)
//    A0 – Purple – Pin 13 – GPIO21 (GPIO27 on later models)
//    D7 – Grey   - Pin 22 – GPIO25
//    D6 – White  - Pin 18 – GPIO24
//    D5 – Black  - Pin 16 – GPIO23
//    D4 – Brown  - Pin 15 – GPIO22
//    D3 – Red    - Pin 23 – GPIO11
//    D2 – Orange – Pin 19 – GPIO10
//    D1 – Yellow – Pin 21 – GPIO9
//    D0 – Green  - Pin 24 – GPIO8
//  nRST – Blue   - Pin 7  – GPIO4
// nTUBE – Purple – Pin 11 – GPIO17
//  nIRQ
//  Phi2 – Grey   - Pin 26 – GPIO7
//   RnW – White  - Pin 12 – GPIO18
//   GND – Black  - Pin 25 – GND

#define D0_BASE      (8)
#define D4_BASE      (22)

#define D7_PIN       (25)     // C2
#define D6_PIN       (24)     // C2
#define D5_PIN       (23)     // C2
#define D4_PIN       (22)     // C2
#define D3_PIN       (11)     // C1
#define D2_PIN       (10)     // C1
#define D1_PIN       (9)      // C0
#define D0_PIN       (8)      // C0

#define MAGIC_C0     ((1 << (D0_PIN * 3)) | (1 << (D1_PIN * 3)))
#define MAGIC_C1     ((1 << ((D2_PIN - 10) * 3)) | (1 << ((D3_PIN - 10) * 3)))
#define MAGIC_C2     ((1 << ((D4_PIN - 20) * 3)) | (1 << ((D5_PIN - 20) * 3)))
#define MAGIC_C3     ((1 << ((D6_PIN - 20) * 3)) | (1 << ((D7_PIN - 20) * 3)))

#define A2_PIN_40PIN       (3)
#define A1_PIN_40PIN       (2)
#define A0_PIN_40PIN       (27)

#define A2_PIN_26PIN       (1)
#define A1_PIN_26PIN       (0)
#define A0_PIN_26PIN       (21)

#define PHI2_PIN     (7)
#define NTUBE_PIN    (17)
#define NRST_PIN     (4)
#define RNW_PIN      (18)

#define D7_MASK      (1 << D7_PIN)
#define D6_MASK      (1 << D6_PIN)
#define D5_MASK      (1 << D5_PIN)
#define D4_MASK      (1 << D4_PIN)
#define D3_MASK      (1 << D3_PIN)
#define D2_MASK      (1 << D2_PIN)
#define D1_MASK      (1 << D1_PIN)
#define D0_MASK      (1 << D0_PIN)

#define PHI2_MASK    (1 << PHI2_PIN)
#define NTUBE_MASK   (1 << NTUBE_PIN)
#define NRST_MASK    (1 << NRST_PIN)
#define RNW_MASK     (1 << RNW_PIN)

#define ATTN_MASK    (1 << ATTN_PIN)
#define OVERRUN_MASK (1 << OVERRUN_PIN)
#define GLITCH_MASK  (1 << GLITCH_PIN)

#define D30_MASK     (D3_MASK | D2_MASK | D1_MASK | D0_MASK)
#define D74_MASK     (D7_MASK | D6_MASK | D5_MASK | D4_MASK)
#define D_MASK       (D74_MASK | D30_MASK)


#define TEST_PIN_26PIN     (27)
#define TEST_PIN_40PIN     (21)

#define TEST_PIN     (21)
#define TEST_MASK    (1 << TEST_PIN)
#define TEST2_PIN    (20)
#define TEST2_MASK   (1 << TEST2_PIN)
#define TEST3_PIN    (16)
#define TEST3_MASK   (1 << TEST3_PIN)

#endif
