#include "rpi-base.h"
#include "tube-defs.h"
#include "cache.h"

// Align start each instruction on a 64-byte boundary
.equ    I_ALIGN,     7

.text
.global tube_regs
.global arm_irq_handler
.global arm_firq_handler
.global lock_isr

// =================================================
// LOCK the ISR CODE into cache
// =================================================

lock_isr:
    push    {lr}

    // Copy locking code into un-cached memory
    ldr     r0, =lock_isr_into_icache_start
    ldr     r1, =UNCACHED_MEM_BASE
    mov     r2, #(lock_isr_into_icache_end - lock_isr_into_icache_start)
copy_loop:
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    subs    r2, r2, #4
    bne     copy_loop

    // disable interrupts (so handler doesn't get accidentally executed
    bl      _disable_interrupts
           
    // execute locking code        
    ldr     r0, =UNCACHED_MEM_BASE
    blx     r0

    // enable interrupts
    bl      _enable_interrupts

    pop     {lr}
    mov     pc, lr
        
// This code must only be executed from uncachable memory
// and must be position independant

.align I_ALIGN
lock_isr_into_icache_start:

    // invalidate entire instruction cache (page 3-74)
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 0

    // enable i-cache way 0, lock ways 1, 2, 3 (page 3-89)
    mov     r0, #0xfffffffe
    mcr     p15, 0, r0, c9, c0, 1

    // bring the isr code into cache
    // these loads are relative references, so should relocate
    ldr     r0, isr_code_start_ptr
    ldr     r1, isr_code_end_ptr
lock_loop:
    // prefetch cache line into instruction cache (page 3-76)
    // r0 is the virtual address, with bits 4..0 set to zero
    mcr     p15, 0, r0, c7, c13, 1
    // move to the next cache line
    add     r0, r0, #32
    cmp     r0, r1
    bne     lock_loop
         
    // lock i-cache way 0, enable ways 1, 2, 3 (page 3-89)
    mov     r0, #0xfffffff1
    mcr     p15, 0, r0, c9, c0, 1 

    mov     pc, lr

isr_code_start_ptr:
.word isr_code_start

isr_code_end_ptr:
.word isr_code_end
        
.align I_ALIGN
lock_isr_into_icache_end:
        
// =================================================
// ISR CODE
// =================================================

.align I_ALIGN
isr_code_start:
        

// TODO list
//        - need C code for write handling
//        - adding overrun
//        - need to handle NRST - as an interrupt source - optional!
//        - very optional, make 8 data words be in expanded format

// ARM IRQ handler


        
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


.align I_ALIGN
isr_code_end:

// =================================================
// ISR DATA
// =================================================
        
.align I_ALIGN
isr_data_start:
        
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

.align I_ALIGN
isr_data_end:

        
