#include "rpi-base.h"
#include "tube-defs.h"
     
#define instrREG r7
.text

.global arm_irq_handler
.global arm_fiq_handler

.global arm_fiq_handler_flag1

//
// bit 7 Selects if R7 is used to inform the copro of an interupt event used for fast 6502
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
// =================================================
// ISR CODE
// =================================================
CACHELINE_ALIGN = 5

.align CACHELINE_ALIGN

// Default handlers for FIQ/IRQ do nothing

arm_fiq_handler:
arm_irq_handler:
        subs    pc, lr, #4

// ARM FIQ handler 
arm_fiq_handler_flag1:

      ldr      r12, =MBOX0_READ        // Read the GPU mailbox
      push     {r0-r7,r14}
      ldr      r0, [r12]               // Get Mailbox data 

      BL 	   tube_io_handler
      ldr	   r10,=tube_irq
      pop      {r0-r7,r14}
      ldr	   r10,[r10]
      tst      r10, #FAST6502_BIT      // see if we should signal the 6502 Core 
      tstne	   r10,#RESET_BIT+NMI_BIT+IRQ_BIT
      orrne	   instrREG, instrREG, #1024
        
       
      subs    pc, lr, #4
        
