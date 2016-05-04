// tube-defs.h

#ifndef TUBE_DEFS_H
#define TUBE_DEFS_H

#ifdef __ASSEMBLER__

#include "rpi-base.h"
#define GPFSEL0 (PERIPHERAL_BASE + 0x200000)  // controls GPIOs 0..9
#define GPFSEL1 (PERIPHERAL_BASE + 0x200004)  // controls GPIOs 10..19
#define GPFSEL2 (PERIPHERAL_BASE + 0x200008)  // controls GPIOs 20..29
#define GPSET0  (PERIPHERAL_BASE + 0x20001C)
#define GPCLR0  (PERIPHERAL_BASE + 0x200028)
#define GPLEV0  (PERIPHERAL_BASE + 0x200034)
#define GPEDS0  (PERIPHERAL_BASE + 0x200040)

#endif // __ASSEMBLER__

//    D7 - Pin 22 - GPIO25 
//    D6 - Pin 18 - GPIO24 
//    D5 - Pin 16 - GPIO23 
//    D4 - Pin 15 - GPIO22 
//    D3 - Pin 23 - GPIO11 
//    D2 - Pin 19 - GPIO10 
//    D1 - Pin 21 - GPIO9  
//    D0 - Pin 24 - GPIO8  
//    A2 - Pin  5 - GPIO1  (GPIO3  on later models)
//    A1 - Pin  3 - GPIO0  (GPIO2  on later models)
//    A0 - Pin 13 - GPIO21 (GPIO27 on later models)
//  Phi2 - Pin 26 - GPIO7  
// nTUBE - Pin 11 - GPIO17 
//  nRST - Pin  7 - GPIO4  
//   RnW - Pin 12 - GPIO18 

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

#define A2_PIN       (1)
#define A1_PIN       (0)
#define A0_PIN       (21)
#define PHI2_PIN     (7)
#define NTUBE_PIN    (17)
#define NRST_PIN     (4)
#define RNW_PIN      (18)

#define ATTN_PIN     (31)
#define OVERRUN_PIN  (30)

#define D7_MASK      (1 << D7_PIN)
#define D6_MASK      (1 << D6_PIN)
#define D5_MASK      (1 << D5_PIN)
#define D4_MASK      (1 << D4_PIN)
#define D3_MASK      (1 << D3_PIN)
#define D2_MASK      (1 << D2_PIN)
#define D1_MASK      (1 << D1_PIN)
#define D0_MASK      (1 << D0_PIN)
#define A2_MASK      (1 << A2_PIN)
#define A1_MASK      (1 << A1_PIN)
#define A0_MASK      (1 << A0_PIN)
#define PHI2_MASK    (1 << PHI2_PIN)
#define NTUBE_MASK   (1 << NTUBE_PIN)
#define NRST_MASK    (1 << NRST_PIN)
#define RNW_MASK     (1 << RNW_PIN)

#define ATTN_MASK    (1 << ATTN_PIN)
#define OVERRUN_MASK (1 << OVERRUN_PIN)

#define D30_MASK     (D3_MASK | D2_MASK | D1_MASK | D0_MASK)
#define D74_MASK     (D7_MASK | D6_MASK | D5_MASK | D4_MASK)
#define D_MASK       (D74_MASK | D30_MASK)

#define A_MASK       (A2_MASK | A1_MASK | A0_MASK)

#define PINS_MASK    (A_MASK | D_MASK | RNW_MASK | NRST_MASK)

#ifndef __ASSEMBLER__

extern volatile int nRST;

#endif

#endif
