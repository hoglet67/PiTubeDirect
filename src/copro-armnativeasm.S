#include "tube-defs.h"
#include "copro-armnative.h"
        
.global copro_armnative_enable_mailbox
.global copro_armnative_disable_mailbox
.global copro_armnative_fiq_handler
.global copro_armnative_swi_handler

.global _user_exec
.global _error_handler_wrapper
.global _escape_handler_wrapper
.global _exit_handler_wrapper
        
.equ    CPSR_MODE_USER,         0x10
.equ    CPSR_MODE_FIQ,          0x11
.equ    CPSR_MODE_SYSTEM,       0x1F
.equ    CPSR_FIQ_INHIBIT,       0x40
.equ    CPSR_IRQ_INHIBIT,       0x80

.equ    C_FLAG,      0x20000000

copro_armnative_enable_mailbox:
        mov     r0, #MBOX0_DATAIRQEN    // if the slow flag is not set, then enable mailbox hardware
        ldr     r1, =MBOX0_CONFIG       // to generate interrupts when data is present
        str     r0, [r1]
        mov     pc, lr


copro_armnative_disable_mailbox:
        mov     r0, #0                  // disable mailbox hardware to generate interrupts
        ldr     r1, =MBOX0_CONFIG       // when data is present
        str     r0, [r1]
        mov     pc, lr

#ifdef USE_REENTRANT_FIQ_HANDLER

.global in_tube_intr_flag
      
copro_armnative_fiq_handler:

        push    {r0-r8,lr}             // Save register state to FIQ stack

        ldr     r1, =MBOX0_READ        // Read the GPU mailbox
        ldr     r0, [r1]
        
        bl      tube_io_handler        // Update the Tube ULA emulation

        tst     r0, #4                 // Test for RST
        blne    copro_armnative_reset
        
        tst     r0, #3                 // Test for IRQ or NMI
        beq     no_tube_intr
        
        ldr     r1, =in_tube_intr_flag // Get the address of the in_tube_intr flag
        ldr     r0, [r1]
        tst     r0, #1                 // Test if we are in tube_interrput_handler          
        bne     no_tube_intr           // Yes, so don't call it again
        
        mov     r0, #1            
        str     r0, [r1]               // Set the in_tube_intr flag        
                                     
        mrs     r0, spsr               // Get the saved psr
        push    {r0, r1}               // Store spsr onto stack so we are re-entrant

                                       // Switch to system mode
        msr     CPSR_c, #(CPSR_MODE_SYSTEM | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT)
                                       // User mode registers now in play
        
        push    {r0-r3}                // Save non-callee saved registers
        and     r1, sp, #4             // Ensure stack is 8-byte aligned before calling C
        sub     sp, sp, r1        
        push    {r1, lr}               // push stack alignment offset and LR 
        
        mrs     r0, CPSR               // Re-enable ARM FIQ and IRQ interrupts
        bic     r0, #(CPSR_FIQ_INHIBIT | CPSR_IRQ_INHIBIT)
        msr     CPSR_c, r0

        bl      copro_armnative_tube_interrupt_handler
        
        mrs     r0, CPSR               // Disable ARM FIQ and IRQ interrupts
        orr     r0, #(CPSR_FIQ_INHIBIT | CPSR_IRQ_INHIBIT)
        msr     CPSR_c, r0
        
        pop     {r1, lr}               // Restore LR and stack aligment offset
        add     sp, sp, r1             // Undo stack alignment (if any)
        pop     {r0-r3}                // Restore non-callee saved registers and LR

                                       // Return to FIQ mode
        msr     CPSR_c, #(CPSR_MODE_FIQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT)
                                       // FIQ mode registers now in play
        
        pop     {r0, r1}               // Get spsr from stack.
        msr     spsr_cf, r0            // Restore spsr.

        ldr     r1, =in_tube_intr_flag // Get the address of the in_tube_intr flag
        mov     r0, #0            
        str     r0, [r1]               // Clear the in_tube_intr flag
        
no_tube_intr:
        
        pop     {r0-r8, lr}            // Restore register state from FIQ stack
        subs    pc, lr, #4             // Return from ISR

in_tube_intr_flag:
        .word   0
        
#else

copro_armnative_fiq_handler:

        push    {r0-r7, lr}            // Save everything that is not shadowed
        
        ldr     r1, =MBOX0_READ        // Read the GPU mailbox
        ldr     r0, [r1]
   
        push    {r0}
        bl      tube_io_handler        // Update the Tube ULA emulation
        and     r1, r0, #4             // Get Reset bit which migh have come from 
                                       // write to register 
        pop     {r0}
        orr     r0, r0, r1, LSL #12-2

        bl      copro_armnative_tube_interrupt_handler
        
        pop     {r0-r7, lr}
        subs    pc, lr, #4             // Return from ISR


#endif
        
copro_armnative_swi_handler:
    stmfd    sp!, {r0-r11,r12,lr} // Store registers.
    mov      r1, sp               // Set pointer to parameters.
    mrs      r0, spsr             // Get spsr.
    stmfd    sp!, {r0,r3}         // Store spsr onto stack and another register to maintain
                                  // 8-byte-aligned stack. This is only really needed in case of
                                  // nested SVCs.
    ldr      r0, [lr, #-4]        // Calculate address of SWI instruction and load it into r0.
    bic      r0, r0, #0xFF000000  // Mask off top 8 bits of instruction to give SWI number.
                                  // r0 now contains SWI number
                                  // r1 now contains pointer to stacked registers
    bl       C_SWI_Handler        // Call C routine to handle the SWI.
    ldmfd    sp!, {r0,r3}         // Get spsr from stack.
    msr      spsr_cf, r0          // Restore spsr.
    ldmfd    sp!, {r0-r11,r12,pc}^// Restore registers and return.

_user_exec:

    // This must only be called from supervisor mode....
        
    stmfd    sp!, {r0-r5, lr}     // r0 contains address of the code to execute in user mode
                                  // r1..r3 contain parameters, r4 is a scratch register
                                  // r5 is pushed to maintain 8 byte stack alignment

    mov      r4, r0               // Save r0 into the scratch register
        
    mrs      r0, spsr             // Get spsr    
    stmfd    sp!, {r0}            // Save spsr, as this (and lr) will be corrupted by nested swis

#if defined(RPI2) || defined(RPI3) || defined(RPI4)
    stmfd    sp!, {r0-r3}         // Save the _user_exec params
    bl       CleanDataCache
    ldmfd    sp!, {r0-r3}         // Restore the _user_exec params

    mov      r0, #0
    mcr      p15, 0, r0, c7, c5, 0   // Invalidate entire instruction cache
    isb
#else        
    mov      r0, #0                  // New code may have just been written to memory, so
    mcr      p15, 0, r0, c7, c10, 0  // Clean entire data cache

    mov      r0, #0
    mcr      p15, 0, r0, c7, c5, 0   // Invalidate entire instruction cache
    mcr      p15, 0, r0, c7, c5, 4   // Flush the prefetch buffer
 #endif       
    mrs      r0, cpsr
    bic      r0, r0, #0x1f
    orr      r0, r0, #CPSR_MODE_USER
    msr      cpsr_c, r0           // Change to user mode


    tst      r4, #1               // Bit 0 of the address is used to indicate the carry flag value
    mrs      r0, CPSR                          
    biceq    r0, r0, #C_FLAG      // Set/Clear the carry flag accordingly
    orrne    r0, r0, #C_FLAG
    msr      CPSR_flg, r0
        
    bic      r4, #1               // Now clear this bit, as we should always be jumping to a 32-bit aligned address
        
    mov      r0, r1               // Copy the params
    mov      r1, r2
    mov      r2, r3
    blx      r4                   // Call the address of the user program        
                                  // On return, the exit code is now in R0
    
    svc      0x000016             // Change back to supervisor mode (OS_EnterOS)

    str      r0, [sp, #4]         // Save the exit code so it will end up back in r0
        
    ldmfd    sp!, {r0}            // Get spsr from stack.
    msr      spsr_c, r0           // Restore spsr.
        
    ldmfd    sp!, {r0-r5, pc}     // Restore registers and return.
        
        
    // Called with the error buffer ppointer in R0 and the error handler address in R1
_error_handler_wrapper:

    // Move to user mode
    mov      r4, #CPSR_MODE_USER
    msr      cpsr_c, r4

    // Continue execution of the error handler
    mov      pc, r1

    // Called with the escape flag value in R0 and the escape handler address in R1
_escape_handler_wrapper:

    // Preserve the registers the escape handler will use
    stmfd    sp!, {r11-r12, lr}

    // Move the escape flag into R11
    mov      r11, r0

    // Call the escape handler
    blx      r1

    // Restore stacked resisters and return
    ldmfd    sp!, {r11-r12, lr}
    mov      pc, lr

    // Called with the R12 value in R0 and the exit handler address in R1
_exit_handler_wrapper:

    // Move the "R12 value" into R12
    mov      r12, r0

    // Move to user mode
    mov      r4, #CPSR_MODE_USER
    msr      cpsr_c, r4

    // Continue execution of the exit handler
    mov      pc, r1
