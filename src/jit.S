// Co pro JIT 65C02

// Copyright Dominic Plunkett 2021

// Memory map
// 0x00000000 - 0x00FFFF 64K 65C02 RAM
// 0x00010000 - 0x010FFF 4K ram doubly mapped by the mmu
// 0x00010000 - 0x0FFFFF 1MByte for extra RAM we won't support code in this area

// we use the ram at 0x0C000000 for code as that
// is within jump range of the kernel
// we have up to 0x0CC00000 to use
// 0x0C000000 - 0x0C07FFFF 512K 8 bytes of JIT code for each 65c02 byte

// 0x0C100000 - 0x0C13FFFF 256K 4 bytes for each byte
//                                to signal if byte has been jitted
//                      either mov pc,r14 or B dejit16bit 1,21,22,31,32,33
// This table also needs to wrap at 256K to cope with 65535 wrapping
// This is done with the MMU just like the RAM

// NB this code starts 0x0D400000 + xxx so the above tables are close enough to jump to

// Very simple optimiser such that if the next instruction also sets the NZ flags then the current
// instruction doesn't need to set the flags. This can help to remove pipeline stalls
// especially after a ldrb instruction

#define JITDEBUG 0
//#define DORMANN_TEST 1
//#define DEBUG_TIMING 1
#define OPTIMISMISE 1
#define TIMING_INSTRUCTION 0

/* Interrupts

If an interrupt occurs try putting the current ARM PC and registers on to another stack and put fake address ( e.g. 0xFFFF) and status onto 65C02 stack jump off to vectors
Intercept the RTI instruction we could check for 0xFFFF on stack , restore status.
then return to our secret stack This relies on instructions being re entrant , but this should be fine
*/

// Instructions that get used in smaller chunks to save a jump ( overlapping )
// BIT &0000 so B on first byte
// BIT &00
// NOP6s &0000
// NOP6s &00

// used for scope debug
#include "tube-pins.h"
#include "rpi-mailboxregs.h"
#include "tube-irqbits.h"

#include "tube-defs.h"

#include "copro-65tubejit.h"

// Registers used in 6502 world

#define reg0      r0
#define reg1      r1
#define reg2      r2
#define reg3      r3
#define reg4      r4
#define jittedtable16ptr     r5
#define ram6502   r6

#define flags     r7
#define regY      r8
#define regSP     r9
#define regA      r10
#define regX      r11

#define reg12     r12

#define temp0 r0
#define temp1 r1
#define temp2 r2
#define temp r12

#define JITLETSHIFT 3

#define NEXTJITLET (1<<JITLETSHIFT)

#define JITTEDTABLESHIFT 2

#define BYTEWORDSHIFT 2

#define THREEBYTES 3
#define TWOBYTES 2
#define ONEBYTE 1

#define BEQINSTRUCTION 0x0A000000
#define BNEINSTRUCTION 0x1A000000
#define BCSINSTRUCTION 0x2A000000
#define BCCINSTRUCTION 0x3A000000
#define BMIINSTRUCTION 0x4A000000
#define BPLINSTRUCTION 0x5A000000
#define BINSTRUCTION   0xEA000000
#define BLINSTRUCTION  0xEB000000
// bx lr      0xe12fff1e
// mov pc,r14 0xe1a0f00e  // This is very slow on Pi3B+
#define MOVPCR14INSTRUCTION 0xe12fff1e
#define ARMNOP 0xe320f000
#define ARMBLCC 0x3B000000
#define ARMBLCS 0x2B000000

// Manually create instructions which have 0x00 shifts
// add r1,regx,#00<<24
#define ADDR1REGXZP    .word 0xe28b1400
#define ADDR1REGYCONST .word 0xe2881400
#define MOVREGXCONST   .word 0xe3a0b400
#define MOVREGYCONST   .word 0xe3a08400
#define MOVR1CONST     .word 0xe3a01400
#define CMPREGXCONST   .word 0xe35b0400
#define CMPREGYCONST   .word 0xe3580400
//   mov reg1,#0xFF<<8
#define MOVR0HIGH      .word 0xe3a00C00
#define MOVR1HIGH      .word 0xe3a01C00
#define ADDR1HIGH      .word 0xe2811C00
#define ARMNOPR0QUICK  .word 0xe1a00000
#define ARMNOPR1QUICK  .word 0xe1a01001
#define ARMNOPR6QUICK  .word 0xe1a06006

.equ    N_FLAG,                 0x80000000
.equ    Z_FLAG,                 0x40000000
.equ    C_FLAG,                 0x20000000
.equ    CPSR_FIQ_INHIBIT,       0x40
.equ    CPSR_IRQ_INHIBIT,       0x80

// 6502 status register bits

.equ    N_FLAG6502,  0x80
.equ    V_FLAG6502,  0x40
.equ    X_FLAG6502,  0x20
.equ    B_FLAG6502,  0x10
.equ    D_FLAG6502,  0x08
.equ    I_FLAG6502,  0x04
.equ    Z_FLAG6502,  0x02
.equ    C_FLAG6502,  0x01

// use for optimastions

// exit instruction lengths
#define E1  ( EF + 1 )
#define E2  ( EF + 2 )
#define E3  ( EF + 3 )

#define S1  ( 1 )
#define S2  ( 2 )
#define S3  ( 3 )

// simpify replace algorithm. If we have a replaceable flags setting instruction and the next instruction sets the N flag then we can replace the flags setting instruction with a NOP

#define EF 128

#define ZF 0
#define NF 64
#define CF 0
#define VF 0

#define NZF NF+ZF
#define CNZF CF+NF+ZF
#define VCNZF VF+CF+NF+ZF

.text
.global exec_65tubejit

// **** MACROS ******

.macro DEBUGDISS
#if JITDEBUG

   ldrb  temp,debugflag
   mov   r0,temp2
   mov   r1,r5,LSL #1
  // ldr r4,timingtemp
  // MRC   p15, 0, r1, c15, c12, 1
  // sub   r1,r1,r4
   movs  temp,temp
   BEQ   1f

   push  {r2-r3}
   BL    dissall
   pop   {r2-r3}
  // MRC   p15, 0, r1, c15, c12, 1
  // str r1,timingtemp

1:
#endif
.endm

.macro READ_CYCLE_COUNT
#if (__ARM_ARCH >= 7 )
   mrc   p15, 0, r0, c9, c13, 0
#else
   MRC   p15, 0, r0, c15, c12, 1    // Read Cycle Counter Register
#endif
.endm

.macro SPACE_THREE_ARM_WORDS
#if (__ARM_ARCH > 6 )
   nop
   nop
   nop
#else
   b 1f
   nop
   nop
1:
#endif
.endm

.macro SPACE_TWO_ARM_WORDS
   ARMNOPR6QUICK
   ARMNOPR1QUICK
.endm

.macro FILLINJITTEDTABLES table16
   ldr temp0,=\table16-(4*JITTEDTABLESHIFT)-JITTEDTABLE16
   // temp0 is address to goto
   // temp1 is location for store

   BL fillinjittedtable
.endm

.macro DEJITCLEAN count=0

#if (__ARM_ARCH >= 8 )
   MCR p15, 0, reg12, c7, c11, 1 // clean data cache line
   DSB
   MCR p15, 0, reg12, c7, c5, 1 // invalidate I cache line
   MCR p15, 0, reg12, c7, c5, 7 // invalidate BTB entry
   DSB
   .ifc \count,0
      ISB
   .endif
#else
   #if (__ARM_ARCH == 7 )
      DSB
   #endif

   MCR p15, 0, reg12, c7, c10, 1 // clean data cache line

   #if (__ARM_ARCH == 7 )
      MCR p15, 0, reg12, c7, c5, 1 // invalidate I cache line
      MCR p15, 0, reg12, c7, c5, 7 // invalidate BTB entry
      DSB
   #else
   //.ifc \count,0
      //MCR p15, 0, reg12, c7, c5, 0 //; invalidate I cache and BTB
      //MCR p15, 0, reg0, c7, c10, 4 // DSB
   //.endif
      MCR p15, 0, reg12, c7, c5, 1 // invalidate I cache line // errata
      MCR p15, 0, reg0, c7, c10, 4 // DSB
      MCR p15, 0, reg12, c7, c5, 1 // invalidate I cache line // errata
      MCR p15, 0, reg12, c7, c5, 7 // invalidate BTB entry
      .ifc \count,0
       //  MCR p15, 0, reg0, c7, c5, 6 // flush entire Branch Target Cache
         MCR p15, 0, reg0, c7, c5, 4 // flush prefetch buffer
      .endif
      MCR p15, 0, reg0, c7, c10, 4 // DSB

   #endif
#endif
.endm

.macro DEJITCLEANMID
   #if (__ARM_ARCH >= 7 )
      MCR p15, 0, reg12, c7, c5, 7 // invalidate BTB entry
   #else
      MCR p15, 0, reg12, c7, c5, 7 // invalidate BTB entry
   #endif
.endm

.macro DEBUG_REG reg
   push  {r0-r4,r14}
   mrs   r4, CPSR               // Save 6502 flags
   CPSID if
   mov   r0,\reg
   mov   r1,#8
   BL    dump_hex
   msr   CPSR, r4
   pop   {r0-r4,r14}
.endm

.macro DEBUG_CHAR char
   push  {r0-r4,r14}
   mrs   r4, CPSR               // Save 6502 flags
   CPSID if
   mov   r0,#\char
   BL    RPI_AuxMiniUartWrite
   msr   CPSR, r4
   pop   {r0-r4,r14}
.endm

.macro MAKEBRANCHLINK reg
   mov   \reg,\reg,LSR #BYTEWORDSHIFT
   sub   \reg,\reg,r6,LSR #BYTEWORDSHIFT
   ORR   \reg,\reg,#BLINSTRUCTION
.endm

// Macro for pushing a byte to the stack, whilst correctly handing wrapping
// This relies on the 6502 memory being 256 byte aligned
.macro SPUSH reg
   strb  \reg, [regSP], #-1
   orr   regSP, #0x0100
.endm

.macro JITINDXLOGICAL inst
   ADDR1REGXZP
   mov   reg1,reg1,LSR #24
   .word ldrhldrsb-16
   \inst regA,regA,reg0
.endm

.macro JITINDYLOGICAL inst
   ldrh  reg1,[ram6502,#0x00]
   mov   reg0,regY,LSR #24
   ldrsb reg0,[reg0,reg1]
   \inst regA,regA,reg0
.endm

.macro JITINDLOGICAL inst
   ldrh  reg1,[ram6502,#0x00]
   nop
   ldrsb reg0,[reg1]
   \inst regA,regA,reg0
.endm

.macro JITZPLOGICAL inst
   ldrsb reg0,[ram6502,#0x00]
   ARMNOPR6QUICK
   // fall into JITIMMLOGICAL
.endm

.macro JITZPXLOGICAL inst
   ADDR1REGXZP
   mov   reg1,reg1,LSR#24
   ldrsb reg0,[reg1]
   \inst regA,regA,reg0
.endm

.macro JITIMMLOGICAL inst
   ARMNOPR6QUICK
   \inst regA,regA,reg0
.endm

.macro JITABSLOGICAL inst
   JITABSLDRSB
   SPACE_THREE_ARM_WORDS
   \inst regA,regA,reg0
.endm

.macro JITABSXYLOGICAL inst index
   JITABSXYLDRSB \index reg0
   SPACE_TWO_ARM_WORDS
   \inst regA,regA,reg0
.endm

.macro JITRMB bitnum
   ldrb  reg0,[ram6502,#00]
   nop
   bic   reg0, #(1 << \bitnum)
   strb  reg0,[ram6502,#00]
.endm

.macro JITSMB bitnum
   ldrb  reg0,[ram6502,#00]
   nop
   orr   reg0, #(1 << \bitnum)
   strb  reg0,[ram6502,#00]
.endm

.macro JITBBR bitnum
   ldrb  reg0,[ram6502,#0x00]
   mrs   reg1, CPSR
   tst   reg0, #(1 << \bitnum)
   .word bbrreturn-20

   //   addne pc,r14,#4 // return to jit
   //   msr     CPSR_flg, reg1
   //   mov pc,r14

   B     .+8  // taken
   msr   CPSR_flg, reg1
.endm

.macro JITBBS bitnum
   ldrb  reg0,[ram6502,#0x00]
   mrs   reg1, CPSR
   tst   reg0, #(1 << \bitnum)
   .word bbsreturn-20

   //   addne pc,r14,#4 // return to jit
   //   msr     CPSR_flg, reg1
   //   mov pc,r14

   B .+8  // taken
   msr   CPSR_flg, reg1
.endm

.macro JITBRANCHVCS
   msr   CPSR_flg, reg1
.endm

.macro JITABSSTORE reg

.ifc \reg,regA
   MOVR1HIGH
   SPACE_THREE_ARM_WORDS
   strb  regA,[reg1,#0]!
.else
   MOVR1HIGH
   ARMNOPR6QUICK
   mov   reg0,\reg,LSR #24
   ARMNOPR6QUICK
   strb  reg0,[reg1,#0]!
.endif
   .word JITTEDTABLE16-28
.endm

.macro JITABS reg=reg1
.ifc \reg,reg1
   MOVR1HIGH
.else
   MOVR0HIGH
.endif
   orr   \reg,\reg,#0x00  // NB armv8 faster option
.endm

.macro JITABSXYLDRSB index reg
   mov   reg1, \index, LSR #24
   ADDR1HIGH  //add \reg,\reg, #00<<8
   ldrsb \reg, [reg1,#00]
.endm

.macro JITABSLDRB
   MOVR1HIGH
   ldrb  reg12,[reg1,#0x00]
.endm

.macro JITABSLDRBWB
   MOVR1HIGH
   ldrb  reg12,[reg1,#0x00]!
.endm

.macro JITABSLDRSB
   MOVR1HIGH
   ldrsb reg0,[reg1,#0x00]
.endm

.macro JITZPSTAZ reg
   ARMNOPR0QUICK
   mov   reg1,#00
   strb  \reg,[ram6502,#0x00]
   .word JITTEDTABLE16-20
.endm

.macro JITZPST reg
   mov   reg0,\reg,LSR #24
   mov   reg1,#00
   strb  reg0,[ram6502,#0x00]
   .word JITTEDTABLE16-20
.endm

.macro JITABSXYST reg index
   JITABS
   strb  \reg,[reg1,\index,LSR#24]!
   ARMNOPR6QUICK
   add   reg0,jittedtable16ptr,reg1,LSL#JITTEDTABLESHIFT
   blx   reg0
.endm

.macro ASL6
   add   reg12, reg12, reg12
   strb  reg12,[reg1]
   movs  reg12, reg12, lsl #24
.endm

.macro ROL6
   adc   reg12, reg12, reg12
   strb  reg12,[reg1]
   lsls  reg12, reg12, #24
.endm

.macro LSR6
   lsrs  reg12, reg12, #1
   strb  reg12,[reg1]
.endm

.macro ROR6
   orrcs reg12, reg12, #0x100
   rrxs  reg12, reg12
   strb  reg12,[reg1]
.endm

.macro INTR vector setbrk  fake     // vector is -2 for IRQ/BRK or -6 for NMI
                                // setbrk should be 1 for BRK, 0 for IRQ/NMI
   sub   regSP, regSP, #2
   mov   temp, #0x10000
.ifc \fake, FAKE
   sub   temp1,temp,#1         // 0xFFFF
.endif
   strh  temp1, [regSP,#1]         // slight risk of over running the stack
   orr   regSP, #0x0100
   ldrh  temp0, [temp, #\vector]     // Fetch the vector address

.if \setbrk
   orr   temp, flags, #X_FLAG6502+B_FLAG6502 // Always set bit 5 (-) in the pushed value and Set the B bit in the pushed value (in BRK, not in IRQ/NMI)
.else
   orr   temp, flags, #X_FLAG6502            // Always set bit 5 (-) in the pushed value
.endif
   orrmi temp, temp, #N_FLAG6502       // set N_FLAG NB bit defaults to zero
   orrcs temp, temp, #C_FLAG6502       // set C_FLAG NB bit defaults to zero
   orreq temp, temp, #Z_FLAG6502       // set Z_FLAG NB bit defaults to zero
   SPUSH temp
   mov   temp0,temp0,LSL#JITLETSHIFT
   add   temp0,temp0,#JITLET
   orr   flags, flags, #I_FLAG6502 // Set the I bit in the 6502 status register
   bic   flags, flags, #D_FLAG6502 // Clear the D bit in the 6502 status register
 // In the case of a "fake" interrupt, we need to explicitly re-enable FIQ
// interrupts, because the FIQ handler is not going to exit until the 6502
// RTI is executed. We leave IRQ masked at this point to avoid the possibility
// of a re-entrant IRQ, which IRQ handler should not have to deal with.
.ifc \fake, FAKE
   CPSIE if  // enable ARM FIQ interrupts only
.endif
.if \setbrk
   CPSIE if  // enable both ARM IRQ and FIQ interrupts
.endif
  // CPSIE i
   bx    temp0
.endm

.macro  PLP
   uadd8 reg0,regSP,reg12
   ldrb  reg0, [reg0]
   uadd8 regSP,regSP,reg12

   and   reg12, flags, #0xffffff00
   orr   reg12, reg12, reg0        // preserve the other bits in flags, e.g. the slowdown bit
   bic   flags, reg12, #N_FLAG6502+X_FLAG6502+B_FLAG6502+Z_FLAG6502+C_FLAG6502
   and   reg12, reg0, #N_FLAG6502
   and   reg0, reg0, #Z_FLAG6502+C_FLAG6502
   mov   reg12, reg12, lsl #24
   orr   reg12, reg12, reg0, lsl #29
   //   Need to update the ARM flags later with  msr     CPSR_flg, reg12
   // reg1 return address
   // reg12 has 6502 flags to be restored to the ARM
.endm

// **********************************************
// Start of opcode implementations
// **********************************************
Bopc_00: // Opcode 00 - BRK
   ARMNOPR6QUICK
   .word brk-12
Bopc_01: // Opcode 01 - ORA ($00,X)
   JITINDXLOGICAL orrs
Bopc_04: // Opcode 04 - TSB $00
   ldrb  reg12,[ram6502,#00]
   mrs   reg0, CPSR
   mov   reg1,#0x00
   .word tsb-20
Bopc_05: // Opcode 05 - ORA $00
   JITZPLOGICAL orrs
   // fall into JITIMMLOGICAL
Bopc_09: // Opcode 09 - ORA #$00
   JITIMMLOGICAL orrs
Bopc_06:  // Opcode 06 - ASL $00
   ldrb  reg12,[ram6502,#00]
   add   reg12, reg12, reg12
   strb  reg12,[ram6502,#00]
   movs  reg12, reg12, lsl #24
Bopc_06_fast:  // Opcode 06 - ASL $00
   ldrsb reg12,[ram6502,#00]
   ARMNOPR1QUICK
   adds  reg12, reg12, reg12
   strb  reg12,[ram6502,#00]
Bopc_07: // Opcode 07 - RMB0 $00
   JITRMB 0
Bopc_08: // Opcode 08 - PHP
   orr   reg12, flags, #X_FLAG6502+B_FLAG6502
   .word php-12
Bopc_0A: // Opcode 0A - ASL A
   lsls  regA, regA, #25
   sxtb  regA, regA , ror #24
Bopc_0C: // Opcode 0C - TSB $0000
   JITABSLDRBWB
   mrs   reg0, CPSR
   bic   reg0, reg0, #Z_FLAG
   tst   reg12, regA
   .word tsb2-28
Bopc_0D: // Opcode 0D - ORA $0000
   JITABSLOGICAL orrs
Bopc_0E: // Opcode 0E - ASL $0000
   JITABSLDRBWB
   nop
   ASL6
Bopc_0F: // Opcode 0F - BBR0
   JITBBR 0

Bopc_11: // Opcode 11 - ORA ($00),Y
   JITINDYLOGICAL orrs
Bopc_12: // Opcode 12 - ORA ($00)
   JITINDLOGICAL orrs
Bopc_14: // Opcode 14 - TRB $00
   ldrb  reg12,[ram6502,#00]
   mrs   reg0, CPSR
   mov   reg1,#0x00
   .word trb-20
Bopc_15: // Opcode 15 - ORA $00,X
   JITZPXLOGICAL orrs
Bopc_16: // Opcode 16 - ASL $00, X
   ADDR1REGXZP
   ldrb  reg12,[ram6502,reg1,LSR #24]
   mov   reg1,reg1,LSR #24  // can save a cycle here
   .word asl-20
Bopc_17: // Opcode 17 - RMB1 $00
   JITRMB 1
Bopc_19: // Opcode 19 - ORA $0000,Y
   JITABSXYLOGICAL orrs regY
Bopc_1A: // Opcode 1A - INC A
   add   regA, regA, #0x1
   .word aflags-12
Bopc_1A_fast: // Opcode 1A - INC A
   add   regA, regA, #0x1
   sxtb  regA, regA
Bopc_1C: // Opcode 1C - TRB $0000
   JITABSLDRBWB
   mrs   reg0, CPSR
   bic   reg0, reg0, #Z_FLAG
   tst   reg12, regA
   .word trb2-28
Bopc_1D: // Opcode 1D - ORA $0000,X
   JITABSXYLOGICAL orrs regX
Bopc_1E: // Opcode 1E - ASL $0000,X
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]!
   ASL6
Bopc_1F: // Opcode 1F - BBR1
   JITBBR 1

Bopc_20: // Opcode 20 - JSR $0000
   usub8  regSP, regSP, r4
   JITABS
   strh  reg1,[regSP,#1]
   .word JITLET-24
   nop
Bopc_21: // Opcode 21 - AND ($00,X)
   JITINDXLOGICAL ands
Bopc_24: // Opcode 24 - BIT $00
   ldrb  reg12,[ram6502,#0x00]
   .word bit24-12
Bopc_25: // Opcode 25 - AND $00
   JITZPLOGICAL ands
    // fall into JITIMMLOGICAL
Bopc_29: // Opcode 29 - AND #$00
   JITIMMLOGICAL ands
Bopc_26: // Opcode 26 - ROL $00
   ldrb  reg12,[ram6502,#00]
   adc   reg12, reg12, reg12
   strb  reg12,[ram6502,#00]
   movs  reg12, reg12, lsl #24
Bopc_26_fast: // Opcode 26 - ROL $00
   ldrsb reg12,[ram6502,#00]
   nop
   adcs  reg12, reg12, reg12
   strb  reg12,[ram6502,#00]
Bopc_27: // Opcode 27 - RMB2 $00
   JITRMB 2
Bopc_28: // Opcode 28 - PLP
   mov   reg12, #1
   .word plp-12

Bopc_2A: // Opcode 2A - ROL A
   adc   regA, regA, regA
   .word rola-12
Bopc_2A_fast: // Opcode 2A - ROL A
   adcs  regA, regA, regA
   sxtb  regA, regA
Bopc_2D: // Opcode 2D - AND $0000
   JITABSLOGICAL ands
Bopc_2E: // Opcode 2E - ROL $0000
   JITABSLDRBWB
   nop
   ROL6
Bopc_2F: // Opcode 2F - BBR2
   JITBBR 2

Bopc_31: // Opcode 31 - AND ($00),Y
   JITINDYLOGICAL ands
Bopc_32: // Opcode 32 - AND ($00)
   JITINDLOGICAL ands
Bopc_34: // Opcode 34 - BIT $00,X
   ADDR1REGXZP
   ldrb  reg12,[ram6502,reg1,LSR #24]
   bic   flags, flags, #V_FLAG6502
   .word bit-20
Bopc_35: // Opcode 35 - AND $00,X
   JITZPXLOGICAL ands
Bopc_36: // Opcode 36 - ROL $00,X
   ADDR1REGXZP
   ldrb  reg12,[ram6502, reg1,LSR#24]
   .word rol-16
   lsls    reg12, reg12, #24
Bopc_37: // Opcode 37 - RMB3 $00
   JITRMB 3
Bopc_39: // Opcode 39 - AND $0000,Y
   JITABSXYLOGICAL ands regY
Bopc_3A: // Opcode 3A - DEC A
   sub   regA, regA, #0x1
   .word aflags-12
Bopc_3A_fast: // Opcode 3A - DEC A
   sub   regA, regA, #0x1
   sxtb    regA, regA
Bopc_3C: // Opcode 3C - BIT $0000,X
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]
   bic   flags, flags, #V_FLAG6502
   tst   reg12, regA
   .word bit2-28
Bopc_3D: // Opcode 3D - AND $0000,X
   JITABSXYLOGICAL ands regX
Bopc_3E: // Opcode 3E - ROL $0000,X
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]!
   ROL6
Bopc_3F: // Opcode 3F - BBR3
   JITBBR 3

Bopc_40: // Opcode 40 - RTI
   mov   reg12,#1
   .word rti-12
Bopc_41: // Opcode 41 - EOR ($00,X)
   JITINDXLOGICAL eors
Bopc_45: // Opcode 45 - EOR $00
   JITZPLOGICAL eors
// fall into JITIMMLOGICAL
Bopc_49: // Opcode 49 - EOR #$00
   JITIMMLOGICAL eors
Bopc_46: // Opcode 46 - LSR $00
   ldrb  reg0,[ram6502,#00]
   nop
   lsrs  reg0, reg0, #1
   strb  reg0,[ram6502,#00]
Bopc_47: // Opcode 47 - RMB4 $00
   JITRMB 4
Bopc_48: // Opcode 48 - PHA
   SPUSH regA

Bopc_4A: // Opcode 4A - LSR A
   lsrs  regA, regA, #1
   and   regA, regA, #0x7F
Bopc_4D: // Opcode 4D - EOR $0000
   JITABSLOGICAL eors
Bopc_4E: // Opcode 4E - LSR $0000
   JITABSLDRBWB
   SPACE_TWO_ARM_WORDS
   LSR6
Bopc_4F: // Opcode 4F - BBR4
   JITBBR 4


Bopc_51: // Opcode 51 - EOR ($00),Y
   JITINDYLOGICAL eors
Bopc_52: // Opcode 52 - EOR ($00)
   JITINDLOGICAL eors
Bopc_55: // Opcode 55 - EOR $00,X
   JITZPXLOGICAL eors
Bopc_56: // Opcode 56 - LSR $00,X
   ADDR1REGXZP
   ldrb  reg0,[ram6502,reg1,LSR #24]
   lsrs  reg0, reg0, #1
   strb  reg0,[ram6502,reg1,LSR #24]
Bopc_57: // Opcode 57 - RMB5 $00
   JITRMB 5
Bopc_58: // Opcode 58 - CLI
   bic   flags, flags, #I_FLAG6502
   .word cli-12
Bopc_59: // Opcode 59 - EOR $0000,Y
   JITABSXYLOGICAL eors regY
Bopc_5A: // Opcode 5A - PHY
   mov   reg12, regY,LSR  #24
   .word pushtemp-12
Bopc_5D: // Opcode 5D - EOR $0000,X
   JITABSXYLOGICAL eors regX
Bopc_5E: // Opcode 5E - LSR $0000,X
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]!
   nop
   LSR6
Bopc_5F: // Opcode 5F - BBR5
   JITBBR 5

Bopc_60: // Opcode 60 - RTS
   ldrh  reg0,[regSP,#1]
   .word rts-12
Bopc_61: // Opcode 61 - ADC ($00,X)
   ADDR1REGXZP
   mov   reg1,reg1,LSR #24
   ldrh  reg1,[reg1]
   .word ADCload-20
Bopc_64: // Opcode 64 - STZ $00
   JITZPSTAZ ram6502
Bopc_64_fast: // Opcode 64 - STZ $00
   ldr   reg0, [jittedtable16ptr,#00] // address *4
   strb  ram6502, [ram6502,#00]
   teq   reg0, reg3
   .word storestable-20
Bopc_65: // Opcode 65 - ADC $00
   ldrb  reg12,[ram6502,#0x00]
   tst   flags, #D_FLAG6502
   bic   flags, #V_FLAG6502
   .word ADC2-20
Bopc_66: // Opcode 66 - ROR $00
   ldrb  reg12,[ram6502,#00]
   orrcs reg12, reg12, #0x100
   rrxs  reg12, reg12
   strb  reg12,[ram6502,#00]
Bopc_67: // Opcode 67 - RMB6 $00
   JITRMB 6
Bopc_68: // Opcode 68 - PLA
   mov   reg1,#1
   .word pla-12
Bopc_69: // Opcode 69 - ADC #$00
   mov   reg12,#00 // NB could save a cycle later on
   tst   flags, #D_FLAG6502
   bic   flags, #V_FLAG6502
   .word ADC2-20
Bopc_6A: // Opcode 6A - ROR A
   and   regA, regA, #0xFF    // isolate lower byte
   .word rora-12
Bopc_6C: // Opcode 6C - JMP ($0000)
   MOVR1HIGH
   ldrh  reg1,[reg1,#0x00]
   nop
   mov   reg0,#JITLET
   add   reg0,reg0,reg1,LSL #JITLETSHIFT
   bx    reg0
Bopc_6D: // Opcode 6D - ADC $0000
   JITABSLDRB
   tst   flags, #D_FLAG6502
   bic   flags, #V_FLAG6502
   LSL   regA, regA, #24
   .word ADC3-28
Bopc_6E: // Opcode 6E - ROR $0000
   JITABSLDRBWB
   nop
   ROR6
Bopc_6F: // Opcode 6F - BBR6
   JITBBR 6

Bopc_71: // Opcode 71 - ADC ($00),Y
   ldrh  reg1,[ram6502,#0x00]
   tst   flags, #D_FLAG6502
   ldrb  reg12,[reg1,regY,LSR #24]
   .word ADC1-20
Bopc_72: // Opcode 72 - ADC ($00)
   ldrh  reg1,[ram6502,#0x00]
   tst   flags, #D_FLAG6502
   ldrb  reg12,[reg1]
   .word ADC1-20
Bopc_74: // Opcode 74 - STZ $00,X
   ADDR1REGXZP
   strb  ram6502,[ram6502,reg1,LSR #24]
   add   reg0,jittedtable16ptr, reg1,LSR #24-JITTEDTABLESHIFT
   .word jittablezp-20

Bopc_75: // Opcode 75 - ADC $00,X
   ADDR1REGXZP
   tst   flags, #D_FLAG6502
   ldrb  reg12,[ram6502,reg1,LSR#24]
   .word ADC1-20
Bopc_76: // Opcode 76 - ROR $00,X
   ADDR1REGXZP
   mov   reg1,reg1,LSR #24 // can save a cycle here
   ldrb  reg12,[reg1]
   .word ror-20
Bopc_77: // Opcode 77 - RMB7 $00
   JITRMB 7
Bopc_78:  // Opcode 78 - SEI
   orr   flags, flags, #I_FLAG6502
   ARMNOPR6QUICK
Bopc_79: // Opcode 79 - ADC $0000,Y
   JITABS
   ldrb  reg12,[reg1, regY, lsr #24]
   tst   flags, #D_FLAG6502
   bic   flags, #V_FLAG6502
   .word ADC2-28
Bopc_7A: // Opcode 7A - PLY
   mov   reg1,#1
   .word ply-12
Bopc_7C: // Opcode 7C - JMP ($0000,X)
   mov   reg1, regX, LSR #24
   ADDR1HIGH  //add \reg,\reg, #00<<8
   ldrh reg1, [reg1,#00]
   mov   reg0,#JITLET
   nop
   add   pc,reg0,reg1,LSL #JITLETSHIFT
Bopc_7D: // Opcode 7D - ADC $0000,X
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]
   tst   flags, #D_FLAG6502
   bic   flags, #V_FLAG6502
   .word ADC2-28
Bopc_7E: // Opcode 7E - ROR $0000,X
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]!
   ROR6
Bopc_7F: // Opcode 7F - BBR7
   JITBBR 7

Bopc_81: // Opcode 81 - STA ($00,X)
   ADDR1REGXZP
   mov   reg1,reg1,LSR #24
   ldrh  reg1,[reg1]
   .word jitstatable-20
Bopc_84: // Opcode 84 - STY $00
   JITZPST regY
Bopc_85: // Opcode 85 - STA $00
   JITZPSTAZ regA
Bopc_85_fast: // Opcode 85 - STA $00
   ldr   reg0, [jittedtable16ptr,#00] // address *4
   strb  regA, [ram6502,#00]
   teq   reg0, reg3
   .word storestable-20
Bopc_86: // Opcode 86 - STX $00
   JITZPST regX
Bopc_87: // Opcode 87 - SMB0 $00
   JITSMB 0
Bopc_88: // Opcode 88 - DEY
   sub   regY, regY, #0x1000000
   teq   regY, #0
Bopc_89: // Opcode 89 - BIT #$00
   mrs   reg1, CPSR
   tst   regA, #00
   bicne reg1, reg1, #Z_FLAG
   .word bitimm-20
Bopc_8A: // Opcode 8A - TXA
   sxtb  regA, regX, ror #24
   TEQ   regX, #0
Bopc_8C: // Opcode 8C - STY $0000
   JITABSSTORE regY
Bopc_8C_io: // Opcode 8C - STY $0000
   JITABS reg0
   mov   reg12,#1
   mov   reg1,regY,LSR #24
   mrs   reg4, CPSR         // Save 6502 flags and current FIQ/IRQ bits
   .word iostore-28
Bopc_8D: // Opcode 8D - STA $0000
   JITABSSTORE regA
Bopc_8D_io: // Opcode 8D - STA $0000
   JITABS reg0
   mov   reg12,#1
   and   reg1,regA,#0xFF
   mrs   reg4, CPSR         // Save 6502 flags and current FIQ/IRQ bits
   .word iostore-28
Bopc_8E: // Opcode 8E - STX $0000
   JITABSSTORE regX
Bopc_8E_io: // Opcode 8E - STX $0000
   JITABS reg0
   mov   reg12,#1
   mov   reg1,regX,LSR #24
   mrs   reg4, CPSR         // Save 6502 flags and current FIQ/IRQ bits
   .word iostore-28
Bopc_8F: // Opcode 8F - BBS0
   JITBBS 0

Bopc_91: // Opcode 91 - STA ($00),Y
   ldrh  reg1,[ram6502,#00]
   strb  regA,[reg1,regY,LSR #24]!
   add   reg0,jittedtable16ptr, reg1, LSL #JITTEDTABLESHIFT
   BLX   reg0

Bopc_91_fast: // Opcode 91 - STA ($00),Y
   ldrh  reg1,[ram6502,#00]
   strb  regA,[reg1,regY,LSR #24]!
   ldr   reg0, [jittedtable16ptr, reg1, LSL #JITTEDTABLESHIFT]
   .word  storetest-20
Bopc_92: // Opcode 92 - STA ($00)
   ldrh  reg1,[ram6502,#00]
   strb  regA,[reg1]
   add   reg0,jittedtable16ptr, reg1,LSL #JITTEDTABLESHIFT
   BLX   reg0
Bopc_92_fast: // Opcode 92 - STA ($00)
   ldrh  reg1,[ram6502,#00]
   strb  regA,[reg1]
   ldr   reg0, [jittedtable16ptr, reg1, LSL #JITTEDTABLESHIFT]
   .word storetest-20

Bopc_94: // Opcode 94 - STY $00,X
   ADDR1REGXZP
   mov   reg0,regY,LSR #24
   strb  reg0,[ram6502,reg1,LSR #24]
   .word jitstxytable-20
Bopc_95: // Opcode 95 - STA $00,X
   ADDR1REGXZP
   strb  regA,[ram6502,reg1,LSR #24]
   add   reg0,jittedtable16ptr, reg1,LSR #24-JITTEDTABLESHIFT
   .word jittablezp-20
Bopc_96: // Opcode 96 - STX $00,Y
   ADDR1REGYCONST
   mov   reg0,regX,LSR #24
   strb  reg0,[ram6502,reg1,LSR #24]
   .word jitstxytable-20
Bopc_97: // Opcode 97 - SMB1 $00
   JITSMB 1
Bopc_98: // Opcode 98 - TYA
   sxtb  regA, regY, ror #24
   TEQ   regY, #0
Bopc_99: // Opcode 99 - STA $0000,Y
   JITABSXYST regA regY
Bopc_9A: // Opcode 9A - TXS
   bic   regSP, regSP, #0xFF             // clear lower nibble
   orr   regSP, regSP, regX, lsr #24     // put X in lower nibble
Bopc_9C: // Opcode 9C - STZ $0000
   JITABSSTORE ram6502
Bopc_9C_iostore:
   JITABS reg0
   mov   reg12,#1
   mov   reg1,#0
   mrs   reg4, CPSR         // Save 6502 flags and current FIQ/IRQ bits
   .word iostore-28
Bopc_9D: // Opcode 9D - STA $0000,X
   JITABSXYST regA regX
Bopc_9E: // Opcode 9E - STZ $0000,X
   JITABSXYST ram6502 regX
Bopc_9F: // Opcode 9F - BBS1
   JITBBS 1

Bopc_A0: // Opcode A0 - LDY #$00
   MOVREGYCONST
   ARMNOPR6QUICK
   teq   regY,#0
   ARMNOPR6QUICK
Bopc_A0_fast: // Opcode A0 - LDY #$00
   MOVREGYCONST
   SPACE_THREE_ARM_WORDS

Bopc_A1: // Opcode A1 - LDA ($00,X)
   ADDR1REGXZP
   mov   reg1,reg1,LSR #24
   ldrh  reg1,[reg1]
   .word loada-20
Bopc_A2:// Opcode A2 - LDX #$00
   MOVREGXCONST
   ARMNOPR6QUICK
   teq   regX,#0
   ARMNOPR6QUICK
Bopc_A2_fast:// Opcode A2 - LDX #$00
   MOVREGXCONST
   SPACE_THREE_ARM_WORDS
Bopc_A4: // Opcode A4 - LDY $00
   ldrsb  regY,[ram6502,#0x00]
   ARMNOPR6QUICK
   teq   regY,#0
   mov   regY,regY,LSL #24
Bopc_A4_fast: // Opcode A4 - LDY $00
   ldrsb  regY,[ram6502,#0x00]
   SPACE_TWO_ARM_WORDS
   mov   regY,regY,LSL #24
Bopc_A5: // Opcode A5 - LDA $00
   LDRSB regA,[ram6502,#0x00]
   SPACE_TWO_ARM_WORDS
   teq   regA,#0
Bopc_A5_fast: // Opcode A5 - LDA $00
   LDRSB regA,[ram6502,#0x00]
   SPACE_THREE_ARM_WORDS
Bopc_A6: // Opcode A6 - LDX $00
   ldrsb  regX,[ram6502,#0x00]
   ARMNOPR6QUICK
   teq   regX,#0
   mov   regX,regX,LSL #24
Bopc_A6_fast: // Opcode A6 - LDX $00
   ldrsb  regX,[ram6502,#0x00]
   SPACE_TWO_ARM_WORDS
   mov   regX,regX,LSL #24
Bopc_A7: // Opcode A7 - SMB2 $00
   JITSMB 2
Bopc_A8: // Opcode A8 - TAY
   lsl   regY, regA, #24
   TEQ   regA, #0
Bopc_AA: // Opcode AA - TAX
   lsl   regX, regA, #24
   TEQ   regA, #0
Bopc_AC: // Opcode AC - LDY $0000
   JITABSLDRB
   SPACE_TWO_ARM_WORDS
   mov   regY,reg12, LSL #24
   teq   regY,#0
Bopc_AC_io:
   JITABS r0
   .word ioload-16
   mov   regY,r0,LSL #24
   msr   CPSR, reg12         // Restore 6502 flags and FIQ/IRQ bits
   teq   regY,#0
Bopc_AD: // Opcode AD - LDA $0000
   MOVR1HIGH
   ldrsb regA,[reg1,#0x00]
   SPACE_THREE_ARM_WORDS
   teq   regA,#0
Bopc_AD_io: // Opcode AD - LDA $0000
   JITABS reg0
   .word ioload-16
   sxtb  regA,reg0
   msr   CPSR, reg12         // Restore 6502 flags and FIQ/IRQ bits
   teq   regA,#0
Bopc_AE: // Opcode AE - LDX $0000
   JITABSLDRB
   SPACE_TWO_ARM_WORDS
   mov   regX,reg12, LSL #24
   teq   regX,#0
Bopc_AE_io:
   JITABS reg0
   .word ioload-16
   mov   regX,reg0,LSL #24
   msr   CPSR, reg12         // Restore 6502 flags and FIQ/IRQ bits
   teq   regX,#0
Bopc_AF: // Opcode AF - BBS2
   JITBBS 2

Bopc_B1: // Opcode B1 - LDA ($00),Y
   ldrh  reg1,[ram6502,#0x00]
   mov   reg0,regY,LSR #24
   ldrsb regA,[reg1,reg0]
   teq   regA,#0
Bopc_B1_fast: // Opcode B1 - LDA ($00),Y
   ldrh  reg1,[ram6502,#0x00]
   mov   reg0,regY,LSR #24
   ARMNOPR6QUICK
   ldrsb regA,[reg1,reg0]
Bopc_B2: // Opcode B2 - LDA ($00)
   ldrh  reg1,[ram6502,#0x00]
   ARMNOPR6QUICK
   ldrsb regA,[reg1]
   teq   regA,#0
Bopc_B2_fast: // Opcode B2 - LDA ($00)
   ldrh  reg1,[ram6502,#0x00]
   SPACE_TWO_ARM_WORDS
   ldrsb regA,[reg1]

Bopc_B4: // Opcode B4 - LDY $00,X
   ADDR1REGXZP
   ldrb  regY,[ram6502,reg1,LSR#24]
   mov   regY,regY,LSL #24
   teq   regY,#0
Bopc_B5: // Opcode B5 - LDA $00,X
   ADDR1REGXZP
   mov   reg1,reg1,LSR#24
   ldrsb regA,[reg1]
   teq   regA,#0
Bopc_B5_fast: // Opcode B5 - LDA $00,X
   ADDR1REGXZP
   ARMNOPR6QUICK
   mov   reg1,reg1,LSR#24
   ldrsb regA,[reg1]
Bopc_B6: // Opcode B6 - LDX $00,Y
   ADDR1REGYCONST
   ldrb  reg12,[ram6502,reg1,LSR#24]
   mov   regX,reg12,LSL #24
   teq   regX,#0
Bopc_B7: // Opcode B7 - SMB3 $00
   JITSMB 3
Bopc_B8: // Opcode B8 - CLV
   ARMNOPR6QUICK
   bic   flags, #V_FLAG6502
Bopc_B9: // Opcode B9 - LDA $0000,Y
   JITABSXYLDRSB regY regA
   SPACE_TWO_ARM_WORDS
   teq   regA, #0
Bopc_B9_fast:
   JITABSXYLDRSB regY regA
   SPACE_THREE_ARM_WORDS

Bopc_BA: // Opcode BA - TSX
   lsl   regX, regSP, #24
   teq   regX, #0
Bopc_BC: // Opcode BC - LDY $0000,X
   JITABSXYLDRSB regX regY
   ARMNOPR6QUICK
   teq   regY, #0
   mov   regY,regY, lsl #24

Bopc_BC_fast: // Opcode BC - LDY $0000,X
   JITABSXYLDRSB regX regY
   SPACE_TWO_ARM_WORDS
   mov   regY,regY, lsl #24

Bopc_BD: // Opcode BD - LDA $0000,X
   JITABSXYLDRSB regX regA
   SPACE_TWO_ARM_WORDS
   teq   regA, #0
Bopc_BD_fast: // Opcode BD - LDA $0000,X
   JITABSXYLDRSB regX regA
   SPACE_THREE_ARM_WORDS

Bopc_BE: // Opcode BE - LDX $0000,Y
   JITABSXYLDRSB regY regX
   ARMNOPR6QUICK
   teq   regX, #0
   mov   regX,regX, lsl #24

Bopc_BE_fast: // Opcode BE - LDX $0000,Y
   JITABSXYLDRSB regY regX
   SPACE_TWO_ARM_WORDS
   mov   regX,regX, lsl #24
Bopc_BF: // Opcode BF - BBS3
   JITBBS 3

Bopc_C0: // Opcode C0 - CPY #$00
   CMPREGYCONST
   SPACE_THREE_ARM_WORDS
Bopc_C1: // Opcode C1 - CMP ($00,X)
   ADDR1REGXZP
   mov   reg1,reg1,LSR #24
   ldrh  reg1,[reg1]
   .word cmpa-20
Bopc_C4: // Opcode C4 - CPY $00
   ldrb  reg0,[ram6502,#0x00]
   SPACE_TWO_ARM_WORDS
   cmp   regY, reg0, lsl #24
Bopc_C5: // Opcode C5 - CMP $00
   ldrb  reg0,[ram6502,#0x00]
   ARMNOPR6QUICK
   mov   reg1,regA,LSL #24
   cmp   reg1, reg0, LSL #24
Bopc_C6: // Opcode C6 - DEC $00
   ldrb  reg0,[ram6502,#0]
   .word decflags-12
   teq   reg0,#0
   strb  reg0,[ram6502,#0]
Bopc_C6_fast: // Opcode C6 - DEC $00
   ldrb  reg0,[ram6502,#0]
   nop
   sub   reg0, reg0, #1
   strb  reg0,[ram6502,#0]
Bopc_C7: // Opcode C7 - SMB4 $00
   JITSMB 4
Bopc_C8: // Opcode C8 - INY
   add   regY, regY, #0x1000000
   teq   regY, #0
Bopc_C9: // Opcode C9 - CMP #$00
   MOVR1CONST
   rsbs  reg1,reg1,regA,LSL #24
   SPACE_TWO_ARM_WORDS
Bopc_CA:  // Opcode CA - DEX
   sub   regX, regX, #0x1000000
   teq   regX, #0
Bopc_CC: // Opcode CC - CPY $0000
   JITABSLDRB
   SPACE_THREE_ARM_WORDS
   cmp   regY,reg12, lsl #24
Bopc_CD: // Opcode CD - CMP $0000
   JITABSLDRB
   ARMNOPR6QUICK
   mov   reg1,regA,LSL #24
   ARMNOPR6QUICK
   cmp   reg1,reg12,LSL #24
Bopc_CE: // Opcode CE - DEC $0000
   JITABSLDRBWB
   sub   reg12, reg12, #1
   sxtb  reg12, reg12
   teq   reg12, #0
   strb  reg12,[reg1]

Bopc_CF: // Opcode CF - BBS4
   JITBBS 4

Bopc_D1: // Opcode D1 - CMP ($00),Y
   ldrh  reg1,[ram6502,#0x00]
   ldrb  reg0,[reg1,regY, LSR #24]
   mov   reg1,regA, LSL #24
   cmp   reg1,reg0,LSL #24
Bopc_D2: // Opcode D2 - CMP ($00)
   ldrh  reg1,[ram6502,#0x00]
   ldrb  reg0,[reg1]
   mov   reg1,regA, LSL #24
   cmp   reg1,reg0,LSL #24
Bopc_D5: // Opcode D5 - CMP $00,X
   ADDR1REGXZP
   ldrb  reg0,[ram6502,reg1,LSR#24]
   mov   reg1,regA,LSL #24
   cmp   reg1,reg0,LSL #24
Bopc_D6: // Opcode D6 - DEC $00,X
   ADDR1REGXZP
   ldrb  reg0,[ram6502,reg1,LSR#24]
   sub   reg0, reg0, #1
   .word storeXflags-20
Bopc_D7: // Opcode D7 - SMB5 $0
   JITSMB 5
Bopc_D8: // Opcode D8 - CLD
   ARMNOPR6QUICK
   bic   flags, flags, #D_FLAG6502
Bopc_D9: // Opcode D9 - CMP $0000,Y
   JITABS
   ldrb  reg0,[reg1,regY,LSR #24]
   nop
   mov   reg1,regA,LSL #24
   cmp   reg1,reg0,LSL #24
Bopc_DA:  // Opcode DA - PHX
   mov   reg12, regX, LSR #24
   .word pushtemp-12
Bopc_DD: // Opcode DD - CMP $0000,X
   JITABS
   ldrb  reg0,[reg1,regX,LSR #24]
   nop
   mov   reg1,regA,LSL #24
   cmp   reg1,reg0,LSL #24
Bopc_DE: // Opcode DE - DEC $0000,X
   JITABS
   ldrb  reg0,[reg1, regX, lsr #24]!
   sub   reg0, reg0, #1
   strb  reg0,[reg1]
   .word tempflags-28
Bopc_DF: // Opcode DF - BBS5
   JITBBS 5

Bopc_E0: // Opcode E0 - CPX #$00
   CMPREGXCONST
   SPACE_THREE_ARM_WORDS
Bopc_E1: // Opcode E1 - SBC ($00,X)
   ADDR1REGXZP
   mov   reg1,reg1,LSR #24
   ldrh  reg1,[reg1]
   .word SBCload-20
Bopc_E3: // Opcode E3 - NOP6
   mov   temp,#1
   .word debugopcode-12
Bopc_E4: // Opcode E4 - CPX $00
   ldrb  reg0,[ram6502,#0x00]
   SPACE_TWO_ARM_WORDS
   cmp   regX, reg0, lsl #24
Bopc_E5: // Opcode E5 - SBC $00
   ldrb  reg12,[ram6502,#0x00]
   tst   flags, #D_FLAG6502
   bic   flags, #V_FLAG6502
   .word SBC2-20
Bopc_E6: // Opcode E6 - INC $00
   ldrb  reg0,[ram6502,#0]
   .word incflags-12
   teq   reg0,#0
   strb  reg0,[ram6502,#0]
Bopc_E6_fast: // Opcode E6 - INC $00
   ldrb  reg0,[ram6502,#0]
   nop
   add   reg0, reg0, #1
   strb  reg0,[ram6502,#0]
Bopc_E7: // Opcode E7 - SMB6 $00
   JITSMB 6
Bopc_E8: // Opcode E8 - INX
   add   regX, regX, #0x1000000
   teq   regX, #0
Bopc_E9: // Opcode E9 - SBC #$00
   mov   reg12,#00 // NB could save a cycle later on
   tst   flags, #D_FLAG6502
   bic   flags, #V_FLAG6502
   .word SBC2-20
Bopc_EB: // Opcode EB - NOP6 ( Used for instruction timing)
#if defined(TIMING_INSTRUCTION)
   READ_CYCLE_COUNT
   str   r0,[ram6502,#0xE0]
#else
   nop
   nop
#endif
Bopc_EC: // Opcode EC - CPX $0000
   JITABSLDRB
   SPACE_THREE_ARM_WORDS
   cmp   regX,reg12, lsl #24
Bopc_ED: // Opcode ED - SBC $0000
   JITABSLDRB
   tst   flags, #D_FLAG6502
   bic   flags, #V_FLAG6502
   LSL   regA, regA, #24
   .word SBC3-28
Bopc_EE: // Opcode EE - INC $0000
   JITABSLDRBWB
   .word incstore-16
   sxtb  reg12, reg12
   teq   reg12, #0
   .word JITTEDTABLE16-28
Bopc_EF: // Opcode EF - BBS6
   JITBBS 6

Bopc_F1: // Opcode F1 - SBC ($00),Y
   ldrh  reg1,[ram6502,#0x00]
   tst   flags, #D_FLAG6502
   ldrb  reg12,[reg1,regY,LSR #24]
   .word SBC1-20

Bopc_F2: // Opcode F2 - SBC ($00)
   ldrh  reg1,[ram6502,#0x00]
   tst   flags, #D_FLAG6502
   ldrb  reg12,[reg1]
   .word SBC1-20
Bopc_F3: // Opcode F3 - NOP6
   ARMNOPR6QUICK
   .word putcharopcode-12
Bopc_F5: // Opcode F5 - SBC $00,X
   ADDR1REGXZP
   tst   flags, #D_FLAG6502
   ldrb  reg12,[ram6502,reg1,LSR #24]
   .word SBC1-20
Bopc_F6: // Opcode F6 - INC $00,X
   ADDR1REGXZP
   ldrb  reg0,[ram6502,reg1,LSR #24]
   add   reg0, reg0, #1
   .word storeXflags-20
Bopc_F7: // Opcode F7 - SMB7 $00
   JITSMB 7
Bopc_F8: // Opcode F8 - SED
   ARMNOPR6QUICK
   orr   flags, flags, #D_FLAG6502
Bopc_F9: // Opcode F9 - SBC $0000,Y
   JITABS
   ldrb  reg12,[reg1, regY, lsr #24]
   tst   flags, #D_FLAG6502
   bic   flags, #V_FLAG6502
   .word SBC2-28

Bopc_FA: // Opcode FA - PLX
   mov   reg1,#1
   .word plx-12
Bopc_FB: // Opcode FB - NOP6
#if defined(TIMING_INSTRUCTION)
   READ_CYCLE_COUNT
   str   reg0,[ram6502,#0xE4]
#else
   nop
   nop
#endif
Bopc_FD: // Opcode FD - SBC $0000,X
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]
   tst   flags, #D_FLAG6502
   bic   flags, #V_FLAG6502
   .word  SBC2-28
Bopc_FE: // Opcode FE - INC $0000,X
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]!
   .word incstore-20
   sxtb  reg12, reg12
   teq   reg12, #0
Bopc_FF: // Opcode FF - BBS7
   JITBBS 7

Bopc_carry:
   .word invertcarry-8
   ARMNOPR6QUICK
Bopc_18:
   cmn   ram6502,#0
   ARMNOPR6QUICK
Bopc_38:
   cmp   ram6502,#0
   ARMNOPR6QUICK


Bopc_50: // Opcode 50 - BVC
Bopc_70: // Opcode 70 - BVS
   JITBRANCHVCS

// ***** End of JITTED 6502 opcodes

opc_18: // Opcode 18 - CLC
   ldrb  r7,[r12,r4]
   BL optimiseflags
   ldrned  r8,Bopc_18
   Bne jitend1bytestore2
   b opc_invertcarry

opc_38: // Opcode 38 - SEC
   ldrb  r7,[r12,r4]
   BL optimiseflags
   ldrned  r8,Bopc_38
   Bne jitend1bytestore2
opc_invertcarry:
   ldrd  r10,Bopc_carry
   sub   r8,r10,r6
   mov   r8,r8,LSR #BYTEWORDSHIFT
   orr   r8,r8,r9
   mov   r9,r11
   B     jitend1bytestore2

opc_03: // Opcode 03 - NOP6
opc_0B: // Opcode 0B - NOP6
opc_13: // Opcode 13 - NOP6
opc_1B: // Opcode 1B - NOP6
opc_23: // Opcode 23 - NOP6
opc_2B: // Opcode 2B - NOP6
opc_33: // Opcode 33 - NOP6
opc_3B: // Opcode 3B - NOP6
opc_43: // Opcode 43 - NOP6
opc_4B: // Opcode 4B - NOP6
opc_53: // Opcode 53 - NOP6
opc_5B: // Opcode 5B - NOP6
opc_63: // Opcode 63 - NOP6
opc_6B: // Opcode 6B - NOP6
opc_73: // Opcode 73 - NOP6
opc_7B: // Opcode 7B - NOP6
opc_83: // Opcode 83 - NOP6
opc_8B: // Opcode 8B - NOP6
opc_93: // Opcode 93 - NOP6
opc_9B: // Opcode 9B - NOP6
opc_A3: // Opcode A3 - NOP6
opc_AB: // Opcode AB - NOP6
opc_B3: // Opcode B3 - NOP6
opc_BB: // Opcode BB - NOP6
opc_C3: // Opcode C3 - NOP6
opc_CB: // Opcode CB - WAI
opc_D3: // Opcode D3 - NOP6
opc_DB: // Opcode DB - STP
opc_EA: // Opcode EA - NOP6
   mov   r8,r9
   B     jitend1bytestore2

opc_02: // Opcode 02 - NOP6 2
opc_22: // Opcode 22 - NOP6 #$00
opc_42: // Opcode 42 - NOP6 #$00
opc_44: // Opcode 44 - NOP6 $00
opc_54: // Opcode 54 - NOP6 $00,X
opc_62: // Opcode 62 - NOP6 #$00
opc_82: // Opcode 82 - NOP6 #$00
opc_C2: // Opcode C2 - NOP6 #$00
opc_D4: // Opcode D4 - NOP6 $00,X
opc_E2: // Opcode E2 - NOP6 #$00
opc_F4: // NOP6_2
   mov   r5,#TWOBYTES
   B     nop_end

opc_5C: // Opcode 5C - NOP6 $0000
opc_DC: // Opcode DC - NOP6 $0000
opc_FC: // Opcode FC - NOP6 $0000
   mov   r5,#THREEBYTES
nop_end:
   str   r9,[r6]
   BL    fillinjittabledejit16bit1
   B     dojitloop

opc_20: // Opcode 20 - JSR $0000
   LDMIA r9,{r7,r8,r9,r10,r11}
   orr   r4, r4, r5, LSL #8
   add   temp0,temp2,#2
   and   temp1,temp0,#0xFF
   orr   r8,r8,temp0,LSR #8
   orr   r9,r9,temp1

   add   r11,r11,r4,LSL #JITLETSHIFT

   sub   r11,r11,r6
   mov   r11,r11,LSR #BYTEWORDSHIFT
   BIC   r11,r11,#0xFF000000
   ORR   r11,r11,#BINSTRUCTION
   B     jitend3byteexitstore6

opc_24: // Opcode 24 - BIT $00
   ldrd  r8,[r9]
   mov   r5,#TWOBYTES
   BL    fillinjittabledejit16bit1
   orr   r8,r8,r4
   MAKEBRANCHLINK r9
   strd  r8,r9,[r6]
   B     dojitloop

opc_4C: // Opcode 4C - JMP $0000
   mov   r4,r4,LSL#JITLETSHIFT-2
   orr   temp0,r4,r5,LSL #(8+JITLETSHIFT-2)
   add   temp0,temp0,r9
   sub   temp0,temp0,r6,LSR #BYTEWORDSHIFT
   BIC   temp0,temp0,#0xFF000000
   ORR   temp0,temp0,#BINSTRUCTION
   str   temp0,[r6]
   B     jitend3byteexit

opc_50: // Opcode 50 - BVC
   //BRANCHVCS Bopc_50 //testvflagC
opc_70: // Opcode 70 - BVS
  // BRANCHVCS Bopc_70 //testvflagS

// Make BL testvflagC
//      msr CPSR_flg, temp1
//      B xx // branch taken
//      msr CPSR_flg,temp1

   //ldr r10,=\branch-8
   LDR   r11,Bopc_70
   mov   R10,R9

   sxtb  r4,r4
   // fill in BL
   MAKEBRANCHLINK r10
   strd  r10,r11,[r6]
   mov   r10,r4,LSL #1

   BIC   r10,r10,#0xFF000000
   orr   r10,r10,#BINSTRUCTION
   strd  r10,r11,[r6,#8]
   B     jitend2byteexit

opc_6C: // Opcode 6C - JMP ($0000)
   BL    fixuptype2accessquick
   b     jitend3byteexitstore6

opc_7C: // Opcode 7C - JMP ($0000,X)
   BL    fixuptype2accessquickthird
   B     jitend3byteexitstore6

opc_89: // Opcode 89 - BIT #$00
   LDMIA r9,{r8,r9,r10,r11}
   orr   r9,r9,r4
   B     jitend2byteBLendstore

opc_B9: // Opcode B9 - LDA $0000,Y
opc_BD: // Opcode BD - LDA $0000,X
opc_BC: // Opcode BC - LDY $0000,X
opc_BE: // Opcode BE - LDX $0000,Y
#if OPTIMISMISE >0
   ldrb  r7,[r2,#3]
   ldrb  r7,[r12,r7]
   BL optimiseflags
   addne r9,r9,#6*4
#endif
opc_19: // Opcode 19 - ORA $0000,Y
opc_1D: // Opcode 1D - ORA $0000,X
opc_39: // Opcode 39 - AND $0000,Y
opc_3D: // Opcode 3D - AND $0000,X
opc_59: // Opcode 59 - EOR $0000,Y
opc_5D: // Opcode 5D - EOR $0000,X
   BL    fixuptype2accessquickthird
   b     jitend3bytestore

opc_06: // Opcode 06 - ASL $00
opc_26: // Opcode 26 - ROL $00
   ldrb  r7,[r12,r5]
   BL optimiseflags
   Bne   1f
   LDMIA r9,{r8,r9,r10,r11}
   orr   r8,r8,r4
   orr   r10,r10,r4
   B     jitend2bytestore4
1:
   add   r9,r9,#4*4
   LDMIA r9,{r8,r9,r10,r11}
   and   r1,r4,#0xF0
   and   temp,r4,#0x0F
   orr   r8,r8,r1,LSL #4
   orr   r8,r8,temp
   orr   r11,r11,r4
   B     jitend2bytestore4

opc_C6: // Opcode C6 - DEC $00
opc_E6: // Opcode E6 - INC $00
   ldrb  r7,[r12,r5]
   BL optimiseflags
   Bne   1f
   LDMIA r9,{r8,r9,r10,r11}
   orr   r8,r8,r4
   MAKEBRANCHLINK r9
   orr   r11,r11,r4
   B     jitend2bytestore4

opc_04: // Opcode 04 - TSB $00
opc_14: // Opcode 14 - TRB $00
   LDMIA r9,{r8,r9,r10,r11}
   orr   r8,r8,r4
   orr   r10,r10,r4
   B     jitend2byteBLendstore
1:
   add   r9,r9,#4*4
opc_07: // Opcode 07 - RMB0 $00
opc_17: // Opcode 17 - RMB1 $00
opc_27: // Opcode 27 - RMB2 $00
opc_37: // Opcode 37 - RMB3 $00
opc_46: // Opcode 46 - LSR $00
opc_47: // Opcode 47 - RMB4 $00
opc_57: // Opcode 57 - RMB5 $00
opc_66: // Opcode 66 - ROR $00
opc_67: // Opcode 67 - RMB6 $00
opc_77: // Opcode 77 - RMB7 $00
opc_87: // Opcode 87 - SMB0 $00
opc_97: // Opcode 97 - SMB1 $00
opc_A7: // Opcode A7 - SMB2 $00
opc_B7: // Opcode B7 - SMB3 $00
opc_C7: // Opcode C7 - SMB4 $00
opc_D7: // Opcode D7 - SMB5 $00
opc_E7: // Opcode E7 - SMB6 $00
opc_F7: // Opcode F7 - SMB7 $00
   LDMIA r9,{r8,r9,r10,r11}
   orr   r8,r8,r4
   orr   r11,r11,r4
   B     jitend2bytestore4

opc_EE: // Opcode EE - INC $0000
   LDMIA r9,{r7,r8,r9,r10,r11,r12}
   orr   r0,r4,r5,LSL #8
   orr   r7,r7,r5
   orr   r8,r8,r4
   MAKEBRANCHLINK r9
   add   r12,r12,r0, LSL #JITTEDTABLESHIFT
   B     jitend3bytestoreoperandBL

opc_FE: // Opcode FE - INC $0000,X
   LDMIA r9,{r7,r8,r9,r10,r11,r12}
   orr   r7,r7,r5
   orr   r8,r8,r4
   MAKEBRANCHLINK r10
   B     jitend3bytestore

// **********************************************
// End of opcode implementations
// **********************************************

// **********************************************
// Entry point to emulator
// **********************************************
//
// R0 Points to memory

exec_65tubejit:
   push  {r4-r11,lr}
   mov   ram6502,r0  // this is assumed to be zero

// setup ARM cycle counter

#if (__ARM_ARCH >= 7 )
   mrc   p15 ,0, r12, c9, c12, 0
   BIC   r12, r12, #8
   mcr   p15, 0, r12, c9, c12, 0
#else
   MRC   p15, 0, r12, c15, c12, 0   // Read Performance Monitor Control Register
   BIC   r12 ,r12, #8               // clear div 64 bit
   MCR   p15, 0, r12, c15, c12, 0   // Write Performance Monitor Control Register
   //mrc   p15, 0, r12, c1, c0, 1     // Read Auxiliary Register Errata 714068 ( cache lock up bug)
   //orr   r0, r0, #0x10000000        // Set PHD bit ( reduces performance by 10% !!)
   //mcr   p15, 0, r0, c1, c0, 1      // Write Auxiliary Control Register
#endif

// setup table JITLET ( 64K x bl JITLET)
// setup JITTEDTABLE16 with 64K x mov pc,r14
   ldr   temp2,=dojit-JITLET-8
   mov   r0,#JITLET
#if JITTEDTABLESHIFT==3
   ldr   temp1,=0xe1a06006 // ARMNOPR6QUICK
#endif
   ldr   temp,=MOVPCR14INSTRUCTION
   mov   r4,#JITTEDTABLE16

   mov   r3,#0x10000 // 64K
   mov   temp2,temp2,LSR#2
   add   temp2,temp2,#BLINSTRUCTION

loopdejitsetup:
   subs  r3,r3,#1
   str   temp2,[r0],#NEXTJITLET
   sub   temp2,temp2,#NEXTJITLET>>2
#if JITTEDTABLESHIFT==3
   str   temp1,[r4],#4   // NOP JITTEDTABLE16
#endif
   str   temp,[r4],#4   // BX R14 JITTEDTABLE16
   bne   loopdejitsetup

// Setup IO region in JITTEDTABLE16
// When a store occurs for the IO locations
// we assume the data to store is in regA
// This fixes ?&FEF7 = xx ; and other STA indirect instructions ( STZ still broken)
// it doesn't fix P.?&FEF7

   mov   r3,#32
   sub   r4,r4,#((0x10000-0xFEE0)<<JITTEDTABLESHIFT)
#if JITTEDTABLESHIFT==3
   add   r4,r4,#(4*(JITTEDTABLESHIFT-2))
#endif
   ldr   temp,=indirectiostore-(4*JITTEDTABLESHIFT)-(JITTEDTABLE16+(0xFEE0<<JITTEDTABLESHIFT))
   mov   temp,temp,LSR#BYTEWORDSHIFT
   add   temp,temp,#BINSTRUCTION
loopioregion:
   subs  r3,r3,#1
   str   temp,[r4],#1<<JITTEDTABLESHIFT
   sub   temp,temp,#JITTEDTABLESHIFT-1
   bne   loopioregion

// flush caches

#if (__ARM_ARCH >= 7 )
   DSB
// this would be faster to flush the cache by way instead of VA
   mrc   p15, 0,r12,cr0,cr0,1
   and   r0,r12,#0xF
   mov   r1,#4
   lsl   r1,r1,r0
   strb  r1,cachelineI
   ubfx  r1,r12,#16,#4
   mov   r12,#4
   lsl   r12,r12,r1
   strb  r12,cachelineD

   mov   r5,#JITLET
   mov   r0,#JITTEDTABLE16
   add   r1,r0,#0x10000*(1<<JITTEDTABLESHIFT)
cacheflushloopjittable:
   mcr   p15,0,r0,cr7,cr11,1
   add   r0,r0,r12
#if JITTEDTABLESHIFT==3
   mcr   p15,0,r0,cr7,cr11,1
   add   r0,r0,r12
#endif
   mcr   p15,0,r5,cr7,cr11,1
   add   r5,r5,r12
   mcr   p15,0,r5,cr7,cr11,1
   add   r5,r5,r12
   cmp   r1,r0
   bhi   cacheflushloopjittable

   DSB
   ISB
   mov   temp,#0

   MCR   p15, 0, temp, c7, c5, 0 //; invalidate I cache & BTB

   DSB
   ISB
#else
   mov   temp,#0
   MCR   p15, 0, temp, c7, c10, 0 //; clean data cache
   MCR   p15, 0, temp, c7, c5, 0 //; invalidate I cache & BTB
#endif

// run dormann tests directly
#ifdef DORMANN_TEST
   //mov   r14, #0x3400    // 6502 test code
   mov   r14, #0xC000      // 65C02 test
   mov   temp1,#0xFF00
   mov   temp,#0xF3
   orr   temp,temp,#0x60<<8
   strh  temp,[temp1,#0xEE]   // store 0xF3 0x60 at FFEE ( oswrch)
#else
   mov   r0, #0x10000
   ldrh  r14, [r0, #-4]        // Fetch the vector address
#endif

#if JITDEBUG>1
// Enable jit tracing
   mov   temp,#1
   strb  temp,debugflag
#endif

   mov   temp,#0xe3
   mov   temp1,#0x8000
   orr   temp,temp,#0x60<<8
   strh  temp,[temp1,#-2] // 0x7FFE

   add   regSP,ram6502, #0x1F0

   mov   flags, #0x00   // Initialize flags (flags) to zero on power up
                        // This was added to fix a bug where the language transfer
                        // would fail if exec_65tube was entered with certain
                        // values of flags (specifically, with bit 7 = 1).
                        // Code assumes Bits 7,2,1 is zero

   mov   r14,r14,LSL#JITLETSHIFT
   add   r14,r14,#JITLET
   mov   jittedtable16ptr,#JITTEDTABLE16

   CPSID if
   str   r13,stackptr
   str   ram6502,ram6502store

// install our interrupt handler
   ldr   r4,=_interrupt_vector_h
   adr   r0,arm_jit_irq_handler
   adr   r1,arm_fiq_handler_jit
   ldrd  r2,[r4]
   strd  r0,[r4]
   strd  r2,old_irq_vector
   CPSIE if // enable interrupts
   ldr   reg3, =MOVPCR14INSTRUCTION      // constant used to for store compares
   mov   reg2,#JITLET
   mov   reg4, #2 // constant used by jsr rts
   add   reg2,reg2,#NEXTJITLET
   bx    r14

//*************************************************************
reset_handler:
   pop   {r0-r12,r14}        // unstack everything
   pop   {r0-r1}

reset_handler_jit:
// release interrupt handler
   CPSID if
   ldr   r0,=_interrupt_vector_h
   ldrd  r2,old_irq_vector
   strd  r2,[r0]
   CPSIE if

   ldr   r13,stackptr  // Hack to ensure the stack ptr is on the right place
   pop   {r4-r11,pc}

.ltorg
   .align 3
old_irq_vector:
   .word 0
old_fiq_vector:
   .word 0
stackptr:
   .word 0
cachelineD:
   .byte 0
cachelineI:
   .byte 0
debugflag:
   .byte 0
fiqjitflag:
   .byte 0
fiqirqflag:
   .byte 0
.align
ram6502store:
   .word 0

        .align 3
doorbell_const:
        .word DOORBELL
        .word DOORBELLDATA

CACHELINE_ALIGN = 5
.equ    CPSR_MODE_USER,         0x10
.equ    CPSR_MODE_FIQ,          0x11
.equ    CPSR_MODE_SVR,          0x13
.equ    CPSR_MODE_SYSTEM,       0x1F
.equ    CPSR_FIQ_INHIBIT,       0x40
.equ    CPSR_IRQ_INHIBIT,       0x80
.align CACHELINE_ALIGN

// ARM FIQ handler (in FIQ mode)
arm_fiq_handler_jit:
#ifdef USE_DOORBELL
   ldrd     r10, doorbell_const      // Read the GPU doorbell
   push  {r0-r3,r14}
   ldr   r1, [r10]               // read doorbell
#else
   ldr   r11, =MBOX0_READ        // Read the GPU mailbox
   push  {r0-r3,r14}

#endif
   ldr   r0, [r11]               // Get data
   ldr   r10, =tube_irq
   BL    tube_io_handler
   ldr   r12,[r10]
   pop   {r0-r3,r14}
   tst   r12,#RESET_BIT+NMI_BIT+IRQ_BIT
   subeqs pc, lr, #4

// if we are currently jitting the event will be handled at the end of the jitting
   ldrb  r12, fiqjitflag
   movs  r12, r12
   add   r12, r12, #1
   strneb r12, fiqjitflag
   subnes pc, lr, #4

// if we are in an IRQ the event will be handled at the end of the IRQ
   mrs   r12, spsr            // Get the saved psr
   and   r12, #0x1F
   cmp   r12, #0x12
   streqb r12,fiqirqflag
   subeqs pc,lr,#4

   sub   r14,r14,#4
fiq_during_irq:
   SRSFD r13!,#CPSR_MODE_SVR  // saves lr and SPSR to user mode stack

   CPS   #CPSR_MODE_SVR       // drop to user mode

   push  {r0-r12,r14}         // stack everything onto user stack

   ldr   reg0, =tube_irq             // Get IRQ status
   mrs   reg12,CPSR

rtiended:
   CPSID if                   // could come from rti

   ldr   r0, [r0]            // Get Mailbox data

// process events

   tst   r0, #RESET_BIT
   bne   reset_handler
   tst   r0, #NMI_BIT
   bne   handle_nmi
   tst   flags, #I_FLAG6502   // Test whether interrupts are enabled (bit 2 = 0)
   bne   noirq
   tst   r0, #IRQ_BIT         // Bit 0 set indicate IRQ is active
   beq   noirq                // Take the IRQ interrupt

   msr   CPSR_flg, reg12      // Restore the 6502 flags
   INTR  -2, 0, FAKE

noirq:
   pop   {r0-r12,r14}
   RFEFD r13!                 // return from ISR

handle_nmi:
   ldr   temp1, =tube_irq     // Get IRQ status
   BIC   r0,r0,#NMI_BIT       // clear NMI flag
   str   r0,[temp1]           // Store it back again
   msr   CPSR_flg, r12        // Restore the 6502 flags
   INTR  -6, 0, FAKE


arm_jit_irq_handler:
   sub   lr, lr, #4
   push  {r0, r1, r2, r3, ip, lr}
   bl    IRQHandler_main

   CPSID if                   // irqs should already be disabled , but just to be sure
   ldrb  r0,fiqirqflag
   movs  r0,r0
   ldmeq sp!, {r0, r1, r2, r3, ip, pc}^
   mov   r0,#0
   strb  r0,fiqirqflag

// we are in IRQ mode and have had a FIQ event
   ldm   sp!, {r0, r1, r2, r3, ip, lr}
   B fiq_during_irq

ioload:
   mrs   reg12, CPSR         // Save 6502 flags and current FIQ/IRQ bits
   push  {reg12,lr}
   CPSID if                 // Disable FIQ/IRQ at the same time

   bl    tube_parasite_read // Call up to C code to handle parasite read

   ldr   reg3, =MOVPCR14INSTRUCTION      // constant used to for store compares
   mov   reg2, #JITLET
   add   reg2,reg2,#NEXTJITLET
   pop   {reg12, pc}               // Restore registers and return

indirectiostore:
   sub   reg0,reg0,jittedtable16ptr
   and   reg1,regA,#0xFF    // This is an assumption that the value is in regA
   mov   reg0,reg0,LSR #JITTEDTABLESHIFT
   mov   reg12,#1
   mrs   reg4, CPSR         // Save 6502 flags and current FIQ/IRQ bits

iostore:
   strb  reg12,fiqjitflag
   push  {lr}
   bl    tube_parasite_write_banksel // Call up to C code to handle parasite write
   pop   {r3}
   b     testforirq

// **** dejit functions

// so we get here by storing data to a location that has already been jitted
// Normally we having branched with link to the JITTEDTABLE16. which contains BX LR
// if that location has been jitted then the bx lr will have been replaced by b dejit
//
// so now we need to replace all the jitted code for that instruction with
// B dojit.
// the JITTEDTABLE16 entries can be replaced with bx lr
// the caches need to be flushed
// reg1 is the memory location that has been unjitted

// one byte instruction
dejit16bit1: //reg1 pointer to address to dejit
   ldr reg0,=dojit-JITLET-8
   mov reg12,#JITLET
   sub reg0,reg0,reg1,LSL # JITLETSHIFT
   mov reg0,reg0,LSR#BYTEWORDSHIFT
   add reg0,reg0,#BLINSTRUCTION
   str reg0,[reg12,reg1,LSL #JITLETSHIFT]!
   mov reg0,#0 // only used in Armv6 DSB
   DEJITCLEAN 1
   mov reg12,#JITTEDTABLE16
#if JITTEDTABLESHIFT==3
   add reg12,reg12,#4*(JITTEDTABLESHIFT-2)
#endif
   str reg3,[reg12,reg1,LSL #JITTEDTABLESHIFT]!
   DEJITCLEAN 0
   bx lr

// two byte instruction
dejit16bit22:
   sub   reg1, reg1, #1
dejit16bit21: // reg1 , reg1+4
   ldr   reg0,=dojit-JITLET-8
   mov   reg12,#JITLET
   sub   reg0,reg0,reg1,LSL # JITLETSHIFT
   mov   reg0,reg0,LSR#BYTEWORDSHIFT
   add   reg0,reg0,#BLINSTRUCTION
   str   reg0,[reg12,reg1,LSL #JITLETSHIFT]!
   sub   reg0,reg0,#(NEXTJITLET>>2)
   str   reg0,[reg12,#NEXTJITLET]
   mov   reg0,#0 // only used in Armv6 DSB
   DEJITCLEAN 1
   add   reg12, reg12, #NEXTJITLET
   DEJITCLEAN 1

   mov   reg12,#JITTEDTABLE16
#if JITTEDTABLESHIFT==3
   add   reg12,reg12,#4*(JITTEDTABLESHIFT-2)
#endif
   str   reg3,[reg12,reg1,LSL #JITTEDTABLESHIFT]!
   str   reg3,[reg12,#(1<<JITTEDTABLESHIFT)]
   DEJITCLEAN 1
   add   reg12, reg12, #(1<<JITTEDTABLESHIFT)
   DEJITCLEAN 0
   bx    lr

// three byte instruction
dejit16bit33:
   sub   reg1, reg1, #1
dejit16bit32:
   sub   reg1, reg1, #1
dejit16bit31: // reg1 , reg1+4, reg1 + 8
   ldr   reg0,=dojit-JITLET-8
   mov   reg12,#JITLET
   sub   reg0,reg0,reg1,LSL # JITLETSHIFT

   mov   reg0,reg0,LSR#BYTEWORDSHIFT
   add   reg0,reg0,#BLINSTRUCTION

   str   reg0,[reg12,reg1,LSL #JITLETSHIFT]!
   sub   reg0,reg0,#NEXTJITLET>>2
   str   reg0,[reg12,#NEXTJITLET]
   sub   reg0,reg0,#NEXTJITLET>>2
   str   reg0,[reg12,#NEXTJITLET<<1]
   mov   reg0,#0 // only used in Armv6 DSB
   DEJITCLEAN 1
   add   reg12, reg12, #NEXTJITLET
   DEJITCLEANMID
   add   reg12, reg12, #NEXTJITLET
   DEJITCLEAN 1

   mov   reg12,#JITTEDTABLE16
#if JITTEDTABLESHIFT==3
   add   reg12,reg12,#4*(JITTEDTABLESHIFT-2)
#endif
   str   reg3,[reg12,reg1,LSL #JITTEDTABLESHIFT]!
   str   reg3,[reg12,#(1<<JITTEDTABLESHIFT)]
   str   reg3,[reg12,#(2<<JITTEDTABLESHIFT)]
   DEJITCLEAN 1
   add   reg12, reg12, #NEXTJITLET
   DEJITCLEANMID
   add   reg12, reg12, #NEXTJITLET
   DEJITCLEAN 0
   bx    lr

.ltorg
// *************

// **** jit up to a branch or jsr rts rti
dojit:
   mov   r0,#1
   strb  r0,fiqjitflag
// r14 has address of instruction
   mrs   r4, CPSR                   // Save interrupt state (restored by dojitexit)

   sub   r3,r14,#4                  // return address ( first address of jitted code)
   push  {r3-r11}
#ifdef DEBUG_TIMING
      ldr     r8, =ASS_GPSET0     // timing debug code
      mov     r7, #TEST2_MASK
      str     r7, [r8]
#endif
   sub temp2,r3,#JITLET
   mov r5,#0
   mov temp2,temp2,LSR#JITLETSHIFT
   b jitentry

// NB word -16
opc_01: // Opcode 01 - ORA ($00,X)
opc_21: // Opcode 21 - AND ($00,X)
opc_36: // Opcode 36 - ROL $00,X
opc_41: // Opcode 41 - EOR ($00,X)
   LDMIA r9,{r8,r9,r10,r11}
   MAKEBRANCHLINK r10
   B jitend2bytestore4operand

opc_71: // Opcode 71 - ADC ($00),Y
opc_F1: // Opcode F1 - SBC ($00),Y
opc_72: // Opcode 72 - ADC ($00)
opc_F2: // Opcode F2 - SBC ($00)
   LDMIA r9,{r8,r9,r10,r11}
   cmp   r4, #0xff
   beq   replaceldrhadcsbc
   and r1,r4,#0xF0
   and r4,r4,#0x0F
   orr r8,r8,r1,LSL #4
   B jitend2byteBLendoperand

opc_85: // Opcode 85 - STA $00
opc_64: // Opcode 64 - STZ $00
   ldrb  r7,[r12,r5]
   BL    optimiseflags
   beq   opc_86
   add   r9,r9,#4*4
   LDMIA r9,{r8,r9,r10,r11}
   orr   r8,r8,r4,LSL #JITTEDTABLESHIFT
   orr   r9,r9,r4
   add   r11,r11,r4,LSL #3
   mov   r11,r11,LSR #BYTEWORDSHIFT
   sub   r11,r11,r6,LSR #BYTEWORDSHIFT
   ORR   r11,r11,#0x1B000000 // BLNE
   B     jitend2bytestore4


opc_84: // Opcode 84 - STY $00
opc_86: // Opcode 86 - STX $00
   LDMIA r9,{r8,r9,r10,r11}
   orr r9,r9,r4
   orr r10,r10,r4
   add r11,r11,r4,LSL #JITTEDTABLESHIFT
   b jitend2byteBLendstore

opc_16: // Opcode 16 - ASL $00, X
opc_34: // Opcode 34 - BIT $00,X
opc_61: // Opcode 61 - ADC ($00,X)
opc_65: // Opcode 65 - ADC $00
opc_69: // Opcode 69 - ADC #$00
opc_74: // Opcode 74 - STZ $00,X
opc_75: // Opcode 75 - ADC $00,X
opc_76: // Opcode 76 - ROR $00,X
opc_81: // Opcode 81 - STA ($00,X)
opc_94: // Opcode 94 - STY $00,X
opc_95: // Opcode 95 - STA $00,X
opc_96: // Opcode 96 - STX $00,Y
opc_A1: // Opcode A1 - LDA ($00,X)
opc_C1: // Opcode C1 - CMP ($00,X)
opc_D6: // Opcode D6 - DEC $00,X
opc_E1: // Opcode E1 - SBC ($00,X)
opc_E5: // Opcode E5 - SBC $00
opc_E9: // Opcode E9 - SBC #$00
opc_F5: // Opcode F5 - SBC $00,X
opc_F6: // Opcode F6 - INC $00,X
// NB word -20  ( 8+12)
   LDMIA r9,{r8,r9,r10,r11}
jitend2byteBLendoperand:
   orr r8,r8,r4
jitend2byteBLendstore:
   MAKEBRANCHLINK r11
   B jitend2bytestore4

opc_A0: // Opcode A0 - LDY #$00
opc_A2: // Opcode A2 - LDX #$00
opc_B5: // Opcode B5 - LDA $00,X
   ldrb r7,[r12,r5]
   BL optimiseflags
   addne r9,r9,#4*4

opc_15: // Opcode 15 - ORA $00,X
opc_35: // Opcode 35 - AND $00,X
opc_55: // Opcode 55 - EOR $00,X
opc_56: // Opcode 56 - LSR $00,X
opc_B4: // Opcode B4 - LDY $00,X
opc_B6: // Opcode B6 - LDX $00,Y
opc_C0: // Opcode C0 - CPY #$00
opc_C4: // Opcode C4 - CPY $00
opc_C5: // Opcode C5 - CMP $00
opc_C9: // Opcode C9 - CMP #$00
opc_D5: // Opcode D5 - CMP $00,X
opc_E0: // Opcode E0 - CPX #$00
opc_E4: // Opcode E4 - CPX $00

   LDMIA r9,{r8,r9,r10,r11}
jitend2bytestore4operand:
   orr r8,r8,r4
jitend2bytestore4:
   stmia r6,{r8,r9,r10,r11}

jitend2byte:
   FILLINJITTEDTABLES dejit16bit21
   add temp0,temp0,#(dejit16bit22-dejit16bit21-(1<<JITTEDTABLESHIFT))>>2
   mov r5,#TWOBYTES
   str temp0,[temp1,#1<<JITTEDTABLESHIFT]

dojitloop:
   DEBUGDISS
jitentry:
   ldrb  r10,[temp2,r5]!
   mov   r6,temp2,LSL#JITLETSHIFT
   adr   temp0,opcode_table
   add   r6,r6,#JITLET
   mov   r7,r10,LSL #3
   ldrd  r8,r9,[temp0,r7]//,LSL #BYTEWORDSHIFT]
   ldrb  r4,[temp2,#1]
   ldrb  r5,[temp2,#2]
   adr   r12,flags_table
   bx    r8
// entry regs
// r0
// r1
// r2 ptr to 6502 code instruction
// r3 address to return to ( start of jitting)
// r4 operand byte 1
// r5 operand byte 2
// r6 Address to write to JITLET code to
// r7,r8,r9,r10,r11,r12 six registers for COPY6
// exit with r5 being the length of the 6502 instruction
.balign 8
flags_table:
.byte E2  		// Opcode 00 - BRK is really a two byte instruction, so no fixup needed
.byte S2 + NZF // Opcode 01 - ORA ($00,X)
.byte S2  		// Opcode 02 - NOP 2
.byte S1  		// Opcode 03 - NOP
.byte S2 + ZF 	// Opcode 04 - TSB $00
.byte S2 + NZF // Opcode 05 - ORA $00
.byte S2 + CNZF// Opcode 06 - ASL $00
.byte S2  		// Opcode 07 - RMB0 $00
.byte E1  		// Opcode 08 - PHP
.byte S2 + NZF // Opcode 09 - ORA #$00
.byte S1 + CNZF// Opcode 0A - ASL A
.byte S1  		// Opcode 0B - NOP
.byte S3 + ZF 	// Opcode 0C - TSB $0000
.byte S3 + NZF	// Opcode 0D - ORA $0000
.byte S3 + CNZF// Opcode 0E - ASL $0000
.byte E3  		// Opcode 0F - BBR0
.byte E2  		// Opcode 10 - BPL
.byte S2 + NZF	// Opcode 11 - ORA ($00),Y
.byte S2 + NZF	// Opcode 12 - ORA ($00)
.byte S1  		// Opcode 13 - NOP
.byte S2 + ZF 	// Opcode 14 - TRB $00
.byte S2 + NZF	// Opcode 15 - ORA $00,X
.byte S2 + CNZF// Opcode 16 - ASL $00, X
.byte S2  		// Opcode 17 - RMB1 $00
.byte S1 + CF	// Opcode 18 - CLC
.byte S3 + NZF	// Opcode 19 - ORA $0000,Y
.byte S1 + NZF	// Opcode 1A - INC A
.byte S1  		// Opcode 1B - NOP
.byte S3 + ZF	// Opcode 1C - TRB $0000
.byte S3 + NZF	// Opcode 1D - ORA $0000,X
.byte S3 + CNZF// Opcode 1E - ASL $0000,X
.byte E3  		// Opcode 1F - BBR1
.byte E3  		// Opcode 20 - JSR $0000
.byte S2 + NZF	// Opcode 21 - AND ($00,X)
.byte S2  		// Opcode 22 - NOP #$00
.byte S1  		// Opcode 23 - NOP
.byte S2 + NZF+ VF// Opcode 24 - BIT $00
.byte S2 + NZF	// Opcode 25 - AND     $00
.byte S2 + CNZF// Opcode 26 - ROL $00
.byte S2  		// Opcode 27 - RMB2 $00
.byte S1 + VCNZF// Opcode 28 - PLP
.byte S2 + NZF // Opcode 29 - AND     #$00
.byte S1 + CNZF// Opcode 2A - ROL A
.byte S1  		// Opcode 2B - NOP
.byte S3 + NZF+ VF// Opcode 2C - BIT $0000
.byte S3 + NZF	// Opcode 2D - AND     $0000
.byte S3 + CNZF// Opcode 2E - ROL $0000
.byte E3  		// Opcode 2F - BBR2
.byte E2  		// Opcode 30 - BMI
.byte S2 + NZF	// Opcode 31 - AND ($00),Y
.byte S2 + NZF	// Opcode 32 - AND ($00)
.byte S1  		// Opcode 33 - NOP
.byte S2 + NZF+ VF// Opcode 34 - BIT $00,X
.byte S2 + NZF	// Opcode 35 - AND $00,X
.byte S2 + CNZF// Opcode 36 - ROL $00,X
.byte S2  		// Opcode 37 - RMB3 $00
.byte S1 + CF 	// Opcode 38 - SEC
.byte S3 + NZF	// Opcode 39 - AND $0000,Y
.byte S1 + NZF	// Opcode 3A - DEC A
.byte S1  		// Opcode 3B - NOP
.byte S3 + NZF+ VF// Opcode 3C - BIT $0000,X
.byte S3 + NZF	// Opcode 3D - AND $0000,X
.byte S3 + CNZF// Opcode 3E - ROL $0000,X
.byte E3  		// Opcode 3F - BBR3
.byte E1 + VCNZF// Opcode 40 - RTI
.byte S2 + NZF	// Opcode 41 - EOR ($00,X)
.byte S2  		// Opcode 42 - NOP #$00
.byte S1  		// Opcode 43 - NOP
.byte S2  		// Opcode 44 - NOP $00
.byte S2 + NZF	// Opcode 45 - EOR $00
.byte S2 + CNZF// Opcode 46 - LSR $00
.byte S2  		// Opcode 47 - RMB4 $00
.byte S1  		// Opcode 48 - PHA
.byte S2 + NZF	// Opcode 49 - EOR #$00
.byte S1 + CNZF// Opcode 4A - LSR A
.byte S1  		// Opcode 4B - NOP
.byte E3  		// Opcode 4C - JMP $0000
.byte S3 + NZF	// Opcode 4D - EOR $0000
.byte S3 + CNZF// Opcode 4E - LSR $0000
.byte E3  		// Opcode 4F - BBR4
.byte E2  		// Opcode 50 - BVC
.byte S2 + NZF	// Opcode 51 - EOR ($00),Y
.byte S2 + NZF	// Opcode 52 - EOR ($00)
.byte S1  		// Opcode 53 - NOP
.byte S2  		// Opcode 54 - NOP $00,X
.byte S2 + NZF	// Opcode 55 - EOR $00,X
.byte S2 + CNZF// Opcode 56 - LSR $00,X
.byte S2  		// Opcode 57 - RMB5 $00
.byte S1  		// Opcode 58 - CLI
.byte S3 + NZF	// Opcode 59 - EOR $0000,Y
.byte S1  		// Opcode 5A - PHY
.byte S1  		// Opcode 5B - NOP
.byte S3  		// Opcode 5C - NOP $0000
.byte S3 + NZF	// Opcode 5D - EOR $0000,X
.byte S3 + CNZF// Opcode 5E - LSR $0000,X
.byte E3  		// Opcode 5F - BBR5
.byte E1  		// Opcode 60 - RTS
.byte S2 + VCNZF// Opcode 61 - ADC ($00,X)
.byte S2  		// Opcode 62 - NOP #$00
.byte S1  		// Opcode 63 - NOP
.byte S2  		// Opcode 64 - STZ $00
.byte S2 + VCNZF// Opcode 65 - ADC $00
.byte S2 + CNZF// Opcode 66 - ROR $00
.byte S2  		// Opcode 67 - RMB6 $00
.byte S1 + NZF	// Opcode 68 - PLA
.byte S2 + VCNZF// Opcode 69 - ADC #$00
.byte S1 + CNZF// Opcode 6A - ROR A
.byte S1  		// Opcode 6B - NOP
.byte E3  		// Opcode 6C - JMP ($0000)
.byte S3 + VCNZF// Opcode 6D - ADC $0000
.byte S3 + CNZF// Opcode 6E - ROR $0000
.byte E3  		// Opcode 6F - BBR6
.byte E2  		// Opcode 70 - BVS
.byte S2 + VCNZF// Opcode 71 - ADC ($00),Y
.byte S2 + VCNZF// Opcode 72 - ADC ($00)
.byte S1  		// Opcode 73 - NOP
.byte S2  		// Opcode 74 - STZ $00,X
.byte S2 + VCNZF// Opcode 75 - ADC $00,X
.byte S2 + CNZF// Opcode 76 - ROR $00,X
.byte S2  		// Opcode 77 - RMB7 $00
.byte S1  		// Opcode 78 - SEI
.byte S3 + VCNZF// Opcode 79 - ADC $0000,Y
.byte S1 + NZF	// Opcode 7A - PLY
.byte S1  		// Opcode 7B - NOP
.byte E3  		// Opcode 7C - JMP ($0000,X)
.byte S3 + VCNZF// Opcode 7D - ADC $0000,X
.byte S3 + CNZF// Opcode 7E - ROR $0000,X
.byte E3  		// Opcode 7F - BBR7
.byte E2  		// Opcode 80 - BRA
.byte S2  		// Opcode 81 - STA ($00,X)
.byte S2  		// Opcode 82 - NOP #$00
.byte S1  		// Opcode 83 - NOP
.byte S2  		// Opcode 84 - STY $00
.byte S2  		// Opcode 85 - STA $00
.byte S2  		// Opcode 86 - STX $00
.byte S2  		// Opcode 87 - SMB0 $00
.byte S1 + NZF	// Opcode 88 - DEY
.byte S2 + ZF	// Opcode 89 - BIT #$00
.byte S1 + NZF // Opcode 8A - TXA
.byte S1  		// Opcode 8B - NOP
.byte S3  		// Opcode 8C - STY $0000
.byte S3  		// Opcode 8D - STA $0000
.byte S3  		// Opcode 8E - STX $0000
.byte E3  		// Opcode 8F - BBS0
.byte E2  		// Opcode 90 - BCC
.byte S2  		// Opcode 91 - STA ($00),Y
.byte S2  		// Opcode 92 - STA ($00)
.byte S1  		// Opcode 93 - NOP
.byte S2  		// Opcode 94 - STY $00,X
.byte S2  		// Opcode 95 - STA $00,X
.byte S2  		// Opcode 96 - STX $00,Y
.byte S2  		// Opcode 97 - SMB1 $00
.byte S1 + NZF // Opcode 98 - TYA
.byte S3  		// Opcode 99 - STA $0000,Y
.byte S1  		// Opcode 9A - TXS
.byte S1  		// Opcode 9B - NOP
.byte S3  		// Opcode 9C - STZ $0000
.byte S3  		// Opcode 9D - STA $0000,X
.byte S3  		// Opcode 9E - STZ $0000,X
.byte E3  		// Opcode 9F - BBS1
.byte S2 + NZF // Opcode A0 - LDY #$00
.byte S2 + NZF	// Opcode A1 - LDA ($00,X)
.byte S2 + NZF	// Opcode A2 - LDX #$00
.byte S1  		// Opcode A3 - NOP
.byte S2 + NZF	// Opcode A4 - LDY $00
.byte S2 + NZF	// Opcode A5 - LDA $00
.byte S2 + NZF // Opcode A6 - LDX $00
.byte S2  		// Opcode A7 - SMB2 $00
.byte S1 + NZF	// Opcode A8 - TAY - really may not be worth it
.byte S2 + NZF	// Opcode A9 - LDA #$00
.byte S1 + NZF	// Opcode AA - TAX - really may not be worth it
.byte S1  		// Opcode AB - NOP
.byte S3 + NZF	// Opcode AC - LDY $0000
.byte S3 + NZF	// Opcode AD - LDA $0000
.byte S3 + NZF	// Opcode AE - LDX $0000
.byte E3  		// Opcode AF - BBS2
.byte E2  		// Opcode B0 - BCS
.byte S2 + NZF // Opcode B1 - LDA ($00),Y
.byte S2 + NZF // Opcode B2 - LDA ($00)
.byte S1  		// Opcode B3 - NOP
.byte S2 + NZF // Opcode B4 - LDY $00,X
.byte S2 + NZF // Opcode B5 - LDA $00,X
.byte S2 + NZF	// Opcode B6 - LDX $00,Y
.byte S2  		// Opcode B7 - SMB3 $00
.byte S1 + VF 	// Opcode B8 - CLV
.byte S3 + NZF // Opcode B9 - LDA $0000,Y
.byte S1 + NZF	// Opcode BA - TSX
.byte S1  		// Opcode BB - NOP
.byte S3 + NZF // Opcode BC - LDY $0000,X
.byte S3 + NZF // Opcode BD - LDA $0000,X
.byte S3 + NZF // Opcode BE - LDX $0000,Y
.byte E3  		// Opcode BF - BBS3
.byte S2 + CNZF// Opcode C0 - CPY #$00
.byte S2 + CNZF// Opcode C1 - CMP ($00,X)
.byte S2  		// Opcode C2 - NOP #$00
.byte S1  		// Opcode C3 - NOP
.byte S2 + CNZF// Opcode C4 - CPY $00
.byte S2 + CNZF// Opcode C5 - CMP $00
.byte S2 + NZF	// Opcode C6 - DEC $00
.byte S2  		// Opcode C7 - SMB4 $00
.byte S1 + NZF	// Opcode C8 - INY
.byte S2 + CNZF// Opcode C9 - CMP #$00
.byte S1 + NZF // Opcode CA - DEX
.byte S1  		// Opcode CB - WAI
.byte S3 + CNZF// Opcode CC - CPY $0000
.byte S3 + CNZF// Opcode CD - CMP $0000
.byte S3 + NZF	// Opcode CE - DEC $0000
.byte E3  		// Opcode CF - BBS4
.byte E2  		// Opcode D0 - BNE
.byte S2 + CNZF// Opcode D1 - CMP ($00),Y
.byte S2 + CNZF// Opcode D2 - CMP ($00)
.byte S1  		// Opcode D3 - NOP
.byte S2  		// Opcode D4 - NOP $00,X
.byte S2 + CNZF// Opcode D5 - CMP $00,X
.byte S2 + NZF	// Opcode D6 - DEC $00,X
.byte S2  		// Opcode D7 - SMB5 $00
.byte S1  		// Opcode D8 - CLD
.byte S3 + CNZF// Opcode D9 - CMP $0000,Y
.byte S1     	// Opcode DA - PHX
.byte S1  		// Opcode DB - STP
.byte S3  		// Opcode DC - NOP $0000
.byte S3 + CNZF// Opcode DD - CMP $0000,X
.byte S3 + NZF	// Opcode DE - DEX $0000,X
.byte E3  		// Opcode DF - BBS5
.byte S1 + CNZF// Opcode E0 - CPX #$00
.byte S2 + VCNZF// Opcode E1 - SBC ($00,X)
.byte S2  		// Opcode E2 - NOP #$00
.byte S1  		// Opcode E3 -
.byte S2 + CNZF// Opcode E4 - CPX $00
.byte S2 + VCNZF// Opcode E5 - SBC $00
.byte S2 + NZF	// Opcode E6 - INC $00
.byte S2  		// Opcode E7 - SMB6 $00
.byte S1 + NZF	// Opcode E8 - INX
.byte S2 + VCNZF// Opcode E9 - SBC #$00
.byte S1  		// Opcode EA - NOP
.byte S1  		// Opcode EB - NOP ( Borrowed for instruction timing)
.byte S3 + CNZF// Opcode EC - CPX $0000
.byte S3 + VCNZF// Opcode ED - SBC $0000
.byte S3 + NZF // Opcode EE - INC $0000
.byte E3  		// Opcode EF - BBS6
.byte E2  		// Opcode F0 - BEQ
.byte S2 + VCNZF// Opcode F1 - SBC ($00),Y
.byte S2 + VCNZF// Opcode F2 - SBC ($00)
.byte S1  		// Opcode F3 - NOP
.byte S2  		// Opcode F4 -
.byte S2 + VCNZF// Opcode F5 - SBC $00,X
.byte S2 + NZF // Opcode F6 - INC $00,X
.byte S2  		// Opcode F7 - SMB7 $00
.byte S1  		// Opcode F8 - SED
.byte S3 + VCNZF// Opcode F9 - SBC $0000,Y
.byte S1 + NZF // Opcode FA - PLX
.byte S1  		// Opcode FB - NOP
.byte S3  		// Opcode FC - NOP $0000
.byte S3 + VCNZF// Opcode FD - SBC $0000,X
.byte S3 + NZF // Opcode FE - INC $0000,X
.byte E3  		// Opcode FF - BBS7

opcode_table:
.word  opc_00, Bopc_00	// Opcode 00 - BRK is really a two byte instruction, so no fixup needed
.word  opc_01, Bopc_01 	// Opcode 01 - ORA ($00,X)
.word  opc_02, BINSTRUCTION+ (NEXTJITLET>>2)		// Opcode 02 - NOP 2
.word  opc_03, ARMNOP   // Opcode 03 - NOP
.word  opc_04, Bopc_04  // Opcode 04 - TSB $00
.word  opc_05, Bopc_05 	// Opcode 05 - ORA $00
.word  opc_06, Bopc_06  // Opcode 06 - ASL $00
.word  opc_07, Bopc_07 	// Opcode 07 - RMB0 $00
.word  opc_08, Bopc_08  // Opcode 08 - PHP
.word  opc_09, Bopc_09 	// Opcode 09 - ORA #$00
.word  opc_0A, Bopc_0A	// Opcode 0A - ASL A
.word  opc_0B, ARMNOP   // Opcode 0B - NOP
.word  opc_0C, Bopc_0C  // Opcode 0C - TSB $0000
.word  opc_0D, Bopc_0D  // Opcode 0D - ORA $0000
.word  opc_0E, Bopc_0E  // Opcode 0E - ASL $0000
.word  opc_0F, Bopc_0F  // Opcode 0F - BBR0
.word  opc_10, BPLINSTRUCTION // Opcode 10 - BPL
.word  opc_11, Bopc_11  // Opcode 11 - ORA ($00),Y
.word  opc_12, Bopc_12  // Opcode 12 - ORA ($00)
.word  opc_13, ARMNOP   // Opcode 13 - NOP
.word  opc_14, Bopc_14  // Opcode 14 - TRB $00
.word  opc_15, Bopc_15  // Opcode 15 - ORA $00,X
.word  opc_16, Bopc_16  // Opcode 16 - ASL $00, X
.word  opc_17, Bopc_17  // Opcode 17 - RMB1 $00
.word  opc_18, ARMBLCS  // Opcode 18 - CLC
.word  opc_19, Bopc_19  // Opcode 19 - ORA $0000,Y
.word  opc_1A, Bopc_1A  // Opcode 1A - INC A
.word  opc_1B, ARMNOP   // Opcode 1B - NOP
.word  opc_1C, Bopc_1C  // Opcode 1C - TRB $0000
.word  opc_1D, Bopc_1D  // Opcode 1D - ORA $0000,X
.word  opc_1E, Bopc_1E  // Opcode 1E - ASL $0000,X
.word  opc_1F, Bopc_1F  // Opcode 1F - BBR1
.word  opc_20, Bopc_20  // Opcode 20 - JSR $0000
.word  opc_21, Bopc_21  // Opcode 21 - AND ($00,X)
.word  opc_22, BINSTRUCTION+ (NEXTJITLET>>2)  // Opcode 22 - NOP #$00
.word  opc_23, ARMNOP   // Opcode 23 - NOP
.word  opc_24, Bopc_24  // Opcode 24 - BIT $00
.word  opc_25, Bopc_25  // Opcode 25 - AND     $00
.word  opc_26, Bopc_26  // Opcode 26 - ROL $00
.word  opc_27, Bopc_27  // Opcode 27 - RMB2 $00
.word  opc_28, Bopc_28  // Opcode 28 - PLP
.word  opc_29, Bopc_29  // Opcode 29 - AND     #$00
.word  opc_2A, Bopc_2A  // Opcode 2A - ROL A
.word  opc_2B, ARMNOP   // Opcode 2B - NOP
.word  opc_2C; MOVR1HIGH  // Opcode 2C - BIT $0000
.word  opc_2D, Bopc_2D  // Opcode 2D - AND $0000
.word  opc_2E, Bopc_2E  // Opcode 2E - ROL $0000
.word  opc_2F, Bopc_2F  // Opcode 2F - BBR2
.word  opc_30, BMIINSTRUCTION  // Opcode 30 - BMI
.word  opc_31, Bopc_31  // Opcode 31 - AND ($00),Y
.word  opc_32, Bopc_32  // Opcode 32 - AND ($00)
.word  opc_33, ARMNOP   // Opcode 33 - NOP
.word  opc_34, Bopc_34  // Opcode 34 - BIT $00,X
.word  opc_35, Bopc_35  // Opcode 35 - AND $00,X
.word  opc_36, Bopc_36  // Opcode 36 - ROL $00,X
.word  opc_37, Bopc_37  // Opcode 37 - RMB3 $00
.word  opc_38, ARMBLCC  // Opcode 38 - SEC
.word  opc_39, Bopc_39  // Opcode 39 - AND $0000,Y
.word  opc_3A, Bopc_3A  // Opcode 3A - DEC A
.word  opc_3B, ARMNOP   // Opcode 3B - NOP
.word  opc_3C, Bopc_3C  // Opcode 3C - BIT $0000,X
.word  opc_3D, Bopc_3D  // Opcode 3D - AND $0000,X
.word  opc_3E, Bopc_3E  // Opcode 3E - ROL $0000,X
.word  opc_3F, Bopc_3F  // Opcode 3F - BBR3
.word  opc_40, Bopc_40  // Opcode 40 - RTI
.word  opc_41, Bopc_41  // Opcode 41 - EOR ($00,X)
.word  opc_42, BINSTRUCTION+ (NEXTJITLET>>2)  // Opcode 42 - NOP #$00
.word  opc_43, ARMNOP   // Opcode 43 - NOP
.word  opc_44, BINSTRUCTION+ (NEXTJITLET>>2)  // Opcode 44 - NOP $00
.word  opc_45, Bopc_45  // Opcode 45 - EOR $00
.word  opc_46, Bopc_46  // Opcode 46 - LSR $00
.word  opc_47, Bopc_47  // Opcode 47 - RMB4 $00
.word  opc_48, Bopc_48  // Opcode 48 - PHA
.word  opc_49, Bopc_49  // Opcode 49 - EOR #$00
.word  opc_4A, Bopc_4A  // Opcode 4A - LSR A
.word  opc_4B, ARMNOP   // Opcode 4B - NOP
.word  opc_4C, (JITLET-8)>>2 // Opcode 4C - JMP $0000
.word  opc_4D, Bopc_4D  // Opcode 4D - EOR $0000
.word  opc_4E, Bopc_4E  // Opcode 4E - LSR $0000
.word  opc_4F, Bopc_4F  // Opcode 4F - BBR4
.word  opc_50, testvflagC-8  // Opcode 50 - BVC
.word  opc_51, Bopc_51  // Opcode 51 - EOR ($00),Y
.word  opc_52, Bopc_52  // Opcode 52 - EOR ($00)
.word  opc_53, ARMNOP   // Opcode 53 - NOP
.word  opc_54, BINSTRUCTION+ (NEXTJITLET>>2)  // Opcode 54 - NOP $00,X
.word  opc_55, Bopc_55  // Opcode 55 - EOR $00,X
.word  opc_56, Bopc_56  // Opcode 56 - LSR $00,X
.word  opc_57, Bopc_57  // Opcode 57 - RMB5 $00
.word  opc_58, Bopc_58  // Opcode 58 - CLI
.word  opc_59, Bopc_59  // Opcode 59 - EOR $0000,Y
.word  opc_5A, Bopc_5A  // Opcode 5A - PHY
.word  opc_5B, ARMNOP   // Opcode 5B - NOP
.word  opc_5C, BINSTRUCTION+ (NEXTJITLET>>1)   // Opcode 5C - NOP $0000
.word  opc_5D, Bopc_5D  // Opcode 5D - EOR $0000,X
.word  opc_5E, Bopc_5E  // Opcode 5E - LSR $0000,X
.word  opc_5F, Bopc_5F  // Opcode 5F - BBR5
.word  opc_60, Bopc_60  // Opcode 60 - RTS
.word  opc_61, Bopc_61  // Opcode 61 - ADC ($00,X)
.word  opc_62, BINSTRUCTION+ (NEXTJITLET>>2)  // Opcode 62 - NOP #$00
.word  opc_63, ARMNOP   // Opcode 63 - NOP
.word  opc_64, Bopc_64  // Opcode 64 - STZ $00
.word  opc_65, Bopc_65  // Opcode 65 - ADC $00
.word  opc_66, Bopc_66  // Opcode 66 - ROR $00
.word  opc_67, Bopc_67  // Opcode 67 - RMB6 $00
.word  opc_68, Bopc_68  // Opcode 68 - PLA
.word  opc_69, Bopc_69  // Opcode 69 - ADC #$00
.word  opc_6A, Bopc_6A  // Opcode 6A - ROR A
.word  opc_6B, ARMNOP   // Opcode 6B - NOP
.word  opc_6C, Bopc_6C  // Opcode 6C - JMP ($0000)
.word  opc_6D, Bopc_6D  // Opcode 6D - ADC $0000
.word  opc_6E, Bopc_6E  // Opcode 6E - ROR $0000
.word  opc_6F, Bopc_6F  // Opcode 6F - BBR6
.word  opc_70, testvflagS-8  // Opcode 70 - BVS
.word  opc_71, Bopc_71  // Opcode 71 - ADC ($00),Y
.word  opc_72, Bopc_72  // Opcode 72 - ADC ($00)
.word  opc_73, ARMNOP   // Opcode 73 - NOP
.word  opc_74, Bopc_74  // Opcode 74 - STZ $00,X
.word  opc_75, Bopc_75  // Opcode 75 - ADC $00,X
.word  opc_76, Bopc_76  // Opcode 76 - ROR $00,X
.word  opc_77, Bopc_77  // Opcode 77 - RMB7 $00
.word  opc_78, Bopc_78  // Opcode 78 - SEI
.word  opc_79, Bopc_79  // Opcode 79 - ADC $0000,Y
.word  opc_7A, Bopc_7A  // Opcode 7A - PLY
.word  opc_7B, ARMNOP   // Opcode 7B - NOP
.word  opc_7C, Bopc_7C  // Opcode 7C - JMP ($0000,X)
.word  opc_7D, Bopc_7D  // Opcode 7D - ADC $0000,X
.word  opc_7E, Bopc_7E  // Opcode 7E - ROR $0000,X
.word  opc_7F, Bopc_7F  // Opcode 7F - BBR7
.word  opc_80, BINSTRUCTION // Opcode 80 - BRA
.word  opc_81, Bopc_81  // Opcode 81 - STA ($00,X)
.word  opc_82, BINSTRUCTION+ (NEXTJITLET>>2)// Opcode 82 - NOP #$00
.word  opc_83, ARMNOP   // Opcode 83 - NOP
.word  opc_84, Bopc_84  // Opcode 84 - STY $00
.word  opc_85, Bopc_85  // Opcode 85 - STA $00
.word  opc_86, Bopc_86  // Opcode 86 - STX $00
.word  opc_87, Bopc_87  // Opcode 87 - SMB0 $00
.word  opc_88, Bopc_88  // Opcode 88 - DEY - may not be worth it
.word  opc_89, Bopc_89  // Opcode 89 - BIT #$00
.word  opc_8A, Bopc_8A  // Opcode 8A - TXA - may not be worth it
.word  opc_8B, ARMNOP   // Opcode 8B - NOP
.word  opc_8C, Bopc_8C  // Opcode 8C - STY $0000
.word  opc_8D, Bopc_8D  // Opcode 8D - STA $0000
.word  opc_8E, Bopc_8E  // Opcode 8E - STX $0000
.word  opc_8F, Bopc_8F  // Opcode 8F - BBS0
.word  opc_90, BCCINSTRUCTION // Opcode 90 - BCC
.word  opc_91, Bopc_91  // Opcode 91 - STA ($00),Y
.word  opc_92, Bopc_92  // Opcode 92 - STA ($00)
.word  opc_93, ARMNOP   // Opcode 93 - NOP
.word  opc_94, Bopc_94  // Opcode 94 - STY $00,X
.word  opc_95, Bopc_95  // Opcode 95 - STA $00,X
.word  opc_96, Bopc_96  // Opcode 96 - STX $00,Y
.word  opc_97, Bopc_97  // Opcode 97 - SMB1 $00
.word  opc_98, Bopc_98  // Opcode 98 - TYA
.word  opc_99, Bopc_99  // Opcode 99 - STA $0000,Y
.word  opc_9A, Bopc_9A  // Opcode 9A - TXS
.word  opc_9B, ARMNOP   // Opcode 9B - NOP
.word  opc_9C, Bopc_9C  // Opcode 9C - STZ $0000
.word  opc_9D, Bopc_9D  // Opcode 9D - STA $0000,X
.word  opc_9E, Bopc_9E  // Opcode 9E - STZ $0000,X
.word  opc_9F, Bopc_9F  // Opcode 9F - BBS1
.word  opc_A0, Bopc_A0  // Opcode A0 - LDY #$00 -- could be replace 2 but unlikely to be worth it
.word  opc_A1, Bopc_A1  // Opcode A1 - LDA ($00,X)
.word  opc_A2, Bopc_A2  // Opcode A2 - LDX #$00 -- could be replace 2 but unlikely to be worth it
.word  opc_A3, ARMNOP   // Opcode A3 - NOP
.word  opc_A4, Bopc_A4  // Opcode A4 - LDY $00 - may not be worth it ( bigger rewrite would be)
.word  opc_A5, Bopc_A5  // Opcode A5 - LDA $00
.word  opc_A6, Bopc_A6  // Opcode A6 - LDX $00 - may not be worth it ( bigger rewrite would be)
.word  opc_A7, Bopc_A7  // Opcode A7 - SMB2 $00
.word  opc_A8, Bopc_A8  // Opcode A8 - TAY - really may not be worth it
.word  opc_A9, 0xea000001   // Opcode A9 - LDA #$00
.word  opc_AA, Bopc_AA  // Opcode AA - TAX - really may not be worth it
.word  opc_AB, ARMNOP   // Opcode AB - NOP
.word  opc_AC, Bopc_AC  // Opcode AC - LDY $0000 - may not be worth it ( bigger rewrite would be)
.word  opc_AD, Bopc_AD  // Opcode AD - LDA $0000 - may not be worth it ( bigger rewrite would be)
.word  opc_AE, Bopc_AE  // Opcode AE - LDX $0000 - may not be worth it ( bigger rewrite would be)
.word  opc_AF, Bopc_AF  // Opcode AF - BBS2
.word  opc_B0, BCSINSTRUCTION // Opcode B0 - BCS
.word  opc_B1, Bopc_B1  // Opcode B1 - LDA ($00),Y
.word  opc_B2, Bopc_B2  // Opcode B2 - LDA ($00)
.word  opc_B3, ARMNOP   // Opcode B3 - NOP
.word  opc_B4, Bopc_B4  // Opcode B4 - LDY $00,X
.word  opc_B5, Bopc_B5  // Opcode B5 - LDA $00,X
.word  opc_B6, Bopc_B6  // Opcode B6 - LDX $00,Y
.word  opc_B7, Bopc_B7  // Opcode B7 - SMB3 $00
.word  opc_B8, Bopc_B8  // Opcode B8 - CLV
.word  opc_B9, Bopc_B9  // Opcode B9 - LDA $0000,Y
.word  opc_BA, Bopc_BA  // Opcode BA - TSX - may not be worth it
.word  opc_BB, ARMNOP   // Opcode BB - NOP
.word  opc_BC, Bopc_BC  // Opcode BC - LDY $0000,X
.word  opc_BD, Bopc_BD  // Opcode BD - LDA $0000,X
.word  opc_BE, Bopc_BE  // Opcode BE - LDX $0000,Y
.word  opc_BF, Bopc_BF  // Opcode BF - BBS3
.word  opc_C0, Bopc_C0  // Opcode C0 - CPY #$00
.word  opc_C1, Bopc_C1  // Opcode C1 - CMP ($00,X)
.word  opc_C2, BINSTRUCTION+ (NEXTJITLET>>2)  // Opcode C2 - NOP #$00
.word  opc_C3, ARMNOP   // Opcode C3 - NOP
.word  opc_C4, Bopc_C4  // Opcode C4 - CPY $00
.word  opc_C5, Bopc_C5  // Opcode C5 - CMP $00
.word  opc_C6, Bopc_C6  // Opcode C6 - DEC $00
.word  opc_C7, Bopc_C7  // Opcode C7 - SMB4 $00
.word  opc_C8, Bopc_C8  // Opcode C8 - INY
.word  opc_C9, Bopc_C9  // Opcode C9 - CMP #$00
.word  opc_CA, Bopc_CA  // Opcode CA - DEX
.word  opc_CB, ARMNOP   // Opcode CB - WAI
.word  opc_CC, Bopc_CC  // Opcode CC - CPY $0000
.word  opc_CD, Bopc_CD  // Opcode CD - CMP $0000
.word  opc_CE, Bopc_CE  // Opcode CE - DEC $0000
.word  opc_CF, Bopc_CF  // Opcode CF - BBS4
.word  opc_D0, BNEINSTRUCTION  // Opcode D0 - BNE
.word  opc_D1, Bopc_D1  // Opcode D1 - CMP ($00),Y
.word  opc_D2, Bopc_D2  // Opcode D2 - CMP ($00)
.word  opc_D3, ARMNOP   // Opcode D3 - NOP
.word  opc_D4, BINSTRUCTION+ (NEXTJITLET>>2)  // Opcode D4 - NOP $00,X
.word  opc_D5, Bopc_D5  // Opcode D5 - CMP $00,X
.word  opc_D6, Bopc_D6  // Opcode D6 - DEC $00,X
.word  opc_D7, Bopc_D7  // Opcode D7 - SMB5 $00
.word  opc_D8, Bopc_D8  // Opcode D8 - CLD
.word  opc_D9, Bopc_D9  // Opcode D9 - CMP $0000,Y
.word  opc_DA, Bopc_DA  // Opcode DA - PHX
.word  opc_DB, ARMNOP   // Opcode DB - STP
.word  opc_DC, BINSTRUCTION+ (NEXTJITLET>>1)   // Opcode DC - NOP $0000
.word  opc_DD, Bopc_DD  // Opcode DD - CMP $0000,X
.word  opc_DE, Bopc_DE  // Opcode DE - DEX $0000,X
.word  opc_DF, Bopc_DF  // Opcode DF - BBS5
.word  opc_E0, Bopc_E0  // Opcode E0 - CPX #$00
.word  opc_E1, Bopc_E1  // Opcode E1 - SBC ($00,X)
.word  opc_E2, BINSTRUCTION+ (NEXTJITLET>>2)  // Opcode E2 - NOP #$00
.word  opc_E3, Bopc_E3  // Opcode E3 -
.word  opc_E4, Bopc_E4  // Opcode E4 - CPX $00
.word  opc_E5, Bopc_E5  // Opcode E5 - SBC $00
.word  opc_E6, Bopc_E6  // Opcode E6 - INC $00
.word  opc_E7, Bopc_E7  // Opcode E7 - SMB6 $00
.word  opc_E8, Bopc_E8  // Opcode E8 - INX
.word  opc_E9, Bopc_E9  // Opcode E9 - SBC #$00
.word  opc_EA; ARMNOPR6QUICK  // Opcode EA - NOP
.word  opc_EB, Bopc_EB  // Opcode EB - NOP ( Borrowed for instruction timing)
.word  opc_EC, Bopc_EC  // Opcode EC - CPX $0000
.word  opc_ED, Bopc_ED  // Opcode ED - SBC $0000
.word  opc_EE, Bopc_EE  // Opcode EE - INC $0000
.word  opc_EF, Bopc_EF  // Opcode EF - BBS6
.word  opc_F0, BEQINSTRUCTION  // Opcode F0 - BEQ
.word  opc_F1, Bopc_F1  // Opcode F1 - SBC ($00),Y
.word  opc_F2, Bopc_F2  // Opcode F2 - SBC ($00)
.word  opc_F3, Bopc_F3   // Opcode F3 - NOP
.word  opc_F4, BINSTRUCTION+ (NEXTJITLET>>2)  // Opcode F4 - NOP2
.word  opc_F5, Bopc_F5  // Opcode F5 - SBC $00,X
.word  opc_F6, Bopc_F6  // Opcode F6 - INC $00,X
.word  opc_F7, Bopc_F7  // Opcode F7 - SMB7 $00
.word  opc_F8, Bopc_F8  // Opcode F8 - SED
.word  opc_F9, Bopc_F9  // Opcode F9 - SBC $0000,Y
.word  opc_FA, Bopc_FA  // Opcode FA - PLX
.word  opc_FB, Bopc_FB  // Opcode FB - NOP
.word  opc_FC, BINSTRUCTION+ (NEXTJITLET>>1)  // Opcode FC - NOP $0000
.word  opc_FD, Bopc_FD  // Opcode FD - SBC $0000,X
.word  opc_FE, Bopc_FE  // Opcode FE - INC $0000,X
.word  opc_FF, Bopc_FF  // Opcode FF - BBS7


timingtemp:
   .word 0
opc_10: // Opcode 10 - BPL
opc_30: // Opcode 30 - BMI
opc_80: // Opcode 80 - BRA
opc_90: // Opcode 90 - BCC
opc_B0: // Opcode B0 - BCS
opc_D0: // Opcode D0 - BNE
opc_F0: // Opcode F0 - BEQ
   sxtb r4,r4
   ldr r11,=0xea000001 // Branch +2
// do jump maths
// temp0 is where we need to goto NB -2 ie. if we have 6 we goto +8
// so it look like arm and 6502 jumps are the same so if we load the bit signed
// we just need to clear the top byte
   mov r10,r4,LSL #1
   add r10,r10,#2
   BIC r10,r10,#0xFF000000
   orr r10,r10,r9
   STRD r10,r11,[R6]

jitend2byteexit:
   FILLINJITTEDTABLES dejit16bit21
   add temp0,temp0,#(dejit16bit22-dejit16bit21-(1<<JITTEDTABLESHIFT))>>2
   mov r5,#TWOBYTES
   str temp0,[temp1,#1<<JITTEDTABLESHIFT]
  // FILLINJITTEDTABLES dejit16bit22

dojitexit :
   DEBUGDISS
   add r2,r2,r5
   sub r2,r2,#1   // move end pointer to the last byte
// flush BTB icache dcache it might be better to just invalidate a few lines
#if (__ARM_ARCH >= 7 )
   DSB // might not be needed

   // Clean data cache both JITLET
   // r3 has the first address of JITLET instruction
   // r2 has last address

   ldrb r12,cachelineD
   ldrb r11,cachelineI
   mov r2,r2,LSL #BYTEWORDSHIFT        // change to words
   sub r4,r3,#JITLET
   add r2,r2,#JITTEDTABLE16   //
   mov r4,r4,LSR #(JITLETSHIFT-2)
   add r4,r4,#JITTEDTABLE16

   mov r5,r2   // end address needs to flushed (JITTEDTABLE16)
   mov r6,r3   // start address
   mov r7,r4
   sub r10,r12,#1
   bic r4,r4,r10           // round down to the begin of the line

cacheflushloopD:
   mcr p15,0,r4,cr7,cr11,1 // data JITTEDTABLE16 (DCCMVAU)
   add r4,r4,r12
   mcr p15,0,r3,cr7,cr11,1 // data JITLET (NB 2xJITLET compared to table)
   cmp r4,r2
   add r3,r3,r12
   mcr p15,0,r3,cr7,cr11,1 // data JITLET
   add r3,r3,r12
   ble cacheflushloopD

   DSB

   sub r10,r11,#1
   bic r7,r7,r10           // round down the begin of the line

cacheflushloopI:
   MCR p15,0,r7, c7, c5, 1 // instruction (ICIMVAU)
   add r7,r7,r11
   MCR p15,0,r6, c7, c5, 1 // instruction
   cmp r7,r5
   add r6,r6,r11
   MCR p15,0,r6, c7, c5, 1 // instruction
   add r6,r6,r11
   ble cacheflushloopI

   MCR p15, 0, temp0, c7, c5, 6 //; invalidate all of the BTB (BPIALL)
   //MCR p15, 0, reg12, c7, c5, 0 // invalidate I&BTB cache

   DSB
   ISB

#else
// Pizero Cache flush code

#if 1

   // this should flush the cache area used in the jitter.
   // even better if we were to work out if full cache fush would be faster
   // especially because of the ARMv6 cache flush errata which can be seen in the
   // Twinhead demo on the Desire screen

   // MCRR p15,end,start,cxx
   mov temp,#0
   //MCR p15, 0, temp, c7, c5, 6 //; invalidate all of the BTC

   mov r2,r2,LSL #JITLETSHIFT
   add r7,r2,#JITLET
   sub r4,r3,#JITLET
   mov r4,r4,LSR #(JITLETSHIFT-JITTEDTABLESHIFT)
   mov r2,r2,LSR #(JITLETSHIFT-JITTEDTABLESHIFT)
   add r4,r4,#JITTEDTABLE16
   add r2,r2,#JITTEDTABLE16

   MCRR p15,0,r7,r3,c12 // clean data cache  ( jitlets)
   MCRR p15,0,r2,r4,c12 // clean data cache

   MCRR p15,0,r7,r3,c5 // invalidate instruction cache
   MCRR p15,0,r2,r4,c5 // invalidate instruction cache

   MCR p15, 0, temp, c7, c10, 4 // DSB

   MCRR p15,0,r7,r3,c5 // invalidate instruction cache // Do it again ( errata)
   MCRR p15,0,r2,r4,c5 // invalidate instruction cache // Do it again ( errata)
   //MCR p15, 0, r0, c7, c5, 1

   MCR p15, 0, temp, c7, c10, 4 // DSB
 //  MCR p15, 0, temp, c7, c5, 0 //; invalidate I cache and BTC BTAC
   MCR p15, 0, temp, c7, c5, 6 // flush entire Branch Target Cache
   MCR p15, 0, temp, c7, c5, 4 // flush prefetch buffer
   MCR p15, 0, temp, c7, c10, 4 // DSB
   MCR p15, 0, r3, c7, c13, 1    // prefetch instruction cache line
#else
   // clean all the cache
   mov temp,#0
   MCR p15, 0, temp, c7, c10, 0 //; clean data cache line
   //mcr p15, 0, temp, c7, c10, 4 //drain write buffer
   MCR p15, 0, temp, c7, c5, 0 //; invalidate I cache and BTB
   MCR p15, 0, temp, c7, c5, 4 // flush prefetch buffer
#endif

#endif
#ifdef DEBUG_TIMING
        ldr     r8, =ASS_GPCLR0
        mov     r7, #TEST2_MASK
        str     r7, [r8]
#endif
   pop {r3-r11}
#if __ARM_ARCH>6
   PLI   [r3]
#endif
testforirq:
   CPSID if                      // IRQs must also be disabled if FIQs are disabled
   ldrb  r0, fiqjitflag
   mov   reg12, r4
   mov   r1, #0
   strb  r1, fiqjitflag
   cmp   r0, #2
   blt   noirq_jit

irqoccured:
   ldr   r1, =tube_irq
   ldr   r0, [r1]               // Get Mailbox data
   tst   r0, #RESET_BIT
   bne   reset_handler_jit
   tst   r0, #NMI_BIT
   bne   handle_nmi_jit
   tst   flags, #I_FLAG6502      // Test whether interrupts are enabled (bit 2 = 0)
   bne   noirq_jit
   tst   r0, #IRQ_BIT             // Bit 0 set indicate IRQ is active
   beq   noirq_jit

handle_irq_jit:
// deal with IRQ during jitter
   SUB   temp1,r3,#JITLET
   mov   temp1,temp1,LSR #JITLETSHIFT
   ldr   reg3, =MOVPCR14INSTRUCTION      // constant used to for store compares
   mov   reg2,#JITLET
   mov   reg4, #2 // constant used by jsr rts
   add   reg2,reg2,#NEXTJITLET
   msr   CPSR,reg12 // Restore the 6502 flags, and interrupt state present on entry
   INTR  -2, 0

handle_nmi_jit:
// deal with IRQ during jitter
   BIC   r0,r0,#NMI_BIT  // clear NMI flag
   str   r0,[r1]   // Store it back again
   SUB   temp1,r3,#JITLET
   mov   temp1,temp1,LSR #JITLETSHIFT
   ldr   reg3, =MOVPCR14INSTRUCTION      // constant used to for store compares
   mov   reg2,#JITLET
   mov   reg4, #2 // constant used by jsr rts
   add   reg2,reg2,#NEXTJITLET
   msr   CPSR,reg12 // Restore the 6502 flags, and interrupt state present on entry
   INTR  -4, 0

noirq_jit:

   mov   reg2,#JITLET
   mov   r14, r3
   mov   reg4, #2 // constant used by jsr rts
   add   reg2,reg2,#NEXTJITLET
   ldr   reg3, =MOVPCR14INSTRUCTION      // constant used to for store compares
   msr   CPSR,reg12 // Restore the 6502 flags, and interrupt state present on entry
   bx    r14   // this is slow on some system , but we have just flushed the cache which is slow

debugopcode:
#if JITDEBUG
   strb  temp,debugflag
#endif
   bx    lr

opc_60: // Opcode 60 - RTS
   ldrd r8,[r9]
   sub r9,r9,r6
   mov r9,r9,LSR #BYTEWORDSHIFT
   ORR r9,r9,#BINSTRUCTION
   B singleopcodeexitstore

opc_00: // Opcode 00 - BRK
opc_40: // Opcode 40 - RTI
   ldrd  r8,[r9]
   MAKEBRANCHLINK r9
singleopcodeexitstore:
   BL    fillinjittabledejit16bit1store
   B     dojitexit

opc_88: // Opcode 88 - DEY
opc_8A: // Opcode 8A - TXA
opc_98: // Opcode 98 - TYA
opc_A8: // Opcode A8 - TAY
opc_AA: // Opcode AA - TAX
opc_BA: // Opcode BA - TSX
opc_C8: // Opcode C8 - INY
opc_CA: // Opcode CA - DEX
opc_E8: // Opcode E8 - INX
#if (__ARM_ARCH > 6 )
   ldrb r7,[r12,r4]
   ldrd r8,[r9]
   BL optimiseflags
   ldrne r9,=0xe1a06006 // ARMNOPR6QUICK
   b jitend1bytestore2
#endif

opc_0A: // Opcode 0A - ASL A
opc_48: // Opcode 48 - PHA
opc_4A: // Opcode 4A - LSR A
opc_78: // Opcode 78 - SEI
opc_9A: // Opcode 9A - TXS
opc_B8: // Opcode B8 - CLV
opc_D8: // Opcode D8 - CLD
opc_EB: // Opcode EB - NOP6 ( Used for instruction timing)
opc_F8: // Opcode F8 - SED
opc_FB: // Opcode FB - NOP6
   ldrd r8,[r9]
   B jitend1bytestore2

opc_1A: // Opcode 1A - INC A
opc_2A: // Opcode 2A - ROL A
opc_3A: // Opcode 3A - DEC A
   ldrb r7,[r12,r4]
   BL optimiseflags
   ldrned r8,[r9,#2*4]
   Bne jitend1bytestore2

opc_08: // Opcode 08 - PHP
opc_28: // Opcode 28 - PLP
opc_58: // Opcode 58 - CLI
opc_5A: // Opcode 5A - PHY
opc_68: // Opcode 68 - PLA
opc_6A: // Opcode 6A - ROR A
opc_7A: // Opcode 7A - PLY
opc_DA: // Opcode DA - PHX
opc_E3: // Opcode E3 - NOP6
opc_F3: // Opcode F3 - NOP6
opc_FA: // Opcode FA - PLX
   ldrd r8,[r9]
   MAKEBRANCHLINK r9
jitend1bytestore2:
   BL  fillinjittabledejit16bit1store
   B dojitloop

opc_8C: // Opcode 8C - STY $0000
opc_8D: // Opcode 8D - STA $0000
opc_8E: // Opcode 8E - STX $0000
opc_9C: // Opcode 9C - STZ $0000
   cmp   r4,#0xE0
   cmpge r5,#0xFE
   BEQ   iostoreaccess

   LDMIA r9,{r7,r8,r9,r10,r11,r12}
   orr   temp0,r4,r5,LSL #8
   orr   r7,r7,r5
   orr   r11,r11,r4
   add   r12,r12,temp0,LSL #JITTEDTABLESHIFT
   B     jitend3bytestoreoperandBL

iostoreaccess:
   add   r9,r9,#6*4
// .word address-28
opc_0C: // Opcode 0C - TSB $0000
opc_1C: // Opcode 1C - TRB $0000
opc_3C: // Opcode 3C - BIT $0000,X
opc_6D: // Opcode 6D - ADC $0000
opc_79: // Opcode 79 - ADC $0000,Y
opc_7D: // Opcode 7D - ADC $0000,X
opc_DE: // Opcode DE - DEC $0000,X
opc_ED: // Opcode ED - SBC $0000
opc_F9: // Opcode F9 - SBC $0000,Y
opc_FD: // Opcode FD - SBC $0000,X
   LDMIA r9,{r7,r8,r9,r10,r11,r12}
   orr r7,r7,r5
   orr r8,r8,r4
jitend3bytestoreoperandBL:
   MAKEBRANCHLINK r12
   B jitend3bytestore

opc_AD: // Opcode AD - LDA $0000
   cmp   r4,#0xE0
   cmpge r5,#0xFE
   BEQ   loadio
   // ramload fall into below

opc_0D: // Opcode 0D - ORA $0000
opc_2D: // Opcode 2D - AND $0000
opc_4D: // Opcode 4D - EOR $0000
   BL    fixuptype2accessquick
   b     jitend3bytestore

opc_AC: // Opcode AC - LDY $0000
opc_AE: // Opcode AE - LDX $0000
   cmp   r4,#0xE0
   cmpge r5,#0xFE
   BEQ   loadio
   // fall into below
opc_0E: // Opcode 0E - ASL $0000
opc_1E: // Opcode 1E - ASL $0000,X
opc_2E: // Opcode 2E - ROL $0000
opc_3E: // Opcode 3E - ROL $0000,X
opc_4E: // Opcode 4E - LSR $0000
opc_5E: // Opcode 5E - LSR $0000,X
opc_6E: // Opcode 6E - ROR $0000
opc_7E: // Opcode 7E - ROR $0000,X
opc_99: // Opcode 99 - STA $0000,Y
opc_9D: // Opcode 9D - STA $0000,X
opc_9E: // Opcode 9E - STZ $0000,X
opc_CC: // Opcode CC - CPY $0000
opc_CD: // Opcode CD - CMP $0000
opc_CE: // Opcode CE - DEC $0000
opc_D9: // Opcode D9 - CMP $0000,Y
opc_DD: // Opcode DD - CMP $0000,X
opc_EC: // Opcode EC - CPX $0000
threebyteinstruction:
   LDMIA r9,{r7,r8,r9,r10,r11,r12}
   orr r7,r7,r5
   orr r8,r8,r4
jitend3bytestore:
   FILLINJITTEDTABLES dejit16bit31
   STMIA r6,{r7,r8,r9,r10,r11,r12}
   add temp0,temp0,#(dejit16bit32-dejit16bit31-(1<<JITTEDTABLESHIFT))>>2
   mov r5,#THREEBYTES
   str temp0,[temp1,#1<<JITTEDTABLESHIFT]
   add temp0,temp0,#(dejit16bit33-dejit16bit32-(1<<JITTEDTABLESHIFT))>>2
   str temp0,[temp1,#2<<JITTEDTABLESHIFT]
   B dojitloop

loadio:
   add r9,r9,#6*4
   LDMIA r9,{r7,r8,r9,r10,r11,r12}
   orr r7,r7,r5
   orr r8,r8,r4
   MAKEBRANCHLINK r9
   B jitend3bytestore

opc_0F: // Opcode 0F - BBR0
opc_1F: // Opcode 1F - BBR1
opc_2F: // Opcode 2F - BBR2
opc_3F: // Opcode 3F - BBR3
opc_4F: // Opcode 4F - BBR4
opc_5F: // Opcode 5F - BBR5
opc_6F: // Opcode 6F - BBR6
opc_7F: // Opcode 7F - BBR7
opc_8F: // Opcode 8F - BBS0
opc_9F: // Opcode 9F - BBS1
opc_AF: // Opcode AF - BBS2
opc_BF: // Opcode BF - BBS3
opc_CF: // Opcode CF - BBS4
opc_DF: // Opcode DF - BBS5
opc_EF: // Opcode EF - BBS6
opc_FF: // Opcode FF - BBS7
   LDMIA r9,{r7,r8,r9,r10,r11,r12}
   sxtb r5, r5
   mov r5,r5,LSL #1
   BIC r5,r5,#0xFF000000
   orr r11,r5,#BINSTRUCTION
   orr r7,r7,r4
   MAKEBRANCHLINK r10
jitend3byteexitstore6:
   STMIA r6,{r7,r8,r9,r10,r11,r12}

jitend3byteexit:
   FILLINJITTEDTABLES dejit16bit31
   add temp0,temp0,#(dejit16bit32-dejit16bit31-(1<<JITTEDTABLESHIFT))>>2
   mov r5,#THREEBYTES
   str temp0,[temp1,#1<<JITTEDTABLESHIFT]
   add temp0,temp0,#(dejit16bit33-dejit16bit32-(1<<JITTEDTABLESHIFT))>>2
   str temp0,[temp1,#2<<JITTEDTABLESHIFT]
   B dojitexit

fillinjittabledejit16bit1store:
   mov   r5,#ONEBYTE
   strd  r8,r9,[r6]
fillinjittabledejit16bit1:
   ldr   temp0,=dejit16bit1-(4*JITTEDTABLESHIFT)-JITTEDTABLE16
fillinjittedtable:
   mov   temp1,#JITTEDTABLE16

#if JITTEDTABLESHIFT==3
   add   temp1,temp1,#4*(JITTEDTABLESHIFT-2)
   mov   temp2,temp2,LSL #1
   RSB   temp0,temp2,temp0,LSR #BYTEWORDSHIFT
   mov   temp2,temp2,LSR #1
#else
   RSB   temp0,temp2,temp0,LSR #BYTEWORDSHIFT
#endif

   ORR   temp0,temp0,#BINSTRUCTION
   str   temp0,[temp1,temp2,LSL #JITTEDTABLESHIFT]!
   bx    lr

opc_B1: // Opcode B1 - LDA ($00),Y
opc_B2: // Opcode B2 - LDA ($00)
   cmp   r4, #0xFF
   BEQ  replaceldrh
opc_A4: // Opcode A4 - LDY $00
opc_A5: // Opcode A5 - LDA $00
opc_A6: // Opcode A6 - LDX $00

   ldrb r7,[r12,r5]
   BL optimiseflags
   addne r9,r9,#4*4
   b fixupldrh

opc_91: // Opcode 91 - STA ($00),Y
opc_92: // Opcode 92 - STA ($00)
   cmp   r4, #0xFF
   BEQ  replaceldrh
   ldrb  r7,[r12,r5]
   BL    optimiseflags
   beq   fixupldrh // beq when it becomes faster.
   add   r9,r9,#4*4
   LDMIA r9,{r8,r9,r10,r11}
   and   r1, r4, #0xF0
   and   temp, r4, #0x0F
   orr   r8, r8, r1, LSL #4
   orr   r8, r8, temp
   MAKEBRANCHLINK r11
   B     jitend2bytestore4

opc_11: // Opcode 11 - ORA ($00),Y
opc_12: // Opcode 12 - ORA ($00)
opc_31: // Opcode 31 - AND ($00),Y
opc_32: // Opcode 32 - AND ($00)
opc_51: // Opcode 51 - EOR ($00),Y
opc_52: // Opcode 52 - EOR ($00)
opc_D1: // Opcode D1 - CMP ($00),Y
opc_D2: // Opcode D2 - CMP ($00)
   cmp   r4, #0xFF
   BEQ  replaceldrh

opc_05: // Opcode 05 - ORA $00
opc_25: // Opcode 25 - AND $00
opc_45: // Opcode 45 - EOR $00
fixupldrh:
   LDM   r9, {r8,r9,r10,r11}
   and   r1, r4, #0xF0
   and   temp, r4, #0x0F
   orr   r8, r8, r1, LSL #4
   orr   r8, r8, temp
   B     jitend2bytestore4

replaceldrh:
   LDM   r9, {r8,r9,r10,r11}
   LDR   r8,=replacementldrh-8
   MAKEBRANCHLINK r8
   B     jitend2bytestore4

replaceldrhadcsbc:
   LDR   r8,=replacementldrh-8
   MAKEBRANCHLINK r8
   B     jitend2byteBLendstore

fixuptype2accessquick:
   LDM r9,{r7,r8,r9,r10,r11,r12}
   and r1,r4,#0xF0
   and r4,r4,#0x0F
   orr r7,r7,r5
   orr r8,r8,r1,LSL #4
   orr r8,r8,r4
   bx lr

fixuptype2accessquickthird:
   LDM r9,{r7,r8,r9,r10,r11,r12}
   and r1,r4,#0xF0
   and r4,r4,#0x0F
   orr r8,r8,r5
   orr r9,r9,r1,LSL #4
   orr r9,r9,r4
   bx lr

optimiseflags:
   tst   r7,#NF+EF
   bne   optimiseflagsexit
   and   r1,r7,#3       // get length of next instruction
   ldrb  r7,[r12,r10]   // get length of original instruction
   add   r1,r1,r2       // add pointer
 optimiseflagsloop:
   and   r7,#3          // get length
   ldrb  r7,[r1,r7]!    // get next instruction
   ldrb  r7,[r12,r7]    // get flags for next instruction
   tst   r7,#NF+EF
   beq  optimiseflagsloop
optimiseflagsexit:
#if OPTIMISMISE>0
   tst   r7,#NF
#else
   movs  r1,#0
#endif
   bx    LR

// Jits overflow

brk:
   sub   reg1,r14,#JITLET
   mov   reg1,reg1,LSR#JITLETSHIFT
   add   reg1,reg1,#1
   INTR  -2, 1

rti:
   PLP
   ldrh  reg1, [regSP, #1]

   UADD8 regSP, regSP, reg4

   add   reg0,reg1,#1
   cmp   reg0,#0x10000
   ldr   reg0, =tube_irq             // Get IRQ status
   // so we have an irq that happened during execution
   beq   rtiended

   tst   flags, #I_FLAG6502        // Test whether interrupts are enabled (bit 2 = 0)
   bne   dontdoirqnow
   ldr   reg0, [reg0]
   tst   reg0, #1                    // Test for IRQ
   bne   handle_irqrti
dontdoirqnow:
   mov   reg1,reg1,LSL #JITLETSHIFT
   add   reg1,reg1,#JITLET
   msr   CPSR_flg, reg12              // Restore the 6502 flags
   bx    reg1

handle_irqrti:

   msr   CPSR_flg, reg12     // Restore the 6502 flags
   INTR  -2, 0

cli:
   mrs   reg12,CPSR
   B     check_irq_plp

plp:
   PLP
// CHECK_IRQ
check_irq_plp:
   ldr   reg0, =tube_irq             // Get IRQ status
   ldr   reg0, [reg0]
   //mrs     reg12, CPSR                  // Preserve the 6502 flags
   tst   flags, #I_FLAG6502        // Test whether interrupts are enabled (bit 2 = 0)
   eoreq reg0, reg0, #1
   tsteq reg0, #1                    // Test for IRQ
   beq   handle_irqplp
   msr   CPSR_flg, reg12              // Restore the 6502 flags
   bx    lr
.ltorg
handle_irqplp:
   sub   reg1,r14,#JITLET
   mov   reg1,reg1,LSR#JITLETSHIFT

   msr   CPSR_flg, reg12     // Restore the 6502 flags
   INTR  -2, 0

putcharopcode:
   push  {r2,r3,r4,r14}
   mrs   r4, CPSR               // Save 6502 flags
   CPSID if
   mov   r0,regA
   BL    putchar
   msr   CPSR, r4
   pop   {r2,r3,r4,pc}

invertcarry:
   mrs   reg0, CPSR
   eor   reg0, reg0, #C_FLAG
   msr   CPSR_flg, reg0
   bx    lr

testvflagC:
   mrs   reg1, CPSR
   tst   flags, #V_FLAG6502
   addne pc,r14,#8 // return to jit
   bx    lr

testvflagS:
   mrs   reg1, CPSR
   tst   flags, #V_FLAG6502
   addeq pc,r14,#8 // return to jit
   bx    lr

bbrreturn:
   addne pc,r14,#4 // return to jit
   msr   CPSR_flg, reg1
   bx    lr

bbsreturn:
   addeq pc,r14,#4 // return to jit
   msr   CPSR_flg, reg1
   bx    lr

bitimm:
   bicne   reg1, reg1, #Z_FLAG
   orreq   reg1, reg1, #Z_FLAG
   msr     CPSR_flg, reg1
   bx lr

jitstxytable:
   add reg0,jittedtable16ptr,reg1,LSR #24-JITTEDTABLESHIFT
jittablezp:
   mov reg1,reg1,LSR #24
   BX reg0

jitstatable:
   strb regA,[reg1]
   add reg0,jittedtable16ptr, reg1,LSL #JITTEDTABLESHIFT
   BX reg0

storeXflags:
   strb reg0,[ram6502,reg1,LSR #24]
tempflags:
   sxtb reg0, reg0
   teq  reg0, #0
   bx lr

incflags:
   add   reg0, reg0, #1
   sxtb  reg0, reg0
   bx    lr

decflags:
   sub   reg0, reg0, #1
   sxtb  reg0, reg0
   bx    lr

incstore:
   add  reg12, reg12, #1
   strb reg12,[reg1]
   bx lr

plx:
   uadd8 regX,regSP,reg1
   ldrsb  regX, [regX]
   uadd8 regSP,regSP,reg1
   teq   regX, #0
   mov   regX, regX, LSL #24
   bx lr

ply:
   uadd8 regY,regSP,reg1
   ldrsb  regY, [regY]
   uadd8 regSP,regSP,reg1
   teq   regY, #0
   mov   regY, regY, LSL #24
   bx lr

pla:
   uadd8 regA,regSP,reg1
   ldrsb regA, [regA]
   uadd8 regSP,regSP,reg1
   teq   regA, #0
   bx lr

rts:
   add   reg12,reg2,reg0,LSL #JITLETSHIFT
   UADD8 regSP, regSP, reg4
   bx    reg12

rora:
   subcs   regA, regA, #0x100   // if Carry set all other bits ie sign extend
   rrxs    regA, regA
   bx lr

ror:
   ROR6
   bx lr

rol:
   adc  reg12, reg12, reg12
   strb reg12,[ram6502, reg1, LSR #24]
   bx lr

rola:
   lsls    reg1, regA, #24
   sxtb    regA, regA
   bx lr

bitioload: //2c
   push  {r14}
   BL    ioload
   msr   CPSR, reg12         // Restore 6502 flags and FIQ/IRQ bits
   pop   {r14}

bitadjustr14:
   add   r14,r14,#16
   bic   flags, flags, #V_FLAG6502
   tst   r0, regA  // This clears N flag and sets up the Z flag
   mrs   reg12, CPSR
   and   reg1, r0, #N_FLAG6502
   and   r0, r0, #V_FLAG6502
   orr   flags, flags, r0
   orr   reg12, reg12, reg1, LSL #24
   msr   CPSR_flg, reg12
   bx    r14            // this can be slow

bit24: // 24
   add   r14,r14,#8
   bic   flags, flags, #V_FLAG6502
   tst   reg12, regA  // This clears N flag and sets up the Z flag
   mrs   reg0, CPSR
   and   reg1, reg12, #N_FLAG6502
   and   reg12, reg12, #V_FLAG6502
   orr   flags, flags, reg12
   orr   reg0, reg0, reg1, LSL #24
   msr   CPSR_flg, reg0
   bx    r14               // this can be slow

bit: // 34
   tst   reg12, regA  // This clears N flag and sets up the Z flag
bit2: // 3c
   mrs   reg0, CPSR
   and   reg1, reg12, #N_FLAG6502
   and   reg12, reg12, #V_FLAG6502
   orr   flags, flags, reg12
   orr   reg0, reg0, reg1, LSL #24
   msr   CPSR_flg, reg0
   bx    lr

cmpa:
   ldrb  reg0,[reg1]
   mov   reg1,regA,LSL #24
   cmp   reg1,reg0,LSL#24
   bx    lr

loada:
   ldrsb regA,[reg1]
   teq   regA,#0
   bx    lr

aflags:
   sxtb  regA, regA
   teq   regA, #0
   bx    lr

asl:
   ASL6
   bx    lr

php:
   orrmi reg12, reg12, #N_FLAG6502
   orrcs reg12, reg12, #C_FLAG6502
   orreq reg12, reg12, #Z_FLAG6502
pushtemp:
   SPUSH reg12
   bx    lr

//   mrs reg0, CPSR
trb:
   bic reg0, reg0, #Z_FLAG
   tst reg12, regA
trb2:
   bic reg12, reg12, regA
   strb reg12,[reg1]
   orreq   reg0, reg0, #Z_FLAG
   msr     CPSR_flg, reg0
   bx lr

//   mrs reg0, CPSR
tsb:
   bic reg0, reg0, #Z_FLAG
   tst reg12, regA
tsb2:
   orr reg12, reg12, regA
   strb reg12,[reg1]
   orreq   reg0, reg0, #Z_FLAG
   msr     CPSR_flg, reg0
   bx lr

//   mov reg1,reg1,LSR #24
ldrhldrsb:
   ldrh reg0,[reg1]
   ldrsb reg0,[reg0]
   bx lr

replacementldrh:
   ldrb  r1,[ram6502]
   ldrb  r0,[ram6502, #0xFF]
   orr   r1,r0,r1,LSL#8
   bx    lr

//
// Decimal mode functions
//
// TODO **** sort out registers *****
ADCload:
        ldrb   reg12,[reg1]
ADC0:
        tst     flags, #D_FLAG6502
ADC1:
        bic     flags, #V_FLAG6502
ADC2:
        lsl     regA, regA, #24
ADC3:
        subcs   reg12, reg12 ,#0x100    // if carry set then set top 24 bits
        bne     6f
        adcs    regA, regA, reg12, ror #8
        orrvs   flags, #V_FLAG6502
        SXTB    regA, regA, ROR #24
        bx lr
6:
decimal_mode_adc:
        // if carry is set the upper 24 bits of reg12 are set too

        and     reg0, regA, #0x0F<<24
        and     reg1, reg12, #15

        adc     reg1, reg1, reg0, lsr #24
        cmp     reg1, #10
        subcs   reg1, reg1, #10
        and     reg1, reg1, #0x0F
        orrcs   reg12, reg12 ,#0x0F
        and     regA, regA, #0xF0<<24
        orrcs   regA, regA, #0x0F<<24
        adcs    regA, regA, reg12 , ror #8
        orrvs   flags, #V_FLAG6502
        cmpcc   regA, #10<<28
        subcs   regA, regA, #10<<28
        and     regA, regA, #0xF0<<24
        orr     regA, regA, reg1, lsl #24
        sxtb    regA, regA, ror #24
        teq     regA, #0
        bx lr

SBCload:
        ldrb    reg12,[reg1]
SBC0:
        tst     flags, #D_FLAG6502
SBC1:
        bic     flags, #V_FLAG6502
SBC2:
        LSL     regA, regA, #24
SBC3:
        bne     6f
        // probably can remove this with some thought
        subcc   reg12, reg12 ,#0x100        // if carry set then set top 24 bits
        sbcs    regA, regA, reg12, ror #8
        orrvs   flags, #V_FLAG6502
        SXTB    regA, regA, ROR #24
        bx lr
6:

decimal_mode_sbc:
        bic     reg0, reg12,# 0xf0
        and     reg1, regA, #0xf000000
        sbc     reg1, reg1, reg0, ror #8
        sbcs    regA, regA, reg12, ror #8
        orrvs   flags, #V_FLAG6502
        subcc   regA, regA,#0x60<<24
        teq     reg1,#0
        submi   regA, regA, #0x6<<24
        sxtb    regA, regA, ror #24
        teq     regA, #0
        bx lr

storetest:
   teq   reg0, reg3
   add   reg0, jittedtable16ptr, reg1, LSL #JITTEDTABLESHIFT
   bne   1f
   bx    lr
1:
   bx  reg0

opc_09: // Opcode 09 - ORA #$00
opc_29: // Opcode 29 - AND #$00
opc_49: // Opcode 49 - EOR #$00
   ldrd  r8,r9,[r9]
   adr r7,immediatesignextenedtable
   ldr r7,[r7,r4,LSL #BYTEWORDSHIFT]
   strd  r8,r9,[r6,#8]
   STMIA r6,{r7,r8}
   B jitend2byte

opc_A9: // Opcode A9 - LDA #$00
   adr r11,immediatesignextenedtable
   ldr temp,[r11,r4,LSL #BYTEWORDSHIFT]
   str r9,[r6,#4]
  // str r9,[r6,#8]
  // str r9,[r6,#12]
   orr temp,temp,#1<<20// S FLAG
   orr temp,temp,#0xA000 // regA
   str temp,[r6]
   B jitend2byte

opc_2C: // Opcode 2C - BIT $0000

   FILLINJITTEDTABLES dejit16bit1
   cmp   r4,#0xE0
   cmpge r5,#0xFE

   BEQ bitabsio

   orr r8,r9,r5
   adrl r9,bitabstable-12
   add r9,r9,r4,LSL #3
   MAKEBRANCHLINK r9
   mov r5,#THREEBYTES
   strd r8,r9,[r6]
   B dojitloop

bitabsio:
   ldr r8,Bopc_2C_io
   adr r9,bitioload-12
   orr r8,r8,r4
   MAKEBRANCHLINK r9
   mov r5,#THREEBYTES
   strd r8,r9,[r6]
   B dojitloop
.ltorg
Bopc_2C_io: // Opcode 2C - BIT $0000
//   mov r0,#0x00
// use first word  of table
immediatesignextenedtable:
.set imm,0
.rept 128
   mov reg0,#imm
   .set imm,imm+1
.endr
.rept 128
   mov reg0,#0xFFFFFF00+imm
   .set imm,imm+1
.endr

bitabstable:
.set imm,0
.rept 256
   ldrb reg0,[reg1,#imm]
   B bitadjustr14
   .set imm,imm+1
.endr

storestable:
.set imm,0
.rept 256
   mov reg1,#imm
   B JITTEDTABLE16+(imm<<JITTEDTABLESHIFT)
   .set imm,imm+1
.endr
