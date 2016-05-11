#include "rpi-base.h"
#include "tube-defs.h"
#include "cache.h"

// Align start each instruction on a 64-byte boundary
.equ    I_ALIGN,     7

.text
.global tube_regs
.global arm_irq_handler
.global arm_fiq_handler
.global lock_isr
.global gpfsel_data_idle
.global gpfsel_data_driving

// =================================================
// LOCK the ISR CODE into cache
// =================================================

lock_isr:
    push    {lr}

    // Copy locking code into un-cached memory
    ldr     r0, =lock_isr_into_cache_start
    ldr     r1, =UNCACHED_MEM_BASE
    mov     r2, #(lock_isr_into_cache_end - lock_isr_into_cache_start)
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

// TODO: Check whether any of the different regions will "alias"
        
.align I_ALIGN
lock_isr_into_cache_start:

    // invalidate entire instruction cache (page 3-74)
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 0

    // clean and invalidate the entire data cache (page 3-74)
    mov      r0, #0
    mcr      p15, 0, r0, c7, c14, 0

    // enable i-cache/d-cache way 0, lock ways 1, 2, 3 (page 3-89)
    mov     r0, #0xfffffffe
    mcr     p15, 0, r0, c9, c0, 0 // d-cache
    mcr     p15, 0, r0, c9, c0, 1 // i-cache

    // bring the isr code into i-cache
    // these loads are relative references, so should relocate
    ldr     r0, isr_code_start_ptr
    ldr     r1, isr_code_end_ptr
lock_code_loop:
    // prefetch cache line into instruction cache (page 3-76)
    // r0 is the virtual address, with bits 4..0 set to zero
    mcr     p15, 0, r0, c7, c13, 1
    // move to the next cache line
    add     r0, r0, #32
    cmp     r0, r1
    bne     lock_code_loop

    // prefetch the vector instructions (8 words starting at 0x00000000) into the i-cache
    mov     r0, #0x00
    mcr     p15, 0, r0, c7, c13, 1

    // bring the isr data into d-cache
    // these loads are relative references, so should relocate
    ldr     r0, isr_data_start_ptr
    ldr     r1, isr_data_end_ptr
lock_data_loop:
    // load from the cache line to prefetch it
    ldr     r2, [r0]
    // move to the next cache line
    add     r0, r0, #32
    cmp     r0, r1
    bne     lock_data_loop

    // prefetch the vector data (8 words starting at 0x00000020) into the d-cache
    mov     r0, #0x20
    ldr     r2, [r0]

    // bring the isr stack into d-cache
    // these loads are relative references, so should relocate
    ldr     r0, isr_stack_start_ptr
    ldr     r1, isr_stack_end_ptr
lock_stack_loop:
    // load from the cache line to prefetch it
    ldr     r2, [r0]
    // move to the next cache line
    add     r0, r0, #32
    cmp     r0, r1
    bne     lock_stack_loop

    // lock i-cache/d-cache way 0, enable ways 1, 2, 3 (page 3-89)
    mov     r0, #0xfffffff1
    mcr     p15, 0, r0, c9, c0, 0 // d-cache
    mcr     p15, 0, r0, c9, c0, 1 // i-cache

    mov     pc, lr

isr_code_start_ptr:
.word isr_code_start

isr_code_end_ptr:
.word isr_code_end

isr_data_start_ptr:
.word isr_data_start

isr_data_end_ptr:
.word isr_data_end

// TODO these should be somehow imported from arm-startup.S
        
isr_stack_start_ptr:
.word 0x01EE0000 - 0x100

isr_stack_end_ptr:
.word 0x01EE0000


.align I_ALIGN
lock_isr_into_cache_end:

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

arm_irq_handler:
        subs    pc, lr, #4

// ARM FIQ handler

arm_fiq_handler:

        //r8-r11 are shadowed in FIQ mode, so no need to push
        //push    {r8-r11}
        
        ldr     r8, =GPEDS0
        mov     r9, #-1
        str     r9, [r8]                // clear all Pin events

        ldr     r8, =GPLEV0
        ldr     r9, [r8]                // *** very expensive but necessary ***

        tst     r9, #NRST_MASK          // test for reset
        beq     post_mail

        tst     r9, #NTUBE_MASK         // test for glitches
        bne     exit

        tst     r9, #RNW_MASK
        beq     wr_cycle

// READ_CYCLE
        mov     r10, #0
        tst     r9, #A0_MASK
        orrne   r10, #1
        tst     r9, #A1_MASK
        orrne   r10, #2
        tst     r9, #A2_MASK
        orrne   r10, #4

        ldr     r8, =tube_regs
        ldrb    r10, [r8, r10]

        mov     r11, r10, LSL #D0_BASE // low nibble now aligned to GPIO
        and     r11, r11, #D30_MASK
        mov     r10, r10, LSL #D4_BASE-4
        and     r10, r10, #D74_MASK // high nibble in the right place
        orr     r10, r10, r11

        ldr     r8, =GPSET0
        str     r10, [r8]                // *** expensive but necessary ***
        eor     r10, r10, #D30_MASK
        eor     r10, r10, #D74_MASK
        ldr     r8, =GPCLR0
        str     r10, [r8]                // *** expensive but necessary ***

        ldr     r8, =GPFSEL0
        ldr     r11, =(gpfsel_data_driving)
        ldr     r9, [r11]
        str     r9, [r8]                // *** expensive but necessary ***
        ldr     r9, [r11, #4]
        str     r9, [r8, #4]            // *** expensive but necessary ***
        ldr     r9, [r11, #8]
        str     r9, [r8, #8]            // *** expensive but necessary ***

        ldr     r8, =GPLEV0
rd_wait_for_phi2_low:
        ldr     r10, [r8]                // ** very expensive but necessary ***
        tst     r10, #PHI2_MASK
        movne   r9, r10
        bne     rd_wait_for_phi2_low

        ldr     r8, =GPFSEL0
        ldr     r11, =(gpfsel_data_idle)
        ldr     r10, [r11]
        str     r10, [r8]                // *** expensive but necessary ***
        ldr     r10, [r11, #4]
        str     r10, [r8, #4]            // *** expensive but necessary ***
        ldr     r10, [r11, #8]
        str     r10, [r8, #8]            // *** expensive but necessary ***

        tst     r9, #A0_MASK            // don't bother emulator with status reads
        bne     post_mail
        b       exit

// WRITE_CYCLE
wr_cycle:

wr_wait_for_phi2_high:
        ldr     r9, [r8]
        tst     r9, #PHI2_MASK
        beq     wr_wait_for_phi2_high

wr_wait_for_phi2_low:
        ldr     r10, [r8]
        tst     r10, #PHI2_MASK
        movne   r9, r10
        bne     wr_wait_for_phi2_low

post_mail:
        ldr     r10, =PINS_MASK
        and     r9, r9, r10
        orr     r9, r9, #ATTN_MASK
        ldr     r8, =events             // was tube_mailbox
        str     r9, [r8]

#if 0        
        // Switch back to irq mode so ip is not shadowed
        mrs     r8, cpsr
        bic     r8, r8, #0x1F
        orr     r8, r8, #0x12
        msr     cpsr_c, r8
        
        orr     ip, ip, #256<<I_ALIGN   // signal event to 6502 instruction flow

        // Switch back to fiq mode, so we return correctly
        // lr is used here because its shadowed in IRQ mode, so doesn't corrupt the normal lr
        mrs     lr, cpsr
        bic     lr, lr, #0x1F
        orr     lr, lr, #0x11
        msr     cpsr_c, lr
#endif
        
        // Note, the above mode switching could be avoided if we reworked the register assignment
        // in the 6502 emulator to use a register r0..r7 for signalling.
        // I did do this as an experiment, and it's parked in a branch.
        // I'd like to avoid making big changes to that code for now.
        
exit:
        //r8-r11 are shadowed in FIQ mode, so no need to pop
        //pop   {r8-r11}
        subs    pc, lr, #4


.align I_ALIGN
isr_code_end:

// =================================================
// ISR DATA
// =================================================

.align I_ALIGN
isr_data_start:

// Insert the current literal pool, so these get locked in the cache
.ltorg

// Some constants for controlling the data bus        
gpfsel_data_idle:
        .word 0
        .word 0
        .word 0

gpfsel_data_driving:
        .word 0
        .word 0
        .word 0
        
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
