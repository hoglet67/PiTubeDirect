// This code was derived from Acorn's 65tube application
//
// The following changes have been made
// - ported from 26 bit ARM2 to 32 bit ARM
// - replaced the x3 trap instructions with appropriate length NOPs
// - implemented BBRn, BBSn, RMBn, SMBn

#include "copro-65tubeasm.h"

#define TIMING_INSTRUCTION 1

#define I_ALIGN (1<<I_ALIGN_BITS)

// Register usage
// r0       = scratch register
// r1       = scratch register
// r2       = scratch register (used to hold CPRS normally)
// tregs    = a constant value 0f 0x0000FEE0 >> 5 (for tube register address checking)
// flags    = 6502 flags (V, D and I bits maintained here, CZN use ARM flags)
// const    = a constant value of 0xFFFFFF00
// r6  regA   = 6502 A
// r12 regX   = 6502 X
// r8  regY   = 6502 Y
// regSP      = 6502 SP
// regPC (sl) = 6502 PC
// r11 memry (memry) = base address of 64KB of flat memory
// r7 (instt) = base address of 64-byte aligned instruction implementations
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
#define regX      r12
#define regY      r8
#define regSP     r9
#define regPC     r10   
#define memry     r11   
#define instt     copro6502asm_instruction_table

#define EVENT_HANDLER_FLAG copro6502asm_instruction_size
#define EVENT_HANDLER_SLOW_FLAG EVENT_HANDLER_FLAG<<1

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

#include "tube-defs.h"
        
.text
.global exec_65tube
        
.equ    N_FLAG,      0x80000000
.equ    Z_FLAG,      0x40000000
.equ    C_FLAG,      0x20000000
.equ    CPSR_FIQ_INHIBIT,       0x40
.equ    CPSR_IRQ_INHIBIT,       0x80

// Mask used when poping status into flags (RTI, PLP)
// It selects the bits that are managed in flags, i.e. V, D, I
// bits 7-0 are 6502 status reg bits
// bits 31-8 are emulator control bits
.equ    R4_MASK,     0xFFFFFF4C
  
// **********************************************
// Start of macros
// **********************************************

// one byte instructions

.macro FETCH_NEXT_STAGE_0_I
        ADD     lr,instt,r0,LSL #I_ALIGN_BITS
.endm

.macro FETCH_NEXT_STAGE_1_I
        mov     r0, r5
        ldrb    r5, [regPC, #1]!        
.endm

.macro FETCH_NEXT_STAGE_01_I
        FETCH_NEXT_STAGE_0_I
        FETCH_NEXT_STAGE_1_I
.endm
        
// Macros for fetching the next instruction, for two bytes

.macro FETCH_NEXT_STAGE_0 table=instt  src=r5
        ADD     lr,\table,\src,LSL #I_ALIGN_BITS
.endm

.macro FETCH_NEXT_STAGE_1a
        ldrb    r0, [regPC, #1]
.endm

.macro FETCH_NEXT_STAGE_1b
        ldrb    r5, [regPC, #2]!  
.endm

.macro FETCH_NEXT_STAGE_1
        FETCH_NEXT_STAGE_1a
        FETCH_NEXT_STAGE_1b   
.endm

.macro FETCH_NEXT_STAGE_1a_OO
        ldrb    r5, [regPC, #2]! 
.endm

.macro FETCH_NEXT_STAGE_1b_OO
        ldrb    r0, [regPC, #-1]   
.endm
        
.macro FETCH_NEXT_STAGE_2
        bx lr
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

.macro FETCH_NEXT_3B_STAGE_0 sub=NOBSUB
.if \sub==SUBPC
      ldrb    r5, [regPC]  //4 cycles
.else      
.if \sub==indirect
      ldrb    r5, [r0,r5,LSL #8]!//, #1  //5 cycles
      mov     regPC,r0 
.else
      ldrb    r5, [regPC, #1]!  //4 cycles
.endif 
.endif     
.endm
          
.macro FETCH_NEXT_3B_MERGED sub=NOBSUB
.if \sub==SUBPC
      ldrb    lr, [regPC]  //4 cycles
      FETCH_NEXT_STAGE_1
      FETCH_NEXT_STAGE_0 instt lr
      FETCH_NEXT_STAGE_2
.else      
.if \sub==indirect
      ldrb    lr, [r0,r5,LSL #8]//, #1  //5 cycles
      add     regPC,r0,r5,LSL #8
      FETCH_NEXT_STAGE_1a
      FETCH_NEXT_STAGE_0 instt lr
      FETCH_NEXT_STAGE_1b
      FETCH_NEXT_STAGE_2
.else
      ldrb    lr, [regPC, #1]!  //4 cycles
      FETCH_NEXT_STAGE_1
      FETCH_NEXT_STAGE_0 instt lr
      FETCH_NEXT_STAGE_2
.endif 
    

      //ADD     pc,instt,lr,LSL #I_ALIGN_BITS
.endif 
.endm

// Macro for fetching the next instruction and also checking for any pending IRQs
// Used for instructions that can enable interrupts: CLI, PLP , RTI
.macro CHECK_IRQ restoreflags=none
        ldr     r0, =tube_irq
.if \restoreflags==mrs        
        mrs     r2, CPSR                  // Preserve the 6502 flags
.endif
        tst     flags, #4                    // Test whether interrupts are enabled (bit 2 = 0)
        ldreq   r0, [r0]
        eoreq   r0, r0, #1
        tsteq   r0, #1                    // Test for IRQ
        beq     handle_irq_adjust_regPC
        msr     CPSR_flg, r2              // Restore the 6502 flags
.endm

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
        mov     \reg,#1
        uadd8    regSP,regSP,\reg
        FETCH_NEXT_STAGE_0_I
        ldrsb    \reg, [regSP]
        // The original code was just this:
        // ldrb    \reg, [regSP, #1]!
.endm

// Macro for tracing execution
// Add to FETCH_NEXT
// This is currently broken with all the pipline changes that have been done
.macro TRACE_65TUBE
        add     r0, regPC, #1
        ldrb    r1, [regPC, #1]
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
// This is currently broken with all the pipline chnanges that have been done
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
        orr     r0,r0,r5,LSL #8
.endm

.macro LOAD_BYTE sign=SIGN reg=r1
#if defined(USE_MEMORY_POINTER)
        ldrb    \reg, [ r0, memry]
#else
.if \sign==NOSIGN
        ldrb    \reg, [ r0]
.else
        ldrsb   \reg, [ r0]
.endif        
#endif        
.endm   

.macro LOAD_BYTE_SIGN reg=r1
#if defined(USE_MEMORY_POINTER)
        ldrsb    \reg, [ r0, memry]
#else
        ldrsb    \reg, [ r0]
#endif        
.endm   
     

.macro LOAD_BYTE_FETCH_NEXT_STAGE_0 sign=SIGN reg    
        LOAD_BYTE \sign \reg
        FETCH_NEXT_STAGE_0
.endm   

.macro STORE_BYTE reg=r1  src=r0
#if defined(USE_MEMORY_POINTER)
        strb    \reg, [\src, memry]
#else
        strb    \reg, [\src]
#endif        
.endm   

.macro EA_ABSOLUTE_LOAD nocarry=carry sign=SIGN reg=r1
        orr     r0,r0,r5,LSL #8
        FETCH_NEXT_3B_STAGE_0
        LOAD_ABSOLUTE \nocarry \sign \reg
.endm

.macro EA_ABSOLUTE_LOAD_NOCHECK nocarry=carry sign=SIGN reg=r1 src=r0
        orr     \src,r0,r5,LSL #8
        FETCH_NEXT_3B_STAGE_0
        LOAD_ABSOLUTE_NOCHECK \sign \reg \src
.endm

.macro WRAP_MEMORY
#if defined(CLEAR_MEMORY_OVERFLOW)        
        bic     r0, r0, #0x10000
#endif
.endm  

.macro EA_ABSOLUTE_INDEXED reg dest=r0
        EA_ABSOLUTE
        add     \dest, r0, \reg, lsr #24
        WRAP_MEMORY
.endm

.macro EA_ABSOLUTE_INDEXED_LOAD reg nocarry=carry sign=SIGN dest=r1 
        EA_ABSOLUTE_INDEXED \reg     
        FETCH_NEXT_3B_STAGE_0
        LOAD_ABSOLUTE \nocarry \sign \dest
.endm

.macro EA_ABSOLUTE_INDEXED_LOAD_NOCHECK reg nocarry=carry sign=SIGN dest=r1 p=r0
        EA_ABSOLUTE_INDEXED \reg \p
        FETCH_NEXT_3B_STAGE_0
        LOAD_ABSOLUTE_NOCHECK \sign \dest \p
.endm

.macro EA_INDIRECT reg=r0
#if defined(USE_MEMORY_POINTER)
        ldrh    \reg, [r0, memry]
#else
         ldrh    \reg, [r0]
#endif        
.endm

.macro EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0 reg=r1
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_BYTE SIGN \reg
.endm

.macro EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0 reg=r1
        EA_INDIRECT
#if defined(CLEAR_MEMORY_OVERFLOW)        
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_BYTE_SIGN \reg
#else      
 #if defined(USE_MEMORY_POINTER)  
        add     r1, memry, regY, lsr #24
        FETCH_NEXT_STAGE_0
        ldrsb    \reg, [ r0, r1]
 #else
        lsr     r2, regY, #24
        FETCH_NEXT_STAGE_0
        ldrsb    \reg, [ r0, r2]
 #endif 
#endif 
.endm

.macro EA_INDIRECT_X_ADDR
        add     r0, r0, regX, lsr #24
        and     r0, r0, #0xff
        EA_INDIRECT
.endm

.macro EA_INDIRECT_X sign=SIGN
        add     r0, r0, regX, lsr #24
        and     r0, r0, #0xff
        FETCH_NEXT_STAGE_0
        EA_INDIRECT
        LOAD_BYTE \sign
.endm

.macro EA_INDIRECT_X_FETCH_STAGE_0 str=no reg
        add     r0, r0, regX, lsr #24
        and     r0, r0, #0xff
        FETCH_NEXT_STAGE_0
.if \str==no
        EA_INDIRECT
        LOAD_BYTE_SIGN \reg
.else
        EA_INDIRECT \reg
.endif    
.endm

.macro EA_INDIRECT_Y reg=r0
        add     \reg, \reg, regY, lsr #24
        WRAP_MEMORY
.endm

.macro SETUP_ZPX reg
#if defined(USE_MEMORY_POINTER)
        add     r0, \reg, r0, lsl #24
#else
        add     r2,r0, \reg, lsr #24
        bic     r2,r2,#0x100
#endif
.endm
        
        
.macro  STORE_ZPX reg=r1
#if defined(USE_MEMORY_POINTER)
        strb    \reg, [memry, r0, lsr #24]
#else
        strb    \reg, [r2]
#endif        
.endm

.macro  LOAD_ZPX reg=regX fetch=FETCH sign=SIGN dest=r1  
#if defined(USE_MEMORY_POINTER)
        add     r0, \reg, r0, lsl #24
.if \fetch==FETCH        
        FETCH_NEXT_STAGE_0
.endif  
        ldrb    \dest, [memry, r0, lsr #24]
#else
        add     r2,r0, \reg, lsr #24
        bic     r2,r2,#0x100
.if \fetch==FETCH        
        FETCH_NEXT_STAGE_0
.endif  
.if \sign==SIGN
        ldrsb    \dest, [r2]
.else
        ldrb    \dest, [r2]
.endif        
#endif        
.endm

// Macros for reading/writing memory

.macro LOAD_ABSOLUTE nocarry=carry sign=SIGN reg=r1 jump// r0 holds the absolute address
.if \nocarry==carry
        lsr     lr, r0, #5
        teq     tregs, lr          // tregs holds the constant 0xFEE0 >> 5
.else
        cmp     tregs, r0, lsr #5  // tregs holds the constant 0xFEE0 >> 5   
.endif       
#if defined(USE_MEMORY_POINTER) 
        ldrneb  \reg, [r0, memry]    // normal memory read
#else
.ifb \jump
.if \sign==SIGN
        ldrnesb  \reg, [r0]    // normal memory read 
.else
        ldrneb  \reg, [r0]    // normal memory read
.endif
.else
        bne      \jump
.endif        
#endif        
.if \sign==SIGN
.if \reg==r1
        bleq    tube_load_handler_sign  // tube memory access
.else
        bleq    tube_load_handler_regA  // tube memory access
.endif        
.else
        bleq    tube_load_handler  // tube memory access
.endif
.endm

.macro LOAD_ABSOLUTE_NOCHECK sign=SIGN  reg=r1  src=r0// r0 holds the absolute address
.if \sign==SIGN
        ldrsb  \reg, [\src]    // normal memory read
.else
#if defined(USE_MEMORY_POINTER) 
        ldrb  \reg, [\src, memry]    // normal memory read
#else
        ldrb  \reg, [\src]    // normal memory read
#endif        
.endif
.endm

.macro STORE_ABSOLUTE  flags=flags  reg=r1 norestore=store  jump   // r0 holds the absolute address
.if \flags==flags
        mrs     r2, CPSR           // preserve the 6502 flags
        cmp     tregs, r0, lsr #5  // tregs holds the constant 0xFEE0 >> 5 
        FETCH_NEXT_3B_STAGE_0
.endif

.if \flags==carry
        lsr     r2, r0, #5
        teq     tregs, r2          // tregs holds the constant 0xFEE0 >> 5
.endif

.if \flags==nocarry
        cmp     tregs, r0, lsr #5  // tregs holds the constant 0xFEE0 >> 5   
.endif 
      
.if \reg==r1
        beq    tube_store_handler    // tube memory access
.else
        beq    tube_store_handler_regA
.endif       
#if defined(USE_MEMORY_POINTER) 
        strb  \reg, [r0, memry]       // normal memory write
#else
.ifb \jump
        strb  \reg, [r0]       // normal memory write
.else
        Bne       \jump      
.endif
#endif  
.if \norestore==store
.if \flags==flags
        msr     CPSR_flg, r2           // restore 6502 flags, including ARM 
.endif
.endif
        FETCH_NEXT_STAGE_012 
.endm


.macro STORE_ABSOLUTE_NOCHECK flags=notest dest=r1 p=r0
#if defined(USE_MEMORY_POINTER) 
        strb  \dest, [\p, memry]       // normal memory write
#else
        strb  \dest, [\p]       // normal memory write
#endif  
.endm

// Macros for additional Rockwell 65C02 and WDC 65C02 instructions

.macro BBR bitnum
        LOAD_BYTE
        mrs     r2, CPSR
        tst     r1, #(1 << \bitnum)
        sxtabeq regPC, regPC, r5
        ldrb      r1, [regPC, #1]
        ldrb      r0, [regPC, #2]  //hidden in stall
        msr     CPSR_flg, r2
        add       lr, instt, r1, LSL #I_ALIGN_BITS
        ldrb      r5, [regPC, #3]! //hidden in stall
        FETCH_NEXT_STAGE_2            // 5 cycles
.endm

.macro BBS bitnum
        LOAD_BYTE   
        mrs     r2, CPSR
        tst     r1, #(1 << \bitnum)
        sxtabne regPC, regPC, r5
        ldrb    r1, [regPC, #1]
        ldrb    r0, [regPC, #2]  //hidden in stall
        msr     CPSR_flg, r2  
        add       lr, instt, r1, LSL #I_ALIGN_BITS      
        ldrb      r5, [regPC, #3]! //hidden in stall
        FETCH_NEXT_STAGE_2            // 5 cycles
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
#if defined(USE_MEMORY_POINTER)
        sub     r1, regPC, memry                // setbrk should be 1 for 
        //add     r1, #1  
        BRK, 0 for IRQ/NMI
        lsr     r0, r1, #8
        SPUSH   r0
        add     r2, memry, #0x10000
        SPUSH   r1
#else
                    // setbrk should be 1 for BRK, 0 for IRQ/NMI       
        sub     regSP, regSP, #2
        mov     r2, #0x10000
        strh    regPC, [regSP,#1]
        orr     regSP, #0x0100
        
#endif        

#if defined(USE_MEMORY_POINTER)        
        ldrh    r2, [r2, #\vector]        // Fetch the vector address
        add     regPC, memry, r2                // Point to this instruction in memory
#else
        ldrh    regPC, [r2, #\vector]        // Fetch the vector address
        // Point to this instruction in memory
#endif   

.if \setbrk          
        movpl   r0, #0x30                 // Always set bit 5 (-) in the pushed value and Set the B bit in the pushed value (in BRK, not in IRQ/NMI)
        movmi   r0, #0xB0
.else
        movpl   r0, #0x20                 // Always set bit 5 (-) in the pushed value
        movmi   r0, #0xA0
.endif        
        orrcs   r0, r0, #1
        orreq   r0, r0, #2
        orr     r0, r0, flags                // In flags only bits 6 (V), 3 (D) and 2 (I) used; others always zero.

        SPUSH   r0
        orr     flags, flags, #4                // Set the I bit in the 6502 status register
        bic     flags, flags, #8                // Clear the D bit in the 6502 status register
     
.endm

// instruction macros 

.macro  BIT
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a
        and     r2, r1, #N_FLAG
        tst     r1, regA
        and     r1, r1, #0x40
        bic     flags, flags, #0x40
        orr     flags, flags, r1       
        mrs     r1, CPSR
        FETCH_NEXT_STAGE_1b
        bic     r1, r1, #N_FLAG
        orr     r1, r1, r2
        
        msr     CPSR_flg, r1
        FETCH_NEXT_STAGE_2
.endm

.macro ADC_macro reg=r1
        ADC_STAGE1 \reg
        ADC_STAGE2 \reg
.endm

        
.macro ADC_STAGE1 reg fetch=FETCH jump
        tst     flags, #8
        bic     flags, #0x40
.if \fetch==FETCH        
        FETCH_NEXT_STAGE_0
.endif        
        lsl     regA,regA,#24
#if defined(USE_MEMORY_POINTER)
        orrcs   \reg, \reg, #0xff00
        sxthcs  \reg, \reg
#else
        orrcs   \reg, \reg, memry         // memry has been reused and is 0xFFFFFF00 
#endif

.ifb  \jump
        
.else
        beq     \jump 
.endif        
.if \reg==r0        
        bne     decimal_mode_adc_r0
.else
        bne     decimal_mode_adc
.endif 
.endm

.macro ADC_STAGE2 reg   
.if \reg==r0    
        FETCH_NEXT_STAGE_1a_OO
        adcs    regA, regA, \reg, ror #8
        FETCH_NEXT_STAGE_1b_OO
.else
        FETCH_NEXT_STAGE_1a
        adcs    regA, regA, \reg, ror #8
        FETCH_NEXT_STAGE_1b
.endif          
       
        orrvs   flags, #0x40
        SXTB    regA,regA,ROR #24  
        FETCH_NEXT_STAGE_2
.endm

.macro SBC_macro reg=r1
        SBC_STAGE1 \reg
        SBC_STAGE2 \reg
.endm

.macro  SBC_STAGE1 reg fetch=FETCH jump  
        tst     flags, #8
        bic     flags, #0x40
.if \fetch==FETCH        
        FETCH_NEXT_STAGE_0
.endif         
        LSL     regA,regA,#24
#if defined(USE_MEMORY_POINTER)
        orrcc   \reg, \reg, #0xff00
        sxthcc  \reg, \reg
#else
        orrcc   \reg, \reg, memry  // memry here is a const of 0xFFFFFF00
#endif

.ifb \jump

.else
        beq     \jump
.endif        
        
.if \reg==r0         
        bne     decimal_mode_sbc_r0
.else
        bne     decimal_mode_sbc
.endif 
.endm

.macro  SBC_STAGE2 reg      
      
.if \reg==r0        
        FETCH_NEXT_STAGE_1a_OO
        sbcs    regA, regA, \reg, ror #8
        FETCH_NEXT_STAGE_1b_OO
.else
        FETCH_NEXT_STAGE_1a
        sbcs    regA, regA, \reg, ror #8
        FETCH_NEXT_STAGE_1b

.endif
        orrvs   flags, #0x40
        SXTB    regA,regA,ROR #24  
        FETCH_NEXT_STAGE_2
.endm

.macro BRANCH yes no reg=r0
      sxtab\yes regPC, regPC, \reg
      // stall
      ldr\yes\()b  r5, [regPC]
      //stall
      //stall
      ldrb    r0, [regPC, #1]  //hidden in stall
      FETCH_NEXT_STAGE_0
      ldrb    r5, [regPC, #2]! //hidden in stall
      FETCH_NEXT_STAGE_2            // 5 cycles

      // total of 13 if taken. 10 if not taken
.endm      

.macro BRANCH_AL 
      sxtab regPC, regPC, r0
      // stall
      ldrb  r1, [regPC]
      //stall
      ldrb    r0, [regPC, #1]  //hidden in stall
      ldrb    r5, [regPC, #2]! //hidden in stall
      add       lr, instt, r1, LSL #I_ALIGN_BITS
      FETCH_NEXT_STAGE_2            // 5 cycles

      // total of 13 if taken.
.endm    

.macro BRANCH_FLAGS yes no flag reg=r0

.if \reg==r5
   
      // if we are using R5 PC is already at the right place 
      sxtab\yes regPC, regPC, \reg
      ldrb      r1, [regPC, #1]
      ldrb      r0, [regPC, #2]  //hidden in stall
.if \flag==noflags
.else
      msr     CPSR_flg, r2
.endif         
      
      ldrb      r5, [regPC, #3]! //hidden in stall
      add       lr, instt, r1, LSL #I_ALIGN_BITS
.else 
      // if we are using r0 then pc is one place ahead
      sxtab\yes regPC, regPC, \reg
      // stall
      ldr\yes\()b  r5, [regPC]  
.ifb \flag
.else
      msr     CPSR_flg, r2
.endif  
      //stall
      ldrb    r0, [regPC, #1]  //hidden in stall
      FETCH_NEXT_STAGE_0
      ldrb    r5, [regPC, #2]! //hidden in stall 
.endif      
      
      FETCH_NEXT_STAGE_2            // 5 cycles

      // total of 14 if taken. 10 if not taken and not recovering flags
.endm     

.macro LOGICAL_FETCH12 logic
        FETCH_NEXT_STAGE_1
        \logic  regA, regA, r1
        FETCH_NEXT_STAGE_2
.endm   


.macro LOGICAL_FETCH012 logic reg=r1
.if \reg!=r0
        FETCH_NEXT_STAGE_01
        \logic  regA, regA, r1
        FETCH_NEXT_STAGE_2
.else
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        sxtb     r1, \reg
        FETCH_NEXT_STAGE_1b_OO
        \logic  regA, regA, r1
        FETCH_NEXT_STAGE_2
.endif        
.endm        

.macro FLAGS dest=r1 reg=r1
.if \reg==r0 
        lsl     \dest, \reg, #24
        FETCH_NEXT_STAGE_1a
        teq     \dest, #0
        FETCH_NEXT_STAGE_1b
        FETCH_NEXT_STAGE_2 
.else
        FETCH_NEXT_STAGE_1
        lsl     \dest, \reg, #24
        teq     \dest, #0
        FETCH_NEXT_STAGE_2 
.endif
.endm

.macro FLAGS_SIGN dest=r1 reg=r1
.if \reg==r0 
        sxtb     \dest, \reg
        FETCH_NEXT_STAGE_1a
        teq     \dest, #0
        FETCH_NEXT_STAGE_1b
        FETCH_NEXT_STAGE_2 
.else
        FETCH_NEXT_STAGE_1
        sxtb     \dest, \reg
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

.macro FLAGS_SIGN_OO dest=r1 reg=r1
       FETCH_NEXT_STAGE_1b_OO
       sxtb     \dest, \reg
       teq     \dest, #0
       FETCH_NEXT_STAGE_2
.endm

.macro FLAGS_QUICK dest=r1 
       FETCH_NEXT_STAGE_1
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

.macro EVENT_HANDLER_STAGE_0
        ldr     r1, =tube_irq
        mrs     r2, CPSR                // preserve the 6502 flags
   
        bic     instt, instt, #EVENT_HANDLER_FLAG           // ack events
        ldrb    r0, [r1]
        sub     regPC, regPC, #2              // set the instruction back as we haven't executed it
        
        tst     r0, #4                  // Bit 2 set indicate RST is active
        b       Event_Handler 
        .ltorg
        
.endm

.macro EVENT_HANDLER_SLOW_STAGE_0
        ldr     r1, =tube_irq		
        mrs     r2, CPSR                // preserve the 6502 flags
                                        // interrupts are left enabled
        
        ldrb    r0,[r1]
        sub     regPC, regPC, #2        // set the instruction back as we haven't executed it
        tst     r0,#7
        beq     noirq2

        
        b       Event_Handler_Single_Core_Slow
        .ltorg
 .endm 

.macro NOP_1
        FETCH_NEXT_STAGE_01_I
        FETCH_NEXT_STAGE_2
.endm
        
.macro NOP_2
        FETCH_NEXT_STAGE_012
.endm

// **********************************************
// End of macros
// **********************************************

// **********************************************
// Start of opcode implementations
// **********************************************
  
.balign I_ALIGN*256*4 , 0

// Opcode 00 - BRK is really a two byte instruction, so no fixup needed
opcode_00:
        INTR    -2, 1
        FETCH_NEXT_3B_STAGE_0 SUBPC
        B     JUMP_BRK
        .balign I_ALIGN

// Opcode 01 - ORA ($00,X)
opcode_01:
        EA_INDIRECT_X_FETCH_STAGE_0
        LOGICAL_FETCH12 orrs
        .balign I_ALIGN

// Opcode 02 - NOP 2
opcode_02:
        NOP_2  
JUMP_BRK:
        FETCH_NEXT_STAGE_1a
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1b
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 03 - NOP
opcode_03:
        NOP_1
        .balign I_ALIGN

// Opcode 04 - TSB $00
opcode_04:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        mrs     r2, CPSR
        tst     r1, regA
        bic     r2, r2, #Z_FLAG
        orreq   r2, r2, #Z_FLAG
        msr     CPSR_flg, r2
        orr     r1, r1, regA
        STORE_BYTE
        FETCH_NEXT_STAGE_12
        .balign I_ALIGN

// Opcode 05 - ORA $00
opcode_05:
        LOAD_BYTE
        LOGICAL_FETCH012 orrs
        .balign I_ALIGN

// Opcode 06 - ASL $00
opcode_06:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        add     r1, r1, r1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        movs    r1, r1, lsl #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 07 - RMB0 $00
opcode_07:
        RMB 0
         .balign I_ALIGN

// Opcode 08 - PHP
opcode_08:
        FETCH_NEXT_STAGE_01_I
        orr     r1, flags, #0x30
        orrmi   r1, r1, #0x80
        orrcs   r1, r1, #1
        orreq   r1, r1, #2
        SPUSH   r1
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 09 - ORA #$00
opcode_09:
        LOGICAL_FETCH012 orrs r0
        .balign I_ALIGN
// Opcode 0A - ASL A
opcode_0A:
        FETCH_NEXT_STAGE_0_I
        lsls    regA, regA, #25
        FETCH_NEXT_STAGE_1_I        
        sxtb    regA, regA , ror #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 0B - NOP
opcode_0B:
        NOP_1
        .balign I_ALIGN

// Opcode 0C - TSB $0000
opcode_0C:
        mrs     r2, CPSR 
        EA_ABSOLUTE_LOAD_NOCHECK nocarry
        FETCH_NEXT_STAGE_0
        bic     r2, r2, #Z_FLAG
        tst     r1, regA
        FETCH_NEXT_STAGE_1a_OO
        orreq   r2, r2, #Z_FLAG
        orr     r1, r1, regA
        STORE_ABSOLUTE_NOCHECK nocarry
        msr     CPSR_flg, r2
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2  
        .balign I_ALIGN

// Opcode 0D - ORA $0000
opcode_0D:
        EA_ABSOLUTE_LOAD
        LOGICAL_FETCH012 orrs
        .balign I_ALIGN

// Opcode 0E - ASL $0000
opcode_0E:
        orr     r0,r0,r5,LSL #8
        FETCH_NEXT_3B_STAGE_0
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        add     r1, r1, r1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        movs    r1, r1, lsl #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 0F - BBR0
opcode_0F:
        BBR 0
        .balign I_ALIGN

// Opcode 10 - BPL
opcode_10:
        BRANCH pl mi
        .balign I_ALIGN

// Opcode 11 - ORA ($00),Y
opcode_11:
        EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 orrs
        .balign I_ALIGN

// Opcode 12 - ORA ($00)
opcode_12:
        EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 orrs
        .balign I_ALIGN

// Opcode 13 - NOP
opcode_13:
         NOP_1
        .balign I_ALIGN

// Opcode 14 - TRB $00
opcode_14:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        mrs     r2, CPSR
        tst     r1, regA
        bic     r2, r2, #Z_FLAG
        FETCH_NEXT_STAGE_1a_OO
        orreq   r2, r2, #Z_FLAG
        msr     CPSR_flg, r2
        bic     r1, r1, regA
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2 
        .balign I_ALIGN

// Opcode 15 - ORA $00,X
opcode_15:
        LOAD_ZPX
        LOGICAL_FETCH12 orrs
        .balign I_ALIGN
        
// Opcode 16 - ASL $00, X
opcode_16:
        LOAD_ZPX
        FETCH_NEXT_STAGE_1
        add     r1, r1, r1
        STORE_ZPX
        movs    r1, r1, lsl #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 17 - RMB1 $00
opcode_17:
        RMB 1
        .balign I_ALIGN

// Opcode 18 - CLC
opcode_18: 
        mrs       r1, CPSR
        FETCH_NEXT_STAGE_01_I
        bxcc      lr
        bic       r1, r1, #C_FLAG
        msr       CPSR_flg, r1
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 19 - ORA $0000,Y
opcode_19:
        EA_ABSOLUTE_INDEXED_LOAD regY
        LOGICAL_FETCH012 orrs
        .balign I_ALIGN

// Opcode 1A - INC A
opcode_1A:
        add     regA, regA, #0x1
        FETCH_NEXT_STAGE_01_I
        sxtb    regA, regA
        teq     regA, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 1B - NOP
opcode_1B:
        NOP_1
        .balign I_ALIGN

// Opcode 1C - TRB $0000
opcode_1C:
        mrs     r2, CPSR  
        EA_ABSOLUTE_LOAD_NOCHECK nocarry
        FETCH_NEXT_STAGE_0
        bic     r2, r2, #Z_FLAG
        tst     r1, regA
        FETCH_NEXT_STAGE_1a_OO
        orreq   r2, r2, #Z_FLAG
        bic     r1, r1, regA
        STORE_ABSOLUTE_NOCHECK nocarry
        msr     CPSR_flg, r2
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2  
        .balign I_ALIGN

// Opcode 1D - ORA $0000,X
opcode_1D:
        EA_ABSOLUTE_INDEXED_LOAD regX
        LOGICAL_FETCH012 orrs
        .balign I_ALIGN

// Opcode 1E - ASL $0000,X
opcode_1E:
        EA_ABSOLUTE_INDEXED_LOAD_NOCHECK regX nocarry SIGN r1 r2
        FETCH_NEXT_STAGE_1a
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1b
        add     r1, r1, r1
        STORE_ABSOLUTE_NOCHECK notest r1 r2
        movs    r1, r1, lsl #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 1F - BBR1
opcode_1F:
        BBR 1
        .balign I_ALIGN

// Opcode 20 - JSR $0000
opcode_20:
        ldrb    r1, [r0,r5,LSL #8]//,#1 // modified FETCH_NEXT_3B_STAGE_0 
        strh    regPC, [regSP,#-1]
        add     regPC,r0,r5,LSL #8
        sub     regSP, regSP, #2
        ADD     lr,instt,r1,LSL #I_ALIGN_BITS
        FETCH_NEXT_STAGE_1a
        orr     regSP, #0x0100
        FETCH_NEXT_STAGE_1b
        bx      lr  
        //ADD     pc,instt,r1,LSL #I_ALIGN_BITS


#if 0        
#if defined(USE_MEMORY_POINTER) 
        sub     regSP, regSP, #2  
        mov     lr, regPC
        orr     regPC,r0,r5,LSL #8   
        ldrb    r5, [regPC, memry ]! // modified FETCH_NEXT_3B_STAGE_0
     //   add     regPC, regPC ,#1
        strh    lr, [regSP,#1]
#else
        orr     r0,r0,r5,LSL #8 
        sub     regSP, regSP, #2
       // mov     r1,r1          // ****** NOP in stall ???? ******
        ldrb    r5, [r0]//,r5,LSL #8]!//,#1 // modified FETCH_NEXT_3B_STAGE_0 
        strh    regPC, [regSP,#1]
        mov     regPC,r0  
#endif
        
        orr     regSP, #0x0100
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_12
        
#endif        
        .balign I_ALIGN

// Opcode 21 - AND ($00,X)
opcode_21:
        EA_INDIRECT_X_FETCH_STAGE_0
        LOGICAL_FETCH12 ands
        .balign I_ALIGN

// Opcode 22 - NOP #$00
opcode_22:
        NOP_2
        .balign I_ALIGN

// Opcode 23 - NOP
opcode_23:
        NOP_1
        .balign I_ALIGN

// Opcode 24 - BIT $00
opcode_24:
        LOAD_BYTE
JUMP_BIT:
        BIT
        .balign I_ALIGN

// Opcode 25 - AND     $00
opcode_25:
        LOAD_BYTE
        LOGICAL_FETCH012 ands
        .balign I_ALIGN

// Opcode 26 - ROL $00
opcode_26:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        adc     r1, r1, r1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        lsls    r1, r1, #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 27 - RMB2 $00
opcode_27:
        RMB 2
        .balign I_ALIGN

// Opcode 28 - PLP 
opcode_28:
        FETCH_NEXT_STAGE_0_I
        PLP
        B      JUMP_PLP
        .balign I_ALIGN

// Opcode 29 - AND     #$00
opcode_29:
        LOGICAL_FETCH012 ands r0
        .balign I_ALIGN

// Opcode 2A - ROL A
opcode_2A:
        FETCH_NEXT_STAGE_0_I
        adc     regA, regA, regA
        FETCH_NEXT_STAGE_1_I
        lsls    r1, regA, #24
        sxtb    regA, regA
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 2B - NOP
opcode_2B:
        B     opcode_EA
       
JUMP_PLP:
        CHECK_IRQ
        FETCH_NEXT_STAGE_1_I
        FETCH_NEXT_STAGE_2
        .ltorg       
        .balign I_ALIGN

// Opcode 2C - BIT $0000
opcode_2C:
        EA_ABSOLUTE_LOAD
        B      JUMP_BIT
        .balign I_ALIGN

// Opcode 2D - AND     $0000
opcode_2D:
        EA_ABSOLUTE_LOAD
        LOGICAL_FETCH012 ands
        .balign I_ALIGN

// Opcode 2E - ROL $0000
opcode_2E:
        orr     r0,r0,r5,LSL #8
        FETCH_NEXT_3B_STAGE_0
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        adc     r1, r1, r1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        lsls    r1, r1, #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 2F - BBR2
opcode_2F:
        BBR 2
        .balign I_ALIGN

// Opcode 30 - BMI
opcode_30:
        BRANCH mi pl
        .balign I_ALIGN
        
// Opcode 31 - AND ($00),Y
opcode_31:
        EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 ands
        .balign I_ALIGN
        
// Opcode 32 - AND ($00)
opcode_32:
        EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 ands
        .balign I_ALIGN

// Opcode 33 - NOP
opcode_33:
        NOP_1
        .balign I_ALIGN

// Opcode 34 - BIT $00,X
opcode_34:
        LOAD_ZPX regX
        B    JUMP_BIT
        .balign I_ALIGN

// Opcode 35 - AND $00,X
opcode_35:
        LOAD_ZPX
        LOGICAL_FETCH12 ands
        .balign I_ALIGN

// Opcode 36 - ROL $00,X
opcode_36:
        LOAD_ZPX regX
        FETCH_NEXT_STAGE_1
        adc    r1, r1, r1
        STORE_ZPX
        movs    r1, r1, lsl #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 37 - RMB3 $00
opcode_37:
        RMB 3
        .balign I_ALIGN

// Opcode 38 - SEC
opcode_38:
        mrs       r1, CPSR
        FETCH_NEXT_STAGE_01_I
        bxcs      lr
        orr       r1, r1, #C_FLAG
        msr       CPSR_flg, r1   // takes two cycles
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 39 - AND $0000,Y
opcode_39:
        EA_ABSOLUTE_INDEXED_LOAD regY
        LOGICAL_FETCH012 ands
        .balign I_ALIGN

// Opcode 3A - DEC A
opcode_3A:
        sub     regA, regA, #1
        FETCH_NEXT_STAGE_0_I
        sxtb    regA, regA
        FETCH_NEXT_STAGE_1_I
        teq     regA, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 3B - NOP
opcode_3B:
        B      opcode_EA
JUMP_RTI:
        sub    regPC ,#1
        FETCH_NEXT_3B_STAGE_0
        CHECK_IRQ
        FETCH_NEXT_STAGE_012        
        .balign I_ALIGN

// Opcode 3C - BIT $0000,X
opcode_3C:
        EA_ABSOLUTE_INDEXED_LOAD regX
        B      JUMP_BIT
        .balign I_ALIGN

// Opcode 3D - AND $0000,X
opcode_3D:
        EA_ABSOLUTE_INDEXED_LOAD regX
        LOGICAL_FETCH012 ands
        .balign I_ALIGN

// Opcode 3E - ROL $0000,X
opcode_3E:
        EA_ABSOLUTE_INDEXED_LOAD_NOCHECK regX carry SIGN r1 r2
        FETCH_NEXT_STAGE_1a
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1b
        adc     r1, r1, r1
        STORE_ZPX
        movs    r1, r1, lsl #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 3F - BBR3
opcode_3F:
        BBR 3
        .balign I_ALIGN

// Opcode 40 - RTI
opcode_40:
        PLP
#if defined(USE_MEMORY_POINTER)
        ldrh    r1, [regSP, #1]
        mov     r0, #2
        UADD8   regSP, regSP, r0
        add     regPC, memry, r1
#else
        ldrh    regPC, [regSP, #1]
        mov     r0, #2
        UADD8   regSP, regSP, r0
#endif        
        B      JUMP_RTI
        .balign I_ALIGN

// Opcode 41 - EOR ($00,X)
opcode_41:
        EA_INDIRECT_X_FETCH_STAGE_0
        LOGICAL_FETCH12 eors
        .balign I_ALIGN

// Opcode 42 - NOP #$00
opcode_42:
        NOP_2
        .balign I_ALIGN

// Opcode 43 - NOP
opcode_43:
        NOP_1
        .balign I_ALIGN

// Opcode 44 - NOP $00
opcode_44:
        NOP_2
        .balign I_ALIGN

// Opcode 45 - EOR $00
opcode_45:
        LOAD_BYTE
        LOGICAL_FETCH012 eors
        .balign I_ALIGN

// Opcode 46 - LSR $00
opcode_46:
        LOAD_BYTE_FETCH_NEXT_STAGE_0 NOSIGN
        FETCH_NEXT_STAGE_1a_OO
        lsrs    r1, r1, #1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 47 - RMB4 $00
opcode_47:
        RMB 4
        .balign I_ALIGN

// Opcode 48 - PHA
opcode_48:
        FETCH_NEXT_STAGE_01_I
        SPUSH   regA
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 49 - EOR #$00
opcode_49:
        LOGICAL_FETCH012 eors r0
        .balign I_ALIGN

// Opcode 4A - LSR A
opcode_4A:
        FETCH_NEXT_STAGE_0_I
        uxtb    regA, regA
        FETCH_NEXT_STAGE_1_I
        lsrs    regA, regA, #1
        FETCH_NEXT_STAGE_2
         .balign I_ALIGN

// Opcode 4B - NOP
opcode_4B:
        NOP_1
        .balign I_ALIGN

// Opcode 4C - JMP $0000
opcode_4C:
#if defined(USE_MEMORY_POINTER)
        EA_ABSOLUTE
        add     regPC, r0, memry
#else        
       // orr     regPC,r0,r5,LSL #8
        //stall
#endif        
        FETCH_NEXT_3B_MERGED indirect
         .balign I_ALIGN

// Opcode 4D - EOR $0000
opcode_4D:
        EA_ABSOLUTE_LOAD
        LOGICAL_FETCH012 eors
        .balign I_ALIGN

// Opcode 4E - LSR $0000
opcode_4E:
        orr     r0,r0,r5,LSL #8
        FETCH_NEXT_3B_STAGE_0
        LOAD_BYTE_FETCH_NEXT_STAGE_0 NOSIGN
        FETCH_NEXT_STAGE_1a_OO
        lsrs    r1, r1, #1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 4F - BBR4
opcode_4F:
        BBR 4
        .balign I_ALIGN

// Opcode 50 - BVC
opcode_50:
        mrs     r2, CPSR
        tst     flags, #0x40
        BRANCH_FLAGS  eq ne msr
        .balign I_ALIGN

// Opcode 51 - EOR ($00),Y
opcode_51:
        EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 eors
        .balign I_ALIGN

// Opcode 52 - EOR ($00)
opcode_52:
        EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0
        LOGICAL_FETCH12 eors
        .balign I_ALIGN

// Opcode 53 - NOP
opcode_53:
        NOP_1
        .balign I_ALIGN

// Opcode 54 - NOP $00,X
opcode_54:
        NOP_2
        .balign I_ALIGN

// Opcode 55 - EOR $00,X
opcode_55:
        LOAD_ZPX
        LOGICAL_FETCH12 eors
        .balign I_ALIGN

// Opcode 56 - LSR $00,X
opcode_56:
        LOAD_ZPX regX FETCH NOSIGN
        FETCH_NEXT_STAGE_1
        lsrs    r1, r1, #1
        STORE_ZPX
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 57 - RMB5 $00
opcode_57:
        RMB 5
        .balign I_ALIGN

// Opcode 58 - CLI
opcode_58:
        bic     flags, flags, #4
        FETCH_NEXT_STAGE_0_I
        CHECK_IRQ mrs
        FETCH_NEXT_STAGE_1_I
        FETCH_NEXT_STAGE_2
        .ltorg
        .balign I_ALIGN

// Opcode 59 - EOR $0000,Y
opcode_59:
        EA_ABSOLUTE_INDEXED_LOAD regY
        LOGICAL_FETCH012 eors
        .balign I_ALIGN

// Opcode 5A - PHY
opcode_5A:
        FETCH_NEXT_STAGE_01_I
        lsr     r1, regY, #24
        SPUSH   r1
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 5B - NOP
opcode_5B:
        NOP_1
        .balign I_ALIGN

// Opcode 5C - NOP $0000
opcode_5C:
        FETCH_NEXT_3B_MERGED
        .balign I_ALIGN

// Opcode 5D - EOR $0000,X
opcode_5D:
        EA_ABSOLUTE_INDEXED_LOAD regX
        LOGICAL_FETCH012 eors
        .balign I_ALIGN

// Opcode 5E - LSR $0000,X
opcode_5E:
        EA_ABSOLUTE_INDEXED_LOAD_NOCHECK regX nocarry NOSIGN
        lsrs    r1, r1, #1
        STORE_ABSOLUTE_NOCHECK   //carry
        FETCH_NEXT_STAGE_012
        .balign I_ALIGN

// Opcode 5F - BBR5
opcode_5F:
        BBR 5
        .balign I_ALIGN

// Opcode 60 - RTS 
opcode_60:
        ldrh    regPC, [regSP, #1]
#if defined(USE_MEMORY_POINTER)        
        add     r1, memry, #1
        mov     r0, #2
        UADD8   regSP, regSP, r0
#else
        // stall
        mov     r0, #2
        UADD8   regSP, regSP, r0
#endif      
        ldrb      r1, [regPC, #1]
        ldrb      r0, [regPC, #2]  //hidden in stall
        ldrb      r5, [regPC, #3]! //hidden in stall
        add       lr, instt, r1, LSL #I_ALIGN_BITS
        FETCH_NEXT_STAGE_2

        .balign I_ALIGN

// Opcode 61 - ADC ($00,X)
opcode_61:
        EA_INDIRECT_X NOSIGN
        ADC_STAGE1 r1 NOFETCH       
        ADC_STAGE2 r1 
        .balign I_ALIGN

// Opcode 62 - NOP #$00
opcode_62:
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_12    
         .balign I_ALIGN

// Opcode 63 - NOP
opcode_63:
        NOP_1
        .balign I_ALIGN

// Opcode 64 - STZ $00
opcode_64:
        FETCH_NEXT_STAGE_0
        STORE_BYTE instt
        FETCH_NEXT_STAGE_12
        .balign I_ALIGN

// Opcode 65 - ADC_macro $00
opcode_65:
        LOAD_BYTE NOSIGN
        ADC_STAGE1 r1
JUMP_ADC_STAGE2:        
        ADC_STAGE2 r1
        .balign I_ALIGN

// Opcode 66 - ROR $00
opcode_66:
        LOAD_BYTE_FETCH_NEXT_STAGE_0 NOSIGN
        FETCH_NEXT_STAGE_1a_OO
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 67 - RMB6 $00
opcode_67:
        RMB 6
        .balign I_ALIGN

// Opcode 68 - PLA
opcode_68:
        SPOP_FETCH_STAGE_0_I regA
        FETCH_NEXT_STAGE_1_I
        teq     regA, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 69 - ADC_macro #$00
opcode_69:
        ADC_STAGE1 r0     
        ADC_STAGE2 r0
        .balign I_ALIGN

// Opcode 6A - ROR A
opcode_6A:
        biccc   regA, regA, #256
        orrcs   regA, regA, #256
        FETCH_NEXT_STAGE_0_I
        rrxs    regA, regA
        FETCH_NEXT_STAGE_1_I
        sxtb    regA, regA   
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 6B - NOP
opcode_6B:
        NOP_1
        .balign I_ALIGN

// Opcode 6C - JMP ($0000) 
opcode_6C:
        EA_ABSOLUTE  
#if defined(USE_MEMORY_POINTER)        
        EA_INDIRECT
        add     regPC, r0, memry
#else
        EA_INDIRECT regPC
#endif        
        FETCH_NEXT_3B_MERGED SUBPC
        .balign I_ALIGN

// Opcode 6D - ADC_macro $0000
opcode_6D:
        EA_ABSOLUTE_LOAD carry NOSIGN
        ADC_STAGE1 r1 FETCH JUMP_ADC_STAGE2
        .balign I_ALIGN

// Opcode 6E - ROR $0000
opcode_6E:
        EA_ABSOLUTE_LOAD_NOCHECK carry NOSIGN
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 6F - BBR6
opcode_6F:
        BBR 6
        .balign I_ALIGN

// Opcode 70 - BVS
opcode_70:
        mrs     r2, CPSR
        tst     flags, #0x40
        BRANCH_FLAGS  ne eq msr
         .balign I_ALIGN

// Opcode 71 - ADC_macro ($00),Y
opcode_71:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        EA_INDIRECT_Y
        LOAD_BYTE NOSIGN
        ADC_STAGE1 r1 NOFETCH        
        ADC_STAGE2 r1 
         .balign I_ALIGN

// Opcode 72 - ADC_macro ($00)
opcode_72:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_BYTE NOSIGN
        ADC_STAGE1 r1 NOFETCH       
        ADC_STAGE2 r1 
        .balign I_ALIGN

// Opcode 73 - NOP
opcode_73:
        NOP_1
        .balign I_ALIGN

// Opcode 74 - STZ $00,X
opcode_74:
        SETUP_ZPX regX 
        FETCH_NEXT_STAGE_0
        STORE_ZPX  instt
        FETCH_NEXT_STAGE_12
        .balign I_ALIGN

// Opcode 75 - ADC_macro $00,X
opcode_75:
        LOAD_ZPX regX FETCH NOSIGN
        ADC_STAGE1 r1 NOFETCH       
        ADC_STAGE2 r1
        .balign I_ALIGN

// Opcode 76 - ROR $00,X
opcode_76:
        LOAD_ZPX regX FETCH NOSIGN
        FETCH_NEXT_STAGE_1
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_ZPX
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 77 - RMB7 $00
opcode_77:
        RMB 7
        .balign I_ALIGN

// Opcode 78 - SEI
opcode_78:
        FETCH_NEXT_STAGE_01_I
        orr     flags, flags, #4
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 79 - ADC_macro $0000,Y
opcode_79:
        EA_ABSOLUTE_INDEXED_LOAD regY carry NOSIGN
        ADC_STAGE1 r1  FETCH JUMP_ADC_STAGE2      
         .balign I_ALIGN

// Opcode 7A - PLY
opcode_7A:
        SPOP_FETCH_STAGE_0_I   regY
        FETCH_NEXT_STAGE_1_I
        lsl     regY, regY, #24
        teq     regY, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 7B - NOP
opcode_7B:
         NOP_1   
        .balign I_ALIGN

// Opcode 7C - JMP ($0000,X)
opcode_7C:
        EA_ABSOLUTE_INDEXED regX
#if defined(USE_MEMORY_POINTER)        
        EA_INDIRECT
        add     regPC, r0, memry
#else
        EA_INDIRECT regPC
#endif        
        FETCH_NEXT_3B_MERGED SUBPC
        .balign I_ALIGN
        
// Opcode 7D - ADC_macro $0000,X
opcode_7D:
        EA_ABSOLUTE_INDEXED_LOAD regX carry NOSIGN
        ADC_STAGE1 r1 FETCH JUMP_ADC_STAGE2
        .balign I_ALIGN

// Opcode 7E - ROR $0000,X
opcode_7E:
        EA_ABSOLUTE_INDEXED_LOAD_NOCHECK regX carry NOSIGN
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        orrcs   r1, r1, #0x100
        rrxs    r1, r1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 7F - BBR7
opcode_7F:
        BBR 7
        .balign I_ALIGN

// Opcode 80 - BRA
opcode_80:
        BRANCH_AL
        .balign I_ALIGN

// Opcode 81 - STA ($00,X)
opcode_81:
        EA_INDIRECT_X_FETCH_STAGE_0 store r1
        FETCH_NEXT_STAGE_1
        STORE_BYTE    regA r1
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 82 - NOP #$00
opcode_82:
        NOP_2
        .balign I_ALIGN

// Opcode 83 - NOP
opcode_83:
        NOP_1
        .balign I_ALIGN

// Opcode 84 - STY $00
opcode_84:
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        lsr     r1, regY, #24
        STORE_BYTE    
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 85 - STA $00
opcode_85:
        STORE_BYTE    regA
        FETCH_NEXT_STAGE_012
        .balign I_ALIGN

// Opcode 86 - STX $00
opcode_86:
        FETCH_NEXT_STAGE_0
        lsr     r1, regX, #24
        STORE_BYTE
        FETCH_NEXT_STAGE_12
        .balign I_ALIGN

// Opcode 87 - SMB0 $00
opcode_87:
        SMB 0
        .balign I_ALIGN

// Opcode 88 - DEY
opcode_88:
        FETCH_NEXT_STAGE_01_I
        sub     regY, regY, #0x1000000
        teq     regY, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 89 - BIT #$00
opcode_89:
        FETCH_NEXT_STAGE_0
        sxtb    r0,r0
        mrs     r1, CPSR
        tst     regA, r0
        bic     r1, r1, #Z_FLAG
        orreq   r1, r1, #Z_FLAG
        msr     CPSR_flg, r1
        FETCH_NEXT_STAGE_12
        .balign I_ALIGN

// Opcode 8A - TXA
opcode_8A:
        FETCH_NEXT_STAGE_01_I
        sxtb    regA, regX, ror #24
        TEQ     regA, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 8B - NOP
opcode_8B:
        NOP_1
        .balign I_ALIGN

// Opcode 8C - STY $0000
opcode_8C:
        EA_ABSOLUTE
        lsr     r1, regY, #24
        STORE_ABSOLUTE flags r1
        .balign I_ALIGN

// Opcode 8D - STA $0000
opcode_8D:
        EA_ABSOLUTE
        STORE_ABSOLUTE flags regA
        .balign I_ALIGN

// Opcode 8E - STX $0000
opcode_8E:
        EA_ABSOLUTE
        lsr     r1, regX, #24
        STORE_ABSOLUTE flags r1
        .balign I_ALIGN

// Opcode 8F - BBS0
opcode_8F:
        BBS 0
        .balign I_ALIGN

// Opcode 90 - BCC
opcode_90:
        BRANCH cc cs
        .balign I_ALIGN

// Opcode 91 - STA ($00),Y
opcode_91:
        EA_INDIRECT r1
        FETCH_NEXT_STAGE_01
        strb regA,[r1,regY,lsr #24]
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 92 - STA ($00)
opcode_92:
        EA_INDIRECT r1
        FETCH_NEXT_STAGE_01
        STORE_BYTE regA r1
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 93 - NOP
opcode_93:
        NOP_1       
        .balign I_ALIGN

// Opcode 94 - STY $00,X
opcode_94:
        FETCH_NEXT_STAGE_0
        SETUP_ZPX regX
        lsr     r1, regY, #24
        STORE_ZPX
        FETCH_NEXT_STAGE_12
        .balign I_ALIGN

// Opcode 95 - STA $00,X
opcode_95:
        FETCH_NEXT_STAGE_0
        SETUP_ZPX regX //add     r0, regX, r0, lsl #24
        FETCH_NEXT_STAGE_1
        STORE_ZPX regA
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 96 - STX $00,Y
opcode_96:
        FETCH_NEXT_STAGE_0
        SETUP_ZPX regY // add     r0, regY, r0, lsl #24
        lsr     r1, regX, #24
        STORE_ZPX
        FETCH_NEXT_STAGE_12
        .balign I_ALIGN

// Opcode 97 - SMB1 $00
opcode_97:
        SMB 1
        .balign I_ALIGN

// Opcode 98 - TYA
opcode_98:
        FETCH_NEXT_STAGE_01_I
        sxtb    regA, regY,ror #24
        TEQ     regA, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 99 - STA $0000,Y
opcode_99:
        EA_ABSOLUTE_INDEXED regY
        STORE_ABSOLUTE flags regA 
        .balign I_ALIGN

// Opcode 9A - TXS
opcode_9A:
        FETCH_NEXT_STAGE_01_I
#if defined(USE_MEMORY_POINTER)        
        add     regSP, memry, #0x100
#else
        mov     regSP,#0x100
#endif        
        add     regSP, regSP, regX, lsr #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode 9B - NOP
opcode_9B:
        NOP_1
        .balign I_ALIGN

// Opcode 9C - STZ $0000
opcode_9C:
        EA_ABSOLUTE
        FETCH_NEXT_3B_STAGE_0
        STORE_BYTE  instt
        FETCH_NEXT_STAGE_012
        .balign I_ALIGN

// Opcode 9D - STA $0000,X
opcode_9D:
        EA_ABSOLUTE_INDEXED regX
        STORE_ABSOLUTE flags regA
        .balign I_ALIGN

// Opcode 9E - STZ $0000,X
opcode_9E:
        EA_ABSOLUTE_INDEXED regX
        FETCH_NEXT_3B_STAGE_0
        STORE_BYTE  instt
        FETCH_NEXT_STAGE_012
        .balign I_ALIGN

// Opcode 9F - BBS1
opcode_9F:
        BBS 1
        .balign I_ALIGN

// Opcode A0 - LDY #$00
opcode_A0:
        FETCH_NEXT_STAGE_0
        FLAGS   regY r0
        .balign I_ALIGN

// Opcode A1 - LDA ($00,X)
opcode_A1:
        EA_INDIRECT_X_FETCH_STAGE_0 no regA
        FLAGS_QUICK   regA
        .balign I_ALIGN

// Opcode A2 - LDX #$00
opcode_A2:
        FETCH_NEXT_STAGE_0
        FLAGS   regX r0
        .balign I_ALIGN

// Opcode A3 - NOP
opcode_A3:
        NOP_1
        .balign I_ALIGN

// Opcode A4 - LDY $00
opcode_A4:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FLAGS   regY
        .balign I_ALIGN

// Opcode A5 - LDA $00
opcode_A5:
        LOAD_BYTE_FETCH_NEXT_STAGE_0 no regA
        FLAGS_QUICK   regA
        .balign I_ALIGN

// Opcode A6 - LDX $00
opcode_A6:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FLAGS   regX
        .balign I_ALIGN

// Opcode A7 - SMB2 $00
opcode_A7:
        SMB 2
        .balign I_ALIGN

// Opcode A8 - TAY
opcode_A8:
        FETCH_NEXT_STAGE_01_I
        lsl   regY, regA, #24
        TEQ   regA, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode A9 - LDA #$00
opcode_A9:
        FETCH_NEXT_STAGE_0
        FLAGS_SIGN regA r0
        .balign I_ALIGN

// Opcode AA - TAX
opcode_AA:
        FETCH_NEXT_STAGE_01_I
        lsl    regX, regA, #24
        TEQ    regA, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode AB - NOP
opcode_AB:
        NOP_1
        .balign I_ALIGN

// Opcode AC - LDY $0000
opcode_AC:
         EA_ABSOLUTE_LOAD
         FETCH_NEXT_STAGE_0
         FETCH_NEXT_STAGE_1
         lsl    regY,r1,#24
         teq    regY,#0
         FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode AD - LDA $0000
opcode_AD:
        EA_ABSOLUTE_LOAD carry SIGN regA
        FETCH_NEXT_STAGE_0
        FLAGS_QUICK   regA
        .balign I_ALIGN

// Opcode AE - LDX $0000
opcode_AE:
        EA_ABSOLUTE_LOAD
        FETCH_NEXT_STAGE_0
         FLAGS   regX
        .balign I_ALIGN

// Opcode AF - BBS2
opcode_AF:
        BBS 2
        .balign I_ALIGN

// Opcode B0 - BCS
opcode_B0:
        BRANCH cs cc
        .balign I_ALIGN

// Opcode B1 - LDA ($00),Y
opcode_B1:
        EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0 regA
        FLAGS_QUICK regA
        .balign I_ALIGN

// Opcode B2 - LDA ($00)
opcode_B2:
        EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0 regA
        FLAGS_QUICK regA
        .balign I_ALIGN

// Opcode B3 - NOP
opcode_B3:
        B      opcode_EA
        
tube_load_handler:
        push    {r0,r2,r3,r5,r12,lr} 
        mrs     r5, CPSR               // Save 6502 flags
        bl      tube_parasite_read
        mov     r1,r0
        msr     CPSR_flg, r5
        pop     {r0,r2,r3,r5,r12,pc}

tube_load_handler_sign:
        push    {r0,r2,r3,r5,r12,lr} 
        mrs     r5, CPSR               // Save 6502 flags
        bl      tube_parasite_read
        sxtb    r1,r0
        msr     CPSR_flg, r5
        pop     {r0,r2,r3,r5,r12,pc}          
        .balign I_ALIGN

// Opcode B4 - LDY $00,X
opcode_B4:   
        LOAD_ZPX
        FETCH_NEXT_STAGE_1
        lsl    regY,r1,#24
        teq    regY,#0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode B5 - LDA $00,X
opcode_B5:
        LOAD_ZPX regX FETCH SIGN regA
        FLAGS_QUICK   regA
        .balign I_ALIGN

// Opcode B6 - LDX $00,Y
opcode_B6:
        LOAD_ZPX regY
         FLAGS   regX
        .balign I_ALIGN

// Opcode B7 - SMB3 $00
opcode_B7:
        SMB 3
        .balign I_ALIGN

// Opcode B8 - CLV
opcode_B8:
        FETCH_NEXT_STAGE_01_I
        bic     flags, #0x40
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode B9 - LDA $0000,Y
opcode_B9:
         EA_ABSOLUTE_INDEXED_LOAD regY carry SIGN regA
         FETCH_NEXT_STAGE_0
         FLAGS_QUICK   regA
        .balign I_ALIGN

// Opcode BA - TSX
opcode_BA:
        FETCH_NEXT_STAGE_01_I
        lsl     regX, regSP, #24
        teq     regX, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode BB - NOP
opcode_BB:
        B      opcode_EA
tube_load_handler_regA_flags:
        push    {r0,r2,r3,r5,r12} 
        mrs     r5, CPSR               // Save 6502 flags
        bl      tube_parasite_read
        sxtb    regA,r0
        msr     CPSR_flg, r5
        pop     {r0,r2,r3,r5,r12}   
        FETCH_NEXT_STAGE_0
         FLAGS_QUICK   regA        
        
        .balign I_ALIGN

// Opcode BC - LDY $0000,X
opcode_BC:
        //EA_ABSOLUTE_INDEXED_LOAD regX
        EA_ABSOLUTE_INDEXED regX     
        FETCH_NEXT_3B_STAGE_0
        LOAD_ABSOLUTE carry SIGN r1
        FETCH_NEXT_STAGE_0
        //FLAGS   regY
         FETCH_NEXT_STAGE_1
         lsl    regY,r1,#24
         teq    regY,#0
         FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode BD - LDA $0000,X
opcode_BD:
         EA_ABSOLUTE_INDEXED regX     
         FETCH_NEXT_3B_STAGE_0
         lsr     lr, r0, #5
         teq     tregs, lr          // tregs holds the constant 0xFEE0 >> 5
         Beq      tube_load_handler_regA_flags
         ldrsb  regA,[r0] 
         FETCH_NEXT_STAGE_0
         FLAGS_QUICK   regA
        .balign I_ALIGN

// Opcode BE - LDX $0000,Y
opcode_BE:
        EA_ABSOLUTE_INDEXED_LOAD regY
         FETCH_NEXT_STAGE_0
         FLAGS   regX
        .balign I_ALIGN

// Opcode BF - BBS3
opcode_BF:
        BBS 3
        .balign I_ALIGN

// Opcode C0 - CPY #$00
opcode_C0:
        FETCH_NEXT_STAGE_0
        cmp     regY, r0, lsl #24
        FETCH_NEXT_STAGE_12
        .balign I_ALIGN

// Opcode C1 - CMP ($00,X)
opcode_C1:
        EA_INDIRECT_X_FETCH_STAGE_0
        lsl     r2, regA, #24
        FETCH_NEXT_STAGE_1
        cmp     r2, r1, LSL #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode C2 - NOP #$00
opcode_C2:
        NOP_2       
        .balign I_ALIGN

// Opcode C3 - NOP
opcode_C3:
        NOP_1
        .balign I_ALIGN

// Opcode C4 - CPY $00
opcode_C4:
        LOAD_BYTE
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1
        cmp     regY, r1, lsl #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode C5 - CMP $00
opcode_C5:
        LOAD_BYTE
        FETCH_NEXT_STAGE_0
        lsl     r2, regA, #24
        FETCH_NEXT_STAGE_1
        cmp     r2, r1, LSL #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode C6 - DEC $00
opcode_C6:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        sub     r1, r1, #1   
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        sxtb    r1,r1
        TEQ     r1,#0
        FETCH_NEXT_STAGE_2

        .balign I_ALIGN

// Opcode C7 - SMB4 $00
opcode_C7:
        SMB 4
        .balign I_ALIGN

// Opcode C8 - INY
opcode_C8:
        add     regY, regY, #0x1000000
        FETCH_NEXT_STAGE_01_I
        teq     regY, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode C9 - CMP #$00
opcode_C9:
        FETCH_NEXT_STAGE_0
        LSL     r1, regA, #24
        FETCH_NEXT_STAGE_1a_OO
        cmp     r1, r0, LSL #24
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode CA - DEX
opcode_CA:
        FETCH_NEXT_STAGE_01_I
        sub     regX, regX, #0x1000000
        teq     regX, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode CB - WAI
opcode_CB:
        // TODO - Implement this
        FETCH_NEXT_STAGE_01_I
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode CC - CPY $0000
opcode_CC:
        EA_ABSOLUTE_LOAD nocarry
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1
        cmp     regY, r1, lsl #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode CD - CMP $0000
opcode_CD:
        EA_ABSOLUTE_LOAD nocarry
        FETCH_NEXT_STAGE_0
        lsl     r2, regA, #24
        FETCH_NEXT_STAGE_1
        cmp     r2, r1, LSL #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode CE - DEC $0000
opcode_CE:
        EA_ABSOLUTE_LOAD_NOCHECK
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        sub     r1, r1, #1
        STORE_ABSOLUTE_NOCHECK notest
        FETCH_NEXT_STAGE_1b_OO
        sxtb    r1,r1
        TEQ     r1,#0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode CF - BBS4
opcode_CF:
        BBS 4
        .balign I_ALIGN

// Opcode D0 - BNE
opcode_D0:
        BRANCH ne eq
        .balign I_ALIGN

// Opcode D1 - CMP ($00),Y
opcode_D1:
        EA_INDIRECT_Y_LOAD_FETCH_NEXT_STAGE_0
        lsl     r2, regA, #24
        FETCH_NEXT_STAGE_1
        cmp     r2, r1, LSL #24
        FETCH_NEXT_STAGE_2

        .balign I_ALIGN

// Opcode D2 - CMP ($00)
opcode_D2:
        EA_INDIRECT_LOAD_FETCH_NEXT_STAGE_0
        lsl     r2, regA, #24
        FETCH_NEXT_STAGE_1
        cmp     r2, r1, LSL #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode D3 - NOP
opcode_D3:
        NOP_1
        .balign I_ALIGN

// Opcode D4 - NOP $00,X
opcode_D4:
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_12     
        .balign I_ALIGN

// Opcode D5 - CMP $00,X
opcode_D5:
        LOAD_ZPX
        lsl     r2, regA, #24
        FETCH_NEXT_STAGE_1
        cmp     r2, r1, LSL #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode D6 - DEC $00,X
opcode_D6:
        LOAD_ZPX regX
        FETCH_NEXT_STAGE_1
        sub     r1, r1, #1
        STORE_ZPX
        sxtb    r1,r1
        TEQ     r1,#0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode D7 - SMB5 $00
opcode_D7:
        SMB 5
        .balign I_ALIGN

// Opcode D8 - CLD
opcode_D8:
        FETCH_NEXT_STAGE_01_I
        bic     flags, flags, #8
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode D9 - CMP $0000,Y
opcode_D9:
        EA_ABSOLUTE_INDEXED_LOAD regY nocarry
        FETCH_NEXT_STAGE_0
        lsl     r2, regA, #24
        FETCH_NEXT_STAGE_1
        cmp     r2, r1, LSL #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode DA - PHX
opcode_DA:
        FETCH_NEXT_STAGE_01_I
        lsr     r1, regX, #24
        SPUSH   r1
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode DB - STP
opcode_DB:
        // TODO - Implement this
        FETCH_NEXT_STAGE_01_I
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode DC - NOP $0000
opcode_DC:
         FETCH_NEXT_3B_MERGED
        .balign I_ALIGN

// Opcode DD - CMP $0000,X
opcode_DD:
        EA_ABSOLUTE_INDEXED_LOAD regX nocarry
        FETCH_NEXT_STAGE_0
        lsl     r2, regA, #24
        FETCH_NEXT_STAGE_1
        cmp     r2, r1, LSL #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode DE - DEX $0000,X
opcode_DE:
        EA_ABSOLUTE_INDEXED_LOAD_NOCHECK regX 
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        sub     r1, r1, #1
        STORE_ABSOLUTE_NOCHECK notest
        FLAGS_OO 
        .balign I_ALIGN

// Opcode DF - BBS5
opcode_DF:
        BBS 5
        .balign I_ALIGN

// Opcode E0 - CPX #$00
opcode_E0:
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        cmp     regX, r0, lsl #24
        FETCH_NEXT_STAGE_1b_OO
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode E1 - SBC ($00,X)
opcode_E1:
        EA_INDIRECT_X NOSIGN
        SBC_STAGE1     r1 NOFETCH
        SBC_STAGE2     r1
        .balign I_ALIGN

// Opcode E2 - NOP #$00
opcode_E2:
        NOP_2  
        tube_store_handler_regA:
        mov     r1, regA 
tube_store_handler:
        push    {r2-r3,r12} 
        bl      tube_parasite_write_banksel
        pop     {r2-r3,r12}
        FETCH_NEXT_STAGE_0
        msr     CPSR_flg, r2           // restore 6502 flags, including ARM
        FETCH_NEXT_STAGE_12
         .balign I_ALIGN

opcode_E3:
         B     opcode_EA
tube_load_handler_regA:
        push    {r0,r2,r3,r5,r12,lr} 
        mrs     r5, CPSR               // Save 6502 flags
        bl      tube_parasite_read
        sxtb    regA,r0
        msr     CPSR_flg, r5
        pop     {r0,r2,r3,r5,r12,pc}
         .balign I_ALIGN

// Opcode E4 - CPX $00
opcode_E4:
        LOAD_BYTE
        FETCH_NEXT_STAGE_01
        cmp     regX, r1, lsl #24
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode E5 - SBC_macro $00
opcode_E5:
        LOAD_BYTE NOSIGN
        SBC_STAGE1     r1
JUMP_SBC_STAGE2:        
        SBC_STAGE2     r1
        .balign I_ALIGN

// Opcode E6 - INC $00
opcode_E6:
        LOAD_BYTE_FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        add     r1, r1, #1
        STORE_BYTE
        FETCH_NEXT_STAGE_1b_OO
        sxtb    r1,r1   
        TEQ   r1,#0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode E7 - SMB6 $00
opcode_E7:
        SMB 6
        .balign I_ALIGN

// Opcode E8 - INX
opcode_E8:
        FETCH_NEXT_STAGE_01_I
        add     regX, regX, #0x1000000
        teq     regX, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode E9 - SBC_macro #$00
opcode_E9:
         SBC_STAGE1     r0
         SBC_STAGE2     r0 
        .balign I_ALIGN

// Opcode EA - NOP
opcode_EA:
        NOP_1
        .balign I_ALIGN

// Opcode EB - NOP ( Borrowed for instruction timing)
opcode_EB:
#if TIMING_INSTRUCTION
#if defined(RPI2)|| defined(RPI3)
        mrc     p15, 0, lr, c9, c13, 0
#else
        MRC     p15, 0, lr, c15, c12, 1    // Read Cycle Counter Register    
#endif      
        mov     r1,#0xE0
        str     lr,[r1] 
#endif        
        NOP_1
        .balign I_ALIGN

// Opcode EC - CPX $0000
opcode_EC:
        EA_ABSOLUTE_LOAD nocarry
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1
        cmp     regX, r1, lsl #24
        FETCH_NEXT_STAGE_2  
        .balign I_ALIGN

// Opcode ED - SBC_macro $0000
opcode_ED:
        EA_ABSOLUTE_LOAD carry NOSIGN
        SBC_STAGE1     r1 FETCH  JUMP_SBC_STAGE2
        .balign I_ALIGN

// Opcode EE - INC $0000
opcode_EE:
        EA_ABSOLUTE_LOAD_NOCHECK 
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        add     r1, r1, #1
        STORE_ABSOLUTE_NOCHECK notest
        FLAGS_OO
        .balign I_ALIGN

// Opcode EF - BBS6
opcode_EF:
        BBS 6
        .balign I_ALIGN

// Opcode F0 - BEQ
opcode_F0:
        BRANCH eq ne
        .balign I_ALIGN
        
// Opcode F1 - SBC_macro ($00),Y
opcode_F1:
        EA_INDIRECT
        EA_INDIRECT_Y
        FETCH_NEXT_STAGE_0
        LOAD_BYTE NOSIGN
        SBC_STAGE1     r1  NOFETCH
        SBC_STAGE2     r1
        .balign I_ALIGN

// Opcode F2 - SBC_macro ($00)
opcode_F2:
        EA_INDIRECT
        FETCH_NEXT_STAGE_0
        LOAD_BYTE NOSIGN
        SBC_STAGE1     r1 NOFETCH
        SBC_STAGE2     r1
        .balign I_ALIGN

// Opcode F3 - NOP
opcode_F3:
        NOP_1
        .balign I_ALIGN

// NOP_2
opcode_F4:
        NOP_2
        .balign I_ALIGN

// Opcode F5 - SBC_macro $00,X
opcode_F5:
        LOAD_ZPX regX FETCH NOSIGN
        SBC_STAGE1     r1  NOFETCH
        SBC_STAGE2     r1
        .balign I_ALIGN

// Opcode F6 - INC $00,X
opcode_F6:
        LOAD_ZPX regX
        FETCH_NEXT_STAGE_1
        add     r1, r1, #1
        STORE_ZPX
        sxtb    r1,r1
        TEQ     r1,#0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode F7 - SMB7 $00
opcode_F7:
        SMB 7
        .balign I_ALIGN

// Opcode F8 - SED
opcode_F8:
        FETCH_NEXT_STAGE_01_I
        orr     flags, flags, #8
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode F9 - SBC_macro $0000,Y
opcode_F9:
        EA_ABSOLUTE_INDEXED_LOAD regY carry NOSIGN
        SBC_STAGE1     r1  FETCH JUMP_SBC_STAGE2
        .balign I_ALIGN

// Opcode FA - PLX
opcode_FA:
        SPOP_FETCH_STAGE_0_I    regX
        FETCH_NEXT_STAGE_1_I
        lsl     regX, regX, #24
        teq     regX, #0
        FETCH_NEXT_STAGE_2
        .balign I_ALIGN

// Opcode FB - NOP
opcode_FB:
#if TIMING_INSTRUCTION
#if defined(RPI2)|| defined(RPI3)
        mrc     p15, 0, lr, c9, c13, 0
#else
        MRC     p15, 0, lr, c15, c12, 1    // Read Cycle Counter Register    
#endif      
        mov     r1,#0xE4
        str     lr,[r1] 
#endif
        NOP_1

        .balign I_ALIGN

// Opcode FC - NOP $0000
opcode_FC:
        FETCH_NEXT_3B_MERGED
        .balign I_ALIGN

// Opcode FD - SBC_macro $0000,X
opcode_FD:
        EA_ABSOLUTE_INDEXED_LOAD regX carry NOSIGN
        SBC_STAGE1     r1 FETCH JUMP_SBC_STAGE2
        .balign I_ALIGN

// Opcode FE - INC $0000,X
opcode_FE:
        EA_ABSOLUTE_INDEXED_LOAD_NOCHECK regX
        FETCH_NEXT_STAGE_0
        FETCH_NEXT_STAGE_1a_OO
        add     r1, r1, #1
        STORE_ABSOLUTE_NOCHECK notest
        FLAGS_OO
        .balign I_ALIGN

// Opcode FF - BBS7
opcode_FF:
        BBS 7
        .balign I_ALIGN

// **********************************************
// End of opcode implementations
// **********************************************

.rept 256
        EVENT_HANDLER_STAGE_0
        .balign I_ALIGN
.endr



// **********************************************
// Instruction timings
// **********************************************

// First byte is the number of cycles the instruction takes ( minus 1 for known jmps RTS as new pc will be detected and so extra cycle will be added)
// Second byte signifies number of bytes of instruction ) ie where PC will be 

timing_table:
.byte	7	,	1	//	0
.byte	6	,	2	//	1
.byte	2	,	2	//	2
.byte	1	,	1	//	3
.byte	5	,	2	//	4
.byte	3	,	2	//	5
.byte	5	,	2	//	6
.byte	5	,	2	//	7
.byte	3	,	1	//	8
.byte	2	,	2	//	9
.byte	2	,	1	//	A
.byte	1	,	1	//	B
.byte	6	,	3	//	C
.byte	4	,	3	//	D
.byte	6	,	3	//	E
.byte	5	,	3	//	F
.byte	2	,	2	//	10
.byte	5	,	2	//	11
.byte	5	,	2	//	12
.byte	1	,	1	//	13
.byte	5	,	2	//	14
.byte	4	,	2	//	15
.byte	6	,	2	//	16
.byte	5	,	2	//	17
.byte	2	,	1	//	18
.byte	4	,	3	//	19
.byte	2	,	1	//	1A
.byte	1	,	1	//	1B
.byte	6	,	3	//	1C
.byte	4	,	3	//	1D
.byte	7	,	3	//	1E
.byte	5	,	3	//	1F
.byte	5	,	3	//	20
.byte	6	,	2	//	21
.byte	2	,	2	//	22
.byte	1	,	1	//	23
.byte	3	,	2	//	24
.byte	3	,	2	//	25
.byte	5	,	2	//	26
.byte	5	,	2	//	27
.byte	4	,	1	//	28
.byte	2	,	2	//	29
.byte	2	,	1	//	2A
.byte	1	,	1	//	2B
.byte	4	,	3	//	2C
.byte	4	,	3	//	2D
.byte	6	,	3	//	2E
.byte	5	,	3	//	2F
.byte	2	,	2	//	30
.byte	5	,	2	//	31
.byte	5	,	2	//	32
.byte	1	,	1	//	33
.byte	4	,	2	//	34
.byte	4	,	2	//	35
.byte	6	,	2	//	36
.byte	5	,	2	//	37
.byte	2	,	1	//	38
.byte	4	,	3	//	39
.byte	2	,	1	//	3A
.byte	1	,	1	//	3B
.byte	4	,	3	//	3C
.byte	4	,	3	//	3D
.byte	7	,	3	//	3E
.byte	5	,	3	//	3F
.byte	5	,	1	//	40
.byte	6	,	2	//	41
.byte	2	,	2	//	42
.byte	1	,	1	//	43
.byte	3	,	2	//	44
.byte	3	,	2	//	45
.byte	5	,	2	//	46
.byte	5	,	2	//	47
.byte	3	,	1	//	48
.byte	2	,	2	//	49
.byte	2	,	1	//	4A
.byte	1	,	1	//	4B
.byte	2	,	3	//	4C
.byte	4	,	3	//	4D
.byte	6	,	3	//	4E
.byte	5	,	3	//	4F
.byte	2	,	2	//	50
.byte	5	,	2	//	51
.byte	5	,	2	//	52
.byte	1	,	1	//	53
.byte	4	,	2	//	54
.byte	4	,	2	//	55
.byte	6	,	2	//	56
.byte	5	,	2	//	57
.byte	2	,	1	//	58
.byte	4	,	3	//	59
.byte	3	,	1	//	5A
.byte	1	,	1	//	5B
.byte	8	,	3	//	5C
.byte	4	,	3	//	5D
.byte	7	,	3	//	5E
.byte	5	,	3	//	5F
.byte	5	,	1	//	60
.byte	6	,	2	//	61
.byte	2	,	2	//	62
.byte	1	,	1	//	63
.byte	3	,	2	//	64
.byte	3	,	2	//	65
.byte	5	,	2	//	66
.byte	5	,	2	//	67
.byte	4	,	1	//	68
.byte	2	,	2	//	69
.byte	2	,	1	//	6A
.byte	1	,	1	//	6B
.byte	5	,	3	//	6C
.byte	4	,	3	//	6D
.byte	6	,	3	//	6E
.byte	5	,	3	//	6F
.byte	2	,	2	//	70
.byte	5	,	2	//	71
.byte	5	,	2	//	72
.byte	1	,	1	//	73
.byte	4	,	2	//	74
.byte	4	,	2	//	75
.byte	6	,	2	//	76
.byte	5	,	2	//	77
.byte	2	,	1	//	78
.byte	4	,	3	//	79
.byte	4	,	1	//	7A
.byte	1	,	1	//	7B
.byte	5	,	3	//	7C
.byte	4	,	3	//	7D
.byte	7	,	3	//	7E
.byte	5	,	3	//	7F
.byte	3	,	2	//	80
.byte	6	,	2	//	81
.byte	2	,	2	//	82
.byte	1	,	1	//	83
.byte	3	,	2	//	84
.byte	3	,	2	//	85
.byte	3	,	2	//	86
.byte	5	,	2	//	87
.byte	2	,	1	//	88
.byte	2	,	2	//	89
.byte	2	,	1	//	8A
.byte	1	,	1	//	8B
.byte	4	,	3	//	8C
.byte	4	,	3	//	8D
.byte	4	,	3	//	8E
.byte	5	,	3	//	8F
.byte	2	,	2	//	90
.byte	6	,	2	//	91
.byte	5	,	2	//	92
.byte	1	,	1	//	93
.byte	4	,	2	//	94
.byte	4	,	2	//	95
.byte	4	,	2	//	96
.byte	5	,	2	//	97
.byte	2	,	1	//	98
.byte	5	,	3	//	99
.byte	2	,	1	//	9A
.byte	1	,	1	//	9B
.byte	4	,	3	//	9C
.byte	5	,	3	//	9D
.byte	5	,	3	//	9E
.byte	5	,	3	//	9F
.byte	2	,	2	//	A0
.byte	6	,	2	//	A1
.byte	2	,	2	//	A2
.byte	1	,	1	//	A3
.byte	3	,	2	//	A4
.byte	3	,	2	//	A5
.byte	3	,	2	//	A6
.byte	5	,	2	//	A7
.byte	2	,	1	//	A8
.byte	2	,	2	//	A9
.byte	2	,	1	//	AA
.byte	1	,	1	//	AB
.byte	4	,	3	//	AC
.byte	4	,	3	//	AD
.byte	4	,	3	//	AE
.byte	5	,	3	//	AF
.byte	2	,	2	//	B0
.byte	5	,	2	//	B1
.byte	5	,	2	//	B2
.byte	1	,	1	//	B3
.byte	4	,	2	//	B4
.byte	4	,	2	//	B5
.byte	4	,	2	//	B6
.byte	5	,	2	//	B7
.byte	2	,	1	//	B8
.byte	4	,	3	//	B9
.byte	2	,	1	//	BA
.byte	1	,	1	//	BB
.byte	4	,	3	//	BC
.byte	4	,	3	//	BD
.byte	4	,	3	//	BE
.byte	5	,	3	//	BF
.byte	2	,	2	//	C0
.byte	6	,	2	//	C1
.byte	2	,	2	//	C2
.byte	1	,	1	//	C3
.byte	3	,	2	//	C4
.byte	3	,	2	//	C5
.byte	5	,	2	//	C6
.byte	5	,	2	//	C7
.byte	2	,	1	//	C8
.byte	2	,	2	//	C9
.byte	2	,	1	//	CA
.byte	2	,	1	//	CB
.byte	4	,	3	//	CC
.byte	4	,	3	//	CD
.byte	6	,	3	//	CE
.byte	5	,	3	//	CF
.byte	2	,	2	//	D0
.byte	5	,	2	//	D1
.byte	5	,	2	//	D2
.byte	1	,	1	//	D3
.byte	4	,	2	//	D4
.byte	4	,	2	//	D5
.byte	6	,	2	//	D6
.byte	5	,	2	//	D7
.byte	2	,	1	//	D8
.byte	4	,	3	//	D9
.byte	3	,	1	//	DA
.byte	2	,	1	//	DB
.byte	4	,	3	//	DC
.byte	4	,	3	//	DD
.byte	7	,	3	//	DE
.byte	5	,	3	//	DF
.byte	2	,	2	//	E0
.byte	6	,	2	//	E1
.byte	2	,	2	//	E2
.byte	1	,	1	//	E3
.byte	3	,	2	//	E4
.byte	3	,	2	//	E5
.byte	5	,	2	//	E6
.byte	5	,	2	//	E7
.byte	2	,	1	//	E8
.byte	2	,	2	//	E9
.byte	2	,	1	//	EA
.byte	1	,	1	//	EB
.byte	4	,	3	//	EC
.byte	4	,	3	//	ED
.byte	6	,	3	//	EE
.byte	5	,	3	//	EF
.byte	2	,	2	//	F0
.byte	5	,	2	//	F1
.byte	5	,	2	//	F2
.byte	1	,	1	//	F3
.byte	4	,	2	//	F4
.byte	4	,	2	//	F5
.byte	6	,	2	//	F6
.byte	5	,	2	//	F7
.byte	2	,	1	//	F8
.byte	4	,	3	//	F9
.byte	4	,	1	//	FA
.byte	1	,	1	//	FB
.byte	4	,	3	//	FC
.byte	4	,	3	//	FD
.byte	7	,	3	//	FE
.byte	5	,	3	//	FF

   
// **********************************************
// Entry point to emulator
// **********************************************
//
// R0 Points to memory 
// R1 0=FAST core
//    1=SLOW core

// Globals used
//   copro_speed 
                          

exec_65tube:
        push    {r4-r11,lr}
#if defined(USE_MEMORY_POINTER)
        mov     memry, r0                  // set memry (memry) to the 64K 6502 memory array
#else
        mov     memry, #0xFFFFFF00
#endif        
        ldr     instt, =opcode_00  // set instt (r7) to the base address of the 6502 instruction handler
        movs    R1,R1
        orrne   instt, instt, #EVENT_HANDLER_FLAG+EVENT_HANDLER_SLOW_FLAG

        BL      tube_enable_fast6502

        ldr     tregs, =(0x0000fee0 >> 5)  // initialize the constant in tregs

        mov     flags, #0x00               // Initialize flags (flags) to zero on power up
                                        // This was added to fix a bug where the language transfer
                                        // would fail if exec_65tube was entered with certain
                                        // values of flags (specifically, with bit 7 = 1).
                                        // This is very strange, because flags bit 7 is not used!
                                        // To reproduce, initialize flags to 0x80

        // setup ARM cycle counter
#if defined(RPI2)|| defined(RPI3)
        mrc      p15 ,0, r0, c9, c12, 0
        BIC      r0, r0, #8  
        mcr      p15, 0, r0, c9, c12, 0
        mrc      p15, 0, r0, c9, c13, 0
#else
        MRC      p15, 0, r0, c15, c12, 0    // Read Performance Monitor Control Register
        BIC      r0 ,r0, #8                 // clear div 64 bit
        MCR      p15, 0, r0, c15, c12, 0    // Write Performance Monitor Control Register
        MRC      p15, 0, r0, c15, c12, 1    // Read Cycle Counter Register    
#endif        
        str      r0,targettime
        ldr      lr,=copro_speed
        ldr      lr,[lr]
        str      lr,copro_speed6502

handle_rst:
        orr     flags, flags, #4              // Set the I bit
        bic     flags, flags, #8              // Clear the D bit
#if defined(USE_MEMORY_POINTER)        
        add     r0, memry, #0x10000
        ldrh    r0, [r0, #-4]           // Fetch the address in FFFC,FFFD
        add     regPC, memry, r0              // Point to this instruction in memory
#else
        mov    r0, #0x10000
        ldrh    regPC, [r0, #-4]        // Fetch the vector address

#endif
        FETCH_NEXT_3B_MERGED  SUBPC             // Start executing instructions

exec_65tube_exit:                       // We get here from the Event handler when RST is detected        
        bl      tube_disable_fast6502
        pop     {r4-r11,pc}
        
// 6502 Event handler
// if USE_MULTICORE is defined, we have come here only from fetch_next above.
// if USE_MULTICORE is not defined, we have come here via a branch from the second instruction table

        .balign I_ALIGN
Event_Handler:
        bne     exec_65tube_exit        // exit immediately if active edge seen
        tst     r0, #2                  // Bit 1 set indicate NMI is active
        bne     handle_nmi              // Take the NMI interrupt

        tst     flags, #4               // Test whether interrupts are enabled (bit 2 = 0)
        bne     noirq
        tst     r0, #1                  // Bit 1 set indicate IRQ is active
        bne     handle_irq              // Take the IRQ interrupt

noirq:
        FETCH_NEXT_3B_STAGE_0  SUBPC
        msr     CPSR, r2                // restore the 6502 flags and re-enable ARM interrupts
        FETCH_NEXT_STAGE_012            // go back to executing 6502 instructions        

handle_irq_adjust_regPC:        
        sub     regPC, regPC ,#1
handle_irq:
        msr     CPSR, r2                // Restore the 6502 flags and re-enable ARM interrupts
        INTR    -2, 0
        FETCH_NEXT_3B_MERGED SUBPC

handle_nmi:
        orr     r0,r2, #(CPSR_FIQ_INHIBIT | CPSR_IRQ_INHIBIT)
        msr     CPSR_c,r0                 // Disable interrupts  
        ldr     r0,[r1]                 // load tube_irq again as it might have changed  
        BIC     r0,r0,#2                // clear NMI flag     
        str     r0,[r1]                 // Store it back again  
        msr     CPSR_cf, r2                // Restore the 6502 flags and re-enable ARM interrupts
        INTR    -6, 0
        FETCH_NEXT_3B_MERGED SUBPC

lastPC:
   .word 0
targettime:
   .word 0
   
copro_speed6502:
   .word 0 

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
        tst     r0, #4                  // Bit 2 set indicate RST is active
        bne     exec_65tube_exit        // exit immediately if active edge seen

        tst     r0, #2                  // Bit 1 set indicate NMI is active
        bne     handle_nmi2             // Take the NMI interrupt

        tst     flags, #4               // Test whether interrupts are enabled (bit 2 = 0)
        bne     noirq2
        tst     r0, #1                  // Bit 1 set indicate IRQ is active
        bne     handle_irq2             // Take the IRQ interrupt

noirq2:
        ldrb    r0, [regPC]             // get next instruction
        adr     r1, timing_table
        ldr     lr, lastPC

        ldrb    r0, [r1,r0,LSL#1]!       // get instruction time
        ldrb    r1, [r1,#1]              // get instruction length
        cmp     lr, regPC
        ldr     lr, copro_speed6502      // load copro_speed
        ADDNE   r0, r0, #1  
        
        add     r1, r1, regPC
        str     r1, lastPC
        
        // get last time
        LDR     r1, targettime
        MUL     r0, lr, r0
        ldrb    r5, [regPC]             // get next instruction
        // add on new time
        ADD     lr, r1, r0, LSR #8         // shift by 8 
        // save time new time.
        str     lr, targettime

        // loop until current time(R0) > nexttime(R1) ( nb deal with wrap around)
waste_time:       
#if defined(RPI2)|| defined(RPI3)
        mrc     p15, 0, lr, c9, c13, 0
#else
        MRC     p15, 0, lr, c15, c12, 1    // Read Cycle Counter Register    
#endif        
        sub     lr, lr, r1          // if this goes negative we are running too slow
        CMP     lr, r0, LSR #8
        BCC     waste_time

        msr     CPSR, r2                // restore the 6502 flags        
        bic     lr, instt, #EVENT_HANDLER_FLAG+EVENT_HANDLER_SLOW_FLAG     // ack any events

        ldrb    r0, [regPC, #1]
        FETCH_NEXT_STAGE_0 lr
        ldrb    r5, [regPC, #2]!
        bx      lr

handle_irq2:
        msr     CPSR, r2                // Restore the 6502 flags and re-enable ARM interrupts
        INTR    -2, 0
        b       execute_one_instruction

        
handle_nmi2:
        orr     r0,r2, #(CPSR_FIQ_INHIBIT | CPSR_IRQ_INHIBIT)
        msr     CPSR_c,r0               // Disable interrupts  
        ldr     r0,[r1]                 // load tube_irq again as it might have changed  
        BIC     r0,r0,#2                // clear NMI flag     
        str     r0,[r1]                 // Store it back again  
        msr     CPSR_cf, r2             // Restore the 6502 flags and re-enable ARM interrupts
        INTR    -6, 0
execute_one_instruction:
        bic     lr, instt, #EVENT_HANDLER_FLAG+EVENT_HANDLER_SLOW_FLAG     // ack any events

        ldrb    r5, [regPC]            // same as FETCH_NEXT_STAGE_012 but set bit in instt to return immediately to event handler
        ldrb    r0, [regPC, #1]
        FETCH_NEXT_STAGE_0 lr
        ldrb    r5, [regPC, #2]!
        bx      lr
                
decimal_mode_adc:
        // if carry is set the upper 24 bits of R1 are set too

        and     r0, regA, #0x0F<<24
        and     r2, r1, #15
        adc     r2, r2, r0, lsr #24
        cmp     r2, #10
        subcs   r2, r2, #10
        and     r2, r2, #0x0F
        orrcs   r1, r1 ,#0x0F
        and     regA, regA, #0xF0<<24
        orrcs   regA, regA, #0x0F<<24
        adcs    regA, regA, r1 , ror #8   
        orrvs   flags, #0x40
        cmpcc   regA, #10<<28
        subcs   regA, regA, #10<<28
        FETCH_NEXT_STAGE_1
        and     regA, regA, #0xF0<<24
        orr     regA, regA, r2, lsl #24
        sxtb    regA, regA, ror #24
        teq     regA, #0
        FETCH_NEXT_STAGE_2

decimal_mode_sbc:
        bic     r0, r1,# 0xf0
        and     r2, regA, #0xf000000
        sbc     r2, r2, r0, ror #8 
        FETCH_NEXT_STAGE_1a
        sbcs    regA, regA, r1, ror #8
        FETCH_NEXT_STAGE_1b
        orrvs   flags, #0x40
        subcc   regA, regA,#0x60<<24
        teq     r2,#0
        submi   regA, regA, #0x6<<24
        sxtb    regA, regA, ror #24
        teq     regA, #0
        FETCH_NEXT_STAGE_2

decimal_mode_adc_r0:
        and     r1, regA, #0x0F<<24
        and     r2, r0, #15
        adc     r2, r2, r1, lsr #24
        cmp     r2, #10
        subcs   r2, r2, #10
        and     r2, r2, #0x0F
        orrcs   r0, r0 ,#0x0F
        and     regA, regA, #0xF0<<24
        orrcs   regA, regA, #0x0F<<24
        adcs    regA, regA, r0 , ror #8
        orrvs   flags, #0x40
        cmpcc   regA, #10<<28
        subcs   regA, regA, #10<<28
        FETCH_NEXT_STAGE_1
        and     regA, regA, #0xF0<<24
        orr     regA, regA, r2, lsl #24
        sxtb    regA, regA, ror #24
        teq     regA, #0
        FETCH_NEXT_STAGE_2

decimal_mode_sbc_r0:
        bic     r1, r0,# 0xf0
        and     r2, regA, #0xf000000
        sbc     r2, r2, r1, ror #8 
        sbcs    regA, regA, r0, ror #8
        FETCH_NEXT_STAGE_1a
        orrvs   flags, #0x40
        subcc   regA, regA, #0x60<<24
        FETCH_NEXT_STAGE_1b
        teq     r2,#0
        submi   regA, regA, #0x6<<24
        sxtb    regA, regA, ror #24
        teq     regA, #0
        FETCH_NEXT_STAGE_2


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
      
      // lets get out of here

#endif  

//.rept 256
//        B    Event_Handler
//        .balign I_ALIGN
//.endr
      .balign I_ALIGN*256 , 0


.rept 256
        EVENT_HANDLER_SLOW_STAGE_0
        .balign I_ALIGN
.endr        
