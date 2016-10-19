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
// r4       = 6502 flags (V, D and I bits maintained here, CZN use ARM flags)
// r5       = a constant value of 0xFFFFFF00
// r6       = 6502 A
// r7       = 6502 X
// r8       = 6502 Y
// r9       = 6502 SP
// r10 (sl) = 6502 PC
// r11 (fp) = base address of 64KB of flat memory
// r12 (ip) = base address of 64-byte aligned instruction implementations
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
//        add     pc, ip, r0, lsl #I_ALIGN
//
// This gives us a nice hook into the instruction handler
// When the ARM IRQ detects a valid interrupt
// it adds a constant ( 256 << I_ALIGN) to ip to move into a second instruction handler
// This instruction handler can just be 256 Branches to the Event handler
// this code is executed so rarely absolute performance isn.t an issue and we aren.t worried about it fitting in the cache


// A check needs to be that more than than one event isn.t being added. This can be
// simplified if we align the code to a 0 boundary and just orr ip,ip,#256<<I_ALIGN
//
// once we are in the event handler we do
// disable arm IRQs // this prevents us missing events
// bic ip,ip,#256<<I_ALIGN
// reenable IRQs // now if another event happens we will service it at the next instruction boundary
// sub sl,sl,#1 // set instruction pointer ready to be stacked
// check for reset ( if we have a reset we are happy to drop any other outstanding events
// Check for NMI ( if there is also an IRQ present is will get checked for at the RTI)
// Check for IRQs enabled ( R4)
// if so check if we have an IRQ

#include "rpi-base.h"
#include "tube-defs.h"
        
.text
.global exec_65tube
.global mpu_memory

#if !defined(USE_MULTICORE) && defined(USE_HW_MAILBOX)
.global Event_Handler_Dispatch_Table
.global Event_Handler
.global Event_Handler_Single_Core_Slow
#endif
        
.equ    N_FLAG,      0x80000000
.equ    Z_FLAG,      0x40000000
.equ    C_FLAG,      0x20000000

.equ    ARM_INT_MASK,0xC0

// Mask used when poping status into r4 (RTI, PLP)
// It selects the bits that are managed in r4, i.e. V, D, I
// bits 7-0 are 6502 status reg bits
// bits 31-8 are emulator control bits
.equ    R4_MASK,     0xFFFFFF4C

// Emulator control bits
#if defined(USE_MULTICORE) || !defined(USE_HW_MAILBOX)
.equ    SLOW_FLAG,   0x00000100
#endif
        
// **********************************************
// Start of macros
// **********************************************

#if defined(USE_MULTICORE) || !defined(USE_HW_MAILBOX)

.macro FETCH_NEXT_STAGE_1_I
.endm

.macro FETCH_NEXT_STAGE_2_I
        b       fetch_next_i
.endm
        
// Macros for fetching the next instruction, for two and three byte instructions

.macro FETCH_NEXT_STAGE_0
.endm

.macro FETCH_NEXT_STAGE_1
.endm
        
.macro FETCH_NEXT_STAGE_2
        b       fetch_next
.endm

.macro FETCH_NEXT_STAGE_12
        b       fetch_next
.endm

.macro FETCH_NEXT_MERGED
        b       fetch_next
.endm

// Macro for fetching the next instruction and also checking for any pending IRQs
// Used for instructions that can enable interrupts: CLI, PLP, RTI
.macro FETCH_NEXT_CHECK_IRQ
        b       fetch_next_check_irq
.endm
        
#else

.macro FETCH_NEXT_STAGE_1_I
        ldr     lr, [ip, r0, lsl #2]
.endm

.macro FETCH_NEXT_STAGE_2_I
        ldrb    r0, [sl], #1
        mov     pc, lr
.endm
        
// Macros for fetching the next instruction, for two and three byte instructions

.macro FETCH_NEXT_STAGE_0
        ldrb    r2, [sl], #1
.endm

.macro FETCH_NEXT_STAGE_1
        ldrb    r0, [sl], #1
        ldr     lr, [ip, r2, lsl #2]
.endm
        
.macro FETCH_NEXT_STAGE_2
        mov     pc, lr
.endm

.macro FETCH_NEXT_STAGE_12
        ldr     lr, [ip, r2, lsl #2]
        ldrb    r0, [sl], #1
        mov     pc, lr
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
        ldreq   r0, =tube_irq
        ldreq   r0, [r0]
        eoreq   r0, r0, #1
        tsteq   r0, #1                    // Test for IRQ
        beq     handle_irq
        msr     CPSR_flg, r2              // Restore the 6502 flags
        FETCH_NEXT_MERGED
        .ltorg
.endm

#endif

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
        orr     r2, r2, #ARM_INT_MASK
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
        push    {r0-r1}
        ldr     r1, =histogram_memory
        add     r1, r2, lsl #2
        ldr     r0, [r1]
        add     r0, #1
        str     r0, [r1]
        pop     {r0-r1}
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

.macro INTR vector setbrk                 // vector is -2 for IRQ/BRK or -6 for NMI
        sub     r1, sl, fp                // setbrk should be 1 for BRK, 0 for IRQ/NMI
        lsr     r0, r1, #8
        SPUSH   r0
        SPUSH   r1
        movpl   r0, #0x20                 // Always set bit 5 (-) in the pushed value
        movmi   r0, #0xA0
        orrcs   r0, r0, #1
        orreq   r0, r0, #2
        orr     r0, r0, r4                // In r4 only bits 6 (V), 3 (D) and 2 (I) used; others always zero.
.if \setbrk        
        orr     r0, r0, #0x10             // Set the B bit in the pushed value (in BRK, not in IRQ/NMI)
.endif
        SPUSH   r0
        orr     r4, r4, #4                // Set the I bit in the 6502 status register
        bic     r4, r4, #8                // Clear the D bit in the 6502 status register
        add     r0, fp, #0x10000
        ldrh    r0, [r0, #\vector]        // Fetch the vector address
        add     sl, fp, r0                // Point to this instruction in memory
.endm

// **********************************************
// End of macros
// **********************************************

// **********************************************
// Start of Opcode Jump Table
// **********************************************
        
//     256 words = 1K bytes
//     align on a 2K boundary so ISR setting bit 10 can jump to the events table

.align 11

opcode_jump_table:       
        .word   opcode_00
        .word   opcode_01
        .word   opcode_02
        .word   opcode_03
        .word   opcode_04
        .word   opcode_05
        .word   opcode_06
        .word   opcode_07
        .word   opcode_08
        .word   opcode_09
        .word   opcode_0A
        .word   opcode_0B
        .word   opcode_0C
        .word   opcode_0D
        .word   opcode_0E
        .word   opcode_0F
        .word   opcode_10
        .word   opcode_11
        .word   opcode_12
        .word   opcode_13
        .word   opcode_14
        .word   opcode_15
        .word   opcode_16
        .word   opcode_17
        .word   opcode_18
        .word   opcode_19
        .word   opcode_1A
        .word   opcode_1B
        .word   opcode_1C
        .word   opcode_1D
        .word   opcode_1E
        .word   opcode_1F
        .word   opcode_20
        .word   opcode_21
        .word   opcode_22
        .word   opcode_23
        .word   opcode_24
        .word   opcode_25
        .word   opcode_26
        .word   opcode_27
        .word   opcode_28
        .word   opcode_29
        .word   opcode_2A
        .word   opcode_2B
        .word   opcode_2C
        .word   opcode_2D
        .word   opcode_2E
        .word   opcode_2F
        .word   opcode_30
        .word   opcode_31
        .word   opcode_32
        .word   opcode_33
        .word   opcode_34
        .word   opcode_35
        .word   opcode_36
        .word   opcode_37
        .word   opcode_38
        .word   opcode_39
        .word   opcode_3A
        .word   opcode_3B
        .word   opcode_3C
        .word   opcode_3D
        .word   opcode_3E
        .word   opcode_3F
        .word   opcode_40
        .word   opcode_41
        .word   opcode_42
        .word   opcode_43
        .word   opcode_44
        .word   opcode_45
        .word   opcode_46
        .word   opcode_47
        .word   opcode_48
        .word   opcode_49
        .word   opcode_4A
        .word   opcode_4B
        .word   opcode_4C
        .word   opcode_4D
        .word   opcode_4E
        .word   opcode_4F
        .word   opcode_50
        .word   opcode_51
        .word   opcode_52
        .word   opcode_53
        .word   opcode_54
        .word   opcode_55
        .word   opcode_56
        .word   opcode_57
        .word   opcode_58
        .word   opcode_59
        .word   opcode_5A
        .word   opcode_5B
        .word   opcode_5C
        .word   opcode_5D
        .word   opcode_5E
        .word   opcode_5F
        .word   opcode_60
        .word   opcode_61
        .word   opcode_62
        .word   opcode_63
        .word   opcode_64
        .word   opcode_65
        .word   opcode_66
        .word   opcode_67
        .word   opcode_68
        .word   opcode_69
        .word   opcode_6A
        .word   opcode_6B
        .word   opcode_6C
        .word   opcode_6D
        .word   opcode_6E
        .word   opcode_6F
        .word   opcode_70
        .word   opcode_71
        .word   opcode_72
        .word   opcode_73
        .word   opcode_74
        .word   opcode_75
        .word   opcode_76
        .word   opcode_77
        .word   opcode_78
        .word   opcode_79
        .word   opcode_7A
        .word   opcode_7B
        .word   opcode_7C
        .word   opcode_7D
        .word   opcode_7E
        .word   opcode_7F
        .word   opcode_80
        .word   opcode_81
        .word   opcode_82
        .word   opcode_83
        .word   opcode_84
        .word   opcode_85
        .word   opcode_86
        .word   opcode_87
        .word   opcode_88
        .word   opcode_89
        .word   opcode_8A
        .word   opcode_8B
        .word   opcode_8C
        .word   opcode_8D
        .word   opcode_8E
        .word   opcode_8F
        .word   opcode_90
        .word   opcode_91
        .word   opcode_92
        .word   opcode_93
        .word   opcode_94
        .word   opcode_95
        .word   opcode_96
        .word   opcode_97
        .word   opcode_98
        .word   opcode_99
        .word   opcode_9A
        .word   opcode_9B
        .word   opcode_9C
        .word   opcode_9D
        .word   opcode_9E
        .word   opcode_9F
        .word   opcode_A0
        .word   opcode_A1
        .word   opcode_A2
        .word   opcode_A3
        .word   opcode_A4
        .word   opcode_A5
        .word   opcode_A6
        .word   opcode_A7
        .word   opcode_A8
        .word   opcode_A9
        .word   opcode_AA
        .word   opcode_AB
        .word   opcode_AC
        .word   opcode_AD
        .word   opcode_AE
        .word   opcode_AF
        .word   opcode_B0
        .word   opcode_B1
        .word   opcode_B2
        .word   opcode_B3
        .word   opcode_B4
        .word   opcode_B5
        .word   opcode_B6
        .word   opcode_B7
        .word   opcode_B8
        .word   opcode_B9
        .word   opcode_BA
        .word   opcode_BB
        .word   opcode_BC
        .word   opcode_BD
        .word   opcode_BE
        .word   opcode_BF
        .word   opcode_C0
        .word   opcode_C1
        .word   opcode_C2
        .word   opcode_C3
        .word   opcode_C4
        .word   opcode_C5
        .word   opcode_C6
        .word   opcode_C7
        .word   opcode_C8
        .word   opcode_C9
        .word   opcode_CA
        .word   opcode_CB
        .word   opcode_CC
        .word   opcode_CD
        .word   opcode_CE
        .word   opcode_CF
        .word   opcode_D0
        .word   opcode_D1
        .word   opcode_D2
        .word   opcode_D3
        .word   opcode_D4
        .word   opcode_D5
        .word   opcode_D6
        .word   opcode_D7
        .word   opcode_D8
        .word   opcode_D9
        .word   opcode_DA
        .word   opcode_DB
        .word   opcode_DC
        .word   opcode_DD
        .word   opcode_DE
        .word   opcode_DF
        .word   opcode_E0
        .word   opcode_E1
        .word   opcode_E2
        .word   opcode_E3
        .word   opcode_E4
        .word   opcode_E5
        .word   opcode_E6
        .word   opcode_E7
        .word   opcode_E8
        .word   opcode_E9
        .word   opcode_EA
        .word   opcode_EB
        .word   opcode_EC
        .word   opcode_ED
        .word   opcode_EE
        .word   opcode_EF
        .word   opcode_F0
        .word   opcode_F1
        .word   opcode_F2
        .word   opcode_F3
        .word   opcode_F4
        .word   opcode_F5
        .word   opcode_F6
        .word   opcode_F7
        .word   opcode_F8
        .word   opcode_F9
        .word   opcode_FA
        .word   opcode_FB
        .word   opcode_FC
        .word   opcode_FD
        .word   opcode_FE
        .word   opcode_FF

// **********************************************
// End of Opcode Jump Table
// **********************************************

#if !defined(USE_MULTICORE) && defined(USE_HW_MAILBOX)
// **********************************************
// Start of Event Handler Jump Table
// **********************************************

Event_Handler_Dispatch_Table:

.rept 256
        .word    Event_Handler
.endr
        
// **********************************************
// End of Event Handler Jump Table
// **********************************************
#endif

// **********************************************
// Entry point to emulator
// **********************************************

exec_65tube:
        push    {r0-r12,lr}

        mov     fp, r0                  // set fp (r11) to the 64K 6502 memory array
        ldr     ip, =opcode_jump_table  // set ip (r12) to the base address of the 6502 instruction handler

        ldr     r3, =(0x0000fef8 >> 3)  // initialize the constant in r3

        mov     r4, #0x00               // Initialize r4 (flags) to zero on power up
                                        // This was added to fix a bug where the language transfer
                                        // would fail if exec_65tube was entered with certain
                                        // values of r4 (specifically, with bit 7 = 1).
                                        // This is very strange, because r4 bit 7 is not used!
                                        // To reproduce, initialize r4 to 0x80

#if defined(USE_MULTICORE) || !defined(USE_HW_MAILBOX)
        tst     r1, #1                  // Copy the slowdown bit into r4
        orrne   r4, r4, #SLOW_FLAG
#endif
        
        ldr     r5, =0xffffff00         // initialize the constant in r5

#if defined(USE_HW_MAILBOX)
        mov     r0, #MBOX0_DATAIRQEN    // if the slow flag is not set, then enable mailbox hardware
        ldr     r1, =MBOX0_CONFIG       // to generate interrupts when data is present
        str     r0, [r1]
        mov     r0, #0x80 + 65          // route that to FIQ
        ldr     r1, =FIQCTRL
        str     r0, [r1]
#else        
        mov     r0, #0                  // initialize tube mailbox (between isr and event handler)
        ldr     r1, =tube_mailbox
        ldr     r1, [r1]
        str     r0, [r1]
#endif

        mrs     r0, CPSR                // Enable ARM IRQs and FIQs
        bic     r0, r0, #ARM_INT_MASK
        msr     CPSR_c, r0

handle_rst:
        orr     r4, r4, #4              // Set the I bit
        bic     r4, r4, #8              // Clear the D bit
        add     r0, fp, #0x10000
        ldrh    r0, [r0, #-4]           // Fetch the address in FFFC,FFFD
        add     sl, fp, r0              // Point to this instruction in memory
        FETCH_NEXT_MERGED               // Start executing instructions

exec_65tube_exit:                       // We get here from the Event handler when RST is detected
#if defined(USE_HW_MAILBOX)
        mov     r0, #0                  // disable mailbox hardware to generate interrupts
        ldr     r1, =MBOX0_CONFIG       // when data is present
        str     r0, [r1]
        mov     r0, #0                  // disable FIQ
        ldr     r1, =FIQCTRL
        str     r0, [r1]
#endif        
        msr     CPSR, r2                // re-enable ARM interrupts
        pop     {r0-r12,pc}

#if defined(USE_MULTICORE) || !defined(USE_HW_MAILBOX)
fetch_next_i:
        sub     sl, sl, #1
        
fetch_next:
        mrs     r2, CPSR                  // Preserve the 6502 flags

        tst     r4, #SLOW_FLAG            // Check the slowdown bit
        bne     waste_time                // forward branch will predict as not taken
        
fetch_next_check_event:
#if defined(USE_HW_MAILBOX)
        ldr     r0, =MBOX0_STATUS         // Check the tube mailbox for "events" (e.g. tube irq)
        ldr     r0, [r0]
        tst     r0, #MBOX0_EMPTY
        beq     Event_Handler
#else
        ldr     r0, =tube_mailbox         // Check the tube mailbox for "events" (e.g. tube irq)
        ldr     r0, [r0]
        ldr     r0, [r0]
        tst     r0, #ATTN_MASK
        bne     Event_Handler
#endif
        msr     CPSR_flg, r2              // Restore the 6502 flags

        ldrb    r2, [sl], #1
        ldrb    r0, [sl], #1

#ifdef HISTOGRAM
        HISTOGRAM_65TUBE        
#endif

        ldr     pc, [ip, r2, lsl #2]

fetch_next_check_irq:
        mrs     r2, CPSR                  // Preserve the 6502 flags
        tst     r4, #4                    // Test whether interrupts are enabled (bit 2 = 0)
        ldreq   r0, =tube_irq
        ldreq   r0, [r0]
        eoreq   r0, r0, #1
        tsteq   r0, #1                    // Test for IRQ
        beq     handle_irq
        msr     CPSR_flg, r2              // Restore the 6502 flags
        b       fetch_next

waste_time:
        mov     r0, #250                  // TODO: Recalibrate
waste_time_loop:
        subs    r0, r0, #1
        bne     waste_time_loop
        b       fetch_next_check_event
        
#endif        
        
// 6502 Event handler
// if USE_MULTICORE is defined, we have come here only from fetch_next above.
// if USE_MULTICORE is not defined, we have come here via a branch from the second instruction table

Event_Handler:
#if !defined(USE_MULTICORE) && defined(USE_HW_MAILBOX)
        mrs     r2, CPSR                // preserve the 6502 flags
#endif
        orr     r1, r2, #ARM_INT_MASK           // disable ARM IRQs and FIQs
        msr     CPSR_c, r1
        
#if defined(USE_HW_MAILBOX)
        ldr     r1, =FIQCTRL            // Re-enable ARM Mailbox FIQ interrupt
        mov     r0, #0xC1               // It's safe to do this before reading the mailbox
        str     r0, [r1]                // As ARM FIQ interrupts currently disabled
        ldr     r1, =MBOX0_READ
        ldr     r0, [r1]
        lsr     r0, r0, #4
#else
        ldr     r1, =tube_mailbox
        ldr     r1, [r1]
        ldr     r0, [r1]
#endif
#if !defined(USE_MULTICORE) && defined(USE_HW_MAILBOX)
        bic     ip, ip, #1024           // ack events
        sub     sl, sl, #2              // set the instruction back as we haven't executed it
#endif
#if !defined(USE_HW_MAILBOX)
        bic     r0, r0, #ATTN_MASK      // the attention bit
        str     r0, [r1]                // ack the attention bit
#endif

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
        FETCH_NEXT_MERGED               // go back to executing 6502 instructions

handle_irq:
        msr     CPSR, r2                // Restore the 6502 flags and re-enable ARM interrupts
        INTR    -2, 0
        FETCH_NEXT_MERGED

handle_nmi:
        msr     CPSR, r2                // Restore the 6502 flags and re-enable ARM interrupts
        INTR    -6, 0
        FETCH_NEXT_MERGED

#if !defined(USE_MULTICORE) && defined(USE_HW_MAILBOX)

// In slowdown we patch the event handler dispatch table to a special event handler (below)
//
// This implements the following dispatch pattern
// - Usual checks for events, irq, nmi
// - delay for ~600ns
// - execute one instruction, with bit set in ip to return immediately to event handler
//        
// Differences in making a slowdown singlecore event handler:
// 
// 1. Removed the masking of interrupts
//     two lines
//     we want to keep interrupts possible because we're now in the event handler really often
//     and we're OK if ip is updated by a FIQ because we're going to recheck, next instruction
// 
// 2. Added an attention mask check
//     two lines
//     we skip straight to the time wasting if nothing to do
//     we have to skip over the clearing of the mask in case FIQ sets it concurrently
//     we can afford to run unwanted code in the slow case but we can't afford a race condition
// 
// 3. We added a time wasting loop
//     added 4 lines or so
//     we (redundantly) re-enable interupts first because we don't want to slow down FIQ response
// 
// 4. Replaced postamble of FETCH_NEXT_MERGED
//     we added one line
//     we restore the modified table base pointer so we will re-enter this handler
        
Event_Handler_Single_Core_Slow:
        mrs     r2, CPSR                // preserve the 6502 flags
                                        // interrupts are left enabled
        
        bic     ip, ip, #1024           // ack any events
        sub     sl, sl, #2              // set the instruction back as we haven't executed it

#if defined(USE_HW_MAILBOX)
        ldr     r1, =MBOX0_STATUS       // poll the hardware mailbox status
        ldr     r0, [r1]
        tst     r0, #MBOX0_EMPTY        // test if there are any messages
        bne     noirq2
        
        ldr     r1, =MBOX0_READ         // read the top message off the mailbox
        ldr     r0, [r1]
        lsr     r0, r0, #4
#else        
        ldr     r1, =tube_mailbox
        ldr     r1, [r1]        
        ldr     r0, [r1]
        tst     r0, #ATTN_MASK
        beq     noirq2

        bic     r0, r0, #ATTN_MASK      // the attention bit
        str     r0, [r1]                // ack the attention bit
#endif

        push    {r1-r3,ip,lr}           // r0 contains the tube mailbox value
        bl      tube_io_handler
        pop     {r1-r3,ip,lr}
        
        tst     r0, #4                  // Bit 2 set indicate RST is active
        bne     exec_65tube_exit        // exit immediately if active edge seen

        tst     r0, #2                  // Bit 1 set indicate NMI is active
        bne     handle_nmi2             // Take the NMI interrupt

        tst     r4, #4                  // Test whether interrupts are enabled (bit 2 = 0)
        bne     noirq2
        tst     r0, #1                  // Bit 1 set indicate IRQ is active
        bne     handle_irq2             // Take the IRQ interrupt

noirq2:

        mov     r0, #250
waste_time:
        subs    r0, r0, #1
        bne     waste_time

        msr     CPSR, r2                // restore the 6502 flags        

execute_one_instruction:
        ldrb    r2, [sl], #1            // same as FETCH_NEXT_MERGED but set bit in ip to return immediately to event handler
        ldrb    r0, [sl], #1
        ldr     lr, [ip, r2, lsl #2]
        orr     ip, ip, #1024           // Return to event handler after next instruction dispatch        
        mov     pc, lr

handle_irq2:
        msr     CPSR, r2                // Restore the 6502 flags and re-enable ARM interrupts
        INTR    -2, 0
        b       execute_one_instruction

handle_nmi2:
        msr     CPSR, r2                // Restore the 6502 flags and re-enable ARM interrupts
        INTR    -6, 0
        b       execute_one_instruction
        
#endif
                
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

        // Note regarding commented out code below
        //
        // The reason for disabling interrupts was to prevent an interrupt
        // notification (via register ip) being "lost" as ip is pushed
        // then later popped. If an interrupt was allowed to occur in between
        // then the pop would overwrite the flag set in register ip by the
        // interrupt handler.
        //
        // In PiTubeDirect, because interrupts are using to provide the host
        // 6502 with read data, interrupt latency is critical. We cannot affort
        // ever to disable interrupts, or reads are late, and the 6502 get
        // incorrect data (or worse, a bus conflict with a later cycle).
        //
        // Instead of disabling interrupts, we have compiled with -ffixed-ip
        // prevent the C compiler using ip (r12). It's normally a valid scratch
        // register.
        //
        // If the C code calls any system libraries (e.g. printf) then it's
        // likely that ip will get corrupted, so we need to be very careful.
        //
        // It's probably woth redesigning the way the ISR notifies the main
        // code to invoke the event handler. This will only cost a few more
        // emulation cycles.

tube_load_handler:
        push    {r0-r3,lr}                // r0 = address, r2 = current cpsr
//      orr     r2, r2, #ARM_INT_MASK     // disable ARM IRQs and FIQs
//      msr     CPSR_c, r2
//      push    {ip}                      // push ip after disabling interrupts to avoid a race condition
        bl      tube_parasite_read
//      pop     {ip}
        str     r0, [sp, #4]              // save result to r1 on the stack
        pop     {r0-r3,pc}                // r2 (cpsr) is preserved

tube_store_handler:
        push    {r0-r3,lr}                // r0 = address, r1 = data, r2 = current cpsr
//      orr     r2, r2, #ARM_INT_MASK     // disable ARM IRQs and FIQs
//      msr     CPSR_c, r2
//      push    {ip}                      // push ip after disabling interrupts to avoid a race condition
        bl      tube_parasite_write
//      pop     {ip}
        pop     {r0-r3,pc}                // r2 (cpsr) is preserved

handle_rti:
        SPOP    r0
        and     r0, r0, #0xff
        and     r4, r4, #0xffffff00
        orr     r4, r4, r0                // preserve the other bits in r4, e.g. the slowdown bit
        and     r4, r4, #R4_MASK
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

// Insert the current literal pool, otherwise constants are to far away and you get a build error
        .ltorg
                
// **********************************************
// Start of opcode implementations
// **********************************************

// Opcode 03 - NOP
// Opcode 0B - NOP
// Opcode 13 - NOP
// Opcode 1B - NOP
// Opcode 23 - NOP
// Opcode 2B - NOP
// Opcode 33 - NOP
// Opcode 3B - NOP
// Opcode 43 - NOP
// Opcode 4B - NOP
// Opcode 53 - NOP
// Opcode 5B - NOP
// Opcode 63 - NOP
// Opcode 6B - NOP
// Opcode 73 - NOP
// Opcode 7B - NOP
// Opcode 83 - NOP
// Opcode 8B - NOP
// Opcode 93 - NOP
// Opcode 9B - NOP
// Opcode A3 - NOP
// Opcode AB - NOP
// Opcode B3 - NOP
// Opcode BB - NOP
// Opcode C3 - NOP
// Opcode D3 - NOP
// Opcode E3 - NOP
// Opcode EA - NOP
// Opcode FB - NOP
opcode_03:
opcode_0B:
opcode_13:
opcode_1B:
opcode_23:
opcode_2B:
opcode_33:
opcode_3B:
opcode_43:
opcode_4B:
opcode_53:
opcode_5B:
opcode_63:
opcode_6B:
opcode_73:
opcode_7B:
opcode_83:
opcode_8B:
opcode_93:
opcode_9B:
opcode_A3:
opcode_AB:
opcode_B3:
opcode_BB:
opcode_C3:
opcode_D3:
opcode_E3:
opcode_EA:
opcode_EB:
opcode_F3:
opcode_FB:
        sub     sl, sl, #1
        FETCH_NEXT_MERGED

// Opcode 02 - NOP #$00
// Opcode 22 - NOP #$00
// Opcode 42 - NOP #$00
// Opcode 44 - NOP $00
// Opcode 54 - NOP $00,X
// Opcode 62 - NOP #$00
// Opcode 82 - NOP #$00
// Opcode C2 - NOP #$00
// Opcode D4 - NOP $00,X
// Opcode E2 - NOP #$00
// NOP_2
opcode_02:
opcode_22:
opcode_42:
opcode_44:
opcode_54:
opcode_62:
opcode_82:
opcode_C2:
opcode_D4:
opcode_E2:
opcode_F4:
        FETCH_NEXT_MERGED

// Opcode 5C - NOP $0000
// Opcode DC - NOP $0000
// Opcode FC - NOP $0000
// NOP_3
opcode_5C:
opcode_DC:
opcode_FC:
        add     sl, sl, #1
        FETCH_NEXT_MERGED

// Opcode 00 - BRK
opcode_00:
        // BRK is really a two byte instruction, so no fixup needed
        INTR    -2, 1
        FETCH_NEXT_MERGED

// Opcode 01 - ORA ($00,X)
opcode_01:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 04 - TSB $00
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
opcode_05:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 06 - ASL $00
opcode_06:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #1
        strb    r1, [fp, r0]
        tst     r5, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode 07 - RMB0 $00
opcode_07:
        RMB 0

// Opcode 08 - PHP
opcode_08:
        FETCH_NEXT_STAGE_1_I
        orr     r0, r4, #0x30
        orrmi   r0, r0, #0x80
        orrcs   r0, r0, #1
        orreq   r0, r0, #2
        SPUSH   r0
        FETCH_NEXT_STAGE_2_I

// Opcode 09 - ORA #$00
opcode_09:
        FETCH_NEXT_STAGE_0
        lsl     r0, r0, #24
        orrs    r6, r6, r0
        FETCH_NEXT_STAGE_12

// Opcode 0A - ASL A
opcode_0A:
        FETCH_NEXT_STAGE_1_I
        lsls    r6, r6, #1
        FETCH_NEXT_STAGE_2_I

// Opcode 0C - TSB $0000
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
opcode_0D:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2

// Opcode 0E - ASL $0000
opcode_0E:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        lsl     r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        tst     r5, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode 0F - BBR0
opcode_0F:
        BBR 0

// Opcode 10 - BPL
opcode_10:
        sxtabpl sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode 11 - ORA ($00),Y
opcode_11:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 12 - ORA ($00)
opcode_12:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 14 - TRB $00
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
opcode_15:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 16 - ASL $00, X
opcode_16:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #1
        strb    r1, [fp, r0, lsr #24]
        tst     r5, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode 17 - RMB1 $00
opcode_17:
        RMB 1

// Opcode 18 - CLC
opcode_18:
        FETCH_NEXT_STAGE_1_I
        mrs     r0, CPSR
        bic     r0, r0, #C_FLAG
        msr     CPSR_flg, r0
        FETCH_NEXT_STAGE_2_I

// Opcode 19 - ORA $0000,Y
opcode_19:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2

// Opcode 1A - INC A
opcode_1A:
        FETCH_NEXT_STAGE_1_I
        add     r6, r6, #0x1000000
        teq     r6, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 1C - TRB $0000
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
opcode_1D:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        orrs    r6, r6, r1
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2

// Opcode 1E - ASL $0000,X
opcode_1E:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        lsl     r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        tst     r5, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode 1F - BBR1
opcode_1F:
        BBR 1

// Opcode 20 - JSR $0000
opcode_20:
        ldrh    r1, [sl, #-1]
        sub     r9, r9, #1
        strh    sl, [r9], #-1
        add     sl, fp, r1
        FETCH_NEXT_STAGE_0
        orr     r9, #0x0100
        FETCH_NEXT_STAGE_12

// Opcode 21 - AND ($00,X)
opcode_21:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 24 - BIT $00
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
opcode_25:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 26 - ROL $00
opcode_26:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        adc     r1, r1, r1
        strb    r1, [fp, r0]
        lsls    r0, r1, #24
        FETCH_NEXT_STAGE_12

// Opcode 27 - RMB2 $00
opcode_27:
        RMB 2

// Opcode 28 - PLP
opcode_28:
        sub     sl, sl, #1  // fix up FETCH_NEXT reading ahead two bytes
        SPOP    r0
        and     r0, r0, #0xff
        and     r4, r4, #0xffffff00
        orr     r4, r4, r0           // preserve the other bits in r4, e.g. the slowdown bit
        and     r4, r4, #R4_MASK
        mrs     r2, CPSR
        bic     r2, #(N_FLAG + Z_FLAG + C_FLAG)
        and     r1, r0, #0x80
        orr     r2, r2, r1, lsl #24
        and     r1, r0, #0x03
        orr     r2, r2, r1, lsl #29
        msr     CPSR_flg, r2
        FETCH_NEXT_CHECK_IRQ

// Opcode 29 - AND     #$00
opcode_29:
        FETCH_NEXT_STAGE_0
        lsl     r0, r0, #24
        ands    r6, r6, r0
        FETCH_NEXT_STAGE_12

// Opcode 2A - ROL A
opcode_2A:
        FETCH_NEXT_STAGE_1_I
        orrcs   r6, r6, #0x800000
        lsls    r6, r6, #1
        FETCH_NEXT_STAGE_2_I

// Opcode 2C - BIT $0000
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
opcode_2D:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 2E - ROL $0000
opcode_2E:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        adc     r1, r1, r1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsls    r0, r1, #24
        FETCH_NEXT_STAGE_12

// Opcode 2F - BBR2
opcode_2F:
        BBR 2

// Opcode 30 - BMI
opcode_30:
        sxtabmi sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode 31 - AND ($00),Y
opcode_31:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 32 - AND ($00)
opcode_32:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 34 - BIT $00,X
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
opcode_35:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 36 - ROL $00,X
opcode_36:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        adc     r1, r1, r1
        strb    r1, [fp, r0, lsr #24]
        lsls    r0, r1, #24
        FETCH_NEXT_STAGE_12

// Opcode 37 - RMB3 $00
opcode_37:
        RMB 3

// Opcode 38 - SEC
opcode_38:
        FETCH_NEXT_STAGE_1_I
        mrs     r0, CPSR
        orr     r0, r0, #C_FLAG
        msr     CPSR_flg, r0
        FETCH_NEXT_STAGE_2_I

// Opcode 39 - AND $0000,Y
opcode_39:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 3A - DEC A
opcode_3A:
        FETCH_NEXT_STAGE_1_I
        sub     r6, r6, #0x1000000
        teq     r6, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 3C - BIT $0000,X
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
opcode_3D:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        ands    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 3E - ROL $0000,X
opcode_3E:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        adc     r1, r1, r1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsls    r0, r1, #24
        FETCH_NEXT_STAGE_12

// Opcode 3F - BBR3
opcode_3F:
        BBR 3

// Opcode 40 - RTI
opcode_40:
        b       handle_rti

// Opcode 41 - EOR ($00,X)
opcode_41:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 45 - EOR $00
opcode_45:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 46 - LSR $00
opcode_46:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        lsrs    r1, r1, #1
        strb    r1, [fp, r0]
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode 47 - RMB4 $00
opcode_47:
        RMB 4

// Opcode 48 - PHA
opcode_48:
        FETCH_NEXT_STAGE_1_I
        lsr     r0, r6, #24
        SPUSH   r0
        FETCH_NEXT_STAGE_2_I

// Opcode 49 - EOR #$00
opcode_49:
        FETCH_NEXT_STAGE_0
        lsl     r0, r0, #24
        eors    r6, r6, r0
        FETCH_NEXT_STAGE_12

// Opcode 4A - LSR A
opcode_4A:
        FETCH_NEXT_STAGE_1_I
        lsrs    r0, r6, #25
        lsl     r6, r0, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 4C - JMP $0000
opcode_4C:
        EA_ABSOLUTE
        add     sl, r0, fp
        FETCH_NEXT_MERGED

// Opcode 4D - EOR $0000
opcode_4D:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 4E - LSR $0000
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
opcode_4F:
        BBR 4


// Opcode 50 - BVC
opcode_50:
        mrs     r2, CPSR
        tst     r4, #0x40
        sxtabeq sl, sl, r0
        msr     CPSR_flg, r2
        FETCH_NEXT_MERGED

// Opcode 51 - EOR ($00),Y
opcode_51:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 52 - EOR ($00)
opcode_52:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 55 - EOR $00,X
opcode_55:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_2

// Opcode 56 - LSR $00,X
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
opcode_57:
        RMB 5

// Opcode 58 - CLI
opcode_58:
        sub     sl, sl, #1  // fix up FETCH_NEXT reading ahead two bytes
        bic     r4, r4, #4
        FETCH_NEXT_CHECK_IRQ

// Opcode 59 - EOR $0000,Y
opcode_59:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 5A - PHY
opcode_5A:
        FETCH_NEXT_STAGE_1_I
        lsr     r0, r8, #24
        SPUSH   r0
        FETCH_NEXT_STAGE_2_I

// Opcode 5D - EOR $0000,X
opcode_5D:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r1, r1, #24
        eors    r6, r6, r1
        FETCH_NEXT_STAGE_12

// Opcode 5E - LSR $0000,X
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
opcode_5F:
        BBR 5

// Opcode 60 - RTS
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
opcode_61:
        EA_INDIRECT_X
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r5
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 64 - STZ $00
opcode_64:
        FETCH_NEXT_STAGE_0
        strb    r5, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 65 - ADC $00
opcode_65:
        ldrb    r1, [fp, r0]
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r5
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 66 - ROR $00
opcode_66:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 67 - RMB6 $00
opcode_67:
        RMB 6

// Opcode 68 - PLA
opcode_68:
        FETCH_NEXT_STAGE_1_I
        SPOP    r6
        lsl     r6, r6, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 69 - ADC #$00
opcode_69:
        mov     r1, r0  // TODO - could probably optimise this away
        tst     r4, #8
        bne     decimal_mode_adc   
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r5
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 6A - ROR A
opcode_6A:
        FETCH_NEXT_STAGE_1_I
        orrcs   r6, r6, #1
        rors    r0, r6, #25
        lsl     r6, r0, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 6C - JMP ($0000)
opcode_6C:
        ldrb    r1, [sl], #0
        add     r0, r0, r1, lsl #8
        ldrb    r1, [r0, fp]!
        ldrb    r0, [r0, #1]
        add     sl, r1, r0, lsl #8
        add     sl, sl, fp
        FETCH_NEXT_MERGED

// Opcode 6D - ADC $0000
opcode_6D:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r5
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 6E - ROR $0000
opcode_6E:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 6F - BBR6
opcode_6F:
        BBR 6

// Opcode 70 - BVS
opcode_70:
        mrs     r2, CPSR
        tst     r4, #0x40
        sxtabne sl, sl, r0
        msr     CPSR_flg, r2
        FETCH_NEXT_MERGED

// Opcode 71 - ADC ($00),Y
opcode_71:
        EA_INDIRECT_Y
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r5
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 72 - ADC ($00)
opcode_72:
        EA_INDIRECT
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r5
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 74 - STZ $00,X
opcode_74:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        strb    r5, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_12

// Opcode 75 - ADC $00,X
opcode_75:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r5
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 76 - ROR $00,X
opcode_76:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        strb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_12

// Opcode 77 - RMB7 $00
opcode_77:
        RMB 7

// Opcode 78 - SEI
opcode_78:
        FETCH_NEXT_STAGE_1_I
        orr     r4, r4, #4
        FETCH_NEXT_STAGE_2_I

// Opcode 79 - ADC $0000,Y
opcode_79:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r5
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 7A - PLY
opcode_7A:
        FETCH_NEXT_STAGE_1_I
        SPOP    r8
        lsl     r8, r8, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 7C - JMP ($0000,X)
opcode_7C:
        EA_ABSOLUTE_INDEXED r7
        ldrb    r1, [r0, fp]!
        ldrb    r0, [r0, #1]
        add     sl, r1, r0, lsl #8
        add     sl, sl, fp
        FETCH_NEXT_MERGED

// Opcode 7D - ADC $0000,X
opcode_7D:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_adc
        FETCH_NEXT_STAGE_0
        orrcs   r1, r1, r5
        adcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode 7E - ROR $0000,X
opcode_7E:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 7F - BBR7
opcode_7F:
        BBR 7

// Opcode 80 - BRA
opcode_80:
        sxtab   sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode 81 - STA ($00,X)
opcode_81:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        lsr     r1, r6, #24
        STORE_INDIRECT
        FETCH_NEXT_STAGE_12

// Opcode 84 - STY $00
opcode_84:
        FETCH_NEXT_STAGE_0
        lsr     r1, r8, #24
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 85 - STA $00
opcode_85:
        FETCH_NEXT_STAGE_0
        lsr     r1, r6, #24
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 86 - STX $00
opcode_86:
        FETCH_NEXT_STAGE_0
        lsr     r1, r7, #24
        strb    r1, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 87 - SMB0 $00
opcode_87:
        SMB 0

// Opcode 88 - DEY
opcode_88:
        FETCH_NEXT_STAGE_1_I
        sub     r8, r8, #0x1000000
        teq     r8, #0
        FETCH_NEXT_STAGE_2_I

// Opcode 89 - BIT #$00
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
opcode_8A:
        FETCH_NEXT_STAGE_1_I
        movs    r6, r7
        FETCH_NEXT_STAGE_2_I

// Opcode 8C - STY $0000
opcode_8C:
        EA_ABSOLUTE
        lsr     r1, r8, #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 8D - STA $0000
opcode_8D:
        EA_ABSOLUTE
        lsr     r1, r6, #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 8E - STX $0000
opcode_8E:
        EA_ABSOLUTE
        lsr     r1, r7, #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 8F - BBS0
opcode_8F:
        BBS 0

// Opcode 90 - BCC
opcode_90:
        sxtabcc sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode 91 - STA ($00),Y
opcode_91:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        lsr     r1, r6, #24
        STORE_INDIRECT
        FETCH_NEXT_STAGE_12

// Opcode 92 - STA ($00)
opcode_92:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        lsr     r1, r6, #24
        STORE_INDIRECT
        FETCH_NEXT_STAGE_12

// Opcode 94 - STY $00,X
opcode_94:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        lsr     r1, r8, #24
        strb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_12

// Opcode 95 - STA $00,X
opcode_95:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        lsr     r1, r6, #24
        strb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_12

// Opcode 96 - STX $00,Y
opcode_96:
        FETCH_NEXT_STAGE_0
        add     r0, r8, r0, lsl #24
        lsr     r1, r7, #24
        strb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_12

// Opcode 97 - SMB1 $00
opcode_97:
        SMB 1

// Opcode 98 - TYA
opcode_98:
        FETCH_NEXT_STAGE_1_I
        movs    r6, r8
        FETCH_NEXT_STAGE_2_I

// Opcode 99 - STA $0000,Y
opcode_99:
        EA_ABSOLUTE_INDEXED r8
        lsr     r1, r6, #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 9A - TXS
opcode_9A:
        FETCH_NEXT_STAGE_1_I
        add     r9, fp, #0x100
        add     r9, r9, r7, lsr #24
        FETCH_NEXT_STAGE_2_I

// Opcode 9C - STZ $0000
opcode_9C:
        EA_ABSOLUTE
        FETCH_NEXT_STAGE_0
        strb    r5, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 9D - STA $0000,X
opcode_9D:
        EA_ABSOLUTE_INDEXED r7
        lsr     r1, r6, #24
        STORE_ABSOLUTE
        FETCH_NEXT_MERGED

// Opcode 9E - STZ $0000,X
opcode_9E:
        EA_ABSOLUTE_INDEXED r7
        FETCH_NEXT_STAGE_0
        strb    r5, [fp, r0]
        FETCH_NEXT_STAGE_12

// Opcode 9F - BBS1
opcode_9F:
        BBS 1

// Opcode A0 - LDY #$00
opcode_A0:
        FETCH_NEXT_STAGE_0
        lsl     r8, r0, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_12

// Opcode A1 - LDA ($00,X)
opcode_A1:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2

// Opcode A2 - LDX #$00
opcode_A2:
        FETCH_NEXT_STAGE_0
        lsl     r7, r0, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_12

// Opcode A4 - LDY $00
opcode_A4:
        FETCH_NEXT_STAGE_0
        ldrb    r8, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r8, r8, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_2

// Opcode A5 - LDA $00
opcode_A5:
        FETCH_NEXT_STAGE_0
        ldrb    r6, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r6, r6, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2

// Opcode A6 - LDX $00
opcode_A6:
        FETCH_NEXT_STAGE_0
        ldrb    r7, [fp, r0]
        FETCH_NEXT_STAGE_1
        lsl     r7, r7, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_2

// Opcode A7 - SMB2 $00
opcode_A7:
        SMB 2

// Opcode A8 - TAY
opcode_A8:
        FETCH_NEXT_STAGE_1_I
        movs    r8, r6
        FETCH_NEXT_STAGE_2_I

// Opcode A9 - LDA #$00
opcode_A9:
        FETCH_NEXT_STAGE_0
        lsl     r6, r0, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_12

// Opcode AA - TAX
opcode_AA:
        FETCH_NEXT_STAGE_1_I
        movs    r7, r6
        FETCH_NEXT_STAGE_2_I

// Opcode AC - LDY $0000
opcode_AC:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r8, r1, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_12

// Opcode AD - LDA $0000
opcode_AD:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_12

// Opcode AE - LDX $0000
opcode_AE:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r7, r1, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_12

// Opcode AF - BBS2
opcode_AF:
        BBS 2

// Opcode B0 - BCS
opcode_B0:
        sxtabcs sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode B1 - LDA ($00),Y
opcode_B1:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2

// Opcode B2 - LDA ($00)
opcode_B2:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2

// Opcode B4 - LDY $00,X
opcode_B4:
        add     r0, r7, r0, lsl #24
        ldrb    r8, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_0
        lsl     r8, r8, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2

// Opcode B5 - LDA $00,X
opcode_B5:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        ldrb    r6, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        lsl     r6, r6, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_2

// Opcode B6 - LDX $00,Y
opcode_B6:
        FETCH_NEXT_STAGE_0
        add     r0, r8, r0, lsl #24
        ldrb    r7, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        lsl     r7, r7, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_2

// Opcode B7 - SMB3 $00
opcode_B7:
        SMB 3

// Opcode B8 - CLV
opcode_B8:
        FETCH_NEXT_STAGE_1_I
        bic     r4, #0x40
        FETCH_NEXT_STAGE_2_I

// Opcode B9 - LDA $0000,Y
opcode_B9:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_12

// Opcode BA - TSX
opcode_BA:
        FETCH_NEXT_STAGE_1_I
        lsl     r7, r9, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_2_I

// Opcode BC - LDY $0000,X
opcode_BC:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r8, r1, #24
        teq     r8, #0
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2

// Opcode BD - LDA $0000,X
opcode_BD:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r6, r1, #24
        teq     r6, #0
        FETCH_NEXT_STAGE_12

// Opcode BE - LDX $0000,Y
opcode_BE:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        lsl     r7, r1, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_12

// Opcode BF - BBS3
opcode_BF:
        BBS 3

// Opcode C0 - CPY #$00
opcode_C0:
        FETCH_NEXT_STAGE_0
        cmp     r8, r0, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode C1 - CMP ($00,X)
opcode_C1:
        EA_INDIRECT_X
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode C4 - CPY $00
opcode_C4:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        cmp     r8, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode C5 - CMP $00
opcode_C5:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode C6 - DEC $00
opcode_C6:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        sub     r1, r1, #1
        strb    r1, [fp, r0]
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode C7 - SMB4 $00
opcode_C7:
        SMB 4

// Opcode C8 - INY
opcode_C8:
        FETCH_NEXT_STAGE_1_I
        add     r8, r8, #0x1000000
        teq     r8, #0
        FETCH_NEXT_STAGE_2_I

// Opcode C9 - CMP #$00
opcode_C9:
        FETCH_NEXT_STAGE_0
        cmp     r6, r0, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode CA - DEX
opcode_CA:
        FETCH_NEXT_STAGE_1_I
        sub     r7, r7, #0x1000000
        teq     r7, #0
        FETCH_NEXT_STAGE_2_I

// Opcode CB - WAI
opcode_CB:
        // TODO - Implement this
        FETCH_NEXT_STAGE_1_I
        FETCH_NEXT_STAGE_2_I

// Opcode CC - CPY $0000
opcode_CC:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        cmp     r8, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode CD - CMP $0000
opcode_CD:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode CE - DEC $0000
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
opcode_CF:
        BBS 4

// Opcode D0 - BNE
opcode_D0:
        sxtabne sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode D1 - CMP ($00),Y
opcode_D1:
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode D2 - CMP ($00)
opcode_D2:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_INDIRECT
        FETCH_NEXT_STAGE_1
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode D5 - CMP $00,X
opcode_D5:
        FETCH_NEXT_STAGE_0
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        FETCH_NEXT_STAGE_1
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode D6 - DEC $00,X
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
opcode_D7:
        SMB 5

// Opcode D8 - CLD
opcode_D8:
        FETCH_NEXT_STAGE_1_I
        bic     r4, r4, #8
        FETCH_NEXT_STAGE_2_I

// Opcode D9 - CMP $0000,Y
opcode_D9:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode DA - PHX
opcode_DA:
        FETCH_NEXT_STAGE_1_I
        lsr     r0, r7, #24
        SPUSH   r0
        FETCH_NEXT_STAGE_2_I

// Opcode DB - STP
opcode_DB:
        // TODO - Implement this
        FETCH_NEXT_STAGE_1_I
        FETCH_NEXT_STAGE_2_I

// Opcode DD - CMP $0000,X
opcode_DD:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        cmp     r6, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode DE - DEX $0000,X
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
opcode_DF:
        BBS 5

// Opcode E0 - CPX #$00
opcode_E0:
        FETCH_NEXT_STAGE_0
        cmp     r7, r0, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode E1 - SBC ($00,X)
opcode_E1:
        EA_INDIRECT_X
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r5
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode E4 - CPX $00
opcode_E4:
        FETCH_NEXT_STAGE_0
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_1
        cmp     r7, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode E5 - SBC $00
opcode_E5:
        ldrb    r1, [fp, r0]
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r5
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode E6 - INC $00
opcode_E6:
        ldrb    r1, [fp, r0]
        FETCH_NEXT_STAGE_0
        add     r1, r1, #1
        strb    r1, [fp, r0]
        lsl     r0, r1, #24
        teq     r0, #0
        FETCH_NEXT_STAGE_12

// Opcode E7 - SMB6 $00
opcode_E7:
        SMB 6

// Opcode E8 - INX
opcode_E8:
        FETCH_NEXT_STAGE_1_I
        add     r7, r7, #0x1000000
        teq     r7, #0
        FETCH_NEXT_STAGE_2_I

// Opcode E9 - SBC #$00
opcode_E9:
        mov     r1, r0   // TODO - could probably optimise this away
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r5
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode EC - CPX $0000
opcode_EC:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        FETCH_NEXT_STAGE_0
        cmp     r7, r1, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode ED - SBC $0000
opcode_ED:
        EA_ABSOLUTE
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r5
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode EE - INC $0000
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
opcode_EF:
        BBS 6

// Opcode F0 - BEQ
opcode_F0:
        sxtabeq sl, sl, r0
        FETCH_NEXT_MERGED

// Opcode F1 - SBC ($00),Y
opcode_F1:
        EA_INDIRECT_Y
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r5
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode F2 - SBC ($00)
opcode_F2:
        EA_INDIRECT
        LOAD_INDIRECT
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r5
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode F5 - SBC $00,X
opcode_F5:
        add     r0, r7, r0, lsl #24
        ldrb    r1, [fp, r0, lsr #24]
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r5
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode F6 - INC $00,X
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
opcode_F7:
        SMB 7

// Opcode F8 - SED
opcode_F8:
        FETCH_NEXT_STAGE_1_I
        orr     r4, r4, #8
        FETCH_NEXT_STAGE_2_I

// Opcode F9 - SBC $0000,Y
opcode_F9:
        EA_ABSOLUTE_INDEXED r8
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r5
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode FA - PLX
opcode_FA:
        FETCH_NEXT_STAGE_1_I
        SPOP    r7
        lsl     r7, r7, #24
        teq     r7, #0
        FETCH_NEXT_STAGE_2_I

// Opcode FD - SBC $0000,X
opcode_FD:
        EA_ABSOLUTE_INDEXED r7
        LOAD_ABSOLUTE
        tst     r4, #8
        bne     decimal_mode_sbc
        FETCH_NEXT_STAGE_0
        orrcc   r1, r1, r5
        sbcs    r6, r6, r1, ror #8
        FETCH_NEXT_STAGE_1
        bicvc   r4, #0x40
        orrvs   r4, #0x40
        FETCH_NEXT_STAGE_2

// Opcode FE - INC $0000,X
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
opcode_FF:
        BBS 7

// **********************************************
// End of opcode implementations
// **********************************************
                
// **********************************************
// Start of 64K of 6502 RAM
// **********************************************
        
.balign 0x10000
mpu_memory:
.rept 16384
.word 0
.endr

// **********************************************
// End of 64K of 6502 RAM
// **********************************************
        
