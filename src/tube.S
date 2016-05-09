#include "rpi-base.h"
#include "tube-defs.h"

.text
.global tube_regs
.global arm_irq_handler
.global arm_firq_handler
.global tcm_code_start
.global tcm_code_end
        
// TODO list
//        - need C code for write handling
//        - adding overrun
//        - need to handle NRST - as an interrupt source - optional!
//        - very optional, make 8 data words be in expanded format

// ARM IRQ handler

// Align start each instruction on a 64-byte boundary
.equ    I_ALIGN,     7


tcm_code_start:
        
arm_firq_handler:
        subs    pc, lr, #4
        
arm_irq_handler:
        
        push    {r0-r3}
        
        ldr     r0, =GPEDS0
        mov     r1, #-1
        str     r1, [r0]                // clear all Pin events

        ldr     r0, =GPLEV0
        ldr     r1, [r0]                // *** very expensive but necessary ***
        
        tst     r1, #NRST_MASK          // test for reset
        beq     post_mail

        tst     r1, #NTUBE_MASK         // test for glitches
        bne     exit

        tst     r1, #RNW_MASK
        beq     wr_cycle

// READ_CYCLE
        mov     r2, #0
        tst     r1, #A0_MASK
        orrne   r2, #1
        tst     r1, #A1_MASK
        orrne   r2, #2
        tst     r1, #A2_MASK
        orrne   r2, #4

        ldr     r0, =tube_regs
        ldrb    r2, [r0, r2]

        mov     r3, r2, LSL #D0_BASE // low nibble now aligned to GPIO
        and     r3, r3, #D30_MASK
        mov     r2, r2, LSL #D4_BASE-4 
        and     r2, r2, #D74_MASK // high nibble in the right place
        orr     r2, r2, r3
        
        ldr     r0, =GPSET0
        str     r2, [r0]                // *** expensive but necessary ***
        eor     r2, r2, #D30_MASK
        eor     r2, r2, #D74_MASK
        ldr     r0, =GPCLR0
        str     r2, [r0]                // *** expensive but necessary ***
        
        ldr     r0, =GPFSEL0
        ldr     r3, =(gpfsel_data_driving)
        ldr     r1, [r3]
        str     r1, [r0]                // *** expensive but necessary ***
        ldr     r1, [r3, #4]
        str     r1, [r0, #4]            // *** expensive but necessary ***
        ldr     r1, [r3, #8]
        str     r1, [r0, #8]            // *** expensive but necessary ***

        ldr     r0, =GPLEV0
rd_wait_for_phi2_low:
        ldr     r1, [r0]                // ** very expensive but necessary ***     
        tst     r1, #PHI2_MASK
        bne     rd_wait_for_phi2_low

        ldr     r0, =GPFSEL0
        ldr     r3, =(gpfsel_data_idle)
        ldr     r2, [r3]
        str     r2, [r0]                // *** expensive but necessary ***
        ldr     r2, [r3, #4]
        str     r2, [r0, #4]            // *** expensive but necessary ***
        ldr     r2, [r3, #8]
        str     r2, [r0, #8]            // *** expensive but necessary ***

        tst     r1, #A0_MASK            // don't bother emulator with status reads
        bne     post_mail
        b       exit

// WRITE_CYCLE
wr_cycle:
         
wr_wait_for_phi2_high:
        ldr     r1, [r0]        
        tst     r1, #PHI2_MASK
        beq     wr_wait_for_phi2_high

wr_wait_for_phi2_low:
        ldr     r2, [r0]        
        tst     r2, #PHI2_MASK
        movne   r1, r2
        bne     wr_wait_for_phi2_low

post_mail:
        ldr     r2, =PINS_MASK
        and     r1, r1, r2
        orr     r1, r1, #ATTN_MASK
        ldr     r0, =events             // was tube_mailbox
        str     r1, [r0]

        orr     ip, ip, #256<<I_ALIGN   // signal event to 6502 instruction flow

exit:
        pop     {r0-r3}
        subs    pc, lr, #4

// 8 BYTES OF STATE
tube_regs:
        .byte 0xfe
        .byte 0xed
        .byte 0xdc
        .byte 0xcb
        .byte 0xba
        .byte 0xa9
        .byte 0x98
        .byte 0x87

tcm_code_end:
