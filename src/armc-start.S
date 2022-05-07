
//  Part of the Raspberry-Pi Bare Metal Tutorials
//  Copyright (c) 2013-2015, Brian Sidebotham
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.

//  Relocate to 228MB

.equ    STACK_SIZE,           0x00080000

.equ    C0_SVR_STACK,        0
.equ    C0_IRQ_STACK,        (STACK_SIZE*1)
.equ    C0_FIQ_STACK,        STACK_SIZE*2
.equ    C0_USER_STACK,       STACK_SIZE*4
.equ    C0_ABORT_STACK,      STACK_SIZE*5
.equ    C0_UNDEFINED_STACK,  STACK_SIZE*6

#if defined(RPI2) || defined(RPI3) || defined(RPI4)
.equ    C1_SVR_STACK,        STACK_SIZE*7
.equ    C1_IRQ_STACK,        STACK_SIZE*8
.equ    C1_FIQ_STACK,        STACK_SIZE*9
.equ    C1_USER_STACK,       STACK_SIZE*10
.equ    C1_ABORT_STACK,      STACK_SIZE*11
.equ    C1_UNDEFINED_STACK,  STACK_SIZE*12
#endif

.equ    SCTLR_ENABLE_DATA_CACHE,        0x4
.equ    SCTLR_ENABLE_BRANCH_PREDICTION, 0x800
.equ    SCTLR_ENABLE_INSTRUCTION_CACHE, 0x1000

.section ".text.startup"

.global _start
.global __bss_start__
.global __bss_end__
.global _get_cpsr
.global _get_stack_pointer
.global _exception_table
.global _enable_interrupts
.global _set_interrupts
.global _disable_interrupts
.global _enable_unaligned_access
.global _enable_l1_cache
.global _invalidate_icache
.global _invalidate_dcache
.global _invalidate_tlb_mva
.global _data_memory_barrier
.global _data_synchronization_barrier

.global _software_interrupt_vector_h
.global _interrupt_vector_h
.global _fast_interrupt_vector_h

#if defined(RPI2) || defined(RPI3) || defined(RPI4)
.global _get_core
.global _init_core
.global _spin_core
#endif

.global _fast_scroll
.global _fast_clear
.global _main_irq_handler
.global _main_irq_r9_regsp

// From the ARM ARM (Architecture Reference Manual). Make sure you get the
// ARMv5 documentation which includes the ARMv6 documentation which is the
// correct processor type for the Broadcom BCM2835. The ARMv6-M manuals
// available on the ARM website are for Cortex-M parts only and are very
// different.
//
// See ARM section A2.2 (Processor Modes)

.equ    CPSR_MODE_USER,         0x10
.equ    CPSR_MODE_FIQ,          0x11
.equ    CPSR_MODE_IRQ,          0x12
.equ    CPSR_MODE_SVR,          0x13
.equ    CPSR_MODE_ABORT,        0x17
.equ    CPSR_MODE_HYP,          0x1A
.equ    CPSR_MODE_UNDEFINED,    0x1B
.equ    CPSR_MODE_SYSTEM,       0x1F

.equ    CPSR_MODE_MASK,         0x1F

// See ARM section A2.5 (Program status registers)
.equ    CPSR_A_BIT,             0x100
.equ    CPSR_IRQ_INHIBIT,       0x80
.equ    CPSR_FIQ_INHIBIT,       0x40
.equ    CPSR_THUMB,             0x20

_start:
    ldr pc, _reset_h
    ldr pc, _undefined_instruction_vector_h
    ldr pc, _software_interrupt_vector_h
    ldr pc, _prefetch_abort_vector_h
    ldr pc, _data_abort_vector_h
    ldr pc, _unused_handler_h
    ldr pc, _interrupt_vector_h
    ldr pc, _fast_interrupt_vector_h

_reset_h:                           .word   _reset_
_undefined_instruction_vector_h:    .word   _undefined_instruction_handler_
_software_interrupt_vector_h:       .word   _swi_handler_
_prefetch_abort_vector_h:           .word   _prefetch_abort_handler_
_data_abort_vector_h:               .word   _data_abort_handler_
_unused_handler_h:                  .word   _reset_
_interrupt_vector_h:                .word   arm_irq_handler
_fast_interrupt_vector_h:           .word   arm_fiq_handler

.section ".text._reset_"
_reset_:
#if defined(RPI4)
	ldr	    r4,=0xff842000
    mov     r5,#0
	str     r5,[r4]		// disable GIC on rpi4
#endif
    BL _enable_l1_cache
#if defined(RPI2) || defined(RPI3) || defined(RPI4)

    //1. Set the CPACR for access to CP10 and CP11, and clear the ASEDIS and D32DIS bits:
    ldr     r4, =(0xf << 20)
    mcr     p15, 0, r4, c1, c0, 2

    // 2. Set the FPEXC EN bit to enable the NEON MPE:
    mov     r4, #0x40000000
    vmsr    fpexc, r4

    // if kernel_old=0 enter in HYP mode and need to force a switch to SVC mode
    //
    // for now we assume kernel_old=1 and don't execute this core
    //
    // The logs show:
    // SVC mode: cpsr ends with 1d3
    // HYP mode: cpsr ends with 1a3
    mrs     r4, cpsr
    eor     r4, r4, #CPSR_MODE_HYP
    tst     r4, #CPSR_MODE_MASK
    bic     r4 , r4 , #CPSR_MODE_MASK
    orr     r4 , r4 , #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT | CPSR_MODE_SVR
    bne     _not_in_hyp_mode
    orr     r4, r4, #CPSR_A_BIT
    adr     lr, _reset_continue
    msr     spsr_cxsf, r4
    .word 0xE12EF30E  // msr_elr_hyp lr
    .word 0xE160006E  // eret
_not_in_hyp_mode:
    msr    cpsr_c, r4
_reset_continue:

#endif

    // We enter execution in supervisor mode. For more information on
    // processor modes see ARM Section A2.2 (Processor Modes)


    // move the vector pointer to the beginning of the code
    ldr     r4, =_start
    mcr     p15, 0, r4, c12, c0, 0

    // Initialise Stack Pointers ---------------------------------------------

    // We're going to use interrupt mode, so setup the interrupt mode
    // stack pointer which differs to the application stack pointer:
    msr cpsr_c, #(CPSR_MODE_IRQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_IRQ_STACK

    // Also setup the stack used for FIQs

    msr cpsr_c, #(CPSR_MODE_FIQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_FIQ_STACK

    // Also setup the stack used for undefined exceptions
    msr cpsr_c, #(CPSR_MODE_UNDEFINED | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_UNDEFINED_STACK

    // Also setup the stack used for prefetch and data abort exceptions
    msr cpsr_c, #(CPSR_MODE_ABORT | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_ABORT_STACK

    // Finally, a user/system mode stack, although the application will likely reset this
    msr cpsr_c, #(CPSR_MODE_SYSTEM | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_USER_STACK

    // Switch back to supervisor mode (our application mode) and
    // set the stack pointer. Remember that the stack works its way
    // down memory, our heap will work it's way up from after the
    // application.
    msr cpsr_c, #(CPSR_MODE_SVR | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, #C0_SVR_STACK

    // Enable VFP ------------------------------------------------------------

#if defined(RPI2) || defined(RPI3) || defined(RPI4)


#else
    // r1 = Access Control Register
    MRC p15, #0, r4, c1, c0, #2
    // enable full access for p10,11
    ORR r4, r4, #(0xf << 20)
    // ccess Control Register = r1
    MCR p15, #0, r4, c1, c0, #2
    MOV r4, #0
    // flush prefetch buffer because of FMXR below
    MCR p15, #0, r4, c7, c5, #4
    // and CP 10 & 11 were only just enabled
    // Enable VFP itself
    MOV r4,#0x40000000
    // FPEXC = r0
    FMXR FPEXC, r4
#endif

    // The c-startup function which we never return from. This function will
    // initialise the ro data section (most things that have the const
    // declaration) and initialise the bss section variables to 0 (generally
    // known as automatics). It'll then call main

    // clear bss section here using fast clear otherwise C will use 
    // memset which uses byte writes
    // As _fast_clear round lengths down to 64 byte boundaries we need to
    // round up. It doesn't matter if we go too far as we just zero part of the heap. 
    push    {r0-r3}
    ldr     r0,=__bss_start__
    ldr     r2,=__bss_end__+64
    mov     r1,#0
    sub     r2,r2,r0

    BL      _fast_clear
    pop     {r0-r3}
    b       kernel_main


.section ".text._get_stack_pointer"
_get_stack_pointer:
    mov     r0, sp
    bx      lr

.section ".text._get_cpsr"
_get_cpsr:
    mrs     r0, cpsr
    bx      lr

.section ".text._set_interrupts"
_set_interrupts:
    and     r0, r0, #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT    // extract the IRQ/FIQ bits from the value of cpsr passed in
    mrs     r1, cpsr
    bic     r1, r1, #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT
    orr     r1, r1, r0
    msr     cpsr_c, r1
    bx      lr

.section ".text._enable_interrupts"
_enable_interrupts:
    mrs     r0, cpsr
    bic     r0, r0, #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT
    msr     cpsr_c, r0
    bx      lr

.section ".text._disable_interrupts"
_disable_interrupts:
    mrs     r0, cpsr
    orr     r1, r0, #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT
    msr     cpsr_c, r1
    bx      lr

.section ".text._undefined_instruction_handler_"
_undefined_instruction_handler_:
    stmfd    sp!, {r0-r12, lr}
    mrs      r0, spsr             // Get spsr.
    stmfd    sp!, {r0}            // Store spsr onto stack.
    mov      r0, sp
    bl       undefined_instruction_handler

.section ".text._prefetch_abort_handler_"
_prefetch_abort_handler_:
    stmfd    sp!, {r0-r12, lr}
    mrs      r0, spsr             // Get spsr.
    stmfd    sp!, {r0}            // Store spsr onto stack.
    mov      r0, sp
    bl       prefetch_abort_handler

.section ".text._data_abort_handler_"
_data_abort_handler_:
    stmfd    sp!, {r0-r12, lr}
    mrs      r0, spsr             // Get spsr.
    stmfd    sp!, {r0}            // Store spsr onto stack.
    mov      r0, sp
    bl       data_abort_handler

.section ".text._swi_handler_"
_swi_handler_:
    stmfd    sp!, {r0-r12, lr}
    mrs      r0, spsr             // Get spsr.
    stmfd    sp!, {r0}            // Store spsr onto stack.
    mov      r0, sp
    bl       swi_handler

.section ".text._enable_unaligned_access"
_enable_unaligned_access:
    mrc      p15, 0, r0, c1, c0, 0   // read SCTLR
    bic      r0, r0, #2              // A (no unaligned access fault)
    orr      r0, r0, #1 << 22        // U (v6 unaligned access model)
    mcr      p15, 0, r0, c1, c0, 0   // write SCTLR
    bx       lr

    // Enable L1 Cache -------------------------------------------------------
.section ".text._enable_l1_cache"
_enable_l1_cache:

    // R0 = System Control Register
    mrc p15,0,r4,c1,c0,0

    // Enable caches and branch prediction
    orr r4,#SCTLR_ENABLE_BRANCH_PREDICTION
    orr r4,#SCTLR_ENABLE_DATA_CACHE
    orr r4,#SCTLR_ENABLE_INSTRUCTION_CACHE

    // System Control Register = R0
    mcr p15,0,r4,c1,c0,0

    bx      lr

.section ".text._invalidate_icache"
_invalidate_icache:
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 0
    bx      lr

.section ".text._invalidate_dcache"
_invalidate_dcache:
    mov     r0, #0
    mcr     p15, 0, r0, c7, c6, 0
    bx      lr

.section ".text._invalidate_tlb_mva"
_invalidate_tlb_mva:
#if defined(RPI2) || defined(RPI3) || defined(RPI4)
    dsb
#endif

    mcr     p15, 0, r0, c8, c7, 1  // invalidate tlb

#if defined(RPI2) || defined(RPI3) || defined(RPI4)
    MCR p15, 0, r0, c7, c5, 6 //; invalidate all of the BTB (BPIALL)
    dsb
    isb
#endif
    bx      lr

.section ".text._data_memory_barrier"
_data_memory_barrier:
#if defined(RPI2) || defined(RPI3) || defined(RPI4)
    dmb
#else
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 5 // DMB
#endif
    bx      lr

.section ".text._data_synchronization_barrier"
_data_synchronization_barrier:
#if defined(RPI2) || defined(RPI3) || defined(RPI4)
    dsb
#else
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4
#endif
    bx      lr

#ifdef USE_MULTICORE
+.section ".text._init_core"
_init_core:
    // On a Raspberry Pi 2 we enter in HYP mode, and need to force a switch to supervisor mode
    mrs     r0, cpsr
    eor     r0, r0, #CPSR_MODE_HYP
    tst     r0, #CPSR_MODE_MASK
    bic     r0 , r0 , #CPSR_MODE_MASK
    orr     r0 , r0 , #CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT | CPSR_MODE_SVR
    bne     _init_not_in_hyp_mode
    orr     r0, r0, #CPSR_A_BIT
    adr     lr, _init_continue
    msr     spsr_cxsf, r0
    .word 0xE12EF30E  // msr_elr_hyp lr
    .word 0xE160006E  // eret
_init_not_in_hyp_mode:
    msr    cpsr_c, r0

_init_continue:
    ldr  r4,=_start
    // Initialise Stack Pointers ---------------------------------------------

    // We're going to use interrupt mode, so setup the interrupt mode
    // stack pointer which differs to the application stack pointer:
    msr cpsr_c, #(CPSR_MODE_IRQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_IRQ_STACK

    // Also setup the stack used for FIQs
    msr cpsr_c, #(CPSR_MODE_FIQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_FIQ_STACK

    // Also setup the stack used for undefined exceptions
    msr cpsr_c, #(CPSR_MODE_UNDEFINED | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_UNDEFINED_STACK

    // Also setup the stack used for prefetch and data abort exceptions
    msr cpsr_c, #(CPSR_MODE_ABORT | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_ABORT_STACK

    // Finally, a user/system mode stack, although the application will likely reset this
    msr cpsr_c, #(CPSR_MODE_SYSTEM | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_USER_STACK

    // Switch back to supervisor mode (our application mode) and
    // set the stack pointer. Remember that the stack works its way
    // down memory, our heap will work it's way up from after the
    // application.
    msr cpsr_c, #(CPSR_MODE_SVR | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT )
    sub sp, r4, # C1_SVR_STACK

    // Enable VFP ------------------------------------------------------------

    //1. Set the CPACR for access to CP10 and CP11, and clear the ASEDIS and D32DIS bits:
    ldr     r0, =(0xf << 20)
    mcr     p15, 0, r0, c1, c0, 2

    // 2. Set the FPEXC EN bit to enable the NEON MPE:
    mov     r0, #0x40000000
    vmsr    fpexc, r0

    bl      run_core
#endif

#if defined(RPI2) || defined(RPI3) || defined(RPI4)
.section ".text._spin_core"
    // If main does return for some reason, just catch it and stay here.
_spin_core:
#ifdef DEBUG_Multicore
    mov     r0, #'S'
    bl      RPI_AuxMiniUartWrite
    mov     r0, #'P'
    bl      RPI_AuxMiniUartWrite
    mov     r0, #'I'
    bl      RPI_AuxMiniUartWrite
    mov     r0, #'N'
    bl      RPI_AuxMiniUartWrite
    bl     _get_core
    add     r0, r0, #'0'
    bl      RPI_AuxMiniUartWrite
    mov     r0, #'\r'
    bl      RPI_AuxMiniUartWrite
    mov     r0, #'\n'
    bl      RPI_AuxMiniUartWrite
#endif
_spin_core1:
    wfi
    b       _spin_core1

.section ".text._get_core"
_get_core:
    mrc     p15, 0, r0, c0, c0, 5
    and     r0, #3
    bx      lr

#endif

.section ".text._fast_scroll"
// R0 = dst
// R1 = src
// R2 = number of bytes, must be a multiple of 64
// Note: R7 reserved by Fast 6502 FIQ handler, so avoid
_fast_scroll:
        push    {r4, r5, r6, r8, r9, r10, r11, lr}
        bic     r2, r2, #0x4F
_fast_scroll_loop:
        ldmia   r1!,{r3, r4, r5, r6, r8, r9, r10, r11}
        subs    r2, r2, #64
        stmia   r0!,{r3, r4, r5, r6, r8, r9, r10, r11}
        ldmia   r1!,{r3, r4, r5, r6, r8, r9, r10, r11}
        stmia   r0!,{r3, r4, r5, r6, r8, r9, r10, r11}
        bne     _fast_scroll_loop
        pop     {r4, r5, r6, r8, r9, r10, r11, pc}

.section ".text._fast_clear"
// R0 = dst
// R1 = val
// R2 = number of bytes, must be a multiple of 64
// Note: R7 reserved by Fast 6502 FIQ handler, so avoid
_fast_clear:
        push    {r4, r5, r6, r8, r9, r10, r11, lr}
        mov     r3, r1
        mov     r4, r1
        mov     r5, r1
        mov     r6, r1
        mov     r8, r1
        mov     r9, r1
        mov     r10, r1
        mov     r11, r1
        bic     r2, r2, #0x3F
_fast_clear_loop:
        stmia   r0!,{r3, r4, r5, r6, r8, r9, r10, r11}
        subs    r2, r2, #64
        stmia   r0!,{r3, r4, r5, r6, r8, r9, r10, r11}
        bne     _fast_clear_loop
        pop     {r4, r5, r6, r8, r9, r10, r11, pc}

.section ".text._main_irq_handler"
_main_irq_handler:
        // Save a copy of the Jit6502 regsp (r9) for use by the fiq handler
        // when it runs a 6502 IRQ/NMI handler. This pattern works because the
        // ARM IRQ handler is NOT re-entrant.
        str     r9, _main_irq_r9_regsp
        sub     lr, lr, #4
        push    {r0, r1, r2, r3, ip, lr}
        // ARM ABI says that Callee must preserve r4-r8, r10, r11 and SP
        // r9 is platform specific, but GCC seems to callee save that
        bl      RPI_AuxMiniUartIRQHandler
        // Mark the copy Jit6502 regsp (r9) as invalid
        // This is safe to do, as r9 now holds the real regsp again
        mov     r0, #0
        str     r0, _main_irq_r9_regsp
        ldm     sp!, {r0, r1, r2, r3, ip, pc}^

_main_irq_r9_regsp:
        .word 0
