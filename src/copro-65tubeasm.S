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
// tregs       = a constant value 0f 0x0000FEF8 >> 3 (for tube register address checking)
// flags       = 6502 flags (V, D and I bits maintained here, CZN use ARM flags)
// const       = a constant value of 0xFFFFFF00
// r6    regA       = 6502 A
// r7    regX       = 6502 X
// r8    regY       = 6502 Y
// regSP       = 6502 SP
// regPC (sl) = 6502 PC
// memry (memry) = base address of 64KB of flat memory
// r12 (instt) = base address of 64-byte aligned instruction implementations
// r13 (sp) = normal ARM usage
// r14 (lr) = normal ARM usage
// r15 (pc) = normal ARM usage

#define temp0     r0
#define temp1     r1
#define temp2     r2
#define tregs     r3
#define flags     r4
#define const     r5
#define regA      r6
#define regX      r7
#define regY      r8
#define regSP     r9
#define regPC     r10   
#define memry     r11   
#define instt     r12

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
//        ldrb    r0, [regPC], #1
//        add     pc, instt, r0, lsl #I_ALIGN
//
// This gives us a nice hook into the instruction handler
// When the ARM IRQ detects a valid interrupt
// it adds a constant ( 256 << I_ALIGN) to instt to move into a second instruction handler
// This instruction handler can just be 256 Branches to the Event handler
// this code is executed so rarely absolute performance isn.t an issue and we aren.t worried about it fitting in the cache


// A check needs to be that more than than one event isn.t being added. This can be
// simplified if we align the code to a 0 boundary and just orr instt,instt,#256<<I_ALIGN
//
// once we are in the event handler we do
// disable arm IRQs // this prevents us missing events
// bic instt,instt,#256<<I_ALIGN
// reenable IRQs // now if another event happens we will service it at the next instruction boundary
// sub regPC,regPC,#1 // set instruction pointer ready to be stacked
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

// Mask used when poping status into flags (RTI, PLP)
// It selects the bits that are managed in flags, i.e. V, D, I
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

.macro FETCH_NEXT_STAGE_0_I
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

.macro FETCH_NEXT_STAGE_012
        b       fetch_next
.endm

// Macro for fetching the next instruction and also checking for any pending IRQs
// Used for instructions that can enable interrupts: CLI, PLP, RTI
.macro FETCH_NEXT_CHECK_IRQ
        b       fetch_next_check_irq
.endm
        
#else

// one byte instructions

.macro FETCH_NEXT_STAGE_0_I
        ldr     lr, [instt, r0, lsl #2]
.endm

.macro FETCH_NEXT_STAGE_1_I
        mov     r0, r5
        ldrb    r5, [regPC], #1 
.endm

.macro FETCH_NEXT_STAGE_01_I
        FETCH_NEXT_STAGE_0_I
        FETCH_NEXT_STAGE_1_I
.endm
        
// Macros for fetching the next instruction, for two bytes

.macro FETCH_NEXT_STAGE_0
        ldr     lr, [instt, r5, lsl #2]
.endm

.macro FETCH_NEXT_STAGE_1
        ldrb    r0, [regPC], #1
        ldrb    r5, [regPC], #1   
.endm

.macro FETCH_NEXT_STAGE_1a_OO
        ldrb    r5, [regPC, #1] 
.endm

.macro FETCH_NEXT_STAGE_1b_OO
        ldrb    r0, [regPC], #2   
.endm
        
.macro FETCH_NEXT_STAGE_2
        mov     pc, lr
.endm

.macro FETCH_NEXT_STAGE_01
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1  
.endm


.macro FETCH_NEXT_STAGE_12
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2  
.endm

.macro FETCH_NEXT_STAGE_012
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2        
.endm

// and three byte instructions

.macro FETCH_NEXT_3B_STAGE_0
         ldrb    r5, [regPC], #1    
.endm
          
.macro FETCH_NEXT_3B_MERGED
        FETCH_NEXT_3B_STAGE_0
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_2        
.endm

// Macro for fetching the next instruction and also checking for any pending IRQs
// Used for instructions that can enable interrupts: CLI, PLP , RTI
.macro CHECK_IRQ restoreflags=none
.if \restoreflags==mrs        
        mrs     r2, CPSR                  // Preserve the 6502 flags
.endif
        tst     flags, #4                    // Test whether interrupts are enabled (bit 2 = 0)
        ldreq   r0, =tube_irq
        ldreq   r0, [r0]
        eoreq   r0, r0, #1
        tsteq   r0, #1                    // Test for IRQ
        beq     handle_irq_adjust_regPC
        msr     CPSR_flg, r2              // Restore the 6502 flags
.endm

#endif

// Macro for pushing a byte to the stack, whilst correctly handing wrapping
// This relies on the 6502 memory being 64KB aligned
// Optimisations from dp11 on stardot
.macro SPUSH reg
        strb    \reg, [regSP], #-1
        orr     regSP, #0x0100
        // The original code was just this:
        // strb    \reg, [regSP], #-1
.endm

// Macro for poping a byte from the stack, whilst correctly handing wrapping
// This relies on the 6502 memory being 64KB aligned
// Optimisations from dp11 on stardot
.macro SPOP reg
        add     regSP, regSP, #1
        bic     regSP, #0xFF00
        ldrb    \reg, [regSP,#0x0100]!
        // The original code was just this:
        // ldrb    \reg, [regSP, #1]!
.endm

.macro SPOP_FETCH_STAGE_0_I reg
        add     regSP, regSP, #1
        bic     regSP, #0xFF00
        FETCH_NEXT_STAGE_0_I
        ldrb    \reg, [regSP,#0x0100]!
        // The original code was just this:
        // ldrb    \reg, [regSP, #1]!
.endm

// Macro for tracing execution
// Add to FETCH_NEXT
.macro TRACE_65TUBE
        mov     r0, regPC
        ldrb    r1, [regPC]
        mrs     r2, CPSR
        push    {r2}
        orr     r2, r2, #ARM_INT_MASK
        msr     CPSR_c, r2        
        push    {tregs-r12}
        bl      copro_65tube_trace
        pop     {tregs-r12}
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
        orr r0,r0,r5,LSL #8
.endm

.macro LOAD_BYTE
        ldrb    r1, [ r0, memry]
.endm        

.macro LOAD_BYTE_FETCH_NEXT_STAGE_0
        LOAD_BYTE
        FETCH_NEXT_STAGE_0
.endm   

.macro STORE_BYTE reg=r1
        strb    \reg, [r0, memry]
.endm   

.macro EA_ABSOLUTE_LOAD
        orr r0,r0,r5,LSL #8
        FETCH_NEXT_3B_STAGE_0
        LOAD_ABSOLUTE
.endm

.macro EA_ABSOLUTE_LOAD_PART
        orr r0,r0,r5,LSL #8
        FETCH_NEXT_3B_STAGE_0
        LOAD_ABSOLUTE_PART
.endm

.macro WRAP_MEMORY
#if defined(CLEAR_MEMORY_OVERFLOW)        
        bic     r0, r0, #0x10000
#endif
.endm  


.macro EA_ABSOLUTE_INDEXED reg
        EA_ABSOLUTE
        add     r0, r0, \reg, lsr #24
        WRAP_MEMORY
.endm

.macro EA_ABSOLUTE_INDEXED_LOAD reg
        EA_ABSOLUTE_INDEXED \reg     
        FETCH_NEXT_3B_STAGE_0
        LOAD_ABSOLUTE
.endm

.macro EA_ABSOLUTE_INDEXED_LOAD_PART reg
        EA_ABSOLUTE_INDEXED \reg
        FETCH_NEXT_3B_STAGE_0
        LOAD_ABSOLUTE_PART
.endm

.macro EA_INDIRECT
        ldrh    r0, [r0, memry]
.endm

.macro EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_BYTE
.endm

.macro EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        EA_INDIRECT_Y
        LOAD_BYTE
.endm

.macro EA_INDIRECT_X
        add     r0, r0, regX, lsr #24
        and     r0, r0, #0xff
        EA_INDIRECT
        LOAD_BYTE
.endm

.macro EA_INDIRECT_X_FETCH_STAGE_0 str=no
        add     r0, r0, regX, lsr #24
        and     r0, r0, #0xff
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
.if \str==no        
        LOAD_BYTE
.endif        
.endm

.macro EA_INDIRECT_Y
        add     r0, r0, regY, lsr #24
        WRAP_MEMORY
.endm

.macro  STORE_ZPX reg=r1
        strb    \reg, [memry, r0, lsr #24]
.endm

.macro  LOAD_ZPX
        ldrb    r1, [memry, r0, lsr #24]
.endm

// Macros for reading/writing memory

.macro LOAD_ABSOLUTE               // r0 holds the absolute address
        LOAD_ABSOLUTE_PART
        msr     CPSR, r2           // restore 6502 flags, including ARM interrupts
.endm

.macro LOAD_ABSOLUTE_PART               // r0 holds the absolute address
        mrs     r2, CPSR           // preserve the 6502 flags
        cmp     tregs, r0, lsr #3     // tregs holds the constant 0xFEF8 >> 3
        ldrneb  r1, [r0, memry]       // normal memory read
        bleq    tube_load_handler  // tube memory access

.endm

.macro STORE_ABSOLUTE              // r0 holds the absolute address
        mrs     r2, CPSR           // preserve the 6502 flags
        STORE_ABSOLUTE_PART
.endm

.macro STORE_ABSOLUTE_PART
        cmp     tregs, r0, lsr #3     // tregs holds the constant 0xFEF8 >> 3
        strneb  r1, [r0, memry]       // normal memory write
        bleq    tube_store_handler // tube memory access
        msr     CPSR, r2           // restore 6502 flags, including ARM interrupts
.endm

// Macros for additional Rockwell 65C02 and WDC 65C02 instructions

.macro BBR bitnum
        LOAD_BYTE
        mrs     r2, CPSR
        tst     r1, #(1 << \bitnum)
        BRANCH_FLAGS  eq ne msr r5
.endm

.macro BBS bitnum
        LOAD_BYTE   
        mrs     r2, CPSR
        tst     r1, #(1 << \bitnum)
        BRANCH_FLAGS  ne eq msr r5
.endm

.macro RMB bitnum
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        bic     r1, #(1 << \bitnum)
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2
.endm

.macro SMB bitnum
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        orr     r1, #(1 << \bitnum)
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2
.endm

.macro INTR vector setbrk                 // vector is -2 for IRQ/BRK or -6 for NMI
        sub     r1, regPC, memry                // setbrk should be 1 for BRK, 0 for IRQ/NMI
        lsr     r0, r1, #8
        SPUSH   r0
        SPUSH   r1
        movpl   r0, #0x20                 // Always set bit 5 (-) in the pushed value
        movmi   r0, #0xA0
        orrcs   r0, r0, #1
        orreq   r0, r0, #2
        orr     r0, r0, flags                // In flags only bits 6 (V), 3 (D) and 2 (I) used; others always zero.
.if \setbrk        
        orr     r0, r0, #0x10             // Set the B bit in the pushed value (in BRK, not in IRQ/NMI)
.endif
        SPUSH   r0
        orr     flags, flags, #4                // Set the I bit in the 6502 status register
        bic     flags, flags, #8                // Clear the D bit in the 6502 status register
        add     r0, memry, #0x10000
        ldrh    r0, [r0, #\vector]        // Fetch the vector address
        add     regPC, memry, r0                // Point to this instruction in memory
.endm

// instruction macros 

.macro  BIT
        FETCH_NEXT_STAGE_01
        lsl     r2, r1, #24
        tst     r2, regA
        and     r1, r1, #0x40
        bic     flags, flags, #0x40
        orr     flags, flags, r1
        and     r2, r2, #N_FLAG
        mrs     r1, CPSR
        bic     r1, r1, #N_FLAG
        orr     r1, r1, r2
        msr     CPSR_flg, r1
        FETCH_NEXT_STAGE_2
.endm

.macro ADC_macro reg=r1      
        tst     flags, #8
        FETCH_NEXT_STAGE_0  
.if \reg==r0        
        bne     decimal_mode_adc_r0
.else
        bne     decimal_mode_adc    
.endif 
             
        orrcs   \reg, \reg, #0xff00
        sxthcs  \reg, \reg
        FETCH_NEXT_STAGE_1a_OO
        adcs    regA, regA, \reg, ror #8
        FETCH_NEXT_STAGE_1b_OO
        bicvc   flags, #0x40
        orrvs   flags, #0x40
        FETCH_NEXT_STAGE_2   
.endm

.macro SBC_macro reg=r1      
        tst     flags, #8
        FETCH_NEXT_STAGE_0 
.if \reg==r0         
        bne     decimal_mode_sbc_r0
.else
        bne     decimal_mode_sbc
.endif 
          
        orrcc   \reg, \reg, #0xff00  
        sxthcc  \reg, \reg
        FETCH_NEXT_STAGE_1a_OO
        sbcs    regA, regA, \reg, ror #8
        FETCH_NEXT_STAGE_1b_OO
        bicvc   flags, #0x40
        orrvs   flags, #0x40
        FETCH_NEXT_STAGE_2     
.endm

.macro BRANCH yes no reg=r0
      
      sxtab\yes regPC, regPC, \reg
      // stall
      ldr\yes\()b  r5, [regPC,#-1]  
      //stall
      //stall
      //stall
      ldr     lr, [ip, r5, lsl #2]
      ldrb    r0, [regPC], #1 //hidden in stall
      ldrb    r5, [regPC], #1 //hidden in stall         
      FETCH_NEXT_STAGE_2            // 5 cycles

      // total of 14 if taken. 10 if not taken and not recovering flags
.endm      

.macro BRANCH_FLAGS yes no flag reg=r0

.if \reg==r5
   
      // if we are using R5 PC is already at the right place 
      sxtab\yes regPC, regPC, \reg
      ldrb      r1, [regPC], #1
      ldrb      r0, [regPC], #1 //hidden in stall
      ldrb      r5, [regPC], #1 //hidden in stall
      ldr       lr, [instt, r1, lsl #2]
.ifb \flag
.else
      msr     CPSR_flg, r2
.endif          
     
.else 
      // if we are using r0 then pc is one place ahead
      sxtab\yes regPC, regPC, \reg
      // stall
      ldr\yes\()b  r5, [regPC,#-1]  
.ifb \flag
.else
      msr     CPSR_flg, r2
.endif  
      //stall
      ldr     lr, [ip, r5, lsl #2]
      ldrb    r0, [regPC], #1 //hidden in stall
      ldrb    r5, [regPC], #1 //hidden in stall 
.endif      
      
      FETCH_NEXT_STAGE_2            // 5 cycles

      // total of 14 if taken. 11 if not taken and not recovering flags
.endm     

.macro LOGICAL_FETCH12 logic
        FETCH_NEXT_STAGE_1
        lsl     r1, r1, #24
        \logic  regA, regA, r1
        FETCH_NEXT_STAGE_2
.endm   


.macro LOGICAL_FETCH012 logic reg=r1
.if \reg==r1
        FETCH_NEXT_STAGE_01
        lsl     r1, r1, #24
        \logic  regA, regA, r1
        FETCH_NEXT_STAGE_2
.else
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        lsl     r1, \reg, #24
        FETCH_NEXT_STAGE_1b_OO
        \logic  regA, regA, r1
        FETCH_NEXT_STAGE_2
.endif        
.endm        

.macro  TSB 
.endm

.macro FLAGS dest=r1 reg=r1
.if \reg==r0
        FETCH_NEXT_STAGE_1a_OO 
        lsl     \dest, \reg, #24
        FETCH_NEXT_STAGE_1b_OO
        teq     \dest, #0
        FETCH_NEXT_STAGE_2 
.else
        FETCH_NEXT_STAGE_1
        lsl     \dest, \reg, #24
        teq     \dest, #0
        FETCH_NEXT_STAGE_2 
.endif
.endm

.macro FLAGS_OO dest=r1 reg=r1
       FETCH_NEXT_STAGE_1b_OO
       lsl     \dest, \reg, #24
       teq     \dest, #0
       FETCH_NEXT_STAGE_2
.endm

.macro  PLP

        SPOP    r0
        mrs     r2, CPSR
        and     flags, flags, #0xffffff00
        orr     flags, flags, r0           // preserve the other bits in flags, e.g. the slowdown bit 
        and     r1, r0, #0x80
        bic     r2, #(N_FLAG + Z_FLAG + C_FLAG)
        orr     r2, r2, r1, lsl #24
        and     r1, r0, #0x03
        and     flags, flags, #R4_MASK
        orr     r2, r2, r1, lsl #29
        //msr     CPSR_flg, r2
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

        mov     memry, r0                  // set memry (memry) to the 64K 6502 memory array
        ldr     instt, =opcode_jump_table  // set instt (r12) to the base address of the 6502 instruction handler

        ldr     tregs, =(0x0000fef8 >> 3)  // initialize the constant in tregs

        mov     flags, #0x00               // Initialize flags (flags) to zero on power up
                                        // This was added to fix a bug where the language transfer
                                        // would fail if exec_65tube was entered with certain
                                        // values of flags (specifically, with bit 7 = 1).
                                        // This is very strange, because flags bit 7 is not used!
                                        // To reproduce, initialize flags to 0x80

#if defined(USE_MULTICORE) || !defined(USE_HW_MAILBOX)
        tst     r1, #1                  // Copy the slowdown bit into flags
        orrne   flags, flags, #SLOW_FLAG
#endif
        

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
        orr     flags, flags, #4              // Set the I bit
        bic     flags, flags, #8              // Clear the D bit
        add     r0, memry, #0x10000
        ldrh    r0, [r0, #-4]           // Fetch the address in FFFC,FFFD
        add     regPC, memry, r0              // Point to this instruction in memory
        FETCH_NEXT_3B_MERGED               // Start executing instructions

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
        sub     regPC, regPC, #2
        
fetch_next:
        mrs     r2, CPSR                  // Preserve the 6502 flags

        tst     flags, #SLOW_FLAG            // Check the slowdown bit
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

        ldrb    r2, [regPC], #1
        ldrb    r0, [regPC], #1

#ifdef HISTOGRAM
        HISTOGRAM_65TUBE        
#endif

        ldr     pc, [instt, r2, lsl #2]

fetch_next_check_irq:
        mrs     r2, CPSR                  // Preserve the 6502 flags
        tst     flags, #4                    // Test whether interrupts are enabled (bit 2 = 0)
        ldreq   r0, =tube_irq
        ldreq   r0, [r0]
        eoreq   r0, r0, #1
        tsteq   r0, #1                    // Test for IRQ
        beq     handle_irq_adjust_regPC
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
        bic     instt, instt, #1024           // ack events
        sub     regPC, regPC, #3              // set the instruction back as we haven't executed it
#endif
#if !defined(USE_HW_MAILBOX)
        bic     r0, r0, #ATTN_MASK      // the attention bit
        str     r0, [r1]                // ack the attention bit
#endif

        push    {r1-tregs,instt,lr}           // r0 contains the tube mailbox value
        bl      tube_io_handler
        pop     {r1-tregs,instt,lr}
        
        tst     r0, #4                  // Bit 2 set indicate RST is active
        bne     exec_65tube_exit        // exit immediately if active edge seen

        tst     r0, #2                  // Bit 1 set indicate NMI is active
        bne     handle_nmi              // Take the NMI interrupt

        tst     flags, #4                  // Test whether interrupts are enabled (bit 2 = 0)
        bne     noirq
        tst     r0, #1                  // Bit 1 set indicate IRQ is active
        bne     handle_irq              // Take the IRQ interrupt

noirq:
        msr     CPSR, r2                // restore the 6502 flags and re-enable ARM interrupts
        FETCH_NEXT_3B_MERGED               // go back to executing 6502 instructions

handle_irq_adjust_regPC:        
        sub     regPC, regPC ,#2
handle_irq:
        msr     CPSR, r2                // Restore the 6502 flags and re-enable ARM interrupts
        INTR    -2, 0
        FETCH_NEXT_3B_MERGED

handle_nmi:
        msr     CPSR, r2                // Restore the 6502 flags and re-enable ARM interrupts
        INTR    -6, 0
        FETCH_NEXT_3B_MERGED

#if !defined(USE_MULTICORE) && defined(USE_HW_MAILBOX)

// In slowdown we patch the event handler dispatch table to a special event handler (below)
//
// This implements the following dispatch pattern
// - Usual checks for events, irq, nmi
// - delay for ~600ns
// - execute one instruction, with bit set in instt to return immediately to event handler
//        
// Differences in making a slowdown singlecore event handler:
// 
// 1. Removed the masking of interrupts
//     two lines
//     we want to keep interrupts possible because we're now in the event handler really often
//     and we're OK if instt is updated by a FIQ because we're going to recheck, next instruction
// 
// 2. Added an attention mask check
//     two lines
//     we skinstt straight to the time wasting if nothing to do
//     we have to skinstt over the clearing of the mask in case FIQ sets it concurrently
//     we can afford to run unwanted code in the slow case but we can't afford a race condition
// 
// 3. We added a time wasting loop
//     added 4 lines or so
//     we (redundantly) re-enable interupts first because we don't want to slow down FIQ response
// 
// 4. Replaced postamble of FETCH_NEXT_STAGE_012
//     we added one line
//     we restore the modified table base pointer so we will re-enter this handler
        
Event_Handler_Single_Core_Slow:
        mrs     r2, CPSR                // preserve the 6502 flags
                                        // interrupts are left enabled
        
        bic     instt, instt, #1024           // ack any events
        sub     regPC, regPC, #3              // set the instruction back as we haven't executed it

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

        push    {r1-tregs,instt,lr}           // r0 contains the tube mailbox value
        bl      tube_io_handler
        pop     {r1-tregs,instt,lr}
        
        tst     r0, #4                  // Bit 2 set indicate RST is active
        bne     exec_65tube_exit        // exit immediately if active edge seen

        tst     r0, #2                  // Bit 1 set indicate NMI is active
        bne     handle_nmi2             // Take the NMI interrupt

        tst     flags, #4                  // Test whether interrupts are enabled (bit 2 = 0)
        bne     noirq2
        tst     r0, #1                  // Bit 1 set indicate IRQ is active
        bne     handle_irq2             // Take the IRQ interrupt

noirq2:

#if defined(RPIZERO)
        mov     r0, #0x15C              // 0x15C gives 3.00MHz average on a Pi Zero at 1000/400
#elif defined(RPI2)
        mov     r0, #0x388              // 0x388 gives 3.01MHz average on a Pi 2 at 900/350
#elif defined(RPI3)
        mov     r0, #0x3F0              // 0x3F0 gives 3.00MHz average on a Pi 3 at 1000/400
#else
        mov     r0, #0x134              // 0x134 gives 3.02MHz average on a Pi One at 900/350
#endif
waste_time:
        subs    r0, r0, #1
        bne     waste_time

        msr     CPSR, r2                // restore the 6502 flags        

execute_one_instruction:
        ldrb    r2, [regPC], #1            // same as FETCH_NEXT_STAGE_012 but set bit in instt to return immediately to event handler
        ldrb    r0, [regPC], #1
        ldrb    r5, [regPC], #1
        ldr     lr, [instt, r2, lsl #2]
        orr     instt, instt, #1024           // Return to event handler after next instruction dispatch        
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
        and     r0, regA, #0xf000000
        and     r2, r1, #15
        adc     r2, r2, r0, lsr #24
        cmp     r2, #10
        subcs   r2, r2, #10
        lsr     r1, r1, #4
        lsl     r2, r2, #24
        adc     regA, r1, regA, lsr #28
        cmp     regA, #10
        subcs   regA, regA, #10
        FETCH_NEXT_STAGE_1
        orr     regA, r2, regA, lsl #28
        teq     regA, #0
        bicvc   flags, #0x40
        orrvs   flags, #0x40
        FETCH_NEXT_STAGE_2

decimal_mode_sbc:
        and     r0, regA, #0xf000000
        and     r2, r1, #15
        rscs    r2, r2, r0, lsr #24
        addcc   r2, r2, #10
        lsr     r1, r1, #4
        rscs    regA, r1, regA, lsr #28
        addcc   regA, regA, #10
        FETCH_NEXT_STAGE_1a_OO
        orr     regA, r2, regA, lsl #4
        FETCH_NEXT_STAGE_1b_OO
        lsl     regA, regA, #24
        teq     regA, #0
        bicvc   flags, #0x40
        orrvs   flags, #0x40
        FETCH_NEXT_STAGE_2
        
decimal_mode_adc_r0:
        and     r2, r0, #15
        lsr     r1, r0, #4
        and     r0, regA, #0xf000000
        FETCH_NEXT_STAGE_1a_OO
        adc     r2, r2, r0, lsr #24
        cmp     r2, #10
        subcs   r2, r2, #10
        adc     regA, r1, regA, lsr #28
        cmp     regA, #10
        lsl     r2, r2, #24
        subcs   regA, regA, #10
        FETCH_NEXT_STAGE_1b_OO
        orr     regA, r2, regA, lsl #28
        teq     regA, #0
        bicvc   flags, #0x40
        orrvs   flags, #0x40
        FETCH_NEXT_STAGE_2

decimal_mode_sbc_r0:
        and     r2, r0, #15
        lsr     r1, r0, #4
        and     r0, regA, #0xf000000
        FETCH_NEXT_STAGE_1a_OO        
        rscs    r2, r2, r0, lsr #24
        addcc   r2, r2, #10
        rscs    regA, r1, regA, lsr #28
        addcc   regA, regA, #10
        FETCH_NEXT_STAGE_1b_OO        
        orr     regA, r2, regA, lsl #4
        // stall
        lsl     regA, regA, #24
        teq     regA, #0
        bicvc   flags, #0x40
        orrvs   flags, #0x40
        FETCH_NEXT_STAGE_2        
     
        // Note regarding commented out code below
        //
        // The reason for disabling interrupts was to prevent an interrupt
        // notification (via register instt) being "lost" as instt is pushed
        // then later popped. If an interrupt was allowed to occur in between
        // then the pop would overwrite the flag set in register instt by the
        // interrupt handler.
        //
        // In PiTubeDirect, because interrupts are using to provide the host
        // 6502 with read data, interrupt latency is critical. We cannot affort
        // ever to disable interrupts, or reads are late, and the 6502 get
        // incorrect data (or worse, a bus conflict with a later cycle).
        //
        // Instead of disabling interrupts, we have compiled with -ffixed-sl
        // prevent the C compiler using instt (r12). It's normally a valid scratch
        // register.
        //
        // If the C code calls any system libraries (e.g. printf) then it's
        // likely that instt will get corrupted, so we need to be very careful.
        //
        // It's probably woth redesigning the way the ISR notifies the main
        // code to invoke the event handler. This will only cost a few more
        // emulation cycles.

tube_load_handler:
        push    {r0-tregs,lr}                // r0 = address, r2 = current cpsr
//      orr     r2, r2, #ARM_INT_MASK     // disable ARM IRQs and FIQs
//      msr     CPSR_c, r2
//      push    {instt}                      // push instt after disabling interrupts to avoid a race condition
        bl      tube_parasite_read
//      pop     {instt}
        str     r0, [sp, #4]              // save result to r1 on the stack
        pop     {r0-tregs,pc}                // r2 (cpsr) is preserved

tube_store_handler:
        push    {r0-tregs,lr}                // r0 = address, r1 = data, r2 = current cpsr
//      orr     r2, r2, #ARM_INT_MASK     // disable ARM IRQs and FIQs
//      msr     CPSR_c, r2
//      push    {instt}                      // push instt after disabling interrupts to avoid a race condition
        bl      tube_parasite_write
//      pop     {instt}
        pop     {r0-tregs,pc}                // r2 (cpsr) is preserved

handle_rti:
        PLP
        
        ldrh    r1, [regSP, #1]   
        add     regSP, regSP, #2
        bic     regSP, #0xFF00     
        add     regPC, memry, r1
        orr     regSP, #0x0100
        FETCH_NEXT_3B_STAGE_0
        CHECK_IRQ
        FETCH_NEXT_STAGE_012 

// Insert the current literal pool, otherwise constants are to far away and you get a build error
        .ltorg
#if defined(USE_WATCHPOINT)
// *****************************
// *
// * Try and use the the watchpoint monitor to detect tube access
// * 
// *****************************

// _data_abort_vector_h // this should really be exported
#define DATA_ABORT_ADDRESS 0x30

//
// Watchpointint
// 
// Entry R0 = Tube address
//

watchpointinit:

// Check to see if we have already caputured the DATA_ABORT vector

      LDR   R3,=DATA_ABORT_ADDRESS
      LDR   R1,=watchpointentry
      LDR   R2,[R3]
      CMP   R1,R2
      BEQ   watchpointalreadysetup
      STR   r1,[R3]
      LDR   r3,=olddataabortentry
      STR   R2,[R3]
watchpointalreadysetup:
      // Set up watchpoint address
      
      // CP 14 c96 c97 Watchpoint Value WVR
      // opcode_2 =6 Crm = 0 or 1
      // MCR p14,opcode1,r0,crn,crm,opcode2
      //MCR p14,opcode1,r0,crn,0,6
      
      
      //write_wb_reg(96 + i, addr); opcode2 = 6
      
      mcr   p14,0,r0,c0,c0,6
      ADD   R0,r0,#4
      mcr   p14,0,r0,c0,c1,6
      
     // write_wb_reg(112 + i, ctrl); opccode2 = 7
      
      // CP14 c112-c113 WCR
      
      // opcode2 = 7 Crm=0 or 1, C112 C113
      // Bits 8..5 = 1111 hit on each byte in word
      // Bits 4..3 = 11 hit on load and store
      // bits 2..1 = 11 user or privilged mode
      // bit 0 = 1 watch point enable
      
      LDR   R0,=0x1FF
      mcr   p14,0,r0,c0,c0,7
      mcr   p14,0,r0,c0,c1,7
      
      // Set up watchpoint control
      
      // CP14 c1 DSCR set bit 15 debug monitor enable
      
      MOV      R0,#1<<15
      // for arch v6
      mcr   p14,0,R0, c0,c1,0
      
      // for arch v7 v8
      mcr   p14,0,R0, c0,c1, 2
      
      //IMB
      
      MOV   PC,LR
olddataabortentry:      
     .word 0
     
watchpointentry:   
      push {lr}
      push { r0-r3}
      
      // Check if it is a watchpoint entry 
      // if not try the old databort handler
      
      // so we have a watchpoint
      
      // get watchpoint address
      MRC   p15,0,R0,c6,c0,0
      
      // work out which register we need to fix up
      
      // read or write.
      
      // Call Tube data handler
      
      pop { r0-r3}
      pop {lr}
      
      // let get out of here

#endif      
        
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
        FETCH_NEXT_STAGE_01_I
        FETCH_NEXT_STAGE_2

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
        FETCH_NEXT_STAGE_012

// Opcode 5C - NOP $0000
// Opcode DC - NOP $0000
// Opcode FC - NOP $0000
// NOP_3
opcode_5C:
opcode_DC:
opcode_FC:
        FETCH_NEXT_3B_MERGED

// Opcode 00 - BRK
opcode_00:
        // BRK is really a two byte instruction, so no fixup needed
        sub    regPC,regPC,#1
        INTR    -2, 1
        FETCH_NEXT_3B_MERGED

// Opcode 01 - ORA ($00,X)
opcode_01:
        EA_INDIRECT_X_FETCH_STAGE_0
        LOGICAL_FETCH12 orrs

// Opcode 04 - TSB $00
opcode_04:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        mrs     r2, CPSR
        tst     r1, regA, lsr #24
        bic     r2, r2, #Z_FLAG
        orreq   r2, r2, #Z_FLAG
        msr     CPSR_flg, r2
        orr     r1, r1, regA, lsr #24
        STORE_BYTE
        FETCH_NEXT_STAGE_12

// Opcode 05 - ORA $00
opcode_05:
        LOAD_BYTE
        LOGICAL_FETCH012 orrs

// Opcode 06 - ASL $00
opcode_06:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        add     r1, r1, r1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        movs    r1, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode 07 - RMB0 $00
opcode_07:
        RMB 0

// Opcode 08 - PHP
opcode_08:
        FETCH_NEXT_STAGE_01_I
        orr     r1, flags, #0x30
        orrmi   r1, r1, #0x80
        orrcs   r1, r1, #1
        orreq   r1, r1, #2
        SPUSH   r1
        FETCH_NEXT_STAGE_2

// Opcode 09 - ORA #$00
opcode_09:
        LOGICAL_FETCH012 orrs r0

// Opcode 0A - ASL A
opcode_0A:
        FETCH_NEXT_STAGE_01_I
        adds    regA, regA, regA
        FETCH_NEXT_STAGE_2

// Opcode 0C - TSB $0000
opcode_0C:
        EA_ABSOLUTE_LOAD_PART
        tst     r1, regA, lsr #24
        bic     r2, r2, #Z_FLAG
        orreq   r2, r2, #Z_FLAG
        orr     r1, r1, regA, lsr #24
        STORE_ABSOLUTE_PART
        FETCH_NEXT_STAGE_012

// Opcode 0D - ORA $0000
opcode_0D:
        EA_ABSOLUTE_LOAD
        LOGICAL_FETCH012 orrs

// Opcode 0E - ASL $0000
opcode_0E:
        EA_ABSOLUTE_LOAD_PART
        add     r1, r1, r1
        STORE_ABSOLUTE_PART
        FETCH_NEXT_STAGE_01
        movs    r1, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode 0F - BBR0
opcode_0F:
        BBR 0

// Opcode 10 - BPL
opcode_10:
        BRANCH pl mi

// Opcode 11 - ORA ($00),Y
opcode_11:
        EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 orrs

// Opcode 12 - ORA ($00)
opcode_12:
        EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 orrs

// Opcode 14 - TRB $00
opcode_14:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        mrs     r2, CPSR
        tst     r1, regA, lsr #24
        bic     r2, r2, #Z_FLAG
        orreq   r2, r2, #Z_FLAG
        msr     CPSR_flg, r2
        bic     r1, r1, regA, lsr #24
        STORE_BYTE
        FETCH_NEXT_STAGE_12

// Opcode 15 - ORA $00,X
opcode_15:
        add     r0, regX, r0, lsl #24
        FETCH_NEXT_STAGE_0
        LOAD_ZPX
        LOGICAL_FETCH12 orrs
        
// Opcode 16 - ASL $00, X
opcode_16:
        add     r0, regX, r0, lsl #24
        FETCH_NEXT_STAGE_0
        LOAD_ZPX
        FETCH_NEXT_STAGE_1a_OO
        add     r1, r1, r1
        STORE_ZPX
        movs    r1, r1, lsl #24
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2

// Opcode 17 - RMB1 $00
opcode_17:
        RMB 1

// Opcode 18 - CLC
opcode_18:
        FETCH_NEXT_STAGE_01_I
        mrscs     r1, CPSR
        biccs     r1, r1, #C_FLAG
        msrcs     CPSR_flg, r1
        FETCH_NEXT_STAGE_2

// Opcode 19 - ORA $0000,Y
opcode_19:
        EA_ABSOLUTE_INDEXED_LOAD regY
        LOGICAL_FETCH012 orrs

// Opcode 1A - INC A
opcode_1A:
        FETCH_NEXT_STAGE_01_I
        add     regA, regA, #0x1000000
        teq     regA, #0
        FETCH_NEXT_STAGE_2

// Opcode 1C - TRB $0000
opcode_1C:
        EA_ABSOLUTE_LOAD_PART
        tst     r1, regA, lsr #24
        bic     r2, r2, #Z_FLAG
        orreq   r2, r2, #Z_FLAG
        bic     r1, r1, regA, lsr #24
        STORE_ABSOLUTE_PART
        FETCH_NEXT_STAGE_012

// Opcode 1D - ORA $0000,X
opcode_1D:
        EA_ABSOLUTE_INDEXED_LOAD regX
        LOGICAL_FETCH012 orrs

// Opcode 1E - ASL $0000,X
opcode_1E:
        EA_ABSOLUTE_INDEXED_LOAD_PART regX
        add     r1, r1, r1
        STORE_ABSOLUTE_PART
        FETCH_NEXT_STAGE_01
        movs    r1, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode 1F - BBR1
opcode_1F:
        BBR 1

// Opcode 20 - JSR $0000
opcode_20:
        sub     regPC, regPC, #1
        EA_ABSOLUTE
        strh    regPC, [regSP,#-1]
        ldrb    r5, [r0, memry ]! // modified FETCH_NEXT_3B_STAGE_0
        add     regPC, r0 ,#1
        sub     regSP, regSP, #2
        orr     regSP, #0x0100
        FETCH_NEXT_STAGE_012

// Opcode 21 - AND ($00,X)
opcode_21:
        EA_INDIRECT_X_FETCH_STAGE_0
        LOGICAL_FETCH12 ands

// Opcode 24 - BIT $00
opcode_24:
        LOAD_BYTE
        BIT

// Opcode 25 - AND     $00
opcode_25:
        LOAD_BYTE
        LOGICAL_FETCH012 ands

// Opcode 26 - ROL $00
opcode_26:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        adc     r1, r1, r1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        lsls    r1, r1, #24
        FETCH_NEXT_STAGE_2

// Opcode 27 - RMB2 $00
opcode_27:
        RMB 2

// Opcode 28 - PLP 
opcode_28:
        FETCH_NEXT_STAGE_0_I
        PLP
        CHECK_IRQ
        FETCH_NEXT_STAGE_1_I
        FETCH_NEXT_STAGE_2
        .ltorg

// Opcode 29 - AND     #$00
opcode_29:
        LOGICAL_FETCH012 ands r0

// Opcode 2A - ROL A
opcode_2A:
        FETCH_NEXT_STAGE_01_I
        orrcs   regA, regA, #0x800000
        adds    regA, regA, regA
        FETCH_NEXT_STAGE_2

// Opcode 2C - BIT $0000
opcode_2C:
        EA_ABSOLUTE_LOAD
        BIT 

// Opcode 2D - AND     $0000
opcode_2D:
        EA_ABSOLUTE_LOAD
        LOGICAL_FETCH012 ands

// Opcode 2E - ROL $0000
opcode_2E:
        EA_ABSOLUTE_LOAD
        adc     r1, r1, r1
        STORE_ABSOLUTE_PART
        FETCH_NEXT_STAGE_01
        lsls    r1, r1, #24
        FETCH_NEXT_STAGE_2

// Opcode 2F - BBR2
opcode_2F:
        BBR 2

// Opcode 30 - BMI
opcode_30:
        BRANCH mi pl
        
// Opcode 31 - AND ($00),Y
opcode_31:
        EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 ands
        
// Opcode 32 - AND ($00)
opcode_32:
        EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 ands

// Opcode 34 - BIT $00,X
opcode_34:
        add     r0, regX, r0, lsl #24
        LOAD_ZPX
        BIT

// Opcode 35 - AND $00,X
opcode_35:
        add     r0, regX, r0, lsl #24
        FETCH_NEXT_STAGE_0
        LOAD_ZPX
        LOGICAL_FETCH12 ands

// Opcode 36 - ROL $00,X
opcode_36:
        add     r0, regX, r0, lsl #24
        LOAD_ZPX
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        adc     r1, r1, r1
        STORE_ZPX
        lsls    r1, r1, #24
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2

// Opcode 37 - RMB3 $00
opcode_37:
        RMB 3

// Opcode 38 - SEC
opcode_38:
        FETCH_NEXT_STAGE_01_I
        mrscc     r1, CPSR
        orrcc     r1, r1, #C_FLAG
        msrcc     CPSR_flg, r1   // takes two cycles if carry is clear
        FETCH_NEXT_STAGE_2

// Opcode 39 - AND $0000,Y
opcode_39:
        EA_ABSOLUTE_INDEXED_LOAD regY
        LOGICAL_FETCH012 ands

// Opcode 3A - DEC A
opcode_3A:
        FETCH_NEXT_STAGE_01_I
        sub     regA, regA, #0x1000000
        teq     regA, #0
        FETCH_NEXT_STAGE_2

// Opcode 3C - BIT $0000,X
opcode_3C:
        EA_ABSOLUTE_INDEXED_LOAD regX
        BIT

// Opcode 3D - AND $0000,X
opcode_3D:
        EA_ABSOLUTE_INDEXED_LOAD regX
        LOGICAL_FETCH012 ands

// Opcode 3E - ROL $0000,X
opcode_3E:
        EA_ABSOLUTE_INDEXED_LOAD regX
        adc     r1, r1, r1
        STORE_ABSOLUTE_PART
        FETCH_NEXT_STAGE_01
        lsls    r1, r1, #24
        FETCH_NEXT_STAGE_2

// Opcode 3F - BBR3
opcode_3F:
        BBR 3

// Opcode 40 - RTI
opcode_40:
        b       handle_rti

// Opcode 41 - EOR ($00,X)
opcode_41:
        EA_INDIRECT_X_FETCH_STAGE_0
        LOGICAL_FETCH12 eors

// Opcode 45 - EOR $00
opcode_45:
        LOAD_BYTE
        LOGICAL_FETCH012 eors

// Opcode 46 - LSR $00
opcode_46:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        lsrs    r1, r1, #1
        STORE_BYTE
        FLAGS_OO

// Opcode 47 - RMB4 $00
opcode_47:
        RMB 4

// Opcode 48 - PHA
opcode_48:
        FETCH_NEXT_STAGE_01_I
        lsr     r1, regA, #24
        SPUSH   r1
        FETCH_NEXT_STAGE_2

// Opcode 49 - EOR #$00
opcode_49:
        LOGICAL_FETCH012 eors r0

// Opcode 4A - LSR A
opcode_4A:
        lsrs    r1, regA, #25
        FETCH_NEXT_STAGE_01_I
        lsl     regA, r1, #24
        teq     regA, #0
        FETCH_NEXT_STAGE_2

// Opcode 4C - JMP $0000
opcode_4C:
        EA_ABSOLUTE
        add     regPC, r0, memry
        FETCH_NEXT_3B_MERGED

// Opcode 4D - EOR $0000
opcode_4D:
        EA_ABSOLUTE_LOAD
        LOGICAL_FETCH012 eors


// Opcode 4E - LSR $0000
opcode_4E:
        EA_ABSOLUTE_LOAD
        lsrs    r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        FLAGS

// Opcode 4F - BBR4
opcode_4F:
        BBR 4


// Opcode 50 - BVC
opcode_50:
        mrs     r2, CPSR
        tst     flags, #0x40
        BRANCH_FLAGS  eq ne msr

// Opcode 51 - EOR ($00),Y
opcode_51:
        EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 eors

// Opcode 52 - EOR ($00)
opcode_52:
        EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 eors

// Opcode 55 - EOR $00,X
opcode_55:
        add     r0, regX, r0, lsl #24
        FETCH_NEXT_STAGE_0
        LOAD_ZPX
        LOGICAL_FETCH12 eors

// Opcode 56 - LSR $00,X
opcode_56:
        add     r0, regX, r0, lsl #24
        LOAD_ZPX
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        lsrs    r1, r1, #1
        STORE_ZPX
        FLAGS_OO

// Opcode 57 - RMB5 $00
opcode_57:
        RMB 5

// Opcode 58 - CLI
opcode_58:
        bic     flags, flags, #4
        FETCH_NEXT_STAGE_0_I
        CHECK_IRQ mrs
        FETCH_NEXT_STAGE_1_I
        FETCH_NEXT_STAGE_2
        .ltorg

// Opcode 59 - EOR $0000,Y
opcode_59:
        EA_ABSOLUTE_INDEXED_LOAD regY
        LOGICAL_FETCH012 eors

// Opcode 5A - PHY
opcode_5A:
        FETCH_NEXT_STAGE_01_I
        lsr     r1, regY, #24
        SPUSH   r1
        FETCH_NEXT_STAGE_2

// Opcode 5D - EOR $0000,X
opcode_5D:
        EA_ABSOLUTE_INDEXED_LOAD regX
        LOGICAL_FETCH012 eors

// Opcode 5E - LSR $0000,X
opcode_5E:
        EA_ABSOLUTE_INDEXED_LOAD regX
        lsrs    r1, r1, #1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_0
        FLAGS

// Opcode 5F - BBR5
opcode_5F:
        BBR 5

// Opcode 60 - RTS 
opcode_60:
        ldrh    r1, [regSP, #1]
        add     regPC, memry, #1
        add     regSP, regSP, #2
        add     regPC, regPC, r1
        FETCH_NEXT_3B_STAGE_0
        bic     regSP, #0xFF00
        orr     regSP, #0x0100
        FETCH_NEXT_STAGE_012

// Opcode 61 - ADC ($00,X)
opcode_61:
        EA_INDIRECT_X
        ADC_macro     r1


// Opcode 64 - STZ $00
opcode_64:
        FETCH_NEXT_STAGE_0
        STORE_BYTE instt
        FETCH_NEXT_STAGE_12

// Opcode 65 - ADC_macro $00
opcode_65:
        LOAD_BYTE
        ADC_macro     r1

// Opcode 66 - ROR $00
opcode_66:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2

// Opcode 67 - RMB6 $00
opcode_67:
        RMB 6

// Opcode 68 - PLA
opcode_68:
        SPOP_FETCH_STAGE_0_I    regA
        FETCH_NEXT_STAGE_1_I
        lsl     regA, regA, #24
        teq     regA, #0
        FETCH_NEXT_STAGE_2

// Opcode 69 - ADC_macro #$00
opcode_69:
        ADC_macro     r0

// Opcode 6A - ROR A
opcode_6A:
        orrcs   regA, regA, #1
        FETCH_NEXT_STAGE_0_I
        rors    r1, regA, #25
        FETCH_NEXT_STAGE_1_I
        lsl     regA, r1, #24
        teq     regA, #0       
        FETCH_NEXT_STAGE_2

// Opcode 6C - JMP ($0000) 
opcode_6C:
        EA_ABSOLUTE    
        EA_INDIRECT
        add     regPC, r0, memry
        FETCH_NEXT_3B_MERGED

// Opcode 6D - ADC_macro $0000
opcode_6D:
        EA_ABSOLUTE_LOAD
        ADC_macro     r1

// Opcode 6E - ROR $0000
opcode_6E:
        EA_ABSOLUTE_LOAD
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_012

// Opcode 6F - BBR6
opcode_6F:
        BBR 6

// Opcode 70 - BVS
opcode_70:
        mrs     r2, CPSR
        tst     flags, #0x40
        BRANCH_FLAGS  ne eq msr

// Opcode 71 - ADC_macro ($00),Y
opcode_71:
        EA_INDIRECT
        EA_INDIRECT_Y
        LOAD_BYTE
        ADC_macro     r1

// Opcode 72 - ADC_macro ($00)
opcode_72:
        EA_INDIRECT
        LOAD_BYTE
        ADC_macro     r1

// Opcode 74 - STZ $00,X
opcode_74:
        add     r0, regX, r0, lsl #24
        FETCH_NEXT_STAGE_0
        STORE_ZPX  instt
        FETCH_NEXT_STAGE_12

// Opcode 75 - ADC_macro $00,X
opcode_75:
        add     r0, regX, r0, lsl #24
        LOAD_ZPX
        ADC_macro     r1 

// Opcode 76 - ROR $00,X
opcode_76:
        add     r0, regX, r0, lsl #24
        LOAD_ZPX
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_ZPX
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2

// Opcode 77 - RMB7 $00
opcode_77:
        RMB 7

// Opcode 78 - SEI
opcode_78:
        FETCH_NEXT_STAGE_01_I
        orr     flags, flags, #4
        FETCH_NEXT_STAGE_2

// Opcode 79 - ADC_macro $0000,Y
opcode_79:
        EA_ABSOLUTE_INDEXED_LOAD regY
        ADC_macro     r1

// Opcode 7A - PLY
opcode_7A:
        SPOP_FETCH_STAGE_0_I   regY
        FETCH_NEXT_STAGE_1_I
        lsl     regY, regY, #24
        teq     regY, #0
        FETCH_NEXT_STAGE_2

// Opcode 7C - JMP ($0000,X)
opcode_7C:
        EA_ABSOLUTE_INDEXED regX
        EA_INDIRECT
        add     regPC, r0, memry
        FETCH_NEXT_3B_MERGED

// Opcode 7D - ADC_macro $0000,X
opcode_7D:
        EA_ABSOLUTE_INDEXED_LOAD regX
        ADC_macro      r1

// Opcode 7E - ROR $0000,X
opcode_7E:
        EA_ABSOLUTE_INDEXED_LOAD regX
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_012

// Opcode 7F - BBR7
opcode_7F:
        BBR 7

// Opcode 80 - BRA
opcode_80:
        BRANCH al al 

// Opcode 81 - STA ($00,X)
opcode_81:
        EA_INDIRECT_X_FETCH_STAGE_0 store
        FETCH_NEXT_STAGE_1a_OO
        lsr     r1, regA, #24
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2

// Opcode 84 - STY $00
opcode_84:
        FETCH_NEXT_STAGE_0
        lsr     r1, regY, #24
        STORE_BYTE    
        FETCH_NEXT_STAGE_12

// Opcode 85 - STA $00
opcode_85:
        FETCH_NEXT_STAGE_0
        lsr     r1, regA, #24
        STORE_BYTE
        FETCH_NEXT_STAGE_12

// Opcode 86 - STX $00
opcode_86:
        FETCH_NEXT_STAGE_0
        lsr     r1, regX, #24
        STORE_BYTE
        FETCH_NEXT_STAGE_12

// Opcode 87 - SMB0 $00
opcode_87:
        SMB 0

// Opcode 88 - DEY
opcode_88:
        FETCH_NEXT_STAGE_01_I
        sub     regY, regY, #0x1000000
        teq     regY, #0
        FETCH_NEXT_STAGE_2

// Opcode 89 - BIT #$00
opcode_89:
        lsl     r0, r0, #24
        mrs     r2, CPSR
        and     r2, r2, #N_FLAG
        tst     r0, regA
        mrs     r1, CPSR
        bic     r1, r1, #N_FLAG
        orr     r1, r1, r2
        FETCH_NEXT_STAGE_0
        msr     CPSR_flg, r1
        FETCH_NEXT_STAGE_12

// Opcode 8A - TXA
opcode_8A:
        FETCH_NEXT_STAGE_01_I
        movs    regA, regX
        FETCH_NEXT_STAGE_2

// Opcode 8C - STY $0000
opcode_8C:
        EA_ABSOLUTE
        FETCH_NEXT_3B_STAGE_0
        lsr     r1, regY, #24
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_012

// Opcode 8D - STA $0000
opcode_8D:
        EA_ABSOLUTE
        FETCH_NEXT_3B_STAGE_0
        lsr     r1, regA, #24
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_012

// Opcode 8E - STX $0000
opcode_8E:
        EA_ABSOLUTE
        FETCH_NEXT_3B_STAGE_0
        lsr     r1, regX, #24
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_012

// Opcode 8F - BBS0
opcode_8F:
        BBS 0

// Opcode 90 - BCC
opcode_90:
        BRANCH cc cs

// Opcode 91 - STA ($00),Y
opcode_91:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        EA_INDIRECT_Y
        lsr     r1, regA, #24
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2

// Opcode 92 - STA ($00)
opcode_92:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        lsr     r1, regA, #24
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2

// Opcode 94 - STY $00,X
opcode_94:
        FETCH_NEXT_STAGE_0
        add     r0, regX, r0, lsl #24
        lsr     r1, regY, #24
        STORE_ZPX
        FETCH_NEXT_STAGE_12

// Opcode 95 - STA $00,X
opcode_95:
        FETCH_NEXT_STAGE_0
        add     r0, regX, r0, lsl #24
        lsr     r1, regA, #24
        STORE_ZPX
        FETCH_NEXT_STAGE_12

// Opcode 96 - STX $00,Y
opcode_96:
        FETCH_NEXT_STAGE_0
        add     r0, regY, r0, lsl #24
        lsr     r1, regX, #24
        STORE_ZPX
        FETCH_NEXT_STAGE_12

// Opcode 97 - SMB1 $00
opcode_97:
        SMB 1

// Opcode 98 - TYA
opcode_98:
        FETCH_NEXT_STAGE_01_I
        movs    regA, regY
        FETCH_NEXT_STAGE_2

// Opcode 99 - STA $0000,Y
opcode_99:
        EA_ABSOLUTE_INDEXED regY
        FETCH_NEXT_3B_STAGE_0
        lsr     r1, regA, #24
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_012

// Opcode 9A - TXS
opcode_9A:
        FETCH_NEXT_STAGE_01_I
        add     regSP, memry, #0x100
        add     regSP, regSP, regX, lsr #24
        FETCH_NEXT_STAGE_2

// Opcode 9C - STZ $0000
opcode_9C:
        EA_ABSOLUTE
        FETCH_NEXT_3B_STAGE_0
        STORE_BYTE  instt
        FETCH_NEXT_STAGE_012

// Opcode 9D - STA $0000,X
opcode_9D:
        EA_ABSOLUTE_INDEXED regX
        FETCH_NEXT_3B_STAGE_0
        lsr     r1, regA, #24
        STORE_ABSOLUTE
        FETCH_NEXT_STAGE_012

// Opcode 9E - STZ $0000,X
opcode_9E:
        EA_ABSOLUTE_INDEXED regX
        FETCH_NEXT_3B_STAGE_0
        STORE_BYTE  instt
        FETCH_NEXT_STAGE_012

// Opcode 9F - BBS1
opcode_9F:
        BBS 1

// Opcode A0 - LDY #$00
opcode_A0:
        FETCH_NEXT_STAGE_0
        FLAGS   regY r0

// Opcode A1 - LDA ($00,X)
opcode_A1:
        EA_INDIRECT_X_FETCH_STAGE_0
        FLAGS   regA

// Opcode A2 - LDX #$00
opcode_A2:
        FETCH_NEXT_STAGE_0
        FLAGS   regX r0

// Opcode A4 - LDY $00
opcode_A4:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FLAGS   regY

// Opcode A5 - LDA $00
opcode_A5:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FLAGS   regA

// Opcode A6 - LDX $00
opcode_A6:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FLAGS   regX

// Opcode A7 - SMB2 $00
opcode_A7:
        SMB 2

// Opcode A8 - TAY
opcode_A8:
        FETCH_NEXT_STAGE_01_I
        movs    regY, regA
        FETCH_NEXT_STAGE_2

// Opcode A9 - LDA #$00
opcode_A9:
        FETCH_NEXT_STAGE_0
        FLAGS regA r0

// Opcode AA - TAX
opcode_AA:
        FETCH_NEXT_STAGE_01_I
        movs    regX, regA
        FETCH_NEXT_STAGE_2

// Opcode AC - LDY $0000
opcode_AC:
        EA_ABSOLUTE_LOAD
        FETCH_NEXT_STAGE_0
        FLAGS   regY

// Opcode AD - LDA $0000
opcode_AD:
        EA_ABSOLUTE_LOAD
        FETCH_NEXT_STAGE_0
        FLAGS   regA

// Opcode AE - LDX $0000
opcode_AE:
        EA_ABSOLUTE_LOAD
        FETCH_NEXT_STAGE_0
        FLAGS   regX

// Opcode AF - BBS2
opcode_AF:
        BBS 2

// Opcode B0 - BCS
opcode_B0:
        BRANCH cs cc

// Opcode B1 - LDA ($00),Y
opcode_B1:
        EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0
        FLAGS regA

// Opcode B2 - LDA ($00)
opcode_B2:
        EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0
        FLAGS regA

// Opcode B4 - LDY $00,X
opcode_B4:   
        add     r0, regX, r0, lsl #24
        FETCH_NEXT_STAGE_0
        LOAD_ZPX
        FLAGS   regY

// Opcode B5 - LDA $00,X
opcode_B5:
        add     r0, regX, r0, lsl #24
        FETCH_NEXT_STAGE_0
        LOAD_ZPX
        FLAGS   regA

// Opcode B6 - LDX $00,Y
opcode_B6:
        add     r0, regY, r0, lsl #24
        FETCH_NEXT_STAGE_0
        LOAD_ZPX
        FLAGS   regX

// Opcode B7 - SMB3 $00
opcode_B7:
        SMB 3

// Opcode B8 - CLV
opcode_B8:
        FETCH_NEXT_STAGE_01_I
        bic     flags, #0x40
        FETCH_NEXT_STAGE_2

// Opcode B9 - LDA $0000,Y
opcode_B9:
        EA_ABSOLUTE_INDEXED_LOAD regY
        FETCH_NEXT_STAGE_0
        FLAGS    regA

// Opcode BA - TSX
opcode_BA:
        FETCH_NEXT_STAGE_01_I
        lsl     regX, regSP, #24
        teq     regX, #0
        FETCH_NEXT_STAGE_2

// Opcode BC - LDY $0000,X
opcode_BC:
        EA_ABSOLUTE_INDEXED_LOAD regX
        FETCH_NEXT_STAGE_0
        FLAGS    regY

// Opcode BD - LDA $0000,X
opcode_BD:
        EA_ABSOLUTE_INDEXED_LOAD regX
        FETCH_NEXT_STAGE_0
        FLAGS   regA

// Opcode BE - LDX $0000,Y
opcode_BE:
        EA_ABSOLUTE_INDEXED_LOAD regY
        FETCH_NEXT_STAGE_0
        FLAGS   regX

// Opcode BF - BBS3
opcode_BF:
        BBS 3

// Opcode C0 - CPY #$00
opcode_C0:
        FETCH_NEXT_STAGE_0
        cmp     regY, r0, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode C1 - CMP ($00,X)
opcode_C1:
        EA_INDIRECT_X_FETCH_STAGE_0
        FETCH_NEXT_STAGE_1
        cmp     regA, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode C4 - CPY $00
opcode_C4:
        LOAD_BYTE
        FETCH_NEXT_STAGE_01
        cmp     regY, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode C5 - CMP $00
opcode_C5:
        LOAD_BYTE
        FETCH_NEXT_STAGE_01
        cmp     regA, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode C6 - DEC $00
opcode_C6:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        sub     r1, r1, #1
        STORE_BYTE
        FLAGS_OO

// Opcode C7 - SMB4 $00
opcode_C7:
        SMB 4

// Opcode C8 - INY
opcode_C8:
        FETCH_NEXT_STAGE_01_I
        add     regY, regY, #0x1000000
        teq     regY, #0
        FETCH_NEXT_STAGE_2

// Opcode C9 - CMP #$00
opcode_C9:
        FETCH_NEXT_STAGE_0
        cmp     regA, r0, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode CA - DEX
opcode_CA:
        FETCH_NEXT_STAGE_01_I
        sub     regX, regX, #0x1000000
        teq     regX, #0
        FETCH_NEXT_STAGE_2

// Opcode CB - WAI
opcode_CB:
        // TODO - Implement this
        FETCH_NEXT_STAGE_01_I
        FETCH_NEXT_STAGE_2

// Opcode CC - CPY $0000
opcode_CC:
        EA_ABSOLUTE_LOAD
        cmp     regY, r1, lsl #24
        FETCH_NEXT_STAGE_012

// Opcode CD - CMP $0000
opcode_CD:
        EA_ABSOLUTE_LOAD
        cmp     regA, r1, lsl #24
        FETCH_NEXT_STAGE_012

// Opcode CE - DEC $0000
opcode_CE:
        EA_ABSOLUTE_LOAD_PART
        sub     r1, r1, #1
        STORE_ABSOLUTE_PART
        FETCH_NEXT_STAGE_0
        FLAGS

// Opcode CF - BBS4
opcode_CF:
        BBS 4

// Opcode D0 - BNE
opcode_D0:
        BRANCH ne eq

// Opcode D1 - CMP ($00),Y
opcode_D1:
        EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1
        cmp     regA, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode D2 - CMP ($00)
opcode_D2:
        EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1
        cmp     regA, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode D5 - CMP $00,X
opcode_D5:
        add     r0, regX, r0, lsl #24
        FETCH_NEXT_STAGE_0
        LOAD_ZPX
        FETCH_NEXT_STAGE_1
        cmp     regA, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode D6 - DEC $00,X
opcode_D6:
        add     r0, regX, r0, lsl #24
        LOAD_ZPX
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        sub     r1, r1, #1
        STORE_ZPX
        FLAGS_OO

// Opcode D7 - SMB5 $00
opcode_D7:
        SMB 5

// Opcode D8 - CLD
opcode_D8:
        FETCH_NEXT_STAGE_01_I
        bic     flags, flags, #8
        FETCH_NEXT_STAGE_2

// Opcode D9 - CMP $0000,Y
opcode_D9:
        EA_ABSOLUTE_INDEXED_LOAD regY
        FETCH_NEXT_STAGE_01
        cmp     regA, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode DA - PHX
opcode_DA:
        FETCH_NEXT_STAGE_01_I
        lsr     r1, regX, #24
        SPUSH   r1
        FETCH_NEXT_STAGE_2

// Opcode DB - STP
opcode_DB:
        // TODO - Implement this
        FETCH_NEXT_STAGE_01_I
        FETCH_NEXT_STAGE_2

// Opcode DD - CMP $0000,X
opcode_DD:
        EA_ABSOLUTE_INDEXED_LOAD regX
        FETCH_NEXT_STAGE_01
        cmp     regA, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode DE - DEX $0000,X
opcode_DE:
        EA_ABSOLUTE_INDEXED_LOAD_PART regX
        sub     r1, r1, #1
        STORE_ABSOLUTE_PART
        FETCH_NEXT_STAGE_0
        FLAGS

// Opcode DF - BBS5
opcode_DF:
        BBS 5

// Opcode E0 - CPX #$00
opcode_E0:
        FETCH_NEXT_STAGE_0
        cmp     regX, r0, lsl #24
        FETCH_NEXT_STAGE_12

// Opcode E1 - SBC_macro ($00,X)
opcode_E1:
        EA_INDIRECT_X
        SBC_macro     r1 

// Opcode E4 - CPX $00
opcode_E4:
        LOAD_BYTE
        FETCH_NEXT_STAGE_01
        cmp     regX, r1, lsl #24
        FETCH_NEXT_STAGE_2

// Opcode E5 - SBC_macro $00
opcode_E5:
        LOAD_BYTE
        SBC_macro     r1 

// Opcode E6 - INC $00
opcode_E6:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        add     r1, r1, #1
        STORE_BYTE
        FLAGS_OO

// Opcode E7 - SMB6 $00
opcode_E7:
        SMB 6

// Opcode E8 - INX
opcode_E8:
        FETCH_NEXT_STAGE_01_I
        add     regX, regX, #0x1000000
        teq     regX, #0
        FETCH_NEXT_STAGE_2

// Opcode E9 - SBC_macro #$00
opcode_E9:
        SBC_macro     r0 

// Opcode EC - CPX $0000
opcode_EC:
        EA_ABSOLUTE_LOAD
        cmp     regX, r1, lsl #24
        FETCH_NEXT_STAGE_012

// Opcode ED - SBC_macro $0000
opcode_ED:
        EA_ABSOLUTE_LOAD
        SBC_macro     r1 

// Opcode EE - INC $0000
opcode_EE:
        EA_ABSOLUTE_LOAD_PART 
        add     r1, r1, #1
        STORE_ABSOLUTE_PART
        FETCH_NEXT_STAGE_0
        FLAGS

// Opcode EF - BBS6
opcode_EF:
        BBS 6

// Opcode F0 - BEQ
opcode_F0:
        BRANCH eq ne
        
// Opcode F1 - SBC_macro ($00),Y
opcode_F1:
        EA_INDIRECT
        EA_INDIRECT_Y
        LOAD_BYTE
        SBC_macro     r1 

// Opcode F2 - SBC_macro ($00)
opcode_F2:
        EA_INDIRECT
        LOAD_BYTE
        SBC_macro     r1 

// Opcode F5 - SBC_macro $00,X
opcode_F5:
        add     r0, regX, r0, lsl #24
        LOAD_ZPX
        SBC_macro     r1 

// Opcode F6 - INC $00,X
opcode_F6:
        add     r0, regX, r0, lsl #24
        LOAD_ZPX
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        add     r1, r1, #1
        STORE_ZPX
        FLAGS_OO

// Opcode F7 - SMB7 $00
opcode_F7:
        SMB 7

// Opcode F8 - SED
opcode_F8:
        FETCH_NEXT_STAGE_01_I
        orr     flags, flags, #8
        FETCH_NEXT_STAGE_2

// Opcode F9 - SBC_macro $0000,Y
opcode_F9:
        EA_ABSOLUTE_INDEXED_LOAD regY
        SBC_macro     r1 

// Opcode FA - PLX
opcode_FA:
        SPOP_FETCH_STAGE_0_I    regX
        FETCH_NEXT_STAGE_1_I
        lsl     regX, regX, #24
        teq     regX, #0
        FETCH_NEXT_STAGE_2

// Opcode FD - SBC_macro $0000,X
opcode_FD:
        EA_ABSOLUTE_INDEXED_LOAD regX
        SBC_macro     r1 

// Opcode FE - INC $0000,X
opcode_FE:
        EA_ABSOLUTE_INDEXED_LOAD_PART regX
        add     r1, r1, #1
        STORE_ABSOLUTE_PART
        FETCH_NEXT_STAGE_0
        FLAGS

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
