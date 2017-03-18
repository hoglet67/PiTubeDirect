#include "rpi-base.h"
#include "tube-defs.h"
     
#define instrREG r7
     
.text
.global tube_regs_block
.global tube_mailbox_block
.global arm_irq_handler
.global arm_fiq_handler

.global arm_fiq_handler_flag1
     

.global gpfsel_data_idle
.global gpfsel_data_driving


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


// =================================================
// ISR DATA
// =================================================

.align CACHELINE_ALIGN
isr_data_start:

tube_regs_block:
        .word (0xf << D4_BASE) | (0xe << D0_BASE)
        .word (0xe << D4_BASE) | (0xd << D0_BASE)
        .word (0xd << D4_BASE) | (0xc << D0_BASE)
        .word (0xc << D4_BASE) | (0xb << D0_BASE)
        .word (0xb << D4_BASE) | (0xa << D0_BASE)
        .word (0xa << D4_BASE) | (0x9 << D0_BASE)
        .word (0x9 << D4_BASE) | (0x8 << D0_BASE)
        .word (0x8 << D4_BASE) | (0x7 << D0_BASE)

.align CACHELINE_ALIGN        
// Some constants for controlling the data bus        
gpfsel_data_idle:
        .word 0
        .word 0
        .word 0

gpfsel_data_driving:
        .word 0
        .word 0
        .word 0
        
// Mailbox between tube isr and events handler (in C)
tube_mailbox_block:
        .word 0
 // Insert the current literal pool, so these get locked in the cache
.ltorg
        
