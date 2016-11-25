#include "tube-defs.h"

.global copro_armnative_enable_mailbox
.global copro_armnative_disable_mailbox
.global copro_armnative_fiq_handler
.global copro_armnative_swi_handler

.global _user_exec
.global _error_handler_wrapper
.global _escape_handler_wrapper
.global _exit_handler_wrapper

.equ    CPSR_MODE_USER,         0x10
.equ    CPSR_FIQ_INHIBIT,       0x40
.equ    CPSR_IRQ_INHIBIT,       0x80

copro_armnative_enable_mailbox:
        mov     r0, #MBOX0_DATAIRQEN    // if the slow flag is not set, then enable mailbox hardware
        ldr     r1, =MBOX0_CONFIG       // to generate interrupts when data is present
        str     r0, [r1]
        mov     r0, #0x80 + 65          // route that to FIQ
        ldr     r1, =FIQCTRL
        str     r0, [r1]        
        mov     pc, lr


copro_armnative_disable_mailbox:
        mov     r0, #0                  // disable mailbox hardware to generate interrupts
        ldr     r1, =MBOX0_CONFIG       // when data is present
        str     r0, [r1]
        mov     r0, #0                  // disable FIQ
        ldr     r1, =FIQCTRL
        str     r0, [r1]
        mov     pc, lr

#ifdef USE_REENTRANT_FIQ_HANDLER
        
copro_armnative_fiq_handler:

        push    {r0-r12, lr}           // Save everything that is not shadowed

        ldr     r1, =MBOX0_READ        // Read the GPU mailbox
        ldr     r0, [r1]
        lsr     r0, r0, #4             // Shift so it looks like the format of the software mailbox
        
        bl      tube_io_handler        // Update the Tube ULA emulation

        tst     r0, #7                 // Test for IRQ, NMI or RST
        beq     no_tube_intr
        
        ldr     r1, =in_tube_intr      // Get the address of the in_tube_intr flag
        ldr     r0, [r1]
        tst     r0, #1                 // Test if we are in tube_interrput_handler          
        bne     no_tube_intr           // Yes, so don't call it again
        
        mrs     r0, spsr               // Get the saved psr
        push    {r0,r1}                // Store spsr onto stack so we are re-entrant
        
        mov     r0, #1            
        str     r0, [r1]               // Set the in_tube_intr flag        
                                     
        mrs     r0, CPSR               // Re-enable ARM FIQ interrupts
        bic     r0, #CPSR_FIQ
        msr     CPSR_c, r0

        bl      copro_armnative_tube_interrupt_handler
        
        mrs     r0, CPSR               // Disable ARM FIQ interrupts
        orr     r0, #CPSR_FIQ
        msr     CPSR_c, r0

        ldr     r1, =in_tube_intr      // Get the address of the in_tube_intr flag
        mov     r0, #0            
        str     r0, [r1]               // Clear the in_tube_intr flag        
        
        pop     {r0, r1}               // Get spsr from stack.
        msr     spsr_cf, r0            // Restore spsr.
        
no_tube_intr:
        
        pop     {r0-r12, lr}
        subs    pc, lr, #4             // Return from ISR

in_tube_intr:
        .word   0
        
#else

copro_armnative_fiq_handler:

        push    {r0-r7, lr}            // Save everything that is not shadowed
        
        ldr     r1, =MBOX0_READ        // Read the GPU mailbox
        ldr     r0, [r1]
        lsr     r0, r0, #4             // Shift so it looks like the format of the software mailbox

        push    {r0}
        bl      tube_io_handler        // Update the Tube ULA emulation
        pop     {r0}
        
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

    mov      r0, #0                  // New code may have just been written to memory, so
    mcr      p15, 0, r0, c7, c10, 0  // Clean entire data cache
        
    mrs      r0, cpsr
    bic      r0, r0, #0x1f
    orr      r0, r0, #CPSR_MODE_USER
    msr      cpsr_c, r0           // Change to user mode

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
    mov      r4, #(CPSR_MODE_USER | CPSR_IRQ_INHIBIT )
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
    mov      r4, #(CPSR_MODE_USER | CPSR_IRQ_INHIBIT )
    msr      cpsr_c, r4

    // Continue execution of the exit handler
    mov      pc, r1
