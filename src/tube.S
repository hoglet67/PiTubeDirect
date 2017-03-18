#include "rpi-base.h"
#include "tube-defs.h"
     
#define instrREG r7
     
.text

.global arm_irq_handler
.global arm_fiq_handler

.global arm_fiq_handler_flag1

// =================================================
// ISR CODE
// =================================================
CACHELINE_ALIGN = 5

.align CACHELINE_ALIGN
isr_code_start:

// Default handlers for FIQ/IRQ do nothing

arm_fiq_handler:
arm_irq_handler:
        subs    pc, lr, #4

// ARM FIQ handler 
arm_fiq_handler_flag1:

        // Disable the FIQ interrupt (the C code will re-enable)
        // This is necessary because we don't want the FIQ handler to have to read
        // the mailbox, so the FIQ condition will not be cleared until later
        ldr     r8, =FIQCTRL
        mov     r9, #0
        str     r9, [r8]
        
        orr     instrREG, instrREG, #1024   // signal event to 6502 instruction flow

        subs    pc, lr, #4
        
