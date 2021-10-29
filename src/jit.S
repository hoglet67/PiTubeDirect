// Co pro JIT 65C02

// Copyright Dominic Plunkett 2021

// Memory map
// 0x00000000 - 0x00FFFF 64K 65C02 RAM
// 0x00010000 - 0x010FFF 4K ram doubly mapped by the mmu
// 0x00010000 - 0x0FFFFF 1MByte for extra RAM we won't support code in this area

// we use the ram at 0x0D000000 for code as that
// is within jump range of the kernel
// we have up to 0x0DC00000 to use
// 0x0D000000 - 0x0D07FFFF 256K 8 bytes for JIT code for each 65c02 byte

#define JITLET 0x0D000000

// 0x0D100000 - 0x0D13FFFF 256K 4 bytes for each byte
//                                to signal if byte has been jitted
//                      either mov pc,r14 or B dejit16bit 1,21,22,31,32,33
// This table also needs to wrap at 256K to cope with 65535 wrapping
// This is done with the MMU just like the RAM

#define JITTEDTABLE16 0x0d100000

// NB this code starts 0x0e400000 + xxx

//#define JITDEBUG 1
//#define DORMANN_TEST 1

#define TIMING_INSTRUCTION 1

/* Interrupts

If an interrupt occurs try putting the current ARM PC and registers on to another stack and put fake address ( e.g. 0xFFFF) and status onto 65C02 stack jump off to vectors
Intercept the RTI instruction we could check for 0xFFFF on stack , restore status.
then return to our secret stack This relies on instructions being re entrant , but this should be fine
*/

// Instructions that get used in smaller chunks to save a jump ( overlapping )
// BIT &0000 so B on first byte
// BIT &00 //TODO ?
// NOP6s &0000
// NOP6s &00

// used for scope debug
#include "tube-defs.h"

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

#define reg12      r12

#define temp0 r0
#define temp1 r1
#define temp2 r2
#define temp r12

#define JITLETSHIFT 3

#define NEXTJITLET (1<<JITLETSHIFT)

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

#include "tube-defs.h"

.text
.global exec_65tubejit

// **** MACROS ******

.macro FILLINJITTEDTABLES table16
   ldr temp0,=\table16-8-JITTEDTABLE16
   // temp0 is address to goto
   // temp1 is location for store

   BL fillinjittedtable
.endm

.macro DEJITCLEAN count=0

#if defined(RPI4)|| defined(RPI3)
   //bic reg3,reg12,#0x3f // hack
      MCR p15, 0, reg12, c7, c11, 1 // clean data cache line
      DSB
      MCR p15, 0, reg12, c7, c5, 1 // invalidate I cache line
      MCR p15, 0, reg12, c7, c5, 7 // invalidate BTB entry
      DSB
      .ifc \count,0
         ISB
      .endif
#else
   #if defined(RPI2)
      DSB
   #endif
   MCR p15, 0, reg12, c7, c10, 1 // clean data cache line

   #if defined(RPI2)
      MCR p15, 0, reg12, c7, c5, 1 // invalidate I cache line
      MCR p15, 0, reg12, c7, c5, 7 // invalidate BTB entry
      DSB
   #else
   .ifc \count,0
      MCR p15, 0, reg12, c7, c5, 0 //; invalidate I cache and BTB
      MCR p15, 0, reg3, c7, c10, 4 // DSB
   .endif
     // MCR p15, 0, reg3, c7, c10, 4 // DSB
     // MCR p15, 0, reg12, c7, c5, 1 // invalidate I cache line // errata
   #endif
#endif
.endm

.macro DEJITCLEANMID
   #if defined(RPI4)|| defined(RPI3) || defined(RPI2)
      MCR p15, 0, reg12, c7, c5, 7 // invalidate BTB entry
   #endif
.endm

.macro DEBUG_REG reg
   push {r0-r12,r14}
   mrs     regSP, CPSR               // Save 6502 flags
   mov r0,\reg
   BL dump_hex
   msr     CPSR_flg, regSP
   pop {r0-r12,r14}
.endm

.macro DEBUG_CHAR char
   push {r0-r12,r14}
   mrs     regSP, CPSR               // Save 6502 flags
   mov r0,#\char
   BL RPI_AuxMiniUartWrite
   msr     CPSR_flg, regSP
   pop {r0-r12,r14}
.endm

.macro COPY1 start
   ldr r7,\start
   str r7,[r6]
.endm

.macro COPY2 start
   ldr r8,\start
   ldr r9,4+\start
   strd r8,r9,[r6]
.endm

.macro COPY3 start
   adr temp1,\start
   LDMIA temp1,{r8,r9,r10}
   STMIA r6,{r8,r9,10}
   //strd r8,r9,[r6]
   //str r10,[r6,#8]
.endm

.macro COPY3LOAD start
   adr temp1,\start
   LDMIA temp1,{r8,r9,r10}
.endm
.macro COPY3STORE
   STMIA r6,{r8,r9,r10}
   //strd r8,r9,[r6]
   //str r10,[r6,#8]
.endm
.macro COPY3STOREINC
   STMIA r6!,{r8,r9,r10}
   //strd r8,r9,[r6]
   //str r10,[r6,#8]
.endm
.macro COPY4 start
   adr temp1,\start
   LDMIA temp1,{r8,r9,r10,r11}
   stmia r6,{r8,r9,r10,r11}
   //strd r8,r9,[r6]
   //strd r10,r11,[r6,#8]
.endm
.macro COPY4LOAD start
   adr temp1,\start
   LDMIA temp1,{r8,r9,r10,r11}
.endm
.macro COPY4STORE
   stmia r6,{r8,r9,r10,r11}
   //strd r8,r9,[r6]
   //strd r10,r11,[r6,#8]
.endm
.macro COPY5 start
   adr temp1,\start
   LDMIA temp1,{r7,r8,r9,r10,r11}
   STMIA r6,{r7,r8,r9,r10,r11}
.endm
.macro COPY5LOAD start
   adr temp1,\start
   LDMIA temp1,{r7,r8,r9,r10,r11}
.endm
.macro COPY5STORE
   STMIA r6,{r7,r8,r9,r10,r11}
.endm
.macro COPY5STOREINC
   STMIA r6!,{r7,r8,r9,r10,r11}
.endm
.macro COPY6 start
   adr temp1,\start
   LDMIA temp1,{r7,r8,r9,r10,r11,r12}
   STMIA r6,{r7,r8,r9,r10,r11,r12}
.endm
.macro COPY6LOAD start
   adr temp1,\start
   LDMIA temp1,{r7,r8,r9,r10,r11,r12}
.endm
.macro COPY6STORE
   STMIA r6,{r7,r8,r9,r10,r11,r12}
.endm

.macro MAKEBRANCHLINK reg
   mov \reg,\reg,LSR #2
   sub \reg,\reg,r6,LSR #2
   ORR \reg,\reg,#BLINSTRUCTION
.endm

// Macro for pushing a byte to the stack, whilst correctly handing wrapping
// This relies on the 6502 memory being 256 byte aligned
.macro SPUSH reg
   strb    \reg, [regSP], #-1
   orr     regSP, #0x0100
.endm

.macro NOP6
   ldr r8,B_NOP
   mov r9,r8
   B jitend1bytestore2
.endm

.macro NOP6_1
   mov temp,#BINSTRUCTION
   ORR temp,temp,#(NEXTJITLET>>2) // B $+8
   str temp,[r6]
   FILLINJITTEDTABLES dejit16bit1
   add temp2,temp2,#2
   B dojitloop
.endm

.macro NOP6_2
   mov temp,#BINSTRUCTION
   ORR temp,temp,#(NEXTJITLET>>1) // B $+10
   str temp,[r6]
   FILLINJITTEDTABLES dejit16bit1
   add temp2,temp2,#3
   B dojitloop
.endm

.macro JITINDXLOGICAL inst
   ADDR1REGXZP
   mov reg1,reg1,LSR #24
   .word ldrhldrsb-16
   \inst regA,regA,reg0
.endm

.macro JITINDYLOGICAL inst
   ldrh reg0,[ram6502,#0x00]
   mov reg1,regY,LSR #24
   ldrsb reg0,[reg0,reg1]
   \inst regA,regA,reg0
.endm

.macro JITINDLOGICAL inst
   ldrh reg0,[ram6502,#0x00]
   nop
   ldrsb reg0,[reg0]
   \inst regA,regA,reg0
.endm

.macro JITZPLOGICAL inst
   ldrsb reg0,[ram6502,#0x00]
   nop
   nop
   \inst regA,regA,reg0
.endm

.macro JITZPXLOGICAL inst
   ADDR1REGXZP
   mov reg1,reg1,LSR#24
   ldrsb reg0,[reg1]
   \inst regA,regA,reg0
.endm

.macro IMMLOAD block
   ldr r7,=immediatesignextenedtable
   adr reg1,\block
   LDMIA reg1,{r8,r9}
   ldr r7,[r7,r4,LSL #2]
   strd  r8,r9,[r6,#8]
   STMIA r6,{r7,r8}
   B jitend2byte
.endm

.macro JITIMMLOGICAL inst
   nop
   \inst regA,regA,reg0
.endm

.macro JITABSLOGICAL inst
   JITABSLDRSB
   nop
   nop
   nop
   \inst regA,regA,reg0
.endm

.macro JITABSYLOGICAL inst
   JITABS
   mov reg0,regY,LSR #24
   ldrsb reg0,[reg1,reg0] // can save a cycle by reordering the above
   nop
   \inst regA,regA,reg0
.endm

.macro JITABSXLOGICAL inst
   JITABS
   mov reg0,regX, LSR #24
   ldrsb reg0,[reg1,reg0] // can save a cycle by reordering the above
   nop
   \inst regA,regA,reg0
.endm

.macro JITRMB bitnum
   ldrb reg0,[ram6502,#00]
   nop
   bic  reg0, #(1 << \bitnum)
   strb reg0,[ram6502,#00]
.endm

.macro JITSMB bitnum
   ldrb reg0,[ram6502,#00]
   nop
   orr  reg0, #(1 << \bitnum)
   strb reg0,[ram6502,#00]
.endm

.macro BBRS block blout
   adr r1,\block

   // fill in BL
   B  jitend3byteexitBBRS
.endm

.macro JITBBR bitnum
   ldrb    reg0,[ram6502,#0x00]
   mrs     reg1, CPSR
   tst     reg0, #(1 << \bitnum)
   .word bbrreturn-20

   //   addne pc,r14,#4 // return to jit
   //   msr     CPSR_flg, reg1
   //   mov pc,r14

   B .+8  // taken
   msr     CPSR_flg, reg1
.endm

.macro JITBBS bitnum
   ldrb    reg0,[ram6502,#0x00]
   mrs     reg1, CPSR
   tst     reg0, #(1 << \bitnum)
   .word bbsreturn-20

   //   addne pc,r14,#4 // return to jit
   //   msr     CPSR_flg, reg1
   //   mov pc,r14

   B .+8  // taken
   msr     CPSR_flg, reg1
.endm

.macro BRANCH type
   sxtb r4,r4
   ldr temp1,B_NOP
// do jump maths
// temp0 is where we need to goto NB -2 ie. if we have 6 we goto +8
// so it look like arm and 6502 jumps are the same so if we load the bit signed
// we just need to clear the top byte
   mov temp0,r4,LSL #1
   add temp0,temp0,#2
   BIC temp0,temp0,#0xFF000000
   orr temp0,temp0,#\type
   b branchsavewords
.endm

.macro BRANCHVCS block branch
// Make BL testvflagC
//      msr CPSR_flg, temp1
//      B xx // branch taken
//      msr CPSR_flg,temp1

   ldr r0,=\branch-8
   LDR r9,\block
   sxtb r4,r4
   // fill in BL
   BL createbranchlinkm8

   mov temp0,r4,LSL #1
   str r9,[R6,#4]
   BIC temp0,temp0,#0xFF000000
   str r9,[R6,#12]
   orr temp0,temp0,#BINSTRUCTION
   str temp0,[r6,#8]
   B jitend2byteexit
.endm

.macro JITBRANCHVCS
   msr     CPSR_flg, reg1
.endm

.macro SINGLEOPCODEBLOUT block
   ldrd r8,\block
   B singleopcodeBLend
.endm

.macro SINGLEOPCODEEXIT block
   ldrd r8,\block
   sub r9,r9,r6
   mov r9,r9,LSR #2
   ORR r9,r9,#BINSTRUCTION
   B singleopcodeexitstore
.endm

.macro SINGLEOPCODEBLEXIT block
   ldrd r8,\block
   B singleopcodeexit
.endm

.macro SINGLEBYTE block
   ldrd r8,\block
   B jitend1bytestore2
.endm

.macro DOUBLEBYTESETFIRSTBYTE block
   adr temp1,\block
   B doublebyteend
.endm

// BL at third instruction -16
.macro DOUBLEBYTESETFIRSTBYTEBLOUT8 block
   adr temp1,\block
   B jitend2byteBL8
.endm

// last word is the address of the branch-20
.macro DOUBLEBYTESETFIRSTBYTEBLOUTQ block
   adr temp1,\block
   B jitend2byteBLend
.endm

.macro DOUBLEBYTEBYTET2BLOUT block
   adr temp1,\block
   B jitendT2byte
.endm

.macro DOUBLEBYTET2 block
   adr temp1,\block
   B fixuptype2accessload4
.endm

.macro DOUBLEBYTESETTWOBYTES block offset
   COPY4LOAD \block
   orr r8,r8,r4
.ifc \offset,8
   orr r10,r10,r4
.endif

.ifc \offset,12
   orr r11,r11,r4
.endif
   B jitend2bytestore4
.endm

.macro DOUBLEBYTESETTWOBYTES12 block offset
   adr temp1,\block
   B doublebyteendsettwobytes12
.endm

.macro TRIPLEBYTEABS block
   adr temp1,\block
   B  jitend3byteloadstoreoperand
.endm

.macro TRIPLEBYTEABST2 block
   adr temp1,\block
   BL fixuptype2accessquick
   b jitend3bytestore
.endm

.macro TRIPLEBYTEABSBLOUT20 block
   adr temp1,\block
   B  jitend3byteloadstoreoperandBL
.endm

.macro TRIPLEBYTEABSBLOUT block address offset
.ifc \offset,8
   adr temp1,\block
   B  jitend3byteloadstoreoperandBL8
.else
   COPY6LOAD \block
   orr r7,r7,r5
   orr r8,r8,r4
   MAKEBRANCHLINK r10
   B jitend3bytestore
.endif

.endm

.macro ABSSTORE block
   COPY5LOAD \block
   mov r12,r12, LSL #2
   add r12,r12,#JITTEDTABLE16
   orr r7,r7,r5
   orr r11,r11,r4
   sub r12,r12,#28
   B  jitend3bytestoreoperandBL
.endm

.macro JITABSSTORE reg
   MOVR1HIGH
   nop
.ifc \reg,regA
   nop
   nop
   strb regA,[reg1,#0]!
.else
   mov reg0,\reg,LSR #24
   nop
   strb reg0,[reg1,#0]!
.endif
.endm

.macro ASSIGNCARRY carry
   ldrd r8,Bopc_carry
   sub r8,r8,r6
   mov r8,r8,LSR #2
.ifc \carry,clear
   orr r8,r8,#ARMBLCS
.else
   orr r8,r8,#ARMBLCC
.endif
   B jitend1bytestore2
.endm

.macro JITABS reg=reg1
.ifc \reg,reg1
   MOVR1HIGH
.else
   MOVR0HIGH
.endif
   orr \reg,\reg,#0x00  // NB armv8 faster option
.endm

.macro JITABSLDRB
   MOVR1HIGH
   ldrb reg12,[reg1,#0x00]
.endm

.macro JITABSLDRBWB
   MOVR1HIGH
   ldrb reg12,[reg1,#0x00]!
.endm

.macro JITABSLDRSB
   MOVR1HIGH
   ldrsb reg0,[reg1,#0x00]
.endm

.macro ZPSTAZ block
   COPY3LOAD \block
   mov r11,r4,LSL #2
   add r11,r11,#JITTEDTABLE16
   orr r8,r8,r4
   orr r9,r9,r4
   sub r11,r11,#20
   b jitend2byteBLendstore
.endm

.macro JITZPSTAZ reg
   strb  \reg,[ram6502,#0x00]
   mov reg1,#00
   nop
.endm

.macro ZPST block
   COPY3LOAD \block
   mov r11,r4,LSL #2
   add r11,r11,#JITTEDTABLE16
   orr r9,r9,r4
   orr r10,r10,r4
   sub r11,r11,#20
   b jitend2byteBLendstore
.endm

.macro JITZPST reg
   mov reg0,\reg,LSR #24
   mov reg1,#00
   strb reg0,[ram6502,#0x00]
.endm

.macro JITABSXYST reg index
   JITABS
   strb \reg,[reg1,\index,LSR#24]!
   nop
   add reg0,jittedtable16ptr,reg1,LSL#2
   blx reg0
.endm

.macro ASL6
   add   reg12, reg12, reg12
   strb  reg12,[reg1]
   movs  reg12, reg12, lsl #24
.endm

.macro ROL6
   adc  reg12, reg12, reg12
   strb reg12,[reg1]
   lsls    reg12, reg12, #24
.endm

.macro LSR6
   lsrs reg12, reg12, #1
   strb reg12,[reg1]
.endm

.macro ROR6
   orrcs   reg12, reg12, #0x100
   rrxs    reg12, reg12
   strb reg12,[reg1]
.endm

.macro INTR vector setbrk  fake     // vector is -2 for IRQ/BRK or -6 for NMI
                                // setbrk should be 1 for BRK, 0 for IRQ/NMI
   sub     regSP, regSP, #2
   mov     temp, #0x10000
.ifc \fake, FAKE
   mov temp1,#0xFF
   orr temp1,#0xFF<<8
.else
   // else return address is presetup in temp1
.endif
   strh    temp1, [regSP,#1]         // slight risk of over running the stack
   orr     regSP, #0x0100
   ldrh    temp0, [temp, #\vector]     // Fetch the vector address

.if \setbrk
   orr     temp, flags, #X_FLAG6502+B_FLAG6502 // Always set bit 5 (-) in the pushed value and Set the B bit in the pushed value (in BRK, not in IRQ/NMI)
.else
   orr     temp, flags, #X_FLAG6502            // Always set bit 5 (-) in the pushed value
.endif
   orrmi   temp, temp, #N_FLAG6502       // set N_FLAG NB bit defaults to zero
   orrcs   temp, temp, #C_FLAG6502       // set C_FLAG NB bit defaults to zero
   orreq   temp, temp, #Z_FLAG6502       // set Z_FLAG NB bit defaults to zero
   SPUSH   temp
   orr     flags, flags, #I_FLAG6502 // Set the I bit in the 6502 status register
   bic     flags, flags, #D_FLAG6502 // Clear the D bit in the 6502 status register
   mov temp0,temp0,LSL#JITLETSHIFT
   add temp0,temp0,#JITLET
// In the case of a "fake" interrupt, we need to explicitly re-enable FIQ
// interrupts, because the FIQ handler is not going to exit until the 6502
// RTI is executed. We leave IRQ masked at this point to avoid the possibility
// of a re-entrant IRQ, which IRQ handler should not have to deal with.
.ifc \fake, FAKE
   CPSIE   f  // enable ARM FIQ interrupts only
.endif
.if \setbrk
   CPSIE   if  // enable both ARM IRQ and FIQ interrupts
.endif
   bx temp0
.endm

.macro  PLP
   uadd8 reg2,regSP,reg12
   ldrb  reg2, [reg2]
   uadd8 regSP,regSP,reg12

   and  reg12, flags, #0xffffff00
   orr  reg12, reg12, reg2        // preserve the other bits in flags, e.g. the slowdown bit
   bic  flags, reg12, #N_FLAG6502+X_FLAG6502+B_FLAG6502+Z_FLAG6502+C_FLAG6502
   and  reg12, reg2, #N_FLAG6502
   and  reg2, reg2, #Z_FLAG6502+C_FLAG6502
   mov  reg12, reg12, lsl #24
   orr  reg12, reg12, reg2, lsl #29
   //   Need to update the ARM flags later with  msr     CPSR_flg, reg12
   // reg1 return address
   // reg12 has 6502 flags to be restored to the ARM
.endm

// **********************************************
// Start of opcode implementations
// **********************************************

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
   NOP6

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
   NOP6_1

opc_5C: // Opcode 5C - NOP6 $0000
opc_DC: // Opcode DC - NOP6 $0000
opc_FC: // Opcode FC - NOP6 $0000
   NOP6_2
opc_10: BRANCH BPLINSTRUCTION // Opcode 10 - BPL

opc_30: BRANCH BMIINSTRUCTION // Opcode 30 - BMI

opc_80: BRANCH BINSTRUCTION   // Opcode 80 - BRA

opc_90: BRANCH BCCINSTRUCTION // Opcode 90 - BCC

opc_B0: BRANCH BCSINSTRUCTION // Opcode B0 - BCS

opc_D0: BRANCH BNEINSTRUCTION // Opcode D0 - BNE

opc_F0: BRANCH BEQINSTRUCTION // Opcode F0 - BEQ

.ltorg
.balign 8
opc_08: SINGLEOPCODEBLOUT Bopc_08 // Opcode 08 - PHP

opc_0A: SINGLEBYTE Bopc_0A        // Opcode 0A - ASL A

opc_1A: SINGLEOPCODEBLOUT Bopc_1A // Opcode 1A - INC A

opc_28: SINGLEOPCODEBLOUT Bopc_28 // Opcode 28 - PLP

opc_2A: SINGLEOPCODEBLOUT Bopc_2A // Opcode 2A - ROL A

opc_3A: SINGLEOPCODEBLOUT Bopc_3A // Opcode 3A - DEC A

opc_48: SINGLEBYTE Bopc_48        // Opcode 48 - PHA

opc_4A: SINGLEBYTE Bopc_4A        // Opcode 4A - LSR A

opc_58: SINGLEOPCODEBLOUT Bopc_58 // Opcode 58 - CLI

opc_5A: SINGLEOPCODEBLOUT Bopc_5A // Opcode 5A - PHY

opc_68: SINGLEOPCODEBLOUT Bopc_68 // Opcode 68 - PLA

opc_6A: SINGLEOPCODEBLOUT Bopc_6A // Opcode 6A - ROR A

opc_78: SINGLEBYTE Bopc_78        // Opcode 78 - SEI

opc_7A: SINGLEOPCODEBLOUT Bopc_7A // Opcode 7A - PLY

opc_88: SINGLEBYTE Bopc_88        // Opcode 88 - DEY

opc_8A: SINGLEBYTE Bopc_8A        // Opcode 8A - TXA

opc_98: SINGLEBYTE Bopc_98        // Opcode 98 - TYA

opc_9A: SINGLEBYTE Bopc_9A        // Opcode 9A - TXS

opc_A8: SINGLEBYTE Bopc_A8        // Opcode A8 - TAY

opc_AA: SINGLEBYTE Bopc_AA        // Opcode AA - TAX

opc_B8: SINGLEBYTE Bopc_B8        // Opcode B8 - CLV

opc_BA: SINGLEBYTE Bopc_BA        // Opcode BA - TSX

opc_C8: SINGLEBYTE Bopc_C8        // Opcode C8 - INY

opc_CA: SINGLEBYTE Bopc_CA        // Opcode CA - DEX

opc_D8: SINGLEBYTE Bopc_D8        // Opcode D8 - CLD

opc_DA: SINGLEOPCODEBLOUT Bopc_DA // Opcode DA - PHX

opc_E3: SINGLEOPCODEBLOUT Bopc_E3 // Opcode E3 - NOP6

opc_E8: SINGLEBYTE Bopc_E8        // Opcode E8 - INX

opc_EB: SINGLEBYTE Bopc_EB        // Opcode EB - NOP6 ( Used for instruction timing)

opc_F3: SINGLEOPCODEBLOUT Bopc_F3 // Opcode F3 - NOP6

opc_F8: SINGLEBYTE Bopc_F8        // Opcode F8 - SED

opc_FA: SINGLEOPCODEBLOUT Bopc_FA // Opcode FA - PLX

.balign 8
Bopc_08:
   orr      reg12, flags, #X_FLAG6502+B_FLAG6502
   .word    php-12
Bopc_0A:
   lsls     regA, regA, #25
   sxtb     regA, regA , ror #24
Bopc_1A:
   add      regA, regA, #0x1
   .word    aflags-12
Bopc_28:
   mov      reg12,#1
   .word    plp-12
Bopc_2A:
   adc      regA, regA, regA
   .word    rola-12
Bopc_3A:
   sub      regA, regA, #0x1
   .word    aflags-12
Bopc_48:
   SPUSH regA
Bopc_4A:
   lsrs     regA, regA, #1
   and      regA, regA, #0x7F
Bopc_58:
   bic      flags, flags, #I_FLAG6502
   .word    cli-12
Bopc_5A:
   mov      reg12, regY,LSR  #24
   .word    pushtemp-12
Bopc_68:
   mov      reg1,#1
   .word    pla-12
Bopc_6A:
   and      regA, regA, #0xFF    // isolate lower byte
   .word    rora-12
Bopc_78:
   orr      flags, flags, #I_FLAG6502
   nop
Bopc_7A:
   mov      reg1,#1
   .word    ply-12
Bopc_88:
   sub      regY, regY, #0x1000000
   teq      regY, #0
Bopc_8A:
   sxtb     regA, regX, ror #24
   TEQ      regA, #0
Bopc_98:
   sxtb     regA, regY, ror #24
   TEQ      regA, #0
Bopc_9A:
   bic      regSP, regSP, #0xFF             // clear lower nibble
   orr      regSP, regSP, regX, lsr #24     // put X in lower nibble
Bopc_A8:
   lsl      regY, regA, #24
   TEQ      regA, #0
Bopc_AA:
   lsl      regX, regA, #24
   TEQ      regA, #0
Bopc_B8:
   nop
   bic      flags, #V_FLAG6502
Bopc_BA:
   lsl      regX, regSP, #24
   teq      regX, #0
Bopc_C8:
   add      regY, regY, #0x1000000
   teq      regY, #0
Bopc_CA:
   sub      regX, regX, #0x1000000
   teq      regX, #0
Bopc_D8:
   nop
   bic      flags, flags, #D_FLAG6502
Bopc_DA:
   mov      reg12, regX, LSR #24
   .word    pushtemp-12
Bopc_E3:
   mov      reg1,#1
   .word    debugopcode-12
Bopc_E8:
   add      regX, regX, #0x1000000
   teq      regX, #0
Bopc_EB:
#if defined(TIMING_INSTRUCTION)
#if defined(RPI2)|| defined(RPI3) || defined(RPI4)
   mrc     p15, 0, reg0, c9, c13, 0
#else
   MRC     p15, 0, reg0, c15, c12, 1    // Read Cycle Counter Register
#endif
   str     reg0,[ram6502,#0xE0]
#else
   nop
   nop
#endif
Bopc_F3:
   nop
   .word putcharopcode-12
Bopc_F8:
   nop
   orr      flags, flags, #D_FLAG6502
Bopc_FA:
   mov      reg1,#1
   .word    plx-12
Bopc_FB:
#if defined(TIMING_INSTRUCTION)
#if defined(RPI2) || defined(RPI3) || defined(RPI4)
   mrc     p15, 0, reg0, c9, c13, 0
#else
   MRC     p15, 0, reg0, c15, c12, 1    // Read Cycle Counter Register
#endif
   str     reg0,[ram6502,#0xE4]
#else
   nop
   nop
#endif

Bopc_carry:
   .word invertcarry-8
   nop
Bopc_00:
   nop
   .word brk-12
Bopc_40:
   mov   reg12,#1
   .word rti-12
Bopc_60:
   ldrh reg0,[regSP,#1]
   .word rts-12
Bopc_01:
   JITINDXLOGICAL orrs
Bopc_04:
   mov reg1,#0x00
   ldrb reg12,[reg1]
   mrs reg0, CPSR
   .word tsb-20
Bopc_05:
   JITZPLOGICAL orrs
Bopc_06:
   ldrb reg12,[ram6502,#00]
   add   reg12, reg12, reg12
   strb  reg12,[ram6502,#00]
   movs  reg12, reg12, lsl #24
Bopc_07:
   JITRMB 0
Bopc_09:
   JITIMMLOGICAL orrs
Bopc_0C:
   JITABSLDRBWB
   mrs reg0, CPSR
   bic reg0, reg0, #Z_FLAG
   tst reg12, regA
   .word tsb2-28
Bopc_0D:
   JITABSLOGICAL orrs
Bopc_0E:
   JITABSLDRBWB
   nop
   ASL6

opc_FB: // Opcode FB - NOP6
   SINGLEBYTE Bopc_FB
opc_18: // Opcode 18 - CLC
   ASSIGNCARRY clear
opc_38: // Opcode 38 - SEC
   ASSIGNCARRY set
opc_00: // Opcode 00 - BRK is really a two byte instruction
   SINGLEOPCODEBLEXIT Bopc_00
opc_40: // Opcode 40 - RTI
   SINGLEOPCODEBLEXIT Bopc_40
opc_60: // Opcode 60 - RTS
   SINGLEOPCODEEXIT Bopc_60
opc_01: // Opcode 01 - ORA ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUT8 Bopc_01
opc_04: // Opcode 04 - TSB $00
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_04
opc_05: // Opcode 05 - ORA $00
   DOUBLEBYTET2 Bopc_05
opc_06: // Opcode 06 - ASL $00
   DOUBLEBYTESETTWOBYTES Bopc_06 8
opc_07: // Opcode 07 - RMB0 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_07 12
opc_09: // Opcode 09 - ORA #$00
   IMMLOAD Bopc_09
opc_0C: // Opcode 0C - TSB $0000
   TRIPLEBYTEABSBLOUT20 Bopc_0C
opc_0D: // Opcode 0D - ORA $0000
   TRIPLEBYTEABST2 Bopc_0D
opc_0E: // Opcode 0E - ASL $0000
   TRIPLEBYTEABS Bopc_0E
opc_0F: // Opcode 0F - BBR0
   BBRS Bopc_0F bbrreturn

opc_11: // Opcode 11 - ORA ($00),Y
   DOUBLEBYTET2 Bopc_11

opc_12: // Opcode 12 - ORA ($00)
   DOUBLEBYTET2 Bopc_12

opc_14: // Opcode 14 - TRB $00
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_14

opc_15: // Opcode 15 - ORA $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_15

opc_16: // Opcode 16 - ASL $00, X
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_16

opc_17: // Opcode 17 - RMB1 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_17 12

opc_19: // Opcode 19 - ORA $0000,Y
   TRIPLEBYTEABS Bopc_19

opc_1C: // Opcode 1C - TRB $0000
   TRIPLEBYTEABSBLOUT20 Bopc_1C

opc_1D: // Opcode 1D - ORA $0000,X
   TRIPLEBYTEABS Bopc_1D

opc_1E: // Opcode 1E - ASL $0000,X
   TRIPLEBYTEABS Bopc_1E

opc_1F: // Opcode 1F - BBR1
   BBRS Bopc_1F bbrreturn

opc_20:// Opcode 20 - JSR $0000

   ldrh temp,[temp2,#1]
   COPY5LOAD Bopc_20
   add temp0,temp2,#2
   and temp1,temp0,#0xFF
   orr r7,r7,temp0,LSR #8
   orr r8,r8,temp1
   COPY5STOREINC
   B createbranchtojitlet

opc_21: // Opcode 21 - AND ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUT8 Bopc_21

opc_24: // Opcode 24 - BIT $00
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_24

opc_25: // Opcode 25 - AND $00
   DOUBLEBYTET2 Bopc_25

opc_26: // Opcode 26 - ROL $00
   DOUBLEBYTESETTWOBYTES Bopc_26 8

opc_27: // Opcode 27 - RMB2 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_27 12

opc_29: // Opcode 29 - AND #$00
   IMMLOAD Bopc_29

opc_2C: // Opcode 2C - BIT $0000
   ldrh temp0,[temp2,#1]

   mov temp,#(0xFEE0>>5) & 0xFF
   orr temp,temp, #(0xFEE0>>5) & 0xFF00
   cmp temp, temp0, lsr #5  //  constant 0xFEE0 >> 5
   BEQ bitabsio

   ldr r7,Bopc_2C
   ldr temp0,=bitabstable-8
   orr r7,r7,r5
   str r7,[r6],#4
   add temp0,temp0,r4,LSL #3
   BL createbranchlinkm8
   FILLINJITTEDTABLES dejit16bit1
   add temp2,temp2,#3
   B dojitloop

bitabsio:
   ldr r7,Bopc_2C_io
   ldr temp0,=bitioload-8
   orr r7,r7,r4
   str r7,[r6],#4
   BL createbranchlinkm8
   FILLINJITTEDTABLES dejit16bit1
   add temp2,temp2,#3
   B dojitloop

opc_2D: // Opcode 2D - AND $0000
   TRIPLEBYTEABST2 Bopc_2D

opc_2E: // Opcode 2E - ROL $0000
   TRIPLEBYTEABS Bopc_2E

opc_2F: // Opcode 2F - BBR2
   BBRS Bopc_2F bbrreturn
opc_31: // Opcode 31 - AND ($00),Y
   DOUBLEBYTET2 Bopc_31

opc_32: // Opcode 32 - AND ($00)
   DOUBLEBYTET2 Bopc_32

opc_34: // Opcode 34 - BIT $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_34

opc_35: // Opcode 35 - AND $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_35

opc_36: // Opcode 36 - ROL $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_36

opc_37: // Opcode 37 - RMB3 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_37 12

opc_39: // Opcode 39 - AND $0000,Y
   TRIPLEBYTEABS Bopc_39

opc_3C: // Opcode 3C - BIT $0000,X
   TRIPLEBYTEABSBLOUT20 Bopc_3C

opc_3D: // Opcode 3D - AND $0000,X
   TRIPLEBYTEABS Bopc_3D

opc_3E: // Opcode 3E - ROL $0000,X
   TRIPLEBYTEABS Bopc_3E

opc_3F: // Opcode 3F - BBR3
   BBRS Bopc_3F bbrreturn

opc_41: // Opcode 41 - EOR ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUT8 Bopc_41

opc_45: // Opcode 45 - EOR $00
   DOUBLEBYTET2 Bopc_45

opc_46: // Opcode 46 - LSR $00
   DOUBLEBYTESETTWOBYTES12 Bopc_46 12

opc_47: // Opcode 47 - RMB4 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_47 12

opc_49: // Opcode 49 - EOR #$00
   IMMLOAD Bopc_49

opc_4C: // Opcode 4C - JMP $0000
   ldrh r12,[temp2,#1]

   B createbranchtojitlet

opc_4D: // Opcode 4D - EOR $0000
   TRIPLEBYTEABST2 Bopc_4D

opc_4E: // Opcode 4E - LSR $0000
   TRIPLEBYTEABS Bopc_4E

opc_4F: // Opcode 4F - BBR4
   BBRS Bopc_4F bbrreturn

opc_50: // Opcode 50 - BVC
   BRANCHVCS Bopc_50 testvflagC

opc_51: // Opcode 51 - EOR ($00),Y
   DOUBLEBYTET2 Bopc_51

opc_52: // Opcode 52 - EOR ($00)
   DOUBLEBYTET2 Bopc_52

opc_55: // Opcode 55 - EOR $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_55

opc_56: // Opcode 56 - LSR $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_56

opc_57: // Opcode 57 - RMB5 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_57 12

opc_59: // Opcode 59 - EOR $0000,Y
   TRIPLEBYTEABS Bopc_59

opc_5D: // Opcode 5D - EOR $0000,X
   TRIPLEBYTEABS Bopc_5D

opc_5E: // Opcode 5E - LSR $0000,X
   TRIPLEBYTEABS Bopc_5E

opc_5F: // Opcode 5F - BBR5
   BBRS Bopc_5F bbrreturn
opc_61: // Opcode 61 - ADC ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_61
opc_64: // Opcode 64 - STZ $00
   ZPSTAZ Bopc_64

opc_65: // Opcode 65 - ADC $00
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_65
Bopc_0F:
   JITBBR 0
Bopc_11:
   JITINDYLOGICAL orrs
Bopc_12:
   JITINDLOGICAL orrs
Bopc_14:
   mov reg1,#0x00
   ldrb reg12,[reg1]
   mrs reg0, CPSR
   .word trb-20
Bopc_15:
   JITZPXLOGICAL orrs
Bopc_16:
   ADDR1REGXZP
   ldrb reg12,[ram6502,reg1,LSR #24]
   mov reg1,reg1,LSR #24  // can save a cycle here
   .word asl-20
Bopc_17:
   JITRMB 1
Bopc_19:
   JITABSYLOGICAL orrs
Bopc_1C:
   JITABSLDRBWB
   mrs reg0, CPSR
   bic reg0, reg0, #Z_FLAG
   tst reg12, regA
   .word trb2-28
Bopc_1D:
   JITABSXLOGICAL orrs
Bopc_1E:
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]!
   ASL6
Bopc_1F:
   JITBBR 1

Bopc_20:
   JITABS
   sub  regSP, regSP, #2
   strh reg1,[regSP,#1]
   orr  regSP, #0x0100
Bopc_21:
   JITINDXLOGICAL ands
Bopc_24:
   ldrb reg12,[ram6502,#0x00]
   bic     flags, flags, #V_FLAG6502
   tst     reg12, regA
   .word bit2-20
Bopc_25:
   JITZPLOGICAL ands
Bopc_26:
   ldrb reg12,[ram6502,#00]
   adc   reg12, reg12, reg12
   strb  reg12,[ram6502,#00]
   movs  reg12, reg12, lsl #24
Bopc_27:
   JITRMB 2

Bopc_29:
   JITIMMLOGICAL ands

Bopc_2C:
   MOVR1HIGH
Bopc_2C_io:
   mov r0,#0x00
Bopc_2D:
   JITABSLOGICAL ands
Bopc_2E:
   JITABSLDRBWB
   nop
   ROL6
Bopc_2F:
   JITBBR 2
Bopc_31:
   JITINDYLOGICAL ands
Bopc_32:
   JITINDLOGICAL ands
Bopc_34:
   ADDR1REGXZP
   ldrb reg12,[ram6502,reg1,LSR #24]
   bic  flags, flags, #V_FLAG6502
   .word bit-20
Bopc_35:
   JITZPXLOGICAL ands
Bopc_36:
   ADDR1REGXZP
   mov reg1,reg1,LSR #24 // can save a cycle here
   ldrb reg12,[reg1]
   .word rol-20
Bopc_37:
   JITRMB 3
Bopc_39:
   JITABSYLOGICAL ands

Bopc_3C:
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]
   bic     flags, flags, #V_FLAG6502
   tst     reg12, regA
   .word bit2-28
Bopc_3D:
   JITABSXLOGICAL ands
Bopc_3E:
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]!
   ROL6
Bopc_3F:
   JITBBR 3

Bopc_41:
   JITINDXLOGICAL eors
Bopc_45:
   JITZPLOGICAL eors
Bopc_46:
   ldrb reg0,[ram6502,#00]
   nop
   lsrs reg0, reg0, #1
   strb  reg0,[ram6502,#00]
Bopc_47:
   JITRMB 4

Bopc_49:
   JITIMMLOGICAL eors

Bopc_4D:
   JITABSLOGICAL eors
Bopc_4E:
   JITABSLDRBWB
B_NOP:
   nop
   nop
   LSR6
Bopc_4F:
   JITBBR 4
Bopc_50:
Bopc_70:
   JITBRANCHVCS
Bopc_51:
   JITINDYLOGICAL eors
Bopc_52:
   JITINDLOGICAL eors
Bopc_55:
   JITZPXLOGICAL eors
Bopc_56:
   ADDR1REGXZP
   ldrb reg0,[ram6502,reg1,LSR #24]
   lsrs reg0, reg0, #1
   strb reg0,[ram6502,reg1,LSR #24]
Bopc_57:
   JITRMB 5

Bopc_59:
   JITABSYLOGICAL eors

Bopc_5D:
   JITABSXLOGICAL eors
Bopc_5E:
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]!
   nop
   LSR6
Bopc_5F:
   JITBBR 5
Bopc_61:
   ADDR1REGXZP
   mov reg1,reg1,LSR #24
   ldrh reg1,[reg1]
   .word ADCload-20
Bopc_64:
   JITZPSTAZ ram6502
Bopc_65:
   ldrb    reg12,[ram6502,#0x00]
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
   .word ADC2-20
Bopc_E5:
   ldrb    reg12,[ram6502,#0x00]
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
   .word SBC2-20
Bopc_66:
   ldrb reg12,[ram6502,#00]
   orrcs   reg12, reg12, #0x100
   rrxs    reg12, reg12
   strb reg12,[ram6502,#00]
Bopc_67:
   JITRMB 6

Bopc_69:
   mov     reg12,#00 // NB could save a cycle later on
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
   .word ADC2-20
Bopc_E9:
   mov     reg12,#00 // NB could save a cycle later on
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
   .word SBC2-20

Bopc_6C:
   MOVR1HIGH
   ldrh reg1,[reg1,#0x00]
   nop
   mov reg0,#JITLET
   add reg0,reg0,reg1,LSL #JITLETSHIFT
   bx reg0
Bopc_6D:
   JITABSLDRB
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
   LSL     regA, regA, #24
   .word ADC3-28
Bopc_ED:
   JITABSLDRB
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
   LSL     regA, regA, #24
   .word SBC3-28
Bopc_6E:
   JITABSLDRBWB
   nop
   ROR6
Bopc_6F:
   JITBBR 6

Bopc_71:
   ldrh    reg1,[ram6502,#0x00]
   tst     flags, #D_FLAG6502
   ldrb    reg12,[reg1,regY,LSR #24]
   .word ADC1-20
Bopc_72:
   ldrh    reg1,[ram6502,#0x00]
   tst     flags, #D_FLAG6502
   ldrb    reg12,[reg1]
   .word ADC1-20
Bopc_74:
   ADDR1REGXZP
   strb ram6502,[ram6502,reg1,LSR #24]
   add reg0,jittedtable16ptr, reg1,LSR #24-2
   BLX reg0

Bopc_75:
   ADDR1REGXZP
   tst     flags, #D_FLAG6502
   ldrb    reg12,[ram6502,reg1,LSR#24]
   .word ADC1-20
Bopc_76:
   ADDR1REGXZP
   mov reg1,reg1,LSR #24 // can save a cycle here
   ldrb reg12,[reg1]
   .word ror-20
Bopc_77:
   JITRMB 7

Bopc_79:
   JITABS
   ldrb    reg12,[reg1, regY, lsr #24]
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
   .word ADC2-28
Bopc_7C:
   JITABS
   mov reg0, regX, LSR #24
   ldrh reg1,[reg0, reg1]
   mov reg0,#JITLET
   add pc,reg0,reg1,LSL #JITLETSHIFT
Bopc_7D:
   JITABS
   ldrb    reg12,[reg1, regX, lsr #24]
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
   .word   ADC2-28
Bopc_7E:
   JITABS
   ldrb  reg12,[reg1, regX, lsr #24]!
   ROR6
Bopc_7F:
   JITBBR 7

Bopc_81:
   ADDR1REGXZP
   mov reg1,reg1,LSR #24
   ldrh reg1,[reg1]
   .word jitstatable-20
Bopc_A1:
   ADDR1REGXZP
   mov reg1,reg1,LSR #24
   ldrh reg1,[reg1]
   .word loada-20
Bopc_C1:
   ADDR1REGXZP
   mov reg1,reg1,LSR #24
   ldrh reg1,[reg1]
   .word cmpa-20
Bopc_E1:
   ADDR1REGXZP
   mov reg1,reg1,LSR #24
   ldrh reg1,[reg1]
   .word SBCload-20
Bopc_84:
   JITZPST regY
Bopc_85:
   JITZPSTAZ regA
Bopc_86:
   JITZPST regX
Bopc_87:
   JITSMB 0

Bopc_89:
   mrs     reg1, CPSR
   tst     regA, #00
   bicne   reg1, reg1, #Z_FLAG
   .word bitimm-20

Bopc_8C:
   JITABSSTORE regY
Bopc_8C_io:
   JITABS r0
   nop
   mov reg1,regY,LSR #24
   nop
   .word iostore-28
Bopc_8D:
   JITABSSTORE regA
Bopc_8D_io:
   JITABS r0
   nop
   and reg1,regA,#0xFF
   nop
   .word iostore-28
Bopc_8E:
   JITABSSTORE regX
Bopc_8E_io:
   JITABS r0
   nop
   mov reg1,regX,LSR #24
   nop
   .word iostore-28
Bopc_8F:
   JITBBS 0
Bopc_91:
   ldrh reg1,[ram6502,#00]
   strb regA,[reg1,regY,LSR #24]!
   add reg0,jittedtable16ptr, reg1,LSL #2
   BLX reg0

Bopc_92:
   ldrh reg1,[ram6502,#00]
   strb regA,[reg1]
   add reg0,jittedtable16ptr, reg1,LSL #2
   BLX reg0

Bopc_94:
   ADDR1REGXZP
   mov reg0,regY,LSR #24
   strb reg0,[ram6502,reg1,LSR #24]
   .word jitstxytable-20
Bopc_95:
   ADDR1REGXZP
   strb regA,[ram6502,reg1,LSR #24]
   add reg0,jittedtable16ptr, reg1,LSR #24-2
   BLX reg0

Bopc_96:
   ADDR1REGYCONST
   mov reg0,regX,LSR #24
   strb reg0,[ram6502,reg1,LSR #24]
   .word jitstxytable-20
Bopc_97:
   JITSMB 1

Bopc_99:
   JITABSXYST regA regY

Bopc_9C:
   JITABSSTORE ram6502
Bopc_9D:
   JITABSXYST regA regX
Bopc_9E:
   JITABSXYST ram6502 regX
Bopc_9F:
   JITBBS 1



opc_E5: // Opcode E5 - SBC $00
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_E5

opc_66: // Opcode 66 - ROR $00
   DOUBLEBYTESETTWOBYTES12 Bopc_66 12

opc_67: // Opcode 67 - RMB6 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_67 12

opc_69: // Opcode 69 - ADC #$00
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_69
opc_E9: // Opcode E9 - SBC #$00
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_E9

opc_6C: // Opcode 6C - JMP ($0000)
   adr   r1,Bopc_6C
   BL fixuptype2accessquick
   b jitend3byteexitstore6

opc_6D: // Opcode 6D - ADC $0000
   TRIPLEBYTEABSBLOUT20 Bopc_6D
opc_ED: // Opcode ED - SBC $0000
   TRIPLEBYTEABSBLOUT20 Bopc_ED

opc_6E: // Opcode 6E - ROR $0000
   TRIPLEBYTEABS Bopc_6E

opc_6F: // Opcode 6F - BBR6
   BBRS Bopc_6F bbrreturn
opc_70: // Opcode 70 - BVS
   BRANCHVCS Bopc_70 testvflagS

opc_71: // Opcode 71 - ADC ($00),Y
   DOUBLEBYTEBYTET2BLOUT Bopc_71
opc_F1: // Opcode F1 - SBC ($00),Y
   DOUBLEBYTEBYTET2BLOUT Bopc_F1

opc_72: // Opcode 72 - ADC ($00)
   DOUBLEBYTEBYTET2BLOUT Bopc_72
opc_F2: // Opcode F2 - SBC ($00)
   DOUBLEBYTEBYTET2BLOUT Bopc_F2

opc_74: // Opcode 74 - STZ $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_74

opc_75: // Opcode 75 - ADC $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_75
opc_76: // Opcode 76 - ROR $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_76
opc_F5: // Opcode F5 - SBC $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_F5

opc_77: // Opcode 77 - RMB7 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_77 12

opc_79: // Opcode 79 - ADC $0000,Y
   TRIPLEBYTEABSBLOUT20 Bopc_79
opc_F9: // Opcode F9 - SBC $0000,Y
   TRIPLEBYTEABSBLOUT20 Bopc_F9

opc_7C: // Opcode 7C - JMP ($0000,X)
   COPY6LOAD Bopc_7C
   orr r7,r7,r5
   orr r8,r8,r4
   B  jitend3byteexitstore6

opc_7D: // Opcode 7D - ADC $0000,X
   TRIPLEBYTEABSBLOUT20 Bopc_7D
opc_FD: // Opcode FD - SBC $0000,X
   TRIPLEBYTEABSBLOUT20 Bopc_FD

opc_7E: // Opcode 7E - ROR $0000,X
   TRIPLEBYTEABS Bopc_7E

opc_7F: // Opcode 7F - BBR7
   BBRS Bopc_7F bbrreturn




Bopc_F1:
   ldrh    reg1,[ram6502,#0x00]
   tst     flags, #D_FLAG6502
   ldrb    reg12,[reg1,regY,LSR #24]
   .word SBC1-20

Bopc_F2:
   ldrh    reg1,[ram6502,#0x00]
   tst     flags, #D_FLAG6502
   ldrb    reg12,[reg1]
   .word SBC1-20

Bopc_F5:
   ADDR1REGXZP
   tst     flags, #D_FLAG6502
   ldrb    reg12,[ram6502,reg1,LSR#24]
   .word SBC1-20

Bopc_F9:
   JITABS
   ldrb    reg12,[reg1, regY, lsr #24]
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
   .word SBC2-28


Bopc_FD:
   JITABS
   ldrb    reg12,[reg1, regX, lsr #24]
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
   .word    SBC2-28

opc_81: // Opcode 81 - STA ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_81
opc_A1: // Opcode A1 - LDA ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_A1
opc_C1: // Opcode C1 - CMP ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_C1
opc_E1: // Opcode E1 - SBC ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_E1

opc_84: // Opcode 84 - STY $00
   ZPST Bopc_84

opc_85: // Opcode 85 - STA $00
   ZPSTAZ Bopc_85

opc_86: // Opcode 86 - STX $00
   ZPST Bopc_86

opc_87: // Opcode 87 - SMB0 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_87 12

opc_89: // Opcode 89 - BIT #$00
   COPY4LOAD Bopc_89
   orr r9,r9,r4
   B jitend2byteBLendstore

opc_8C: // Opcode 8C - STY $0000
   ldrh r12,[temp2,#1]
   mov temp0,#(0xFEE0>>5) & 0xFF
   orr temp0,temp0, #(0xFEE0>>5) & 0xFF00
   cmp temp0, r12, lsr #5  // constant 0xFEE0 >> 5
   BEQ 9f
   ABSSTORE Bopc_8C
9:
   TRIPLEBYTEABSBLOUT20 Bopc_8C_io

opc_8D: // Opcode 8D - STA $0000
   ldrh r12,[temp2,#1]
   mov temp0,#(0xFEE0>>5) & 0xFF
   orr temp0,temp0, #(0xFEE0>>5) & 0xFF00
   cmp temp0, r12, lsr #5  // constant 0xFEE0 >> 5
   BEQ 9f

   ABSSTORE Bopc_8D

9:
   TRIPLEBYTEABSBLOUT20 Bopc_8D_io

opc_8E: // Opcode 8E - STX $0000
   ldrh temp,[temp2,#1]
   mov temp0,#(0xFEE0>>5) & 0xFF
   orr temp0,temp0, #(0xFEE0>>5) & 0xFF00
   cmp temp0, r12, lsr #5  // constant 0xFEE0 >> 5
   BEQ 9f

   ABSSTORE Bopc_8E
9:
   TRIPLEBYTEABSBLOUT20 Bopc_8E_io

opc_8F: // Opcode 8F - BBS0
   BBRS Bopc_8F bbsreturn

opc_91: // Opcode 91 - STA ($00),Y
   DOUBLEBYTET2 Bopc_91

opc_92: // Opcode 92 - STA ($00)
   DOUBLEBYTET2 Bopc_92

opc_94: // Opcode 94 - STY $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_94

opc_95: // Opcode 95 - STA $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_95

opc_96: // Opcode 96 - STX $00,Y
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_96

opc_97: // Opcode 97 - SMB1 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_97 12

opc_99: // Opcode 99 - STA $0000,Y
   TRIPLEBYTEABS Bopc_99

opc_9C: // Opcode 9C - STZ $0000
   ldrh r12,[temp2,#1]
   ABSSTORE Bopc_9C

opc_9D: // Opcode 9D - STA $0000,X
   TRIPLEBYTEABS Bopc_9D

opc_9E: // Opcode 9E - STZ $0000,X
   TRIPLEBYTEABS Bopc_9E

opc_9F: // Opcode 9F - BBS1
   BBRS Bopc_9F bbsreturn

opc_A0: // Opcode A0 - LDY #$00
   DOUBLEBYTESETFIRSTBYTE Bopc_A0

opc_A2: // Opcode A2 - LDX #$00
   DOUBLEBYTESETFIRSTBYTE Bopc_A2

opc_A4: // Opcode A4 - LDY $00
   DOUBLEBYTESETFIRSTBYTE Bopc_A4

opc_A5: // Opcode A5 - LDA $00
   DOUBLEBYTET2 Bopc_A5

opc_A6: // Opcode A6 - LDX $00
   DOUBLEBYTESETFIRSTBYTE Bopc_A6

opc_A7: // Opcode A7 - SMB2 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_A7 12

opc_A9: // Opcode A9 - LDA #$00
   LDR r8,B_NOP
   ldr r11,=immediatesignextenedtable
   str r8,[r6,#4]
   str r8,[r6,#8]
   str r8,[r6,#12]
   ldr temp,[r11,r4,LSL #2]
   orr temp,temp,#1<<20// S FLAG
   orr temp,temp,#0xA000 // regA
   str temp,[r6]
   B jitend2byte

opc_AC: // Opcode AC - LDY $0000
   ldrh temp0,[temp2,#1]

   mov temp,#(0xFEE0>>5) & 0xFF
   orr temp,temp, #(0xFEE0>>5) & 0xFF00
   cmp temp, temp0, lsr #5  // constant 0xFEE0 >> 5
   BEQ 9f

   TRIPLEBYTEABS Bopc_AC

9:
   TRIPLEBYTEABSBLOUT Bopc_AC_io ioload 8

opc_AD: // Opcode AD - LDA $0000
   ldrh temp0,[temp2,#1]

   mov temp,#(0xFEE0>>5) & 0xFF
   orr temp,temp, #(0xFEE0>>5) & 0xFF00
   cmp temp, temp0, lsr #5  // constant 0xFEE0 >> 5
   BEQ loadio
   // ramload
   TRIPLEBYTEABST2 Bopc_AD

loadio:
   TRIPLEBYTEABSBLOUT Bopc_AD_io ioload 8

opc_AE: // Opcode AE - LDX $0000
   ldrh temp0,[temp2,#1]

   mov temp,#(0xFEE0>>5) & 0xFF
   orr temp,temp, #(0xFEE0>>5) & 0xFF00
   cmp temp, temp0, lsr #5  // constant 0xFEE0 >> 5
   BEQ 9f

   TRIPLEBYTEABS Bopc_AE

9:
   TRIPLEBYTEABSBLOUT Bopc_AE_io ioload 8

opc_AF: // Opcode AF - BBS2
   BBRS Bopc_AF bbsreturn

opc_B1: // Opcode B1 - LDA ($00),Y
   DOUBLEBYTET2 Bopc_B1

opc_B2: // Opcode B2 - LDA ($00)
   DOUBLEBYTET2 Bopc_B2

opc_B4: // Opcode B4 - LDY $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_B4

opc_B5: // Opcode B5 - LDA $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_B5

opc_B6: // Opcode B6 - LDX $00,Y
   DOUBLEBYTESETFIRSTBYTE Bopc_B6

opc_B7: // Opcode B7 - SMB3 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_B7 12

opc_B9: // Opcode B9 - LDA $0000,Y
   TRIPLEBYTEABS Bopc_B9

opc_BC: // Opcode BC - LDY $0000,X
   TRIPLEBYTEABS Bopc_BC

opc_BD: // Opcode BD - LDA $0000,X
   TRIPLEBYTEABS Bopc_BD

opc_BE: // Opcode BE - LDX $0000,Y
   TRIPLEBYTEABS Bopc_BE

opc_BF: // Opcode BF - BBS3
   BBRS Bopc_BF bbsreturn
opc_C0: // Opcode C0 - CPY #$00
   DOUBLEBYTESETFIRSTBYTE Bopc_C0

opc_C4: // Opcode C4 - CPY $00
   DOUBLEBYTESETFIRSTBYTE Bopc_C4

opc_C5: // Opcode C5 - CMP $00
   DOUBLEBYTESETFIRSTBYTE Bopc_C5

opc_C6: // Opcode C6 - DEC $00
   COPY4LOAD Bopc_C6
   orr r8,r8,r4
   orr r10,r10,r4
   B jitend2byteBLendstore

opc_C7: // Opcode C7 - SMB4 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_C7 12

opc_C9: // Opcode C9 - CMP #$00
   DOUBLEBYTESETFIRSTBYTE Bopc_C9

opc_CC: // Opcode CC - CPY $0000
   TRIPLEBYTEABS Bopc_CC

opc_CD: // Opcode CD - CMP $0000
   TRIPLEBYTEABS Bopc_CD

opc_CE: // Opcode CE - DEC $0000
   TRIPLEBYTEABS Bopc_CE

opc_CF: // Opcode CF - BBS4
   BBRS Bopc_CF bbsreturn
.ltorg
Bopc_A0:
   MOVREGYCONST
   nop
   teq regY,#0
   nop

Bopc_A2:
   MOVREGXCONST
   nop
   teq regX,#0
   nop
Bopc_A4:
   ldrb regY,[ram6502,#0x00]
   nop
   mov regY,regY,LSL #24
   teq regY,#0
Bopc_A5:
   LDRSB regA,[ram6502,#0x00]
   nop
   nop
   teq regA,#0
Bopc_A6:
   ldrb regX,[ram6502,#0x00]
   nop
   mov regX,regX,LSL #24
   teq regX,#0
Bopc_A7:
   JITSMB 2

Bopc_AC:
   JITABSLDRB
   nop
   nop
   mov regY,reg12, LSL #24
   teq regY,#0
Bopc_AC_io:
   JITABS r0
   .word ioload-16
   mov regY,r0,LSL #24
   nop
   teq regY,#0
Bopc_AD:
   MOVR1HIGH
   ldrsb regA,[reg1,#0x00]
   nop
   nop
   nop
   teq regA,#0
Bopc_AD_io:
   JITABS r0
   .word ioload-16
   sxtb regA,r0
   nop
   teq regA,#0
Bopc_AE:
   JITABSLDRB
   nop
   nop
   mov regX,reg12, LSL #24
   teq regX,#0
Bopc_AE_io:
   JITABS r0
   .word ioload-16
   mov regX,r0,LSL #24
   nop
   teq regX,#0
Bopc_AF:
   JITBBS 2

Bopc_B1:
   ldrh reg0,[ram6502,#0x00]
   mov reg1,regY,LSR #24
   ldrsb regA,[reg1,reg0]
   teq   regA,#0
Bopc_B2:
   ldrh reg0,[ram6502,#0x00]
   nop
   ldrsb regA,[reg0]
   teq   regA,#0
Bopc_B4:
   ADDR1REGXZP
   ldrb regY,[ram6502,reg1,LSR#24]
   mov regY,regY,LSL #24
   teq   regY,#0
Bopc_B5:
   ADDR1REGXZP
   mov reg1,reg1,LSR#24
   ldrsb regA,[reg1]
   teq   regA,#0
Bopc_B6:
   ADDR1REGYCONST
   ldrb reg12,[ram6502,reg1,LSR#24]
   mov regX,reg12,LSL #24
   teq   regX,#0
Bopc_B7:
   JITSMB 3

Bopc_B9:
   JITABS
   mov  regA, regY, lsr #24
   ldrsb regA,[reg1, regA]
   nop
   teq  regA, #0

Bopc_BC:
   JITABS
   ldrb regY,[reg1, regX, lsr #24]
   nop
   mov  regY,regY, lsl #24
   teq  regY, #0
Bopc_BD:
   JITABS                    // mov r1,regx,LSL#24
   mov  regA, regX, lsr #24  // ADD r1,r1,#MSB<<8
   ldrsb regA,[reg1, regA]   // ldrsb regA,[reg1,#LSB] // Should be fast
   nop
   teq  regA, #0
Bopc_BE:
   JITABS
   ldrb regX,[reg1, regY, lsr #24]
   nop
   mov  regX,regX, lsl #24
   teq  regX, #0
Bopc_BF:
   JITBBS 3
Bopc_C0:
   CMPREGYCONST
   nop
   nop
   nop

Bopc_C4:
   ldrb reg0,[ram6502,#0x00]
   nop
   nop
   cmp regY, reg0, lsl #24
Bopc_C5:
   ldrb reg0,[ram6502,#0x00]
   mov reg1,regA,LSL #24
   nop
   cmp reg1, reg0, LSL #24
Bopc_C6:
   ldrb reg0,[ram6502,#0]
   sub  reg0, reg0, #1
   strb reg0,[ram6502,#0]
   .word tempflags-20
Bopc_C7:
   JITSMB 4

Bopc_C9:
   MOVR1CONST
   nop
   rsbs reg1,reg1,regA,LSL #24
   nop

Bopc_CC:
   JITABSLDRB
   nop
   nop
   nop
   cmp regY,reg12, lsl #24
Bopc_CD:
   JITABSLDRB
   nop
   mov reg1,regA,LSL #24
   nop
   cmp reg1,reg12,LSL #24
Bopc_CE:
   JITABSLDRBWB
   sub  reg12, reg12, #1
   strb reg12,[reg1]
   sxtb reg12, reg12
   teq  reg12, #0
Bopc_CF:
   JITBBS 4
Bopc_D1:
   ldrh reg0,[ram6502,#0x00]
   ldrb reg0,[reg0,regY, LSR #24]
   mov reg1,regA, LSL #24
   cmp reg1,reg0,LSL #24
Bopc_D2:
   ldrh reg0,[ram6502,#0x00]
   ldrb reg0,[reg0]
   mov reg1,regA, LSL #24
   cmp reg1,reg0,LSL #24
Bopc_D5:
   ADDR1REGXZP
   ldrb reg0,[ram6502,reg1,LSR#24]
   mov reg1,regA,LSL #24
   cmp reg1,reg0,LSL #24
Bopc_D6:
   ADDR1REGXZP
   ldrb reg0,[ram6502,reg1,LSR#24]
   sub  reg0, reg0, #1
   .word storeXflags-20
Bopc_D7:
   JITSMB 5

Bopc_D9:
   JITABS
   ldrb reg0,[reg1,regY,LSR #24]
   nop
   mov reg1,regA,LSL #24
   cmp reg1,reg0,LSL #24

Bopc_DD:
   JITABS
   ldrb reg0,[reg1,regX,LSR #24]
   nop
   mov reg1,regA,LSL #24
   cmp reg1,reg0,LSL #24
Bopc_DE:
   JITABS
   ldrb reg0,[reg1, regX, lsr #24]!
   sub  reg0, reg0, #1
   strb reg0,[reg1]
   .word tempflags-28
Bopc_DF:
   JITBBS 5
Bopc_E0:
   CMPREGXCONST
   nop
   nop
   nop

Bopc_E4:
   ldrb reg0,[ram6502,#0x00]
   nop
   nop
   cmp regX, reg0, lsl #24

Bopc_E6:
   ldrb reg0,[ram6502,#0]
   add  reg0, reg0, #1
   strb reg0,[ram6502,#0]
   .word tempflags-20
Bopc_E7:
   JITSMB 6

Bopc_EC:
   JITABSLDRB
   nop
   nop
   nop
   cmp regX,reg12, lsl #24

Bopc_EE:
   JITABSLDRBWB
   .word incstore-16
   sxtb reg12, reg12
   teq  reg12, #0
Bopc_EF:
   JITBBS 6

Bopc_F6:
   ADDR1REGXZP
   ldrb reg0,[ram6502,reg1,LSR#24]
   add  reg0, reg0, #1
   .word storeXflags-20
Bopc_F7:
   JITSMB 7

Bopc_FE:
   JITABS
   ldrb reg12,[reg1, regX, lsr #24]!
   .word incstore-20
   sxtb reg12, reg12
   teq  reg12, #0
Bopc_FF:
   JITBBS 7

opc_D1: // Opcode D1 - CMP ($00),Y
   DOUBLEBYTET2 Bopc_D1

opc_D2: // Opcode D2 - CMP ($00)
   DOUBLEBYTET2 Bopc_D2

opc_D5: // Opcode D5 - CMP $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_D5

opc_D6: // Opcode D6 - DEC $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_D6

opc_D7: // Opcode D7 - SMB5 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_D7 12

opc_D9: // Opcode D9 - CMP $0000,Y
   TRIPLEBYTEABS Bopc_D9

opc_DD: // Opcode DD - CMP $0000,X
   TRIPLEBYTEABS Bopc_DD

opc_DE: // Opcode DE - DEC $0000,X
   TRIPLEBYTEABSBLOUT20 Bopc_DE

opc_DF: // Opcode DF - BBS5
   BBRS Bopc_DF bbsreturn

opc_E0: // Opcode E0 - CPX #$00
   DOUBLEBYTESETFIRSTBYTE Bopc_E0

opc_E4: // Opcode E4 - CPX $00
   DOUBLEBYTESETFIRSTBYTE Bopc_E4

opc_E6: // Opcode E6 - INC $00
   COPY4LOAD Bopc_E6
   orr r8,r8,r4
   orr r10,r10,r4
   B jitend2byteBLendstore

opc_E7: // Opcode E7 - SMB6 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_E7 12

opc_EC: // Opcode EC - CPX $0000
   TRIPLEBYTEABS Bopc_EC

opc_EE: // Opcode EE - INC $0000
   ldrh r12,[temp2,#1]
   COPY5LOAD Bopc_EE
   orr r7,r7,r5
   orr r8,r8,r4
   MAKEBRANCHLINK r9

   mov r12,r12, LSL #2
   add r12,r12,#JITTEDTABLE16
   sub r12,r12,#8+20

   B  jitend3bytestoreoperandBL

opc_EF: // Opcode EF - BBS6
   BBRS Bopc_EF bbsreturn

opc_F6: // Opcode F6 - INC $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUTQ Bopc_F6

opc_F7: // Opcode F7 - SMB7 $00
   DOUBLEBYTESETTWOBYTES12 Bopc_F7 12

opc_FE: // Opcode FE - INC $0000,X
   TRIPLEBYTEABSBLOUT Bopc_FE incstore 12

opc_FF: // Opcode FF - BBS7
   BBRS Bopc_FF bbsreturn

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
   str   r13,stackptr
   mov   ram6502,r0  // this is assumed to be zero
   str   ram6502,ram6502store

// install our interrupt handler

   ldr   temp,=_fast_interrupt_vector_h
   ldr   temp0,[temp]
   str   temp0,old_fiq_vector
   ldr   temp0,=arm_fiq_handler_jit
   str   temp0,[temp]

// setup ARM cycle counter

#if defined(RPI2)|| defined(RPI3)||defined(RPI4)
   mrc   p15 ,0, r0, c9, c12, 0
   BIC   r0, r0, #8
   mcr   p15, 0, r0, c9, c12, 0
   mrc   p15, 0, r0, c9, c13, 0
#else
   MRC   p15, 0, r0, c15, c12, 0    // Read Performance Monitor Control Register
   BIC   r0 ,r0, #8                 // clear div 64 bit
   MCR   p15, 0, r0, c15, c12, 0    // Write Performance Monitor Control Register
   MRC   p15, 0, r0, c15, c12, 1    // Read Cycle Counter Register
#endif

// setup table JITLET ( 64K x bl JITLET)
// setup JITTEDTABLE16 with 64K x mov pc,r14
   ldr   temp2,=dojit-JITLET-8
   mov   r0,#JITLET
   ldr   temp,=MOVPCR14INSTRUCTION
   mov   r4,#JITTEDTABLE16

   mov   r3,#0x10000 // 64K
   mov   temp2,temp2,LSR#2
   add   temp2,temp2,#BLINSTRUCTION

loopdejitsetup:
   subs  r3,r3,#1
   str   temp2,[r0],#NEXTJITLET
   sub   temp2,temp2,#NEXTJITLET>>2
   str   temp,[r4],#4   // JITTEDTABLE16
   bne   loopdejitsetup

// flush caches

#if defined(RPI2) || defined(RPI3) || defined(RPI4)
   DSB
// this would be faster to flush the cache by way instead of VA
   mrc p15, 0,r12,cr0,cr0,1
   ubfx r1,r12,#16,#4
   mov r12,#4
   lsl r12,r12,r1
   str r12,cacheline

   mov r5,#JITLET
   mov r0,#JITTEDTABLE16
   add r1,r0,#0x10000*4
cacheflushloopjittable:
   mcr p15,0,r0,cr7,cr11,1
   add r0,r0,r12
   mcr p15,0,r5,cr7,cr11,1
   add r5,r5,r12
   mcr p15,0,r5,cr7,cr11,1
   add r5,r5,r12
   cmp r1,r0
   bhi cacheflushloopjittable

//   BL CleanDataCache // this may not be correct for Pi4
   DSB
   ISB
   mov temp,#0

   MCR p15, 0, temp, c7, c5, 0 //; invalidate I cache & BTB

   DSB
   ISB
#else
   mov temp,#0
   MCR p15, 0, temp, c7, c10, 0 //; clean data cache
   MCR p15, 0, temp, c7, c5, 0 //; invalidate I cache & BTB
#endif

   mov   r0, #0x10000
   ldrh  temp0, [r0, #-4]        // Fetch the vector address

// run dormann tests directly
#ifdef DORMANN_TEST
   ldr temp0,=0x3400
   mov temp,#0xF3
   ldr temp1,=0xFFEE
   str temp,[temp1]
   mov temp,#0x60
   str temp,[temp1,#1]
#endif

#if JITDEBUG
// Enable jit tracing
   mov temp,#1
   str temp,debugflag

   mov temp,#0xe3
   ldr temp1,= 0x7FFE
   strb temp,[temp1],#1
   mov temp,#0x60
   strb temp,[temp1]
#endif

   add regSP,ram6502, #0x1F0

   mov flags, #0x00     // Initialize flags (flags) to zero on power up
                        // This was added to fix a bug where the language transfer
                        // would fail if exec_65tube was entered with certain
                        // values of flags (specifically, with bit 7 = 1).
                        // Code assumes Bits 7,2,1 is zero

   mov temp0,temp0,LSL#JITLETSHIFT
   add temp0,temp0,#JITLET
   mov jittedtable16ptr,#JITTEDTABLE16
   bx temp0


//*************************************************************
reset_handler:
// disable IRQS
   CPSID   if                     // Disable ARM FIQ and IRQ interrupts
// unstack
   pop {r0-r2}
   str  r0,fiq_spsr_temp
   str  r1,fiq_lr_temp
   msr  CPSR_flg,r2
   pop {r0-r12,r14}
// Return to FIQ mode
   //msr     CPSR_c, #(CPSR_MODE_FIQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT)
   //ldr r12,fiq_spsr_temp
   //ldr r14,fiq_lr_temp
   //msr     spsr_cf, r12            // Restore spsr.
   //subs    pc, lr, #4             // Return from ISR
   CPSIE   if                     // Re-enable ARM FIQ and IRQ interrupts

exec_65tube_exit:     // We get here from the Event handler when RST is detected
// release interrupt handler
   ldr temp,=_fast_interrupt_vector_h
   ldr temp0,old_fiq_vector
   str temp0,[temp]
   ldr r13,stackptr  // Hack to ensure the stack ptr is on the right place
   pop {r4-r11,pc}
.ltorg
debugflag:
   .word 0
old_fiq_vector:
   .word 0
fiq_lr_temp:
   .word 0
fiq_spsr_temp:
   .word 0
stackptr:
   .word 0
cacheline:
   .word 0
ram6502store:
   .word 0

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
   ldr r12,=DOORBELL
   ldr r10,=tube_irq
   ldr r11,=DOORBELLDATA
   push {r0-r3,r14}
   ldr r0, [r12]               // read doorbell
   ldr r0, [r11]               // Get doorbell data
#else
   ldr r12, =MBOX0_READ        // Read the GPU mailbox
   push {r0-r3,r14}
   ldr r10,=tube_irq
   ldr  r0, [r12]               // Get Mailbox data
#endif
   BL   tube_io_handler
   ldr  r12,[r10]
   pop  {r0-r3,r14}
   tst   r12,#RESET_BIT+NMI_BIT+IRQ_BIT
   subeqs pc, lr, #4

// save lr to ram address
   str  lr,fiq_lr_temp
   mrs  r12, spsr               // Get the saved psr
   str  r12,fiq_spsr_temp

// drop to user mode
   msr     CPSR_c, #(CPSR_MODE_SVR | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT)

// stack everything onto user stack
   push {r0-r12,r14}
// stack ram saved lr
   ldr r0,fiq_spsr_temp
   ldr r1,fiq_lr_temp
   mrs r2, CPSR                // preserve the 6502 flags
   push {r0-r2}

   // If the SPSR indicates FIQ has interrupted IRQ then we need to recover a valid 6502 SP
   mov    r1, regSP
   and    r0, #0x1F
   cmp    r0, #0x12
   ldreq  r0, =_main_irq_r9_regsp
   ldreq  r1, [r0]
   cmp    r1, #0
   movne  regSP, r1

// We need to restore these registers in case the FIQ has interrupted an IRQ which
// has over written them. other register
// e.g. flag register needs to be thought about
   mov jittedtable16ptr,#JITTEDTABLE16
   ldr ram6502, ram6502store

// process events
   ldr      r1,=tube_irq
   ldr      r0, [r1]               // Get Mailbox data
   tst r0,#4
   bne  reset_handler
   tst r0,#2
   bne handle_nmi
   tst     flags, #I_FLAG6502      // Test whether interrupts are enabled (bit 2 = 0)
   bne     noirq
   tst     r0, #1             // Bit 0 set indicate IRQ is active
   bne     handle_irq_fake         // Take the IRQ interrupt

noirq:
// unstack
   pop {r0-r2}
   str  r0,fiq_spsr_temp
   str  r1,fiq_lr_temp
   msr  CPSR_flg,r2
   pop {r0-r12,r14}
// Return to FIQ mode
   msr     CPSR_c, #(CPSR_MODE_FIQ | CPSR_IRQ_INHIBIT | CPSR_FIQ_INHIBIT)
   ldr r12,fiq_spsr_temp
   ldr r14,fiq_lr_temp

   msr     spsr_cf, r12            // Restore spsr.
   subs    pc, lr, #4             // Return from ISR

CHECK_IRQ:
   ldr     reg0, =tube_irq             // Get IRQ status
   ldr     reg0, [reg0]
   //mrs     reg12, CPSR                  // Preserve the 6502 flags
   tst     flags, #I_FLAG6502        // Test whether interrupts are enabled (bit 2 = 0)
   eoreq   reg0, reg0, #1
   tsteq   reg0, #1                    // Test for IRQ
   beq     handle_irq
   msr     CPSR_flg, temp              // Restore the 6502 flags
   mov     temp1,temp1,LSL #JITLETSHIFT
   add   temp1,temp1,#JITLET
   bx    temp1

handle_irq:
   msr  CPSR_flg, temp     // Restore the 6502 flags
   INTR -2, 0

handle_irq_fake:
   msr     CPSR_flg, temp2   // Restore the 6502 flags
   INTR    -2, 0, FAKE

handle_nmi:
   ldr     r0,[r1]   // load tube_irq again as it might have changed
   BIC     r0,r0,#2  // clear NMI flag
   str     r0,[r1]   // Store it back again
   msr     CPSR_flg, r2   // Restore the 6502 flags
   INTR    -6, 0, FAKE
.ltorg

// **** dejit functions

// so we get here by storing data to a location that has already been jitted
// Normaly we having branched with link to the JITTEDTABLE16. which contains BX LR
// if that location has been jitted then the bx lr will have been replaced b dejit
//
// so now we need to replace all the jitted code for that instruction with
// B dojit.
// the JITTEDTABLE16 entries can be replaced with bx lr
// the caches need to be flushed
// reg1 is the memory location that has been unjitted


// one byte instruction
dejit16bit1: //reg1 pointer to address to dejit
   ldr reg2,=dojit-JITLET-8
   ldr reg0,=MOVPCR14INSTRUCTION
   mov reg12,#JITLET
   sub reg2,reg2,reg1,LSL # JITLETSHIFT
   mov reg3,#0 // only used in Armv6 DSB
   mov reg2,reg2,LSR#2
   add reg2,reg2,#BLINSTRUCTION

   str reg2,[reg12,reg1,LSL #JITLETSHIFT]!
   DEJITCLEAN 1
   mov reg12,#JITTEDTABLE16
   str reg0,[reg12,reg1,LSL #2]!
   DEJITCLEAN 0
   bx lr

// two byte instruction
dejit16bit21: // reg1 , reg1+4

   ldr reg2,=dojit-JITLET-8
   ldr reg0,=MOVPCR14INSTRUCTION
   mov reg12,#JITLET
   sub reg2,reg2,reg1,LSL # JITLETSHIFT
   mov reg3,#0 // only used in Armv6 DSB
   mov reg2,reg2,LSR#2
   add reg2,reg2,#BLINSTRUCTION

   str reg2,[reg12,reg1,LSL #JITLETSHIFT]!
   DEJITCLEAN 1
   sub reg2,reg2,#(NEXTJITLET>>2)
   str reg2,[reg12,#NEXTJITLET]!
   DEJITCLEAN 1

   mov reg12,#JITTEDTABLE16
   str reg0,[reg12,reg1,LSL #2]!
   DEJITCLEAN 1
   str reg0,[reg12,#4]!
   DEJITCLEAN 0
   bx lr

dejit16bit22: // reg1 , reg1-4
   ldr reg2,=(dojit-JITLET-8)
   ldr reg0,=MOVPCR14INSTRUCTION
   mov reg12,#JITLET
   sub reg2,reg2,reg1,LSL # JITLETSHIFT
   mov reg3,#0 // only used in Armv6 DSB
   mov reg2,reg2,LSR#2
   add reg2,reg2,#BLINSTRUCTION

   str reg2,[reg12,reg1,LSL #JITLETSHIFT]!
   DEJITCLEAN 1
   add reg2,reg2,#(NEXTJITLET>>2)
   str reg2,[reg12,#-NEXTJITLET]!
   DEJITCLEAN 1

   mov reg12,#JITTEDTABLE16
   str reg0,[reg12,reg1,LSL #2]!
   DEJITCLEAN 1
   str reg0,[reg12,#-4]!
   DEJITCLEAN 0
   bx lr

// three byte instruction
dejit16bit31: // reg1 , reg1+4, reg1 + 8
   ldr reg2,=dojit-JITLET-8
   ldr reg0,=MOVPCR14INSTRUCTION
   mov reg12,#JITLET
   sub reg2,reg2,reg1,LSL # JITLETSHIFT
   mov reg3,#0 // only used in Armv6 DSB
   mov reg2,reg2,LSR#2
   add reg2,reg2,#BLINSTRUCTION

   str reg2,[reg12,reg1,LSL #JITLETSHIFT]!
   DEJITCLEAN 1
   sub reg2,reg2,#NEXTJITLET>>2
   str reg2,[reg12,#NEXTJITLET]!
   DEJITCLEANMID
   sub reg2,reg2,#NEXTJITLET>>2
   str reg2,[reg12,#NEXTJITLET]!
   DEJITCLEAN 1

   mov reg12,#JITTEDTABLE16
   str reg0,[reg12,reg1,LSL #2]!
   DEJITCLEAN 1
   str reg0,[reg12,#4]!
   DEJITCLEANMID
   str reg0,[reg12,#4]!
   DEJITCLEAN 0
   bx lr

dejit16bit32: // reg1 , reg1-4, reg1 + 4

   ldr reg2,=dojit-JITLET-8
   ldr reg0,=MOVPCR14INSTRUCTION
   mov reg12,#JITLET
   sub reg2,reg2,reg1,LSL # JITLETSHIFT
   mov reg3,#0 // only used in Armv6 DSB
   mov reg2,reg2,LSR#2
   add reg2,reg2,#BLINSTRUCTION

   str reg2,[reg12,reg1,LSL #JITLETSHIFT]!
   DEJITCLEANMID
   sub reg2,reg2,#-(NEXTJITLET>>2)
   str reg2,[reg12,#-NEXTJITLET]!
   DEJITCLEAN 1
   sub reg2,reg2,#(NEXTJITLET>>2)*2
   str reg2,[reg12,#NEXTJITLET*2]!
   DEJITCLEAN 1

   mov reg12,#JITTEDTABLE16
   str reg0,[reg12,reg1,LSL #2]!
   DEJITCLEANMID
   str reg0,[reg12,#-4]!
   DEJITCLEAN 1
   str reg0,[reg12,#8]!
   DEJITCLEAN 0
   bx lr

dejit16bit33: // reg1 , reg1-4, reg1 - 8
   ldr reg2,=dojit-JITLET-8
   ldr reg0,=MOVPCR14INSTRUCTION
   mov reg12,#JITLET
   sub reg2,reg2,reg1,LSL # JITLETSHIFT
   mov reg3,#0 // only used in Armv6 DSB
   mov reg2,reg2,LSR#2
   add reg2,reg2,#BLINSTRUCTION

   str reg2,[reg12,reg1,LSL #JITLETSHIFT]!
   DEJITCLEAN 1
   sub reg2,reg2,#-(NEXTJITLET>>2)
   str reg2,[reg12,#-NEXTJITLET]!
   DEJITCLEANMID
   sub reg2,reg2,#-(NEXTJITLET>>2)
   str reg2,[reg12,#-NEXTJITLET]!
   DEJITCLEAN 1

   mov reg12,#JITTEDTABLE16
   str reg0,[reg12,reg1,LSL #2]!
   DEJITCLEAN 1
   str reg0,[reg12,#-4]!
   DEJITCLEANMID
   str reg0,[reg12,#-4]!
   DEJITCLEAN 0
   bx lr

// *************

// **** jit up to a branch or jsr rts rti
dojit:
// r14 has address of instruction
   mrs r4, CPSR                   // Save interrupt state (restored by dojitexit)
   CPSID   if                     // Disable ARM FIQ and IRQ interrupts
   sub r3,r14,#4                  // return address ( first address of jitted code)
   push {r3-r11}
        ldr     r8, =GPSET0     // timing debug code
        mov     r7, #TEST2_MASK
        str     r7, [r8]
   sub temp2,R3,#JITLET
   mov temp2,temp2,LSR#JITLETSHIFT
   b jitentry

// NB word -16
jitend2byteBL8:
   LDMIA temp1,{r8,r9,r10,r11}
   MAKEBRANCHLINK r10
   B jitend2bytestore4operand

jitendT2byte:
   LDMIA temp1,{r8,r9,r10,r11}
   and r1,r4,#0xF0
   and r4,r4,#0x0F
   orr r8,r8,r1,LSL #4
   B jitend2byteBLendoperand

jitend2byteBLend:

// NB word -20  ( 8+12)
   LDMIA temp1,{r8,r9,r10,r11}
jitend2byteBLendoperand:
   orr r8,r8,r4
jitend2byteBLendstore:
   MAKEBRANCHLINK r11
   B jitend2bytestore4

doublebyteendsettwobytes12:
   LDMIA temp1,{r8,r9,r10,r11}
   orr r8,r8,r4
   orr r11,r11,r4
   B jitend2bytestore4

doublebyteend:
   LDMIA temp1,{r8,r9,r10,r11}
jitend2bytestore4operand:
   orr r8,r8,r4
jitend2bytestore4:
   COPY4STORE

jitend2byte:
   FILLINJITTEDTABLES dejit16bit21
   add temp0,temp0,#(dejit16bit22-dejit16bit21-4)>>2
   add temp2,temp2,#2
   str temp0,[temp1,#4]

dojitloop:
#ifdef JITDEBUG
   ldr temp,debugflag
   movs temp,temp
   BEQ 1f

   push {r0-r12}
   mov r0,r6 // r6 can be corrupt now ******** by fixt2
   mov r1,r2
   BL disarm
   pop {r0-r12}
1:
#endif
jitentry:

//debug
#ifdef JITDEBUG
   ldr r0,debugflag
   movs r0,r0
   BEQ 1f

   push {r0-r12}
   mov r0,temp2,
   BL dis6502
   pop {r0-r12}
1:
#endif
   ldrb r7,[temp2]
   mov r6,temp2,LSL#JITLETSHIFT
   adr temp0,opcode_table
   add r6,r6,#JITLET
   ldr temp0,[temp0,r7,LSL#2]
   ldrb r4,[temp2,#1]
   ldrb r5,[temp2,#2]
   bx temp0
// entry regs
// r0
// r1
// r2 ptr to 6502 code instruction
// r3 address to return to ( start of jitting)
// r4 operand byte 1
// r5 operand byte 2
// r6 Address to write to JITLET code to
//r7,r8,r9,r10,r11,r12 six registers for COPY6
opcode_table:
.word opc_00,opc_01,opc_02,opc_03,opc_04,opc_05,opc_06,opc_07
.word opc_08,opc_09,opc_0A,opc_0B,opc_0C,opc_0D,opc_0E,opc_0F
.word opc_10,opc_11,opc_12,opc_13,opc_14,opc_15,opc_16,opc_17
.word opc_18,opc_19,opc_1A,opc_1B,opc_1C,opc_1D,opc_1E,opc_1F
.word opc_20,opc_21,opc_22,opc_23,opc_24,opc_25,opc_26,opc_27
.word opc_28,opc_29,opc_2A,opc_2B,opc_2C,opc_2D,opc_2E,opc_2F
.word opc_30,opc_31,opc_32,opc_33,opc_34,opc_35,opc_36,opc_37
.word opc_38,opc_39,opc_3A,opc_3B,opc_3C,opc_3D,opc_3E,opc_3F
.word opc_40,opc_41,opc_42,opc_43,opc_44,opc_45,opc_46,opc_47
.word opc_48,opc_49,opc_4A,opc_4B,opc_4C,opc_4D,opc_4E,opc_4F
.word opc_50,opc_51,opc_52,opc_53,opc_54,opc_55,opc_56,opc_57
.word opc_58,opc_59,opc_5A,opc_5B,opc_5C,opc_5D,opc_5E,opc_5F
.word opc_60,opc_61,opc_62,opc_63,opc_64,opc_65,opc_66,opc_67
.word opc_68,opc_69,opc_6A,opc_6B,opc_6C,opc_6D,opc_6E,opc_6F
.word opc_70,opc_71,opc_72,opc_73,opc_74,opc_75,opc_76,opc_77
.word opc_78,opc_79,opc_7A,opc_7B,opc_7C,opc_7D,opc_7E,opc_7F
.word opc_80,opc_81,opc_82,opc_83,opc_84,opc_85,opc_86,opc_87
.word opc_88,opc_89,opc_8A,opc_8B,opc_8C,opc_8D,opc_8E,opc_8F
.word opc_90,opc_91,opc_92,opc_93,opc_94,opc_95,opc_96,opc_97
.word opc_98,opc_99,opc_9A,opc_9B,opc_9C,opc_9D,opc_9E,opc_9F
.word opc_A0,opc_A1,opc_A2,opc_A3,opc_A4,opc_A5,opc_A6,opc_A7
.word opc_A8,opc_A9,opc_AA,opc_AB,opc_AC,opc_AD,opc_AE,opc_AF
.word opc_B0,opc_B1,opc_B2,opc_B3,opc_B4,opc_B5,opc_B6,opc_B7
.word opc_B8,opc_B9,opc_BA,opc_BB,opc_BC,opc_BD,opc_BE,opc_BF
.word opc_C0,opc_C1,opc_C2,opc_C3,opc_C4,opc_C5,opc_C6,opc_C7
.word opc_C8,opc_C9,opc_CA,opc_CB,opc_CC,opc_CD,opc_CE,opc_CF
.word opc_D0,opc_D1,opc_D2,opc_D3,opc_D4,opc_D5,opc_D6,opc_D7
.word opc_D8,opc_D9,opc_DA,opc_DB,opc_DC,opc_DD,opc_DE,opc_DF
.word opc_E0,opc_E1,opc_E2,opc_E3,opc_E4,opc_E5,opc_E6,opc_E7
.word opc_E8,opc_E9,opc_EA,opc_EB,opc_EC,opc_ED,opc_EE,opc_EF
.word opc_F0,opc_F1,opc_F2,opc_F3,opc_F4,opc_F5,opc_F6,opc_F7
.word opc_F8,opc_F9,opc_FA,opc_FB,opc_FC,opc_FD,opc_FE,opc_FF


branchsavewords:
   STRD temp0,temp1,[R6]
   str temp1,[r6,#8]
   str temp1,[r6,#12]

jitend2byteexit:
   FILLINJITTEDTABLES dejit16bit21
   add temp0,temp0,#(dejit16bit22-dejit16bit21-4)>>2
   add temp2,temp2,#2
   str temp0,[temp1,#4]
  // FILLINJITTEDTABLES dejit16bit22

dojitexit :
#ifdef JITDEBUG
   ldr r0,debugflag
   movs r0,r0
   BEQ 1f
   push {r0-r12}
   mov r0,r6
   mov r1,r2
   BL disarm
   pop {r0-r12}

 //  DEBUG_REG r3

1:
#endif

// flush BTB icache dcache it might be better to just invalidate a few lines
#if defined(RPI4)|| defined(RPI3)
   DSB // might not be needed

   // Clean data cache both JITLET
   // r3 has the first address of JITLET instruction
   // r2 has last address+1
   ldr r12,cacheline
   mov r2,r2,LSL #JITLETSHIFT
   sub r4,r3,#JITLET
   add r2,r2,#JITLET
   mov r4,r4,LSR #(JITLETSHIFT-2)
   add r4,r4,#JITTEDTABLE16

   sub r0,r12,#1  // this appears to fix an A72 issue where the cache wasn't being
   bic  r3,r3,r0  // flushed if the address was the last word in a cache line
   bic  r4,r4,r0  // and it was being flushed for the second time !!!!

cacheflushloop:
   mcr p15,0,r3,cr7,cr11,1 // data JITLET (NB 2xJITLET compared to table)
 //  MCR p15,0,r3, c7, c5, 1 // instruction
   add r3,r3,r12
   mcr p15,0,r3,cr7,cr11,1 // data JITLET
 //  MCR p15,0,r3, c7, c5, 1 // instruction
   add r3,r3,r12
   mcr p15,0,r4,cr7,cr11,1 // data JITTEDTABLE16
 //  MCR p15,0,r4, c7, c5, 1 // instruction
   add r4,r4,r12
   cmp r3,r2
   blt cacheflushloop

   DSB
   //mov temp0,#0
   //MCR p15, 0, temp0, c7, c5, 6 //; invalidate all of the BTB
   // Why the above of only flushing accessed cache locations doesn't work
   // isn't understood. So we flush the the I cache to fix the issue.
   MCR p15, 0, reg12, c7, c5, 0 // invalidate I&BTB cache

   DSB
   ISB

#else
#if defined(RPI2)
   // Clean data cache both JITLET
   // r3 has the first address of JITLET instruction
   // r6 has last address ( possibly needs to be +8
   mov r0,r3
   mov r2,r2,LSL #JITLETSHIFT
   add r1,r2,#JITLET
   sub r1,r1,r0
   BL _clean_cache_area

   DSB
   mov temp,#0
   MCR p15, 0, temp, c7, c5, 0 //; invalidate I cache
   MCR p15, 0, temp, c7, c5, 6 //; invalidate all of the BTB
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
   MCR p15, 0, temp, c7, c5, 6 //; invalidate all of the BTB

   mov r2,r2,LSL #JITLETSHIFT
   add r7,r2,#JITLET
   sub r4,r3,#JITLET
   mov r4,r4,LSR #(JITLETSHIFT-2)
   mov r2,r2,LSR #(JITLETSHIFT-2)
   add r4,r4,#JITTEDTABLE16
   add r2,r2,#JITTEDTABLE16

   MCRR p15,0,r7,r3,c12 // clean data cache
   MCRR p15,0,r2,r4,c12 // clean data cache

 //  MCRR p15,0,r7,r3,c5 // invalidate instruction cache
 //  MCRR p15,0,r2,r4,c5 // invalidate instruction cache

 //  MCR p15, 0, temp, c7, c10, 4 // DSB

 //  MCRR p15,0,r7,r3,c5 // invalidate instruction cache // Do it again ( errata)
 //  MCRR p15,0,r2,r4,c5 // invalidate instruction cache // Do it again ( errata)
   //MCR p15, 0, r0, c7, c5, 1

   MCR p15, 0, temp, c7, c10, 4 // DSB
   MCR p15, 0, temp, c7, c5, 0 //; invalidate I cache and BTB
   MCR p15, 0, temp, c7, c5, 4 // flush prefetch buffer
   MCR p15, 0, temp, c7, c10, 4 // DSB
#else
   // clean all the cache
   mov temp,#0
   MCR p15, 0, temp, c7, c10, 0 //; clean data cache line
   //mcr p15, 0, temp, c7, c10, 4 //drain write buffer
   MCR p15, 0, temp, c7, c5, 0 //; invalidate I cache and BTB
   MCR p15, 0, temp, c7, c5, 4 // flush prefetch buffer
#endif
        ldr     r8, =GPCLR0
        mov     r7, #TEST2_MASK
        str     r7, [r8]
#endif

#endif

   pop {r3-r11}
   msr CPSR, r4  // Restore the 6502 flags, and interrupt state present on entry

   bx r3  // don't use r14 as it is slow on A72

singleopcodeexit:
   MAKEBRANCHLINK r9
singleopcodeexitstore:
   strd r8,[r6]
   FILLINJITTEDTABLES dejit16bit1
   add temp2,temp2,#1
   B dojitexit

singleopcodeBLend:
   MAKEBRANCHLINK r9
jitend1bytestore2:
   strd r8,[r6]
jitend1byte:
   FILLINJITTEDTABLES dejit16bit1
   add temp2,temp2,#1
   B dojitloop

jitend3byteloadstoreoperandBL8:
   LDMIA temp1,{r7,r8,r9,r10,r11,r12}
   orr r7,r7,r5
   orr r8,r8,r4
   MAKEBRANCHLINK r9
   B jitend3bytestore

// .word address-28
jitend3byteloadstoreoperandBL:
   LDMIA temp1,{r7,r8,r9,r10,r11,r12}
   orr r7,r7,r5
   orr r8,r8,r4
jitend3bytestoreoperandBL:
   MAKEBRANCHLINK r12
   B jitend3bytestore

jitend3byteloadstoreoperand:
   LDMIA temp1,{r7,r8,r9,r10,r11,r12}
jitend3bytestoreoperand:
   orr r7,r7,r5
   orr r8,r8,r4
jitend3bytestore:
   COPY6STORE
jitend3byte:
   FILLINJITTEDTABLES dejit16bit31
   add temp0,temp0,#(dejit16bit32-dejit16bit31-4)>>2
   add temp2,temp2,#3
   str temp0,[temp1,#4]
   add temp0,temp0,#(dejit16bit33-dejit16bit32-4)>>2
   str temp0,[temp1,#8]
   B dojitloop

jitend3byteexitBBRS:
   LDMIA temp1,{r7,r8,r9,r10,r11,r12}
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
   add temp0,temp0,#(dejit16bit32-dejit16bit31-4)>>2
   add temp2,temp2,#3
   str temp0,[temp1,#4]
   add temp0,temp0,#(dejit16bit33-dejit16bit32-4)>>2
   str temp0,[temp1,#8]
   B dojitexit

fillinjittedtable:
   mov temp1,#JITTEDTABLE16
   RSB temp0,temp2,temp0,LSR #2
   ORR temp0,temp0,#BINSTRUCTION
   str temp0,[temp1,temp2,LSL #2]!
   bx lr

// entry temp0 address to goto (JITLET)
//       r6 address to place the branch

createbranchtojitlet:
   mov temp0,r12,LSL#JITLETSHIFT-2
   add temp0,temp0,#JITLET>>2

   sub temp0,temp0,r6,LSR #2
   sub temp0,temp0,#8>>2
   BIC temp0,temp0,#0xFF000000
   ORR temp0,temp0,#BINSTRUCTION
   str temp0,[r6]
   B jitend3byteexit

createbranchlinkm8:
   MAKEBRANCHLINK temp0
   str temp0,[r6]
   bx lr

fixuptype2accessload4:
   LDM temp1,{r8,r9,r10,r11}
fixuptype2accessstore4:
   and r1,r4,#0xF0
   and temp,r4,#0x0F
   orr r8,r8,r1,LSL #4
   orr r8,r8,temp
   B jitend2bytestore4

fixuptype2accessquick:
   LDM r1,{r7,r8,r9,r10,r11,r12}
   and r1,r4,#0xF0
   and r4,r4,#0x0F
   orr r7,r7,r5
   orr r8,r8,r1,LSL #4
   orr r8,r8,r4
   bx lr

ioload:
   push    {lr}               // ram6502 (r6) is used as a working register
   mrs     reg4, CPSR         // Save 6502 flags and current FIQ/IRQ bits
   CPSID   if                 // Disable FIQ/IRQ at the same time
   bl      tube_parasite_read // Call up to C code to handle parasite read
   msr     CPSR, reg4         // Restore 6502 flags and FIQ/IRQ bits
   pop     {pc}               // Restore registers and return

iostore:
   push    {lr}               // ram6502 (r6) is used as a working register
   mrs     reg4, CPSR         // Save 6502 flags and current FIQ/IRQ bits
   CPSID   if                 // Disable FIQ/IRQ at the same time
   bl      tube_parasite_write_banksel // Call up to C code to handle parasite write
   msr     CPSR, reg4         // Restore 6502 flags and FIQ/IRQ bits
   pop     {pc}               // Restore registers and return

// Jits overflow

brk:
   sub reg1,r14,#JITLET
   mov reg1,reg1,LSR#JITLETSHIFT
   add reg1,reg1,#1
   INTR    -2, 1

rti:
   PLP
   ldrh    reg1, [regSP, #1]

   mov     reg0, #2
   UADD8   regSP, regSP, reg0

   add reg0,reg1,#1
   cmp reg0,#0x10000

   // so we have an irq that happened during execution
   beq noirq

   ldr     reg0, =tube_irq             // Get IRQ status
   ldr     reg0, [reg0]
   //mrs     reg12, CPSR                  // Preserve the 6502 flags
   tst     flags, #I_FLAG6502        // Test whether interrupts are enabled (bit 2 = 0)
   eoreq   reg0, reg0, #1
   tsteq   reg0, #1                    // Test for IRQ
   beq     handle_irqrti
   msr     CPSR_flg, reg12              // Restore the 6502 flags
   mov     reg1,reg1,LSL #JITLETSHIFT
   add   reg1,reg1,#JITLET
   bx    reg1

handle_irqrti:

   msr  CPSR_flg, temp     // Restore the 6502 flags
   INTR -2, 0

cli:
   mrs reg12,CPSR
   B check_irq_plp

plp:
   PLP
// CHECK_IRQ
check_irq_plp:
   ldr     reg0, =tube_irq             // Get IRQ status
   ldr     reg0, [reg0]
   //mrs     reg12, CPSR                  // Preserve the 6502 flags
   tst     flags, #I_FLAG6502        // Test whether interrupts are enabled (bit 2 = 0)
   eoreq   reg0, reg0, #1
   tsteq   reg0, #1                    // Test for IRQ
   beq     handle_irqplp
   msr     CPSR_flg, reg12              // Restore the 6502 flags
   bx lr

handle_irqplp:
   sub reg1,r14,#JITLET
   mov reg1,reg1,LSR#JITLETSHIFT

   msr  CPSR_flg, reg12     // Restore the 6502 flags
   INTR -2, 0

putcharopcode:
   push    {regSP, r12,r14}
   mrs     regSP, CPSR               // Save 6502 flags
   ldr reg12,=putchar
   mov r0,regA
   BLX reg12
   msr     CPSR_flg, regSP
   pop     {regSP,r12,pc}

debugopcode:
#if JITDEBUG
   ldr reg1,=debugflag
   str reg12,[reg1]
#endif
   bx lr

invertcarry:
   mrs   reg0, CPSR
   eor   reg0, reg0, #C_FLAG
   msr   CPSR_flg, reg0
   bx lr

testvflagC:
   mrs     reg1, CPSR
   tst     flags, #V_FLAG6502
   addne pc,r14,#8 // return to jit
   bx lr

testvflagS:
   mrs     reg1, CPSR
   tst     flags, #V_FLAG6502
   addeq pc,r14,#8 // return to jit
   bx lr

bbrreturn:
   addne pc,r14,#4 // return to jit
   msr     CPSR_flg, reg1
   bx lr

bbsreturn:
   addeq pc,r14,#4 // return to jit
   msr     CPSR_flg, reg1
   bx lr

bitimm:
   bicne   reg1, reg1, #Z_FLAG
   orreq   reg1, reg1, #Z_FLAG
   msr     CPSR_flg, reg1
   bx lr

jitstxytable:
   add reg0,jittedtable16ptr,reg1,LSR #24-2
   BX reg0

jitstatable:
   strb regA,[reg1]
   add reg0,jittedtable16ptr, reg1,LSL #2
   BX reg0

storeXflags:
   strb reg0,[ram6502,reg1,LSR#24]
tempflags:
   sxtb reg0, reg0
   teq  reg0, #0
   bx lr

incstore:
   add  reg12, reg12, #1
   strb reg12,[reg1]
   bx lr

plx:
   uadd8 regX,regSP,reg1
   ldrb  regX, [regX]
   uadd8 regSP,regSP,reg1
   mov   regX, regX, LSL #24
   teq   regX, #0
   bx lr

ply:
   uadd8 regY,regSP,reg1
   ldrb  regY, [regY]
   uadd8 regSP,regSP,reg1
   mov   regY, regY, LSL #24
   teq   regY, #0
   bx lr

pla:
   uadd8 regA,regSP,reg1
   ldrsb regA, [regA]
   uadd8 regSP,regSP,reg1
   teq   regA, #0
   bx lr

rts:
   mov reg1, #2
   mov reg12,#JITLET
   UADD8 regSP, regSP, reg1
   add  reg1,reg12,#NEXTJITLET
   add  reg1,reg1,reg0,LSL #JITLETSHIFT
   bx reg1

rora:
   subcs   regA, regA, #0x100   // if Carry set all other bits ie sign extend
   rrxs    regA, regA
   bx lr

ror:
   ROR6
   bx lr

rol:
   adc  reg12, reg12, reg12
   strb reg12,[reg1]
   lsls    reg12, reg12, #24
   bx lr

rola:
   lsls    reg1, regA, #24
   sxtb    regA, regA
   bx lr

bitioload: //2c
   push {r14}
   BL      ioload
   pop  {r14}

bitadjustr14:
   add reg2,r14,#16
   bic     flags, flags, #V_FLAG6502
   tst     r0, regA  // This clears N flag and sets up the Z flag
   mrs     reg12, CPSR
   and     reg1, r0, #N_FLAG6502
   and     r0, r0, #V_FLAG6502
   orr     flags, flags, r0
   orr     reg12, reg12, reg1, LSL #24
   msr     CPSR_flg, reg12
   bx  reg2

bit: // 34
   tst     reg12, regA  // This clears N flag and sets up the Z flag
bit2: // 24 3c
   mrs     reg0, CPSR
   and     reg1, reg12, #N_FLAG6502
   and     reg12, reg12, #V_FLAG6502
   orr     flags, flags, reg12
   orr     reg0, reg0, reg1, LSL #24
   msr     CPSR_flg, reg0
   bx lr

cmpa:
   ldrb reg0,[reg1]
   mov reg1,regA,LSL #24
   cmp reg1,reg0,LSL#24
   bx lr

loada:
   ldrsb regA,[reg1]
   teq regA,#0
   bx lr

aflags:
   sxtb    regA, regA
   teq     regA, #0
   bx lr

asl:
   ASL6
   bx lr

php:
   orrmi reg12, reg12, #N_FLAG6502
   orrcs reg12, reg12, #C_FLAG6502
   orreq reg12, reg12, #Z_FLAG6502
pushtemp:
   SPUSH reg12
   bx lr

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

.ltorg

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
