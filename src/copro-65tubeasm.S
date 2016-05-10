// This code was derived from Acorn's 65tube application
//
// The following changes have been made
// - ported from 26 bit ARM2 to 32 bit ARM
// - replaced the x3 trap instructions with appropriate length NOPs
// - implemented BBRn, BBSn, RMBn, SMBn

// Register usage
// r0       = scratch register
// r1       = scratch register
// r2       = scratch register (used to hold CPRS normally)
// r3       = a constant value 0f 0x0000FEF8 >> 3 (for tube register address checking)
// r4       = 6502 flags (only D and I bits maintained here, CVZN use ARM flags)
// r5       = base address of 64-byte aligned instruction implementations
// r6       = 6502 A
// r7       = 6502 X
// r8       = 6502 Y
// r9       = 6502 SP
// r10 (sl) = 6502 PC
// r11 (fp) = base address of 64KB of flat memory
// r12 (ip) = a constant value of 0xFFFFFF00
// r13 (sp) = normal ARM usage
// r14 (lr) = normal ARM usage
// r15 (pc) = normal ARM usage

// Interrupt / Event Handling code by dp11
// this code is not for profit

// We have three external events that we need to get into the 6502 instruction path
//
// RST GPIO07
// NMI GPIO18 (falling edge sensitive)
// IRQ GPIO17 level sensitive

// If we get a reset we should stay in the reset handler until the reset line goes high we can
// ignore all other events

// If we get an NMI falling edge we can service it  The hardware will set the signal high again

// The IRQ is the most difficult case as is may be low and interrupts may be disabled
// So when interrupts are next enabled we need explicitly check the IRQ line
// CLI PLP RTI for instance
// Then we need to fire off the IRQ handler again if IRQ is low

//
// at the end of each 6502 instruction we currently do :
// // FETCH_NEXT this jumps direct to the next instruction
//        ldrb    r0, [sl], #1
//        add     pc, r5, r0, lsl #I_ALIGN
//
// This gives us a nice hook into the instruction handler
// When the ARM IRQ detects a valid interrupt
// it adds a constant ( 256 << I_ALIGN) to r5 to move into a second instruction handler
// This instruction handler can just be 256 Branches to the Event handler
// this code is executed so rarely absolute performance isn.t an issue and we aren.t worried about it fitting in the cache


// A check needs to be that more than than one event isn.t being added. This can be
// simplified if we align the code to a 0 boundary and just orr r5,r5,#256<<I_ALIGN
//
// once we are in the event handler we do
// disable arm IRQs // this prevents us missing events
// bic r5,r5,#256<<I_ALIGN
// reenable IRQs // now if another event happens we will service it at the next instruction boundary
// sub sl,sl,#1 // set instruction pointer ready to be stacked
// check for reset ( if we have a reset we are happy to drop any other outstanding events
// Check for NMI ( if there is also an IRQ present is will get checked for at the RTI)
// Check for IRQs enabled ( R4)
// if so check if we have an IRQ

#include "rpi-base.h"
        
.text
.global events
.global exec_65tube
.global mpu_memory

.equ    N_FLAG,      0x80000000
.equ    Z_FLAG,      0x40000000
.equ    C_FLAG,      0x20000000

// Align start each instruction on a 64-byte boundary
.equ    I_ALIGN,     7


// Macros for fetching the next instruction, for one byte instructions
        
.macro FETCH_NEXT_STAGE_1_I
        add     lr, r5, r0, lsl #I_ALIGN
.endm

.macro FETCH_NEXT_STAGE_2_I
        ldrb    r0, [sl], #1
        mov     pc, lr
        // Insert the current literal pool, otherwise constants are to far away and you get a build error
        .ltorg
.endm
        
// Macros for fetching the next instruction, for two and three byte instructions

.macro FETCH_NEXT_STAGE_0
        ldrb    r2, [sl], #1
.endm

.macro FETCH_NEXT_STAGE_1
        ldrb    r0, [sl], #1
        add     lr, r5, r2, lsl #I_ALIGN
.endm
        
.macro FETCH_NEXT_STAGE_2
        mov     pc, lr
        // Insert the current literal pool, otherwise constants are to far away and you get a build error
        .ltorg
.endm

.macro FETCH_NEXT_STAGE_12
        add     lr, r5, r2, lsl #I_ALIGN
        ldrb    r0, [sl], #1
        mov     pc, lr
        // Insert the current literal pool, otherwise constants are to far away and you get a build error
        .ltorg
.endm

.macro FETCH_NEXT_MERGED
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2        
.endm
        
// Macro for fetching the next instruction and also checking for any pending IRQs
// Used for instructions that can enable interrupts: CLI, PLP, RTI
.macro FETCH_NEXT_CHECK_IRQ
        mrs     r2, CPSR                  // Preserve the 6502 flags
        tst     r4, #4                    // Test whether interrupts are enabled (bit 2 = 0)
        ldreq   r0, =(PERIPHERAL_BASE + 0x200034)
        ldreq   r0, [r0]
        tsteq   r0, #(1 << 17)            // Test for IRQ
        beq     handle_irq
        msr     CPSR_flg, r2              // Restore the 6502 flags
        FETCH_NEXT_MERGED
        // Insert the current literal pool, otherwise constants are to far away and you get a build error
        .ltorg
.endm

// Macro for pushing a byte to the stack, whilst correctly handing wrapping
// This relies on the 6502 memory being 64KB aligned
// Optimisations from dp11 on stardot
.macro SPUSH reg
        strb    \reg, [r9], #-1
        orr     r9, #0x0100
        // The original code was just this:
        // strb    \reg, [r9], #-1
.endm

// Macro for poping a byte from the stack, whilst correctly handing wrapping
// This relies on the 6502 memory being 64KB aligned
// Optimisations from dp11 on stardot
.macro SPOP reg
        add     r9, r9, #1
        bic     r9, #0xFF00
        ldrb    \reg, [r9,#0x0100]!
        // The original code was just this:
        // ldrb    \reg, [r9, #1]!
.endm

// Macro for tracing execution
// Add to FETCH_NEXT
.macro TRACE_65TUBE
        mov     r0, sl
        ldrb    r1, [sl]
        mrs     r2, CPSR
        push    {r2}
        orr     r2, r2, #0x80
        msr     CPSR_c, r2        
        push    {r3-r12}
        bl      copro_65tube_trace
        pop     {r3-r12}
        pop     {r2}
        msr     CPSR, r2
.endm

// Macro to maintain a histogram of opcodes in memory
// Add to FETCH_NEXT
.macro HISTOGRAM_65TUBE
        ldr     r1, =histogram_memory
        add     r1, r0, lsl #2
        ldr     r2, [r1]
        add     r2, #1
        str     r2, [r1]
.endm

// Macros for calculating effective address in r0        
.macro EA_ABSOLUTE
        ldrh    r0, [sl, #-1]
        add     sl, sl, #1
.endm

.macro EA_ABSOLUTE_INDEXED reg
        ldrh    r0, [sl, #-1]
        add     sl, sl, #1
        add     r0, r0, \reg, lsr #24
        bic     r0, r0, #0x10000
.endm

.macro EA_INDIRECT
        ldrh    r0, [r0, fp]
.endm

.macro EA_INDIRECT_X
        add     r0, r0, r7, lsr #24
        and     r0, r0, #0xff
        ldrh    r0, [r0, fp]
.endm

.macro EA_INDIRECT_Y
        ldrh    r0, [r0, fp]
        add     r0, r0, r8, lsr #24
        bic     r0, r0, #0x10000
.endm

// Macros for reading/writing memory
.macro LOAD_INDIRECT
        ldrb    r1, [fp, r0]
.endm

.macro STORE_INDIRECT
        strb    r1, [fp, r0]
.endm

.macro LOAD_ABSOLUTE               // r0 holds the absolute address
        mrs     r2, CPSR           // preserve the 6502 flags
        cmp     r3, r0, lsr #3     // r3 holds the constant 0xFEF8 >> 3
        ldrneb  r1, [fp, r0]       // normal memory read
        bleq    tube_load_handler  // tube memory access
        msr     CPSR, r2           // restore 6502 flags, including ARM interrupts
.endm

.macro STORE_ABSOLUTE              // r0 holds the absolute address
        mrs     r2, CPSR           // preserve the 6502 flags
        cmp     r3, r0, lsr #3     // r3 holds the constant 0xFEF8 >> 3
        strneb  r1, [fp, r0]       // normal memory write
        bleq    tube_store_handler // tube memory access
        msr     CPSR, r2           // restore 6502 flags, including ARM interrupts
.endm

// Macros for 1,2, anbd 3 byte NOPs
.macro NOP_1
        sub     sl, sl, #1
        FETCH_NEXT_MERGED
.endm

.macro NOP_2
        FETCH_NEXT_MERGED
.endm
        
.macro NOP_3
        add     sl, sl, #1
        FETCH_NEXT_MERGED
.endm

// Macros for additional Rockwell 65C02 and WDC 65C02 instructions
.macro BBR bitnum
        ldrb    r1, [fp, r0]
        ldrb    r0, [sl], #1
        mrs     r2, CPSR
        tst     r1, #(1 << \bitnum)
        lsleq   r0, r0, #24
        addeq   sl, sl, r0, asr #24
        msr     CPSR_flg, r2
        FETCH_NEXT_MERGED
.endm

.macro BBS bitnum
        ldrb    r1, [fp, r0]
        ldrb    r0, [sl], #1
        mrs     r2, CPSR
        tst     r1, #(1 << \bitnum)
        lslne   r0, r0, #24
        addne   sl, sl, r0, asr #24
        msr     CPSR_flg, r2
        FETCH_NEXT_MERGED
.endm

.macro RMB bitnum
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        bic     r1, #(1 << \bitnum)
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12
.endm

.macro SMB bitnum
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        orr     r1, #(1 << \bitnum)
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12
.endm

exec_65tube:
        push    {r0-r12,lr}

        mov     fp, r0                  // set fp (r11) to the 64K 6502 memory array
        ldr     r5, =opcode_00          // set r5 (r12) to the base address of the 6502 instruction handler
        ldr     r3, =(0x0000fef8 >> 3)  // initialize the constant in r3
        ldr     r12, =0xffffff00         // initialize the constant in r12

        mov     r0, #0                  // initialize Event register
        str     r0, events

//        ldr     r0, =arm_irq_handler    // Install the ARM interrupt handler
//        mov     r1, #0x38
//        str     r0, [r1]
        
        mrs     r0, CPSR                // Enable ARM IRQs and FIQs
        bic     r0, r0, #0xC0
        msr     CPSR_c, r0

handle_rst:
        orr     r4, r4, #4              // Set the I bit
        bic     r4, r4, #8              // Clear the D bit
        add     r0, fp, #0x10000
        ldrh    r0, [r0, #-4]           // Fetch the address in FFFC,FFFD
        add     sl, fp, r0              // Point to this instruction in memory
        FETCH_NEXT_MERGED                      // Start executing instructions

exec_65tube_exit:                       // We get here from the Event handler when RST is detected
        pop      {r0-r12,pc}

events:
        .word 0
        
// 6502 Event handler
// We have come here via a branch from the second instruction table
Event_Handler:
        mrs     r2, CPSR                // preserve the 6502 flags
        orr     r1, r2, #0xC0           // disable ARM IRQs and FIQs
        msr     CPSR_c, r1
        
        ldr     r0, events
        bic     r5, r5, #256<<I_ALIGN   // ack events
        sub     sl, sl, #2              // set the instruction back as we haven't executed it

        bic     r1, r0, #0x80000000     // the attention bit
        str     r1, events              // ack the attention bit

        push    {r1-r3,ip,lr}           // r0 contains the tube mailbox value
        bl      tube_io_handler
        pop     {r1-r3,ip,lr}
        
        tst     r0, #4                  // Bit 2 set indicate RST is active
        bne     exec_65tube_exit        // exit immediately if active edge seen

        tst     r0, #2                  // Bit 1 set indicate NMI is active
        bne     handle_nmi              // Take the NMI interrupt

        tst     r4, #4                  // Test whether interrupts are enabled (bit 2 = 0)
        bne     noirq
        tst     r0, #1                  // Bit 1 set indicate IRQ is active
        bne     handle_irq              // Take the IRQ interrupt

noirq:
        msr     CPSR, r2                // restore the 6502 flags and re-enable ARM interrupts
        FETCH_NEXT_MERGED                      // go back to executing 6502 instructions

decimal_mode_adc:
        and     r2, r1, #15
        and     r0, r6, #0xf000000
        adc     r2, r2, r0, lsr #24
        cmp     r2, #10
        subcs   r2, r2, #10
        lsl     r2, r2, #24
        lsr     r1, r1, #4
        adc     r6, r1, r6, lsr #28
        cmp     r6, #10
        subcs   r6, r6, #10
        orr     r6, r2, r6, lsl #28
        FETCH_NEXT_STAGE_0
        teq     r6, #0
        bicvc   r4, #0x40
        FETCH_NEXT_STAGE_1
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

decimal_mode_sbc:
        and     r2, r1, #15
        and     r0, r6, #0xf000000
        rscs    r2, r2, r0, lsr #24
        addcc   r2, r2, #10
        lsr     r1, r1, #4
        rscs    r6, r1, r6, lsr #28
        addcc   r6, r6, #10
        orr     r6, r2, r6, lsl #4
        FETCH_NEXT_STAGE_0
        lsl     r6, r6, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

tube_load_handler:
        push    {r0-r3,lr}                // r0 = address, r2 = current cpsr
        orr     r2, r2, #0xC0             // disable ARM IRQs and FIQs
        msr     CPSR_c, r2
        push    {ip}                      // push ip after disabling interrupts to avoid a race condition
        bl      copro_65tube_tube_read
        pop     {ip}
        str     r0, [sp, #4]              // save result to r1 on the stack
        pop     {r0-r3,pc}                // r2 (cpsr) is preserved

tube_store_handler:
        push    {r0-r3,lr}                // r0 = address, r1 = data, r2 = current cpsr
        orr     r2, r2, #0xC0             // disable ARM IRQs and FIQs
        msr     CPSR_c, r2
        push    {ip}                      // push ip after disabling interrupts to avoid a race condition
        bl      copro_65tube_tube_write
        pop     {ip}
        pop     {r0-r3,pc}                // r2 (cpsr) is preserved

handle_irq:
        msr     CPSR, r2                  // Restore the 6502 flags and re-enable ARM interrupts
        sub     r1, sl, fp
        lsr     r0, r1, #8
        SPUSH   r0
        SPUSH   r1
        movpl   r0, #0
        movmi   r0, #0x80
        orrcs   r0, r0, #1
        orreq   r0, r0, #2
        orr     r0, r0, r4
        bic     r0, r0, #0x10             // Make sure B is clear
        SPUSH   r0
        orr     r4, r4, #4                // Set the I bit
        bic     r4, r4, #8                // Clear the D bit
        add     r0, fp, #0x10000
        ldrh    r0, [r0, #-2]             // Fetch the address in FFFE,FFFF
        add     sl, fp, r0                // Point to this instruction in memory
        FETCH_NEXT_MERGED

handle_nmi:
        msr     CPSR, r2                  // Restore the 6502 flags and re-enable ARM interrupts
        sub     r1, sl, fp
        lsr     r0, r1, #8
        SPUSH   r0
        SPUSH   r1
        movpl   r0, #0x20
        movmi   r0, #0xA0
        orrcs   r0, r0, #1
        orreq   r0, r0, #2
        orr     r0, r0, r4
        bic     r0, r0, #0x10             // Make sure B is clear
        SPUSH   r0
        orr     r4, r4, #4                // Set the I bit
        bic     r4, r4, #8                // Clear the D bit
        add     r0, fp, #0x10000
        ldrh    r0, [r0, #-6]             // Fetch the address in FFFA,FFFB
        add     sl, fp, r0                // Point to this instruction in memory
        FETCH_NEXT_MERGED

handle_rti:
        SPOP    r0
        and     r4, r0, #0x4C
        mrs     r2, CPSR
        bic     r2, #(N_FLAG + Z_FLAG + C_FLAG)
        and     r1, r0, #0x80
        orr     r2, r2, r1, lsl #24
        and     r1, r0, #0x03
        orr     r2, r2, r1, lsl #29
        msr     CPSR_flg, r2
        SPOP    r0
        SPOP    r1
        add     sl, r0, r1, lsl #8
        add     sl, sl, fp
        FETCH_NEXT_CHECK_IRQ
                
.balign 0x10000
mpu_memory:
.rept 16384
.word 0
.endr

// **********************************************
// Start of opcode implementations
// **********************************************

// Opcode 00 - BRK
.align I_ALIGN
opcode_00:
        // BRK is really a two byte instruction, so no fixup needed
        sub     r1, sl, fp
        lsr     r0, r1, #8
        SPUSH   r0
        SPUSH   r1
        movpl   r0, #0x30
        movmi   r0, #0xB0
        orrcs   r0, r0, #1
        orreq   r0, r0, #2
        orr     r0, r0, r4
        SPUSH   r0
        add     r0, fp, #0x10000
        ldr     r0, [r0, #-4]
        add     sl, fp, r0, lsr #16
        FETCH_NEXT_MERGED

// Opcode 01 - ORA ($00,X)
.align I_ALIGN
opcode_01:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 02 - NOP #$00
.align I_ALIGN
opcode_02:
        NOP_2

// Opcode 03 - NOP
.align I_ALIGN
opcode_03:
        NOP_1

// Opcode 04 - TSB $00
.align I_ALIGN
opcode_04:
        ldrb    r1, [fp, r0]
        mrs     r2, CPSR
        tst     r1, r6, lsr #24
        bic     r2, r2, #Z_FLAG
        orreq   r2, r2, #Z_FLAG
        msr     CPSR_flg, r2
        FETCH_NEXT_STAGE_0
        orr     r1, r1, r6, lsr #24
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 05 - ORA $00
.align I_ALIGN
opcode_05:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 06 - ASL $00
.align I_ALIGN
opcode_06:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #1
        strb    r1, [fp, r0]
        tst     r12, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode 07 - RMB0 $00
.align I_ALIGN
opcode_07:
        RMB 0

// Opcode 08 - PHP
.align I_ALIGN
opcode_08:
        FETCH_NEXT_STAGE_1_I
        orr     r0, r4, #0x30
        orrmi   r0, r0, #0x80
        orrcs   r0, r0, #1
        orreq   r0, r0, #2
        SPUSH   r0
        FETCH_NEXT_STAGE_2_I

// Opcode 09 - ORA #$00
.align I_ALIGN
opcode_09:
        FETCH_NEXT_STAGE_0
        lsl     r0, r0, #24
        orrs    r6, r6, r0
        FETCH_NEXT_STAGE_12

// Opcode 0A - ASL A
.align I_ALIGN
opcode_0A:
        FETCH_NEXT_STAGE_1_I
        lsls    r6, r6, #1
        FETCH_NEXT_STAGE_2_I

// Opcode 0B - NOP
.align I_ALIGN
opcode_0B:
        NOP_1

// Opcode 0C - TSB $0000
.align I_ALIGN
opcode_0C:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        mrs     r2, CPSR
        tst     r1, r6, lsr #24
        bic     r2, r2, #Z_FLAG
        orreq   r2, r2, #Z_FLAG
        msr     CPSR_flg, r2
        orr     r1, r1, r6, lsr #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 0D - ORA $0000
.align I_ALIGN
opcode_0D:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2

// Opcode 0E - ASL $0000
.align I_ALIGN
opcode_0E:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        lsl     r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        tst     r12, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode 0F - BBR0
.align I_ALIGN
opcode_0F:
        BBR 0

// Opcode 10 - BPL
.align I_ALIGN
opcode_10:
        sxtabpl sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode 11 - ORA ($00),Y
.align I_ALIGN
opcode_11:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 12 - ORA ($00)
.align I_ALIGN
opcode_12:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 13 - NOP
.align I_ALIGN
opcode_13:
        NOP_1

// Opcode 14 - TRB $00
.align I_ALIGN
opcode_14:
        ldrb    r1, [fp, r0]
        mrs     r2, CPSR
        tst     r1, r6, lsr #24
        bic     r2, r2, #Z_FLAG
        orreq   r2, r2, #Z_FLAG
        msr     CPSR_flg, r2
        FETCH_NEXT_STAGE_0
        bic     r1, r1, r6, lsr #24
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 15 - ORA $00,X
.align I_ALIGN
opcode_15:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 16 - ASL $00, X
.align I_ALIGN
opcode_16:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #1
        strb    r1, [fp, r0, lsr #24]
        tst     r12, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode 17 - RMB1 $00
.align I_ALIGN
opcode_17:
        RMB 1

// Opcode 18 - CLC
.align I_ALIGN
opcode_18:
        FETCH_NEXT_STAGE_1_I
        mrs     r0, CPSR
        bic     r0, r0, #C_FLAG
        msr     CPSR_flg, r0
        FETCH_NEXT_STAGE_2_I

// Opcode 19 - ORA $0000,Y
.align I_ALIGN
opcode_19:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2

// Opcode 1A - INC A
.align I_ALIGN
opcode_1A:
        FETCH_NEXT_STAGE_1_I
        add     r6, r6, #0x1000000
        teq     r6, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 1B - NOP
.align I_ALIGN
opcode_1B:
        NOP_1

// Opcode 1C - TRB $0000
.align I_ALIGN
opcode_1C:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        mrs     r2, CPSR
        tst     r1, r6, lsr #24
        bic     r2, r2, #Z_FLAG
        orreq   r2, r2, #Z_FLAG
        msr     CPSR_flg, r2
        bic     r1, r1, r6, lsr #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 1D - ORA $0000,X
.align I_ALIGN
opcode_1D:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2

// Opcode 1E - ASL $0000,X
.align I_ALIGN
opcode_1E:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        lsl     r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        tst     r12, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode 1F - BBR1
.align I_ALIGN
opcode_1F:
        BBR 1

// Opcode 20 - JSR $0000
.align I_ALIGN
opcode_20:
        ldrh    r1, [sl, #-1]
        sub     r9, r9, #1
        strh    sl, [r9], #-1
        add     sl, fp, r1
        FETCH_NEXT_STAGE_0
        orr     r9, #0x0100
        FETCH_NEXT_STAGE_12

// Opcode 21 - AND ($00,X)
.align I_ALIGN
opcode_21:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 22 - NOP #$00
.align I_ALIGN
opcode_22:
        NOP_2

// Opcode 23 - NOP
.align I_ALIGN
opcode_23:
        NOP_1

// Opcode 24 - BIT $00
.align I_ALIGN
opcode_24:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        lsl     r0, r1, #24
        tst     r0, r6
        and     r1, r1, #0x40
        bic     r4, r4, #0x40
        orr     r4, r4, r1
        and     r0, r0, #N_FLAG
        mrs     r1, CPSR
        bic     r1, r1, #N_FLAG
        orr     r1, r1, r0
        msr     CPSR_flg, r1
        FETCH_NEXT_STAGE_12

// Opcode 25 - AND     $00
.align I_ALIGN
opcode_25:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 26 - ROL $00
.align I_ALIGN
opcode_26:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        adc     r1, r1, r1
        strb    r1, [fp, r0]
        lsls    r0, r1, #24
        FETCH_NEXT_STAGE_12

// Opcode 27 - RMB2 $00
.align I_ALIGN
opcode_27:
        RMB 2

// Opcode 28 - PLP
.align I_ALIGN
opcode_28:
        sub     sl, sl, #1  // fix up FETCH_NEXT reading ahead two bytes
        SPOP    r0
        and     r4, r0, #0x4C
        mrs     r2, CPSR
        bic     r2, #(N_FLAG + Z_FLAG + C_FLAG)
        and     r1, r0, #0x80
        orr     r2, r2, r1, lsl #24
        and     r1, r0, #0x03
        orr     r2, r2, r1, lsl #29
        msr     CPSR_flg, r2
        FETCH_NEXT_CHECK_IRQ

// Opcode 29 - AND     #$00
.align I_ALIGN
opcode_29:
        FETCH_NEXT_STAGE_0
        lsl     r0, r0, #24
        ands    r6, r6, r0
        FETCH_NEXT_STAGE_12

// Opcode 2A - ROL A
.align I_ALIGN
opcode_2A:
        FETCH_NEXT_STAGE_1_I
        orrcs   r6, r6, #0x800000
        lsls    r6, r6, #1
        FETCH_NEXT_STAGE_2_I

// Opcode 2B - NOP
.align I_ALIGN
opcode_2B:
        NOP_1

// Opcode 2C - BIT $0000
.align I_ALIGN
opcode_2C:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r0, r1, #24
        tst     r0, r6
        and     r1, r1, #0x40
        bic     r4, r4, #0x40
        orr     r4, r4, r1
        and     r0, r0, #N_FLAG
        mrs     r1, CPSR
        bic     r1, r1, #N_FLAG
        orr     r1, r1, r0
        msr     CPSR_flg, r1
        FETCH_NEXT_STAGE_12

// Opcode 2D - AND     $0000
.align I_ALIGN
opcode_2D:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 2E - ROL $0000
.align I_ALIGN
opcode_2E:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        adc     r1, r1, r1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsls    r0, r1, #24
        FETCH_NEXT_STAGE_12

// Opcode 2F - BBR2
.align I_ALIGN
opcode_2F:
        BBR 2

// Opcode 30 - BMI
.align I_ALIGN
opcode_30:
        sxtabmi sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode 31 - AND ($00),Y
.align I_ALIGN
opcode_31:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 32 - AND ($00)
.align I_ALIGN
opcode_32:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 33 - NOP
.align I_ALIGN
opcode_33:
        NOP_1

// Opcode 34 - BIT $00,X
.align I_ALIGN
opcode_34:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        lsl     r0, r1, #24
        tst     r0, r6
        and     r1, r1, #0x40
        bic     r4, r4, #0x40
        orr     r4, r4, r1
        and     r0, r0, #N_FLAG
        mrs     r1, CPSR
        bic     r1, r1, #N_FLAG
        orr     r1, r1, r0
        msr     CPSR_flg, r1
        FETCH_NEXT_STAGE_12

// Opcode 35 - AND $00,X
.align I_ALIGN
opcode_35:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 36 - ROL $00,X
.align I_ALIGN
opcode_36:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        adc     r1, r1, r1
        strb    r1, [fp, r0, lsr #24]
        lsls    r0, r1, #24
        FETCH_NEXT_STAGE_12

// Opcode 37 - RMB3 $00
.align I_ALIGN
opcode_37:
        RMB 3

// Opcode 38 - SEC
.align I_ALIGN
opcode_38:
        FETCH_NEXT_STAGE_1_I
        mrs     r0, CPSR
        orr     r0, r0, #C_FLAG
        msr     CPSR_flg, r0
        FETCH_NEXT_STAGE_2_I

// Opcode 39 - AND $0000,Y
.align I_ALIGN
opcode_39:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 3A - DEC A
.align I_ALIGN
opcode_3A:
        FETCH_NEXT_STAGE_1_I
        sub     r6, r6, #0x1000000
        teq     r6, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 3B - NOP
.align I_ALIGN
opcode_3B:
        NOP_1
        
// Opcode 3C - BIT $0000,X
.align I_ALIGN
opcode_3C:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r0, r1, #24
        tst     r0, r6
        and     r1, r1, #0x40
        bic     r4, r4, #0x40
        orr     r4, r4, r1
        and     r0, r0, #N_FLAG
        mrs     r1, CPSR
        bic     r1, r1, #N_FLAG
        orr     r1, r1, r0
        msr     CPSR_flg, r1
        FETCH_NEXT_STAGE_12

// Opcode 3D - AND $0000,X
.align I_ALIGN
opcode_3D:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 3E - ROL $0000,X
.align I_ALIGN
opcode_3E:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        adc     r1, r1, r1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsls    r0, r1, #24
        FETCH_NEXT_STAGE_12

// Opcode 3F - BBR3
.align I_ALIGN
opcode_3F:
        BBR 3

// Opcode 40 - RTI
.align I_ALIGN
opcode_40:
        b       handle_rti

// Opcode 41 - EOR ($00,X)
.align I_ALIGN
opcode_41:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 42 - NOP #$00
.align I_ALIGN
opcode_42:
        NOP_2
        
// Opcode 43 - NOP
.align I_ALIGN
opcode_43:
        NOP_1
        
// Opcode 44 - NOP $00
.align I_ALIGN
opcode_44:
        NOP_2

// Opcode 45 - EOR $00
.align I_ALIGN
opcode_45:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 46 - LSR $00
.align I_ALIGN
opcode_46:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        lsrs    r1, r1, #1
        strb    r1, [fp, r0]
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode 47 - RMB4 $00
.align I_ALIGN
opcode_47:
        RMB 4

// Opcode 48 - PHA
.align I_ALIGN
opcode_48:
        FETCH_NEXT_STAGE_1_I
        lsr     r0, r6, #24
        SPUSH   r0
        FETCH_NEXT_STAGE_2_I

// Opcode 49 - EOR #$00
.align I_ALIGN
opcode_49:
        FETCH_NEXT_STAGE_0
        lsl     r0, r0, #24
        eors    r6, r6, r0
        FETCH_NEXT_STAGE_12

// Opcode 4A - LSR A
.align I_ALIGN
opcode_4A:
        FETCH_NEXT_STAGE_1_I
        lsrs    r0, r6, #25
        lsl     r6, r0, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 4B - NOP
.align I_ALIGN
opcode_4B:
        NOP_1
        
// Opcode 4C - JMP $0000
.align I_ALIGN
opcode_4C:
        EA_ABSOLUTE
        add     sl, r0, fp
        FETCH_NEXT_MERGED

// Opcode 4D - EOR $0000
.align I_ALIGN
opcode_4D:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 4E - LSR $0000
.align I_ALIGN
opcode_4E:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        lsrs    r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode 4F - BBR4
.align I_ALIGN
opcode_4F:
        BBR 4


// Opcode 50 - BVC
.align I_ALIGN
opcode_50:
        mrs     r2, CPSR
        tst     r4, #0x40
        sxtabeq sl, sl, r0
        msr     CPSR_flg, r2
        FETCH_NEXT_MERGED

// Opcode 51 - EOR ($00),Y
.align I_ALIGN
opcode_51:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 52 - EOR ($00)
.align I_ALIGN
opcode_52:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 53 - NOP
.align I_ALIGN
opcode_53:
        NOP_1
        
// Opcode 54 - NOP $00,X
.align I_ALIGN
opcode_54:
        NOP_2

// Opcode 55 - EOR $00,X
.align I_ALIGN
opcode_55:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 56 - LSR $00,X
.align I_ALIGN
opcode_56:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        lsrs    r1, r1, #1
        strb    r1, [fp, r0, lsr #24]
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode 57 - RMB5 $00
.align I_ALIGN
opcode_57:
        RMB 5

// Opcode 58 - CLI
.align I_ALIGN
opcode_58:
        sub     sl, sl, #1  // fix up FETCH_NEXT reading ahead two bytes
        bic     r4, r4, #4
        FETCH_NEXT_CHECK_IRQ

// Opcode 59 - EOR $0000,Y
.align I_ALIGN
opcode_59:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 5A - PHY
.align I_ALIGN
opcode_5A:
        FETCH_NEXT_STAGE_1_I
        lsr     r0, r8, #24
        SPUSH   r0
        FETCH_NEXT_STAGE_2_I

// Opcode 5B - NOP
.align I_ALIGN
opcode_5B:
        NOP_1
        
// Opcode 5C - NOP $0000
.align I_ALIGN
opcode_5C:
        NOP_3

// Opcode 5D - EOR $0000,X
.align I_ALIGN
opcode_5D:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 5E - LSR $0000,X
.align I_ALIGN
opcode_5E:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        lsrs    r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode 5F - BBR5
.align I_ALIGN
opcode_5F:
        BBR 5

// Opcode 60 - RTS
.align I_ALIGN
opcode_60:
        ldrh    r0, [r9, #1]
        add     r9, r9, #2
        add     sl, fp, #1
        add     sl, sl, r0
        FETCH_NEXT_STAGE_0
        bic     r9, #0xFF00
        orr     r9, #0x0100
        FETCH_NEXT_STAGE_12

// Opcode 61 - ADC ($00,X)
.align I_ALIGN
opcode_61:
        EA_INDIRECT_X
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r12
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 62 - NOP #$00
.align I_ALIGN
opcode_62:
        NOP_2

// Opcode 63 - NOP
.align I_ALIGN
opcode_63:
        NOP_1
        
// Opcode 64 - STZ $00
.align I_ALIGN
opcode_64:
        FETCH_NEXT_STAGE_0
        strb    r12, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 65 - ADC $00
.align I_ALIGN
opcode_65:
        ldrb    r1, [fp, r0]
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r12
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 66 - ROR $00
.align I_ALIGN
opcode_66:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 67 - RMB6 $00
.align I_ALIGN
opcode_67:
        RMB 6

// Opcode 68 - PLA
.align I_ALIGN
opcode_68:
        FETCH_NEXT_STAGE_1_I
        SPOP    r6
        lsl     r6, r6, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 69 - ADC #$00
.align I_ALIGN
opcode_69:
        mov     r1, r0  // TODO - could probably optimise this away
        tst     r4, #8
        bne     decimal_mode_adc   
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r12
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 6A - ROR A
.align I_ALIGN
opcode_6A:
        FETCH_NEXT_STAGE_1_I
        orrcs   r6, r6, #1
        rors    r0, r6, #25
        lsl     r6, r0, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 6B - NOP
.align I_ALIGN
opcode_6B:
        NOP_1
        
// Opcode 6C - JMP ($0000)
.align I_ALIGN
opcode_6C:
        ldrb    r1, [sl], #0
        add     r0, r0, r1, lsl #8
        ldrb    r1, [r0, fp]!
        ldrb    r0, [r0, #1]
        add     sl, r1, r0, lsl #8
        add     sl, sl, fp
        FETCH_NEXT_MERGED

// Opcode 6D - ADC $0000
.align I_ALIGN
opcode_6D:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r12
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 6E - ROR $0000
.align I_ALIGN
opcode_6E:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 6F - BBR6
.align I_ALIGN
opcode_6F:
        BBR 6

// Opcode 70 - BVS
.align I_ALIGN
opcode_70:
        mrs     r2, CPSR
        tst     r4, #0x40
        sxtabne sl, sl, r0
        msr     CPSR_flg, r2
        FETCH_NEXT_MERGED

// Opcode 71 - ADC ($00),Y
.align I_ALIGN
opcode_71:
        EA_INDIRECT_Y
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r12
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 72 - ADC ($00)
.align I_ALIGN
opcode_72:
        EA_INDIRECT
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r12
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 73 - NOP
.align I_ALIGN
opcode_73:
        NOP_1
        
// Opcode 74 - STZ $00,X
.align I_ALIGN
opcode_74:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        strb    r12, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_12

// Opcode 75 - ADC $00,X
.align I_ALIGN
opcode_75:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r12
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 76 - ROR $00,X
.align I_ALIGN
opcode_76:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        strb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_12

// Opcode 77 - RMB7 $00
.align I_ALIGN
opcode_77:
        RMB 7

// Opcode 78 - SEI
.align I_ALIGN
opcode_78:
        FETCH_NEXT_STAGE_1_I
        orr     r4, r4, #4
        FETCH_NEXT_STAGE_2_I

// Opcode 79 - ADC $0000,Y
.align I_ALIGN
opcode_79:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r12
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 7A - PLY
.align I_ALIGN
opcode_7A:
        FETCH_NEXT_STAGE_1_I
        SPOP    r8
        lsl     r8, r8, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 7B - NOP
.align I_ALIGN
opcode_7B:
        NOP_1
        
// Opcode 7C - JMP ($0000,X)
.align I_ALIGN
opcode_7C:
        EA_ABSOLUTE_INDEXED r7
        ldrb    r1, [r0, fp]!
        ldrb    r0, [r0, #1]
        add     sl, r1, r0, lsl #8
        add     sl, sl, fp
        FETCH_NEXT_MERGED

// Opcode 7D - ADC $0000,X
.align I_ALIGN
opcode_7D:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r12
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 7E - ROR $0000,X
.align I_ALIGN
opcode_7E:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 7F - BBR7
.align I_ALIGN
opcode_7F:
        BBR 7

// Opcode 80 - BRA
.align I_ALIGN
opcode_80:
        sxtab   sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode 81 - STA ($00,X)
.align I_ALIGN
opcode_81:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        lsr     r1, r6, #24
        STORE_INDIRECT
        FETCH_NEXT_STAGE_12

// Opcode 82 - NOP #$00
.align I_ALIGN
opcode_82:
        NOP_2

// Opcode 83 - NOP
.align I_ALIGN
opcode_83:
        NOP_1
        
// Opcode 84 - STY $00
.align I_ALIGN
opcode_84:
        FETCH_NEXT_STAGE_0
        lsr     r1, r8, #24
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 85 - STA $00
.align I_ALIGN
opcode_85:
        FETCH_NEXT_STAGE_0
        lsr     r1, r6, #24
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 86 - STX $00
.align I_ALIGN
opcode_86:
        FETCH_NEXT_STAGE_0
        lsr     r1, r7, #24
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 87 - SMB0 $00
.align I_ALIGN
opcode_87:
        SMB 0

// Opcode 88 - DEY
.align I_ALIGN
opcode_88:
        FETCH_NEXT_STAGE_1_I
        sub     r8, r8, #0x1000000
        teq     r8, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 89 - BIT #$00
.align I_ALIGN
opcode_89:
        lsl     r0, r0, #24
        mrs     r2, CPSR
        and     r2, r2, #N_FLAG
        tst     r0, r6
        mrs     r1, CPSR
        bic     r1, r1, #N_FLAG
        orr     r1, r1, r2
        FETCH_NEXT_STAGE_0
        msr     CPSR_flg, r1
        FETCH_NEXT_STAGE_12

// Opcode 8A - TXA
.align I_ALIGN
opcode_8A:
        FETCH_NEXT_STAGE_1_I
        movs    r6, r7
        FETCH_NEXT_STAGE_2_I

// Opcode 8B - NOP
.align I_ALIGN
opcode_8B:
        NOP_1
        
// Opcode 8C - STY $0000
.align I_ALIGN
opcode_8C:
        EA_ABSOLUTE
        lsr     r1, r8, #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 8D - STA $0000
.align I_ALIGN
opcode_8D:
        EA_ABSOLUTE
        lsr     r1, r6, #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 8E - STX $0000
.align I_ALIGN
opcode_8E:
        EA_ABSOLUTE
        lsr     r1, r7, #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 8F - BBS0
.align I_ALIGN
opcode_8F:
        BBS 0

// Opcode 90 - BCC
.align I_ALIGN
opcode_90:
        sxtabcc sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode 91 - STA ($00),Y
.align I_ALIGN
opcode_91:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        lsr     r1, r6, #24
        STORE_INDIRECT
        FETCH_NEXT_STAGE_12

// Opcode 92 - STA ($00)
.align I_ALIGN
opcode_92:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        lsr     r1, r6, #24
        STORE_INDIRECT
        FETCH_NEXT_STAGE_12

// Opcode 93 - NOP
.align I_ALIGN
opcode_93:
        NOP_1
        
// Opcode 94 - STY $00,X
.align I_ALIGN
opcode_94:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        lsr     r1, r8, #24
        strb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_12

// Opcode 95 - STA $00,X
.align I_ALIGN
opcode_95:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        lsr     r1, r6, #24
        strb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_12

// Opcode 96 - STX $00,Y
.align I_ALIGN
opcode_96:
        FETCH_NEXT_STAGE_0
        add     r0, r8, r0, lsl #24
        lsr     r1, r7, #24
        strb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_12

// Opcode 97 - SMB1 $00
.align I_ALIGN
opcode_97:
        SMB 1

// Opcode 98 - TYA
.align I_ALIGN
opcode_98:
        FETCH_NEXT_STAGE_1_I
        movs    r6, r8
        FETCH_NEXT_STAGE_2_I

// Opcode 99 - STA $0000,Y
.align I_ALIGN
opcode_99:
        EA_ABSOLUTE_INDEXED r8
        lsr     r1, r6, #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 9A - TXS
.align I_ALIGN
opcode_9A:
        FETCH_NEXT_STAGE_1_I
        add     r9, fp, #0x100
        add     r9, r9, r7, lsr #24
        FETCH_NEXT_STAGE_2_I

// Opcode 9B - NOP
.align I_ALIGN
opcode_9B:
        NOP_1
        
// Opcode 9C - STZ $0000
.align I_ALIGN
opcode_9C:
        EA_ABSOLUTE
        FETCH_NEXT_STAGE_0
        strb    r12, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 9D - STA $0000,X
.align I_ALIGN
opcode_9D:
        EA_ABSOLUTE_INDEXED r7
        lsr     r1, r6, #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 9E - STZ $0000,X
.align I_ALIGN
opcode_9E:
        EA_ABSOLUTE_INDEXED r7
        FETCH_NEXT_STAGE_0
        strb    r12, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 9F - BBS1
.align I_ALIGN
opcode_9F:
        BBS 1

// Opcode A0 - LDY #$00
.align I_ALIGN
opcode_A0:
        FETCH_NEXT_STAGE_0
        lsl     r8, r0, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_12

// Opcode A1 - LDA ($00,X)
.align I_ALIGN
opcode_A1:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2

// Opcode A2 - LDX #$00
.align I_ALIGN
opcode_A2:
        FETCH_NEXT_STAGE_0
        lsl     r7, r0, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_12

// Opcode A3 - NOP
.align I_ALIGN
opcode_A3:
        NOP_1
        
// Opcode A4 - LDY $00
.align I_ALIGN
opcode_A4:
        FETCH_NEXT_STAGE_0
        ldrb    r8, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r8, r8, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_2

// Opcode A5 - LDA $00
.align I_ALIGN
opcode_A5:
        FETCH_NEXT_STAGE_0
        ldrb    r6, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r6, r6, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2

// Opcode A6 - LDX $00
.align I_ALIGN
opcode_A6:
        FETCH_NEXT_STAGE_0
        ldrb    r7, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r7, r7, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_2

// Opcode A7 - SMB2 $00
.align I_ALIGN
opcode_A7:
        SMB 2

// Opcode A8 - TAY
.align I_ALIGN
opcode_A8:
        FETCH_NEXT_STAGE_1_I
        movs    r8, r6
        FETCH_NEXT_STAGE_2_I

// Opcode A9 - LDA #$00
.align I_ALIGN
opcode_A9:
        FETCH_NEXT_STAGE_0
        lsl     r6, r0, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_12

// Opcode AA - TAX
.align I_ALIGN
opcode_AA:
        FETCH_NEXT_STAGE_1_I
        movs    r7, r6
        FETCH_NEXT_STAGE_2_I

// Opcode AB - NOP
.align I_ALIGN
opcode_AB:
        NOP_1
        
// Opcode AC - LDY $0000
.align I_ALIGN
opcode_AC:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r8, r1, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_12

// Opcode AD - LDA $0000
.align I_ALIGN
opcode_AD:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_12

// Opcode AE - LDX $0000
.align I_ALIGN
opcode_AE:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r7, r1, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_12

// Opcode AF - BBS2
.align I_ALIGN
opcode_AF:
        BBS 2

// Opcode B0 - BCS
.align I_ALIGN
opcode_B0:
        sxtabcs sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode B1 - LDA ($00),Y
.align I_ALIGN
opcode_B1:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2

// Opcode B2 - LDA ($00)
.align I_ALIGN
opcode_B2:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2

// Opcode B3 - NOP
.align I_ALIGN
opcode_B3:
        NOP_1
        
// Opcode B4 - LDY $00,X
.align I_ALIGN
opcode_B4:
        add     r0, r7, r0, lsl #24
        ldrb    r8, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        lsl     r8, r8, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2

// Opcode B5 - LDA $00,X
.align I_ALIGN
opcode_B5:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        ldrb    r6, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        lsl     r6, r6, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2

// Opcode B6 - LDX $00,Y
.align I_ALIGN
opcode_B6:
        FETCH_NEXT_STAGE_0
        add     r0, r8, r0, lsl #24
        ldrb    r7, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        lsl     r7, r7, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_2

// Opcode B7 - SMB3 $00
.align I_ALIGN
opcode_B7:
        SMB 3

// Opcode B8 - CLV
.align I_ALIGN
opcode_B8:
        FETCH_NEXT_STAGE_1_I
        bic     r4, #0x40
        FETCH_NEXT_STAGE_2_I

// Opcode B9 - LDA $0000,Y
.align I_ALIGN
opcode_B9:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_12

// Opcode BA - TSX
.align I_ALIGN
opcode_BA:
        FETCH_NEXT_STAGE_1_I
        lsl     r7, r9, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_2_I

// Opcode BB - NOP
.align I_ALIGN
opcode_BB:
        NOP_1
        
// Opcode BC - LDY $0000,X
.align I_ALIGN
opcode_BC:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r8, r1, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2

// Opcode BD - LDA $0000,X
.align I_ALIGN
opcode_BD:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_12

// Opcode BE - LDX $0000,Y
.align I_ALIGN
opcode_BE:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r7, r1, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_12

// Opcode BF - BBS3
.align I_ALIGN
opcode_BF:
        BBS 3

// Opcode C0 - CPY #$00
.align I_ALIGN
opcode_C0:
        FETCH_NEXT_STAGE_0
        cmp     r8, r0, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode C1 - CMP ($00,X)
.align I_ALIGN
opcode_C1:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode C2 - NOP #$00
.align I_ALIGN
opcode_C2:
        NOP_2
        
// Opcode C3 - NOP
.align I_ALIGN
opcode_C3:
        NOP_1
        
// Opcode C4 - CPY $00
.align I_ALIGN
opcode_C4:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        cmp     r8, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode C5 - CMP $00
.align I_ALIGN
opcode_C5:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode C6 - DEC $00
.align I_ALIGN
opcode_C6:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        sub     r1, r1, #1
        strb    r1, [fp, r0]
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode C7 - SMB4 $00
.align I_ALIGN
opcode_C7:
        SMB 4

// Opcode C8 - INY
.align I_ALIGN
opcode_C8:
        FETCH_NEXT_STAGE_1_I
        add     r8, r8, #0x1000000
        teq     r8, #0
        FETCH_NEXT_STAGE_2_I

// Opcode C9 - CMP #$00
.align I_ALIGN
opcode_C9:
        FETCH_NEXT_STAGE_0
        cmp     r6, r0, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode CA - DEX
.align I_ALIGN
opcode_CA:
        FETCH_NEXT_STAGE_1_I
        sub     r7, r7, #0x1000000
        teq     r7, #0
        FETCH_NEXT_STAGE_2_I

// Opcode CB - WAI
.align I_ALIGN
opcode_CB:
        // TODO - Implement this
        FETCH_NEXT_STAGE_1_I
        FETCH_NEXT_STAGE_2_I

// Opcode CC - CPY $0000
.align I_ALIGN
opcode_CC:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        cmp     r8, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode CD - CMP $0000
.align I_ALIGN
opcode_CD:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode CE - DEC $0000
.align I_ALIGN
opcode_CE:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        sub     r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode CF - BBS4
.align I_ALIGN
opcode_CF:
        BBS 4

// Opcode D0 - BNE
.align I_ALIGN
opcode_D0:
        sxtabne sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode D1 - CMP ($00),Y
.align I_ALIGN
opcode_D1:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode D2 - CMP ($00)
.align I_ALIGN
opcode_D2:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode D3 - NOP
.align I_ALIGN
opcode_D3:
        NOP_1
        
// Opcode D4 - NOP $00,X
.align I_ALIGN
opcode_D4:
        NOP_2

// Opcode D5 - CMP $00,X
.align I_ALIGN
opcode_D5:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode D6 - DEC $00,X
.align I_ALIGN
opcode_D6:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        sub     r1, r1, #1
        strb    r1, [fp, r0, lsr #24]
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode D7 - SMB5 $00
.align I_ALIGN
opcode_D7:
        SMB 5

// Opcode D8 - CLD
.align I_ALIGN
opcode_D8:
        FETCH_NEXT_STAGE_1_I
        bic     r4, r4, #8
        FETCH_NEXT_STAGE_2_I

// Opcode D9 - CMP $0000,Y
.align I_ALIGN
opcode_D9:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode DA - PHX
.align I_ALIGN
opcode_DA:
        FETCH_NEXT_STAGE_1_I
        lsr     r0, r7, #24
        SPUSH   r0
        FETCH_NEXT_STAGE_2_I

// Opcode DB - STP
.align I_ALIGN
opcode_DB:
        // TODO - Implement this
        FETCH_NEXT_STAGE_1_I
        FETCH_NEXT_STAGE_2_I

// Opcode DC - NOP $0000
.align I_ALIGN
opcode_DC:
        NOP_3

// Opcode DD - CMP $0000,X
.align I_ALIGN
opcode_DD:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode DE - DEX $0000,X
.align I_ALIGN
opcode_DE:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        sub     r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode DF - BBS5
.align I_ALIGN
opcode_DF:
        BBS 5

// Opcode E0 - CPX #$00
.align I_ALIGN
opcode_E0:
        FETCH_NEXT_STAGE_0
        cmp     r7, r0, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode E1 - SBC ($00,X)
.align I_ALIGN
opcode_E1:
        EA_INDIRECT_X
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r12
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode E2 - NOP #$00
.align I_ALIGN
opcode_E2:
        NOP_2

// Opcode E3 - NOP
.align I_ALIGN
opcode_E3:
        NOP_1
        
// Opcode E4 - CPX $00
.align I_ALIGN
opcode_E4:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        cmp     r7, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode E5 - SBC $00
.align I_ALIGN
opcode_E5:
        ldrb    r1, [fp, r0]
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r12
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode E6 - INC $00
.align I_ALIGN
opcode_E6:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        add     r1, r1, #1
        strb    r1, [fp, r0]
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode E7 - SMB6 $00
.align I_ALIGN
opcode_E7:
        SMB 6

// Opcode E8 - INX
.align I_ALIGN
opcode_E8:
        FETCH_NEXT_STAGE_1_I
        add     r7, r7, #0x1000000
        teq     r7, #0
        FETCH_NEXT_STAGE_2_I

// Opcode E9 - SBC #$00
.align I_ALIGN
opcode_E9:
        mov     r1, r0   // TODO - could probably optimise this away
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r12
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode EA - NOP
.align I_ALIGN
opcode_EA:
        NOP_1
        
// Opcode EB - NOP
.align I_ALIGN
opcode_EB:
        NOP_1
        
// Opcode EC - CPX $0000
.align I_ALIGN
opcode_EC:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        cmp     r7, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode ED - SBC $0000
.align I_ALIGN
opcode_ED:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r12
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode EE - INC $0000
.align I_ALIGN
opcode_EE:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        add     r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode EF - BBS6
.align I_ALIGN
opcode_EF:
        BBS 6

// Opcode F0 - BEQ
.align I_ALIGN
opcode_F0:
        sxtabeq sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode F1 - SBC ($00),Y
.align I_ALIGN
opcode_F1:
        EA_INDIRECT_Y
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r12
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode F2 - SBC ($00)
.align I_ALIGN
opcode_F2:
        EA_INDIRECT
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r12
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode F3 - NOP
.align I_ALIGN
opcode_F3:
        NOP_1
        
// Opcode F4 - NOP $00,X
.align I_ALIGN
opcode_F4:
        NOP_2

// Opcode F5 - SBC $00,X
.align I_ALIGN
opcode_F5:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r12
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode F6 - INC $00,X
.align I_ALIGN
opcode_F6:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        add     r1, r1, #1
        strb    r1, [fp, r0, lsr #24]
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode F7 - SMB7 $00
.align I_ALIGN
opcode_F7:
        SMB 7

// Opcode F8 - SED
.align I_ALIGN
opcode_F8:
        FETCH_NEXT_STAGE_1_I
        orr     r4, r4, #8
        FETCH_NEXT_STAGE_2_I

// Opcode F9 - SBC $0000,Y
.align I_ALIGN
opcode_F9:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r12
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode FA - PLX
.align I_ALIGN
opcode_FA:
        FETCH_NEXT_STAGE_1_I
        SPOP    r7
        lsl     r7, r7, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_2_I

// Opcode FB - NOP
.align I_ALIGN
opcode_FB:
        NOP_1
        
// Opcode FC - NOP $0000
.align I_ALIGN
opcode_FC:
        NOP_3

// Opcode FD - SBC $0000,X
.align I_ALIGN
opcode_FD:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r12
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode FE - INC $0000,X
.align I_ALIGN
opcode_FE:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        add     r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode FF - BBS7
.align I_ALIGN
opcode_FF:
        BBS 7

// **********************************************
// End of opcode implementations
// **********************************************

.rept 256
.align I_ALIGN
        B        Event_Handler
.endr
