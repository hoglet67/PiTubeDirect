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

// NB this code starts 0x0e400000
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

// Registers

#define temp0     r0
#define temp1     r1
#define temp2     r2
#define temp3     r3
#define temp4     r4
#define jittedtable16ptr     r5
#define ram6502   r6

#define flags     r7
#define regY      r8
#define regSP     r9
#define regA      r10
#define regX      r11

#define temp      r12

#define JITLETSHIFT 3

#define NEXTJITLET (1<<JITLETSHIFT)

#define BEQINSTRUCTION 0x0A000000
#define BNEINSTRUCTION 0x1A000000
#define BCSINSTRUCTION 0x2A000000
#define BCCINSTRUCTION 0x3A000000
#define BMIINSTRUCTION 0x4A000000
#define BPLINSTRUCTION 0x5A000000
#define BINSTRUCTION   0xEA000000
#define BLINSTRUCTION 0xEB000000
// bx lr      0xe12fff1e
// mov pc,r14 0xe1a0f00e  // This is very slow on Pi3B+
#define MOVPCR14INSTRUCTION 0xe12fff1e
#define ARMNOP 0xe320f000

#define ARMBLCC 0x3B000000
#define ARMBLCS 0x2B000000

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

.macro FILLINJITTEDTABLES table16
   ldr temp0,=\table16
   // temp0 is address to goto
   // temp1 is location for store

   BL MAKEBRANCHFILLINJITTEDTABLES
.endm

.macro DEJITCLEAN reg table scale=2
   mov temp,#\table
   str \reg,[temp,temp1,LSL #\scale]!

#if defined(RPI4)|| defined(RPI3)
   bic temp,temp,#0x3f // hack
      MCR p15, 0, temp, c7, c11, 1 // clean data cache line
      DSB
      MCR p15, 0, temp, c7, c5, 1 // invalidate I cache line
      MCR p15, 0, temp, c7, c5, 7 // invalidate BTB entry
      DSB
#else
   #if defined(RPI2)
      DSB
   #endif
      MCR p15, 0, temp, c7, c10, 1 // clean data cache line
      MCR p15, 0, temp, c7, c5, 1 // invalidate I cache line
      MCR p15, 0, temp, c7, c5, 7 // invalidate BTB entry
   #if defined(RPI2)
      DSB
   #endif
#endif
.endm

.macro DEJITCLEANMID reg table  scale=2
   mov temp,#\table
   str \reg,[temp,temp1,LSL #\scale]!
   MCR p15, 0, temp, c7, c5, 7 // invalidate BTB entry
.endm


// **** MACROS ******

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
   ldr temp,\start
   str temp,[r6]
.endm

.macro COPY2 start
   adr temp1,\start
   LDMIA temp1,{r7,r8}
   STMIA r6,{r7,r8}
.endm

.macro COPY3 start
   adr temp1,\start
   LDMIA temp1,{r7,r8,r9}
   STMIA r6,{r7,r8,r9}
.endm

.macro COPY4 start
   adr temp1,\start
   LDMIA temp1,{r7,r8,r9,r10}
   STMIA r6,{r7,r8,r9,r10}
.endm

.macro COPY5 start
   adr temp1,\start
   LDMIA temp1,{r7,r8,r9,r10,r11}
   STMIA r6,{r7,r8,r9,r10,r11}
.endm

.macro COPY6 start
   adr temp1,\start
   LDMIA temp1,{r7,r8,r9,r10,r11,r12}
   STMIA r6,{r7,r8,r9,r10,r11,r12}
.endm

.macro SETABS MSB=0 LSB=4
   mov temp,temp2,LSR #JITLETSHIFT
   ldrb temp0,[temp,#1]
   ldrb temp,[temp,#2]
   strb temp0,[r6,#\LSB]
   strb temp,[r6,#\MSB]
.endm

.macro SETBYTE LSB=0
   mov temp,temp2,LSR #JITLETSHIFT
   ldrb temp0,[temp,#1]
   strb temp0,[r6,#\LSB]
.endm

.macro SETBYTEHIGH LSB=0
   mov temp,temp2,LSR #JITLETSHIFT
   ldrb temp0,[temp,#2]
   strb temp0,[r6,#\LSB]
.endm

// Macro for pushing a byte to the stack, whilst correctly handing wrapping
// This relies on the 6502 memory being 256 byte aligned
.macro SPUSH reg
   strb    \reg, [regSP], #-1
   orr     regSP, #0x0100
.endm

.macro NOP6
   ldr temp,=ARMNOP
   str temp,[r6]
   B jitend1bytestore
.endm

.macro NOP6_1
   ldr temp,=0xEA000000+(NEXTJITLET>>2) // B $+8
   str temp,[r6]
   FILLINJITTEDTABLES dejit16bit1
   add temp2,temp2,#NEXTJITLET
   B dojitloop
.endm

.macro NOP6_2
   ldr temp,=0xEA000000+(NEXTJITLET>>1) // B $+C
   str temp,[r6]
   FILLINJITTEDTABLES dejit16bit1
   add temp2,temp2,#2*NEXTJITLET
   B dojitloop
.endm

.macro JITINDX
   add temp1,regX,#0xFF<<24
   mov temp1,temp1,LSR #24
   ldrh temp1,[temp1]
.endm

.macro JITINDXLOGICAL inst
   add temp1,regX,#0xFF<<24
   mov temp1,temp1,LSR #24
   BL .+8 // BL ldrhldrsb
   \inst regA,regA,temp
.endm

.macro JITINDYLOGICAL inst
   ldrh temp,[ram6502,#0x00]
   mov temp1,regY,LSR #24
   ldrsb temp,[temp,temp1]
   \inst regA,regA,temp
.endm

.macro JITINDLOGICAL inst
   ldrh temp,[ram6502,#0x00]
   nop
   ldrsb temp,[temp]
   \inst regA,regA,temp
.endm

.macro JITZPLOGICAL inst
   ldrsb temp,[ram6502,#0x00]
   nop
   nop
   \inst regA,regA,temp
.endm

.macro JITZPXLOGICAL inst
   add temp1,regX,#0xFF<<24
   mov temp1,temp1,LSR#24
   ldrsb temp,[temp1]
   \inst regA,regA,temp
.endm

.macro IMMLOAD block
   COPY4 \block
   mov temp,temp2,LSR #JITLETSHIFT
   ldrb temp,[temp,#1]
   ldr temp1,=immediatesignextenedtable
   ldr temp,[temp1,temp,LSL #2]
   str temp,[r6]
   B jitend2byte
.endm

.macro JITIMMLOGICAL inst
   mov temp,#00
   nop
   \inst regA,regA,temp
   nop
.endm

.macro JITABSLOGICAL inst
   JITABSLDRSB
   nop
   nop
   nop
   \inst regA,regA,temp
.endm

.macro JITABSYLOGICAL inst
   JITABS
   mov temp,regY,LSR #24
   ldrsb temp,[temp1,temp]
   nop
   \inst regA,regA,temp
.endm

.macro JITABSXLOGICAL inst
   JITABS
   mov temp,regX, LSR #24
   ldrsb temp,[temp1,temp]
   nop
   \inst regA,regA,temp
.endm

.macro JITRMB bitnum
   ldrb temp,[ram6502,#00]
   nop
   bic  temp, #(1 << \bitnum)
   strb temp,[ram6502,#00]
.endm

.macro JITSMB bitnum
   ldrb temp,[ram6502,#00]
   nop
   orr  temp, #(1 << \bitnum)
   strb temp,[ram6502,#00]
.endm

.macro BBRS block blout
   COPY6 \block
   SETBYTE

   // fill in BL
   ldr r0,=\blout
   add r1,r6,#12
   BL MAKEBRANCHLINK

   mov temp,temp2,LSR #JITLETSHIFT
   ldrsb temp,[temp,#2] // taken branch offset

   mov temp,temp,LSL #1
   BIC  temp,temp,#0xFF000000
   orr temp,temp,#BINSTRUCTION
   str temp,[r6,#16]
   B  jitend3byteexit
.endm

.macro JITBBR bitnum
   ldrb temp,[ram6502,#0x00]
   mrs     temp1, CPSR
   tst     temp, #(1 << \bitnum)
   BL bbrreturn

   //   addne pc,r14,#4 // return to jit
   //   msr     CPSR_flg, temp1
   //   mov pc,r14

   B .+8  // taken
   msr     CPSR_flg, temp1
.endm

.macro JITBBS bitnum
   ldrb temp,[ram6502,#0x00]
   mrs     temp1, CPSR
   tst     temp, #(1 << \bitnum)
   BL bbsreturn

   //   addne pc,r14,#4 // return to jit
   //   msr     CPSR_flg, temp1
   //   mov pc,r14

   B .+8  // taken
   msr     CPSR_flg, temp1
.endm

.macro BRANCH type
   mov temp0,temp2,LSR #JITLETSHIFT
   ldrsb temp0,[temp0,#1]
   ldr temp,=ARMNOP
// do jump maths
// temp0 is where we need to goto NB -2 ie. if we have 6 we goto +8
// so it look like arm and 6502 jumps are the same so if we load the bit signed
// we just need to clear the top byte
   mov temp0,temp0,LSL #1
   add temp0,temp0,#2
   BIC temp0,temp0,#0xFF000000
   orr temp0,temp0,#\type
   str temp0,[r6]
   str temp,[r6,#4]
   str temp,[r6,#8]
   b savelastwordjitend2byteexit
.endm

.macro BRANCHVCS block branch
   COPY4 \block

   // fill in BL
   ldr r0,=\branch
   add r1,r6,#0
   BL MAKEBRANCHLINK

   mov temp,temp2,LSR #JITLETSHIFT
   ldrsb temp,[temp,#1] // taken branch offset

   add temp0,temp2,temp,LSL#JITLETSHIFT
   add temp0,temp0,#16
   add temp1,r6,#8
   BL MAKEBRANCHTOJITLET

   B jitend2byteexit
.endm

.macro JITBRANCHVCS
   BL testvflagC
   //  mrs     temp1, CPSR
   //  tst     flags, #V_FLAG6502
   msr     CPSR_flg, temp1
   b    .+8 // branch taken
   msr     CPSR_flg, temp1
.endm

.macro SINGLEOPCODEBLOUT block address offset
   COPY1 \block
   ldr temp0,=\address
   add temp1,r6,#\offset
   BL MAKEBRANCHLINK
   B jitend1byte
.endm

.macro SINGLEOPCODEEXIT block address offset
   COPY1 \block
   ldr temp0,=\address
   add temp1,r6,#\offset
   BL MAKEBRANCH
   B jitend1byteexit
.endm

.macro SINGLEOPCODEBLEXIT block address offset
   COPY1 \block
   ldr temp0,=\address
   add temp1,r6,#\offset
   BL MAKEBRANCHLINK
   B jitend1byteexit
.endm

.macro SINGLEBYTE block
   COPY2 \block
   B jitend1byte
.endm

.macro DOUBLEBYTESETFIRSTBYTE block
   COPY4 \block
   SETBYTE 0
   B jitend2byte
.endm

.macro DOUBLEBYTESETFIRSTBYTEBLOUT block address offset
.ifc \offset,12
   COPY3 \block
.else
   COPY4 \block
.endif
   SETBYTE 0
   ldr temp0,=\address
   add temp1,r6,#\offset
   BL MAKEBRANCHLINK
   B jitend2byte
.endm

.macro DOUBLEBYTEBYTET2BLOUT block address offset
   COPY3 \block
   mov temp4,r6
   BL fixuptype2access
   ldr temp0,=\address
   add temp1,r6,#\offset
   BL MAKEBRANCHLINK
   B jitend2byte
.endm

.macro DOUBLEBYTET2 block
   COPY4 \block
   mov temp4,r6
   BL fixuptype2access
   B jitend2byte
.endm

.macro DOUBLEBYTESETTWOBYTES block offset=8
   COPY4 \block
   SETBYTE
   strb temp0,[r6,#\offset]
   B jitend2byte
.endm

.macro TRIPLEBYTEABS block
   COPY6 \block
   SETABS 0 4
   B  jitend3byte
.endm

.macro TRIPLEBYTEABST2 block
   COPY6 \block
   mov temp,temp2,LSR #JITLETSHIFT
   ldrb temp,[temp,#2]
   add temp4,r6,#4
   strb temp,[r6]
   BL fixuptype2access
   b jitend3byte
.endm

.macro TRIPLEBYTEABSBLOUT block address offset
.ifc offset,20
   COPY5 \block
.else
   COPY6 \block
.endif
   SETABS 0 4
   ldr temp0,=\address
   add temp1,r6,#\offset
   BL MAKEBRANCHLINK
   B jitend3byte
.endm

.macro ABSSTORE block
   COPY5 \block
   mov temp,temp2,LSR #JITLETSHIFT
   ldrh r14,[temp,#1]
   SETABS 0 16
   add temp1,r6,#20
   mov temp0,r14, LSL #2
   add temp0,temp0,#JITTEDTABLE16
   BL MAKEBRANCHLINK
   B  jitend3byte
.endm

.macro JITABSSTORE reg
   mov temp1,#0xFF<<8
   nop
.ifc \reg,regA
   nop
   nop
   strb regA,[temp1,#0xFF]!
.else
   mov temp0,\reg,LSR #24
   nop
   strb temp0,[temp1,#0xFF]!
.endif
.endm

.macro ASSIGNCARRY carry
   ldr temp0,=invertcarry
   sub temp0,temp0,r6
   sub temp0,temp0,#8-0
   mov temp0,temp0,LSR #2
.ifc \carry,clear
   orr temp0,temp0,#ARMBLCS
.else
   orr temp0,temp0,#ARMBLCC
.endif
   str temp0,[r6]
   ldr temp,=ARMNOP
   B jitend1bytestore
.endm

.macro JITABS reg=temp1
   mov \reg,#0xFF<<8
   orr \reg,\reg,#0xFF  // NB armv8 faster option
.endm

.macro JITABSLDRB
   mov temp1,#0xFF<<8
   ldrb temp,[temp1,#0x00]
.endm

.macro JITABSLDRBWB
   mov temp1,#0xFF<<8
   ldrb temp,[temp1,#0x00]!
.endm

.macro JITABSLDRSB
   mov temp1,#0xFF<<8
   ldrsb temp,[temp1,#0x00]
.endm

.macro ZPSTAZ block
   COPY4 \block
   SETBYTE
   strb temp0,[r6,#4]
   mov temp0,temp0,LSL #2
   add temp0,temp0,#JITTEDTABLE16
   add temp1,r6,#12
   BL MAKEBRANCHLINK
   b jitend2byte
.endm

.macro JITZPSTAZ reg
   strb  \reg,[ram6502,#0x00]
   mov temp1,#00
   nop
   b .+8
.endm

.macro ZPST block
   COPY4 \block
   SETBYTE 8
   strb temp0,[r6,#4]
   mov temp0,temp0,LSL #2
   add temp0,temp0,#JITTEDTABLE16
   add temp1,r6,#12
   BL MAKEBRANCHLINK
   b jitend2byte
.endm

.macro JITZPST reg
   mov temp,\reg,LSR #24
   mov temp1,#00
   strb temp,[ram6502,#0x00]
   B .+8
.endm

.macro JITABSXYST reg index
   JITABS
   strb \reg,[temp1,\index,LSR#24]!
   nop
   add temp,jittedtable16ptr,temp1,LSL#2
   blx temp
.endm

.macro ASL
   add   temp, temp, temp
   strb  temp,[temp1]
   movs  temp, temp, lsl #24
.endm

.macro ROL6
   adc  temp, temp, temp
   strb temp,[temp1]
   lsls    temp, temp, #24
.endm

.macro LSR
   lsrs temp, temp, #1
   strb temp,[temp1]
.endm

.macro ROR6
   orrcs   temp, temp, #0x100
   rrxs    temp, temp
   strb temp,[temp1]
.endm

.macro JITADCSBCABS
   JITABSLDRB
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
   LSL     regA, regA, #24
.endm

.macro JITADCSBCABSX
   JITABS
   ldrb  temp,[temp1, regX, lsr #24]
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
.endm

.macro JITADCSBCABSY
   JITABS
   ldrb  temp,[temp1, regY, lsr #24]
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
.endm

.macro JITADCSBCZP
   ldrb temp,[ram6502,#0x00]
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
.endm

.macro JITADCSBCIMM
   mov temp,#00 // NB could save a cycle later on
   tst     flags, #D_FLAG6502
   bic     flags, #V_FLAG6502
.endm

.macro JITADCSBCINDY
   ldrh temp1,[ram6502,#0x00]
   ldrb temp,[temp1,regY,LSR #24]
   tst     flags, #D_FLAG6502
.endm

.macro JITADCSBCIND
   ldrh temp1,[ram6502,#0x00]
   ldrb temp,[temp1]
   tst     flags, #D_FLAG6502
.endm

.macro JITADCSBCZPX
   add temp1,regX,#0xFF<<24
   ldrb temp,[ram6502,temp1,LSR#24]
   tst     flags, #D_FLAG6502
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
   mov   temp,#1
   uadd8 temp2,regSP,temp
   ldrb  temp2, [temp2]
   uadd8 regSP,regSP,temp

   and  temp, flags, #0xffffff00
   orr  temp, temp, temp2        // preserve the other bits in flags, e.g. the slowdown bit
   bic  flags, temp, #N_FLAG6502+X_FLAG6502+B_FLAG6502+Z_FLAG6502+C_FLAG6502
   and  temp, temp2, #N_FLAG6502
   and  temp2, temp2, #Z_FLAG6502+C_FLAG6502
   mov  temp, temp, lsl #24
   orr  temp, temp, temp2, lsl #29
   //   Need to update the ARM flags later with  msr     CPSR_flg, temp
   // temp1 return address
   // temp has 6502 flags to be restored to the ARM
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

.ltorg
opc_10: // Opcode 10 - BPL
   BRANCH BPLINSTRUCTION

opc_30: // Opcode 30 - BMI
   BRANCH BMIINSTRUCTION

opc_00: // Opcode 00 - BRK is really a two byte instruction
   SINGLEOPCODEBLEXIT Bopc_00 brk 4

opc_01: // Opcode 01 - ORA ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_01 ldrhldrsb 8

opc_04: // Opcode 04 - TSB $00
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_04 tsb 12

opc_05: // Opcode 05 - ORA $00
   DOUBLEBYTET2 Bopc_05

opc_06: // Opcode 06 - ASL $00
   DOUBLEBYTESETTWOBYTES Bopc_06

opc_07: // Opcode 07 - RMB0 $00
   DOUBLEBYTESETTWOBYTES Bopc_07 12

opc_08: // Opcode 08 - PHP
   SINGLEOPCODEBLOUT Bopc_08 php 4

opc_09: // Opcode 09 - ORA #$00
   IMMLOAD Bopc_09

opc_0A: // Opcode 0A - ASL A
   SINGLEBYTE Bopc_0A

opc_0C: // Opcode 0C - TSB $0000
   TRIPLEBYTEABSBLOUT Bopc_0C tsb2 20

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
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_14 trb 12

opc_15: // Opcode 15 - ORA $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_15

opc_16: // Opcode 16 - ASL $00, X
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_16 asl 12

opc_17: // Opcode 17 - RMB1 $00
   DOUBLEBYTESETTWOBYTES Bopc_17 12

opc_18: // Opcode 18 - CLC
   ASSIGNCARRY clear

opc_19: // Opcode 19 - ORA $0000,Y
   TRIPLEBYTEABS Bopc_19

opc_1A: // Opcode 1A - INC A
   SINGLEOPCODEBLOUT Bopc_1A aflags 4

opc_1C: // Opcode 1C - TRB $0000
   TRIPLEBYTEABSBLOUT Bopc_1C trb2 20

opc_1D: // Opcode 1D - ORA $0000,X
   TRIPLEBYTEABS Bopc_1D

opc_1E: // Opcode 1E - ASL $0000,X
   TRIPLEBYTEABS Bopc_1E

opc_1F: // Opcode 1F - BBR1
   BBRS Bopc_1F bbrreturn

opc_20:// Opcode 20 - JSR $0000
   COPY5 Bopc_20

   add temp0,temp2,#NEXTJITLET*2
   mov temp,temp0,LSR #JITLETSHIFT
   mov temp0,temp0,LSR #JITLETSHIFT+8

   strb temp,[r6,#4]
   strb temp0,[r6,#0]

   mov temp,temp2,LSR #JITLETSHIFT
   ldrh temp0,[temp,#1]
   add temp1,r6,#20
   mov temp0,temp0,LSL#JITLETSHIFT

   BL MAKEBRANCHTOJITLET

   B jitend3byteexit
Bopc_00:
   nop
Bopc_01:
   JITINDXLOGICAL orrs
Bopc_04:
   mov temp1,#0x00
   ldrb temp,[temp1]
   mrs temp0, CPSR
Bopc_05:
   JITZPLOGICAL orrs
Bopc_06:
   ldrb temp,[ram6502,#00]
   add   temp, temp, temp
   strb  temp,[ram6502,#00]
   movs  temp, temp, lsl #24
Bopc_07:
   JITRMB 0
Bopc_08:
   orr   temp, flags, #X_FLAG6502+B_FLAG6502
Bopc_09:
   JITIMMLOGICAL orrs
Bopc_0A:
   lsls    regA, regA, #25
   sxtb    regA, regA , ror #24
Bopc_0C:
   JITABSLDRBWB
   mrs temp0, CPSR
   bic temp0, temp0, #Z_FLAG
   tst temp, regA
Bopc_0D:
   JITABSLOGICAL orrs
Bopc_0E:
   JITABSLDRBWB
   nop
   ASL
Bopc_0F:
   JITBBR 0
Bopc_11:
   JITINDYLOGICAL orrs
Bopc_12:
   JITINDLOGICAL orrs
Bopc_14:
   mov temp1,#0x00
   ldrb temp,[temp1]
   mrs temp0, CPSR
Bopc_15:
   JITZPXLOGICAL orrs
Bopc_16:
   add temp1,regX,#0xFF<<24
   ldrb temp,[ram6502,temp1,LSR #24]
   mov temp1,temp1,LSR #24  // can save a cycle here
Bopc_17:
   JITRMB 1
Bopc_19:
   JITABSYLOGICAL orrs
Bopc_1A:
   add     regA, regA, #0x1
Bopc_1C:
   JITABSLDRBWB
   mrs temp0, CPSR
   bic temp0, temp0, #Z_FLAG
   tst temp, regA
Bopc_1D:
   JITABSXLOGICAL orrs
Bopc_1E:
   JITABS
   ldrb  temp,[temp1, regX, lsr #24]!
   ASL
Bopc_1F:
   JITBBR 1

Bopc_20:
   JITABS
   sub  regSP, regSP, #2
   strh temp1,[regSP,#1]
   orr  regSP, #0x0100

Bopc_21:
   JITINDXLOGICAL ands
Bopc_24:
   ldrb temp,[ram6502,#0x00]
   bic     flags, flags, #V_FLAG6502
   tst     temp, regA
Bopc_25:
   JITZPLOGICAL ands
Bopc_26:
   ldrb temp,[ram6502,#00]
   adc   temp, temp, temp
   strb  temp,[ram6502,#00]
   movs  temp, temp, lsl #24
Bopc_27:
   JITRMB 2
Bopc_28:
   nop
Bopc_29:
   JITIMMLOGICAL ands
Bopc_2A:
   adc     regA, regA, regA
Bopc_2C:
   mov temp1,#0xFF<<8
Bopc_2C_io:
   mov r0,#0xFF
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
   add  temp1,regX,#0xFF<<24
   ldrb temp,[ram6502,temp1,LSR #24]
   bic  flags, flags, #V_FLAG6502
Bopc_35:
   JITZPXLOGICAL ands
Bopc_36:
   add temp1,regX,#0xFF<<24
   mov temp1,temp1,LSR #24 // can save a cycle here
   ldrb temp,[temp1]
Bopc_37:
   JITRMB 3
Bopc_39:
   JITABSYLOGICAL ands
Bopc_3A:
   sub     regA, regA, #0x1
Bopc_3C:
   JITABS
   ldrb  temp,[temp1, regX, lsr #24]
   bic     flags, flags, #V_FLAG6502
   tst     temp, regA
Bopc_3D:
   JITABSXLOGICAL ands
Bopc_3E:
   JITABS
   ldrb  temp,[temp1, regX, lsr #24]!
   ROL6
Bopc_3F:
   JITBBR 3

opc_21: // Opcode 21 - AND ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_21 ldrhldrsb 8

opc_24: // Opcode 24 - BIT $00
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_24 bit2 12

opc_25: // Opcode 25 - AND $00
   DOUBLEBYTET2 Bopc_25

opc_26: // Opcode 26 - ROL $00
   DOUBLEBYTESETTWOBYTES Bopc_26

opc_27: // Opcode 27 - RMB2 $00
   DOUBLEBYTESETTWOBYTES Bopc_27 12

opc_28:// Opcode 28 - PLP
   SINGLEOPCODEBLOUT Bopc_28 plp 4

opc_29: // Opcode 29 - AND #$00
   IMMLOAD Bopc_29

opc_2A: // Opcode 2A - ROL A
   SINGLEOPCODEBLOUT Bopc_2A rola 4

opc_2C: // Opcode 2C - BIT $0000

   mov temp,temp2,LSR #JITLETSHIFT
   ldrh temp0,[temp,#1]

   mov temp,#(0xFEE0>>5) & 0xFF
   orr temp,temp, #(0xFEE0>>5) & 0xFF00
   cmp temp, temp0, lsr #5  //  constant 0xFEE0 >> 5
   BEQ bitabsio

   COPY1 Bopc_2C
   SETBYTEHIGH
   ldr temp0,=bitabstable
   ldrb temp1,[temp,#1]
   add temp0,temp0,temp1,LSL #3
   add temp1,r6,#4
   BL MAKEBRANCHLINK
   FILLINJITTEDTABLES dejit16bit1
   add temp2,temp2,#2*NEXTJITLET
   B dojitloop

bitabsio:
   COPY1 Bopc_2C_io
   SETBYTE
   ldr temp0,=bitioload
   add temp1,r6,#4
   BL MAKEBRANCHLINK
   FILLINJITTEDTABLES dejit16bit1
   add temp2,temp2,#2*NEXTJITLET
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
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_34 bit 12

opc_35: // Opcode 35 - AND $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_35

opc_36: // Opcode 36 - ROL $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_36 rol 12

opc_37: // Opcode 37 - RMB3 $00
   DOUBLEBYTESETTWOBYTES Bopc_37 12

opc_38: // Opcode 38 - SEC
   ASSIGNCARRY set

opc_39: // Opcode 39 - AND $0000,Y
   TRIPLEBYTEABS Bopc_39

opc_3A: // Opcode 3A - DEC A
   SINGLEOPCODEBLOUT Bopc_3A aflags 4

opc_3C: // Opcode 3C - BIT $0000,X
   TRIPLEBYTEABSBLOUT Bopc_3C bit2 20

opc_3D: // Opcode 3D - AND $0000,X
   TRIPLEBYTEABS Bopc_3D

opc_3E: // Opcode 3E - ROL $0000,X
   TRIPLEBYTEABS Bopc_3E

opc_3F: // Opcode 3F - BBR3
   BBRS Bopc_3F bbrreturn

opc_40: // Opcode 40 - RTI
   SINGLEOPCODEBLEXIT Bopc_40 rti 4
opc_41: // Opcode 41 - EOR ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_41 ldrhldrsb 8

opc_45: // Opcode 45 - EOR $00
   DOUBLEBYTET2 Bopc_45

opc_46: // Opcode 46 - LSR $00
   DOUBLEBYTESETTWOBYTES Bopc_46 12

opc_47: // Opcode 47 - RMB4 $00
   DOUBLEBYTESETTWOBYTES Bopc_47 12

opc_48: // Opcode 48 - PHA
   SINGLEBYTE Bopc_48

opc_49: // Opcode 49 - EOR #$00
   IMMLOAD Bopc_49

opc_4A: // Opcode 4A - LSR A
   SINGLEBYTE Bopc_4A

opc_4C: // Opcode 4C - JMP $0000
   mov temp,temp2,LSR #JITLETSHIFT
   ldrh temp0,[temp,#1]
   mov temp0,temp0,LSL#JITLETSHIFT
   add temp1,temp2,#JITLET
   BL MAKEBRANCHTOJITLET
   B jitend3byteexit

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
   DOUBLEBYTESETTWOBYTES Bopc_57 12

opc_58: // Opcode 58 - CLI
   SINGLEOPCODEBLOUT Bopc_58 cli 4

opc_59: // Opcode 59 - EOR $0000,Y
   TRIPLEBYTEABS Bopc_59

opc_5A: // Opcode 5A - PHY
   SINGLEOPCODEBLOUT Bopc_5A pushtemp 4

opc_5D: // Opcode 5D - EOR $0000,X
   TRIPLEBYTEABS Bopc_5D

opc_5E: // Opcode 5E - LSR $0000,X
   TRIPLEBYTEABS Bopc_5E

opc_5F: // Opcode 5F - BBR5
   BBRS Bopc_5F bbrreturn

Bopc_40:
   nop
Bopc_41:
   JITINDXLOGICAL eors
Bopc_45:
   JITZPLOGICAL eors
Bopc_46:
   ldrb temp,[ram6502,#00]
   nop
   lsrs temp, temp, #1
   strb  temp,[ram6502,#00]
Bopc_47:
   JITRMB 4
Bopc_48:
   SPUSH regA
Bopc_49:
   JITIMMLOGICAL eors
Bopc_4A:
   lsrs regA, regA, #1
   and  regA, regA, #0x7F
Bopc_4D:
   JITABSLOGICAL eors
Bopc_4E:
   JITABSLDRBWB
   nop
   nop
   LSR
Bopc_4F:
   JITBBR 4
Bopc_50:
   JITBRANCHVCS
Bopc_51:
   JITINDYLOGICAL eors
Bopc_52:
   JITINDLOGICAL eors
Bopc_55:
   JITZPXLOGICAL eors
Bopc_56:
   add temp1,regX,#0xFF<<24
   ldrb temp,[ram6502,temp1,LSR #24]
   lsrs temp, temp, #1
   strb temp,[ram6502,temp1,LSR #24]
Bopc_57:
   JITRMB 5
Bopc_58:
   bic     flags, flags, #I_FLAG6502
Bopc_59:
   JITABSYLOGICAL eors
Bopc_5A:
   mov  temp, regY,LSR  #24
Bopc_5D:
   JITABSXLOGICAL eors
Bopc_5E:
   JITABS
   ldrb  temp,[temp1, regX, lsr #24]!
   nop
   LSR
Bopc_5F:
   JITBBR 5
.ltorg
Bopc_60:
   ldrh temp0,[regSP,#1]
Bopc_61:
   JITINDX
Bopc_64:
   JITZPSTAZ ram6502
Bopc_65:
   JITADCSBCZP
Bopc_66:
   ldrb temp,[ram6502,#00]
   orrcs   temp, temp, #0x100
   rrxs    temp, temp
   strb temp,[ram6502,#00]
Bopc_67:
   JITRMB 6
Bopc_68:
   mov temp,#1
Bopc_69:
   JITADCSBCIMM
Bopc_6A:
   and     regA, regA, #0xFF    // isolate lower byte
Bopc_6C:
   mov temp1,#0xFF<<8
   ldrh temp,[temp1,#0x00]
   nop
   mov temp0,#JITLET
   add temp0,temp0,temp,LSL #JITLETSHIFT
   bx temp0
Bopc_6D:
   JITADCSBCABS
Bopc_6E:
   JITABSLDRBWB
   nop
   ROR6
Bopc_6F:
   JITBBR 6
Bopc_70:
   JITBRANCHVCS
Bopc_71:
   JITADCSBCINDY
Bopc_72:
   JITADCSBCIND
Bopc_74:
   add temp1,regX,#0xFF<<24
   strb ram6502,[ram6502,temp1,LSR #24]
   add temp,jittedtable16ptr, temp1,LSR #24-2
   BLX temp

Bopc_75:
   JITADCSBCZPX
Bopc_76:
   add temp1,regX,#0xFF<<24
   mov temp1,temp1,LSR #24
   ldrb temp,[temp1]
Bopc_77:
   JITRMB 7
Bopc_78:
   orr     flags, flags, #I_FLAG6502
   nop
Bopc_79:
   JITADCSBCABSY
Bopc_7A:
   mov temp,#1
Bopc_7C:
   JITABS
   mov temp, regX, LSR #24
   ldrh temp,[temp, temp1]
   mov temp0,#JITLET
   add pc,temp0,temp,LSL #JITLETSHIFT
Bopc_7D:
   JITADCSBCABSX
Bopc_7E:
   JITABS
   ldrb  temp,[temp1, regX, lsr #24]!
   ROR6
Bopc_7F:
   JITBBR 7

opc_60: // Opcode 60 - RTS
   SINGLEOPCODEEXIT Bopc_60 rts 4

opc_61: // Opcode 61 - ADC ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_61 ADCload 12

opc_64: // Opcode 64 - STZ $00
   ZPSTAZ Bopc_64

opc_65: // Opcode 65 - ADC $00
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_65 ADC2 12

opc_66: // Opcode 66 - ROR $00
   DOUBLEBYTESETTWOBYTES Bopc_66 12

opc_67: // Opcode 67 - RMB6 $00
   DOUBLEBYTESETTWOBYTES Bopc_67 12

opc_68: // Opcode 68 - PLA
   SINGLEOPCODEBLOUT Bopc_68 pla 4

opc_69: // Opcode 69 - ADC #$00
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_69 ADC2 12

opc_6A: // Opcode 6A - ROR A
   SINGLEOPCODEBLOUT Bopc_6A rora 4

opc_6C: // Opcode 6C - JMP ($0000)
   COPY6 Bopc_6C
   mov temp,temp2,LSR #JITLETSHIFT
   ldrb temp,[temp,#2]
   add temp4,r6,#4
   strb temp,[r6]
   BL fixuptype2access
   b jitend3byteexit

opc_6D: // Opcode 6D - ADC $0000
   TRIPLEBYTEABSBLOUT Bopc_6D ADC3 20

opc_6E: // Opcode 6E - ROR $0000
   TRIPLEBYTEABS Bopc_6E

opc_6F: // Opcode 6F - BBR6
   BBRS Bopc_6F bbrreturn

opc_70: // Opcode 70 - BVS
   BRANCHVCS Bopc_70 testvflagS

opc_71: // Opcode 71 - ADC ($00),Y
   DOUBLEBYTEBYTET2BLOUT Bopc_71 ADC1 12

opc_72: // Opcode 72 - ADC ($00)
   DOUBLEBYTEBYTET2BLOUT Bopc_72 ADC1 12

opc_74: // Opcode 74 - STZ $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_74

opc_75: // Opcode 75 - ADC $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_75 ADC1 12

opc_76: // Opcode 76 - ROR $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_76 ror 12

opc_77: // Opcode 77 - RMB7 $00
   DOUBLEBYTESETTWOBYTES Bopc_77 12

opc_78: // Opcode 78 - SEI
   SINGLEBYTE Bopc_78

opc_79: // Opcode 79 - ADC $0000,Y
   TRIPLEBYTEABSBLOUT Bopc_79 ADC2 20

opc_7A: // Opcode 7A - PLY
   SINGLEOPCODEBLOUT Bopc_7A ply 4

opc_7C: // Opcode 7C - JMP ($0000,X)
   COPY6 Bopc_7C
   SETABS 0 4
   B  jitend3byteexit

opc_7D: // Opcode 7D - ADC $0000,X
   TRIPLEBYTEABSBLOUT Bopc_7D ADC2 20

opc_7E: // Opcode 7E - ROR $0000,X
   TRIPLEBYTEABS Bopc_7E

opc_7F: // Opcode 7F - BBR7
   BBRS Bopc_7F bbrreturn

opc_80: // Opcode 80 - BRA
   BRANCH BINSTRUCTION

opc_81: // Opcode 81 - STA ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_81 jitstatable 12

opc_84: // Opcode 84 - STY $00
   ZPST Bopc_84

opc_85: // Opcode 85 - STA $00
   ZPSTAZ Bopc_85

opc_86: // Opcode 86 - STX $00
   ZPST Bopc_86

opc_87: // Opcode 87 - SMB0 $00
   DOUBLEBYTESETTWOBYTES Bopc_87 12

opc_88: // Opcode 88 - DEY
   SINGLEBYTE Bopc_88

opc_89: // Opcode 89 - BIT #$00
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_89 bitimm 12

opc_8A: // Opcode 8A - TXA
   SINGLEBYTE Bopc_8A

opc_8C: // Opcode 8C - STY $0000
   mov temp,temp2,LSR #JITLETSHIFT
   ldrh temp0,[temp,#1]
   mov temp,#(0xFEE0>>5) & 0xFF
   orr temp,temp, #(0xFEE0>>5) & 0xFF00
   cmp temp, temp0, lsr #5  // constant 0xFEE0 >> 5
   BEQ 9f
   ABSSTORE Bopc_8C
9:
   TRIPLEBYTEABSBLOUT Bopc_8C_io iostore 20

opc_8D: // Opcode 8D - STA $0000
   mov temp,temp2,LSR #JITLETSHIFT
   ldrh temp0,[temp,#1]
   mov temp,#(0xFEE0>>5) & 0xFF
   orr temp,temp, #(0xFEE0>>5) & 0xFF00
   cmp temp, temp0, lsr #5  // constant 0xFEE0 >> 5
   BEQ 9f

   ABSSTORE Bopc_8D

9:
   TRIPLEBYTEABSBLOUT Bopc_8D_io iostore 20

opc_8E: // Opcode 8E - STX $0000
   mov temp,temp2,LSR #JITLETSHIFT
   ldrh temp0,[temp,#1]

   mov temp,#(0xFEE0>>5) & 0xFF
   orr temp,temp, #(0xFEE0>>5) & 0xFF00
   cmp temp, temp0, lsr #5  // constant 0xFEE0 >> 5
   BEQ 9f

   ABSSTORE Bopc_8E
9:
   TRIPLEBYTEABSBLOUT Bopc_8E_io iostore 20

opc_8F: // Opcode 8F - BBS0
   BBRS Bopc_8F bbsreturn

Bopc_81:
   add temp1,regX,#0xFF<<24
   mov temp1,temp1,LSR #24
   ldrh temp1,[temp1]
Bopc_84:
   JITZPST regY
Bopc_85:
   JITZPSTAZ regA
Bopc_86:
   JITZPST regX
Bopc_87:
   JITSMB 0
Bopc_88:
   sub     regY, regY, #0x1000000
   teq     regY, #0
Bopc_89:
   mov temp,#0x00
   mrs     temp1, CPSR
   tst     regA, temp // *** can save a cycle here
Bopc_8A:
   sxtb    regA, regX, ror #24
   TEQ     regA, #0
Bopc_8C:
   JITABSSTORE regY
Bopc_8C_io:
   JITABS
   mov r0,temp1 // could save a cycle
   mov r1,regY,LSR #24
   nop
Bopc_8D:
   JITABSSTORE regA
Bopc_8D_io:
   JITABS
   mov r0,temp1 // could save a cycle
   and r1,regA,#0xFF
   nop
Bopc_8E:
   JITABSSTORE regX
Bopc_8E_io:
   JITABS
   mov r0,temp1 // could save a cycle
   mov r1,regX,LSR #24
   nop
Bopc_8F:
   JITBBS 0
Bopc_91:
   ldrh temp1,[ram6502,#00]
   strb regA,[temp1,regY,LSR #24]!
   add temp,jittedtable16ptr, temp1,LSL #2
   BLX temp

Bopc_92:
   ldrh temp1,[ram6502,#00]
   strb regA,[temp1]
   add temp,jittedtable16ptr, temp1,LSL #2
   BLX temp

Bopc_94:
   add temp1,regX,#0xFF<<24
   mov temp,regY,LSR #24
   strb temp,[ram6502,temp1,LSR #24]
Bopc_95:
   add temp1,regX,#0xFF<<24
   strb regA,[ram6502,temp1,LSR #24]
   add temp,jittedtable16ptr, temp1,LSR #24-2
   BLX temp

Bopc_96:
   add temp1,regY,#0xFF<<24
   mov temp,regX,LSR #24
   strb temp,[ram6502,temp1,LSR #24]
Bopc_97:
   JITSMB 1
Bopc_98:
   sxtb    regA, regY, ror #24
   TEQ     regA, #0
Bopc_99:
   JITABSXYST regA regY
Bopc_9A:
   bic     regSP, regSP, #0xFF             // clear lower nibble
   orr     regSP, regSP, regX, lsr #24     // put X in lower nibble
Bopc_9C:
   JITABSSTORE ram6502
Bopc_9D:
   JITABSXYST regA regX
Bopc_9E:
   JITABSXYST ram6502 regX
Bopc_9F:
   JITBBS 1

opc_90: // Opcode 90 - BCC
   BRANCH BCCINSTRUCTION

opc_91: // Opcode 91 - STA ($00),Y
   DOUBLEBYTET2 Bopc_91

opc_92: // Opcode 92 - STA ($00)
   DOUBLEBYTET2 Bopc_92

opc_94: // Opcode 94 - STY $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_94 jitstxytable 12

opc_95: // Opcode 95 - STA $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_95

opc_96: // Opcode 96 - STX $00,Y
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_96 jitstxytable 12

opc_97: // Opcode 97 - SMB1 $00
   DOUBLEBYTESETTWOBYTES Bopc_97 12

opc_98: // Opcode 98 - TYA
   SINGLEBYTE Bopc_98

opc_99: // Opcode 99 - STA $0000,Y
   TRIPLEBYTEABS Bopc_99

opc_9A: // Opcode 9A - TXS
   SINGLEBYTE Bopc_9A

opc_9C: // Opcode 9C - STZ $0000
   ABSSTORE Bopc_9C

opc_9D: // Opcode 9D - STA $0000,X
   TRIPLEBYTEABS Bopc_9D

opc_9E: // Opcode 9E - STZ $0000,X
   TRIPLEBYTEABS Bopc_9E

opc_9F: // Opcode 9F - BBS1
   BBRS Bopc_9F bbsreturn
Bopc_A0:
   mov regY,#0xFF<<24
   nop
   teq regY,#0
   nop
Bopc_A1:
   add temp1,regX,#0xFF<<24
   mov temp1,temp1,LSR#24
   ldrh temp,[temp1]
Bopc_A2:
   mov regX,#0xFF<<24
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
Bopc_A8:
   lsl   regY, regA, #24
   TEQ   regA, #0
Bopc_A9:
   mov regA,#00
   nop
   nop
   nop
Bopc_AA:
   lsl    regX, regA, #24
   TEQ    regA, #0
Bopc_AC:
   JITABSLDRB
   nop
   nop
   mov regY,temp, LSL #24
   teq regY,#0
Bopc_AC_io:
   JITABS r0
   BL ioload
   mov regY,r0,LSL #24
   nop
   teq regY,#0
Bopc_AD:
   mov temp1,#0xFF<<8
   ldrsb regA,[temp1,#0x00]
   nop
   nop
   nop
   teq regA,#0
Bopc_AD_io:
   JITABS r0
   BL ioload
   sxtb regA,r0
   nop
   teq regA,#0
Bopc_AE:
   JITABSLDRB
   nop
   nop
   mov regX,temp, LSL #24
   teq regX,#0
Bopc_AE_io:
   JITABS r0
   BL ioload
   mov regX,r0,LSL #24
   nop
   teq regX,#0
Bopc_AF:
   JITBBS 2
.ltorg
opc_A0: // Opcode A0 - LDY #$00
   DOUBLEBYTESETFIRSTBYTE Bopc_A0

opc_A1: // Opcode A1 - LDA ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_A1 loada 12

opc_A2: // Opcode A2 - LDX #$00
   DOUBLEBYTESETFIRSTBYTE Bopc_A2

opc_A4: // Opcode A4 - LDY $00
   DOUBLEBYTESETFIRSTBYTE Bopc_A4

opc_A5: // Opcode A5 - LDA $00
   DOUBLEBYTET2 Bopc_A5

opc_A6: // Opcode A6 - LDX $00
   DOUBLEBYTESETFIRSTBYTE Bopc_A6

opc_A7: // Opcode A7 - SMB2 $00
   DOUBLEBYTESETTWOBYTES Bopc_A7 12

opc_A8: // Opcode A8 - TAY
   SINGLEBYTE Bopc_A8

opc_A9: // Opcode A9 - LDA #$00
   COPY4 Bopc_A9

   mov temp,temp2,LSR #JITLETSHIFT
   ldrb temp,[temp,#1]
   ldr temp1,=immediatesignextenedtable
   ldr temp,[temp1,temp,LSL #2]
   orr temp,temp,#1<<20// S FLAG
   bic temp,temp,#0xF000
   orr temp,temp,#0xA000 // regA
   str temp,[r6]
   B jitend2byte

opc_AA: // Opcode AA - TAX
   SINGLEBYTE Bopc_AA

opc_AC: // Opcode AC - LDY $0000

   mov temp,temp2,LSR #JITLETSHIFT
   ldrh temp0,[temp,#1]

   mov temp,#(0xFEE0>>5) & 0xFF
   orr temp,temp, #(0xFEE0>>5) & 0xFF00
   cmp temp, temp0, lsr #5  // constant 0xFEE0 >> 5
   BEQ 9f

   TRIPLEBYTEABS Bopc_AC

9:
   TRIPLEBYTEABSBLOUT Bopc_AC_io ioload 8

opc_AD: // Opcode AD - LDA $0000
   mov temp,temp2,LSR #JITLETSHIFT
   ldrh temp0,[temp,#1]

   mov temp,#(0xFEE0>>5) & 0xFF
   orr temp,temp, #(0xFEE0>>5) & 0xFF00
   cmp temp, temp0, lsr #5  // constant 0xFEE0 >> 5
   BEQ loadio
   // ramload
   TRIPLEBYTEABST2 Bopc_AD

loadio:

   TRIPLEBYTEABSBLOUT Bopc_AD_io ioload 8

opc_AE: // Opcode AE - LDX $0000
   mov temp,temp2,LSR #JITLETSHIFT
   ldrh temp0,[temp,#1]

   mov temp,#(0xFEE0>>5) & 0xFF
   orr temp,temp, #(0xFEE0>>5) & 0xFF00
   cmp temp, temp0, lsr #5  // constant 0xFEE0 >> 5
   BEQ 9f

   TRIPLEBYTEABS Bopc_AE

9:
   TRIPLEBYTEABSBLOUT Bopc_AE_io ioload 8

opc_AF: // Opcode AF - BBS2
   BBRS Bopc_AF bbsreturn

opc_B0: // Opcode B0 - BCS
   BRANCH BCSINSTRUCTION

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
   DOUBLEBYTESETTWOBYTES Bopc_B7 12

opc_B8: // Opcode B8 - CLV
   SINGLEBYTE Bopc_B8

opc_B9: // Opcode B9 - LDA $0000,Y
   TRIPLEBYTEABS Bopc_B9

opc_BA: // Opcode BA - TSX
   SINGLEBYTE Bopc_BA

opc_BC: // Opcode BC - LDY $0000,X
   TRIPLEBYTEABS Bopc_BC

opc_BD: // Opcode BD - LDA $0000,X
   TRIPLEBYTEABS Bopc_BD

opc_BE: // Opcode BE - LDX $0000,Y
   TRIPLEBYTEABS Bopc_BE

opc_BF: // Opcode BF - BBS3
   BBRS Bopc_BF bbsreturn

Bopc_B1:
   ldrh temp,[ram6502,#0x00]
   mov temp1,regY,LSR #24
   ldrsb regA,[temp1,temp]
   teq   regA,#0
Bopc_B2:
   ldrh temp,[ram6502,#0x00]
   nop
   ldrsb regA,[temp]
   teq   regA,#0
Bopc_B4:
   add temp1,regX,#0xFF<<24
   ldrb temp,[ram6502,temp1,LSR#24]
   mov regY,temp,LSL #24
   teq   regY,#0
Bopc_B5:
   add temp,regX,#0xFF<<24
   mov temp,temp,LSR#24
   ldrsb regA,[temp]
   teq   regA,#0
Bopc_B6:
   add temp1,regY,#0xFF<<24
   ldrb temp,[ram6502,temp1,LSR#24]
   mov regX,temp,LSL #24
   teq   regX,#0
Bopc_B7:
   JITSMB 3
Bopc_B8:
   nop
   bic     flags, #V_FLAG6502
Bopc_B9:
   JITABS
   mov  temp, regY, lsr #24
   ldrsb regA,[temp1, temp]
   nop
   teq  regA, #0
Bopc_BA:
   lsl     regX, regSP, #24
   teq     regX, #0
Bopc_BC:
   JITABS
   ldrb regY,[temp1, regX, lsr #24]
   nop
   mov  regY,regY, lsl #24
   teq  regY, #0
Bopc_BD:
   JITABS
   mov  temp, regX, lsr #24
   ldrsb regA,[temp1, temp]
   nop
   teq  regA, #0
Bopc_BE:
   JITABS
   ldrb regX,[temp1, regY, lsr #24]
   nop
   mov  regX,regX, lsl #24
   teq  regX, #0
Bopc_BF:
   JITBBS 3
Bopc_C0:
   cmp     regY, #0xFF<<24
   nop
   nop
   nop
Bopc_C1:
   add temp1,regX,#0xFF<<24
   mov temp1,temp1,LSR#24
   ldrh temp1,[temp1]
Bopc_C4:
   ldrb temp,[ram6502,#0x00]
   nop
   nop
   cmp regY, temp, lsl #24
Bopc_C5:
   ldrb temp,[ram6502,#0x00]
   mov temp1,regA,LSL #24
   nop
   cmp temp1, temp, LSL #24
Bopc_C6:
   ldrb temp,[ram6502,#0xFF]
   sub  temp, temp, #1
   strb temp,[ram6502,#0xFF]
Bopc_C7:
   JITSMB 4
Bopc_C8:
   add     regY, regY, #0x1000000
   teq     regY, #0
Bopc_C9:
   mov temp,#0xFF<<24
   nop
   rsbs temp,temp,regA,LSL #24
   nop
Bopc_CA:
   sub     regX, regX, #0x1000000
   teq     regX, #0
Bopc_CC:
   JITABSLDRB
   nop
   nop
   nop
   cmp regY,temp, lsl #24
Bopc_CD:
   JITABSLDRB
   nop
   mov temp1,regA,LSL #24
   nop
   cmp temp1,temp,LSL #24
Bopc_CE:
   JITABSLDRBWB
   sub  temp, temp, #1
   strb temp,[temp1]
   sxtb temp, temp
   teq  temp, #0
Bopc_CF:
   JITBBS 4
Bopc_D1:
   ldrh temp,[ram6502,#0x00]
   ldrb temp,[temp,regY, LSR #24]
   mov temp1,regA, LSL #24
   cmp temp1,temp,LSL #24
Bopc_D2:
   ldrh temp,[ram6502,#0x00]
   ldrb temp,[temp]
   mov temp1,regA, LSL #24
   cmp temp1,temp,LSL #24
Bopc_D5:
   add temp1,regX,#0xFF<<24
   ldrb temp,[ram6502,temp1,LSR#24]
   mov temp1,regA,LSL #24
   cmp temp1,temp,LSL #24
Bopc_D6:
   add temp1,regX,#0xFF<<24
   ldrb temp,[ram6502,temp1,LSR#24]
   sub  temp, temp, #1
Bopc_D7:
   JITSMB 5
Bopc_D8:
   nop
   bic     flags, flags, #D_FLAG6502
Bopc_D9:
   JITABS
   ldrb temp,[temp1,regY,LSR #24]
   nop
   mov temp1,regA,LSL #24
   cmp temp1,temp,LSL #24
Bopc_DA:
   mov     temp, regX, LSR #24
Bopc_DD:
   JITABS
   ldrb temp,[temp1,regX,LSR #24]
   nop
   mov temp1,regA, LSL #24
   cmp temp1,temp,LSL #24
Bopc_DE:
   JITABS
   ldrb temp,[temp1, regX, lsr #24]!
   sub  temp, temp, #1
   strb temp,[temp1]
Bopc_DF:
   JITBBS 5

opc_C0: // Opcode C0 - CPY #$00
   DOUBLEBYTESETFIRSTBYTE Bopc_C0

opc_C1: // Opcode C1 - CMP ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_C1 cmpa 12

opc_C4: // Opcode C4 - CPY $00
   DOUBLEBYTESETFIRSTBYTE Bopc_C4

opc_C5: // Opcode C5 - CMP $00
   DOUBLEBYTESETFIRSTBYTE Bopc_C5

opc_C6: // Opcode C6 - DEC $00
   COPY3 Bopc_C6
   SETBYTE
   strb temp0,[r6,#8]
   ldr temp0,=tempflags
   add temp1,r6,#12
   BL MAKEBRANCHLINK
   B jitend2byte

opc_C7: // Opcode C7 - SMB4 $00
   DOUBLEBYTESETTWOBYTES Bopc_C7 12

opc_C8: // Opcode C8 - INY
   SINGLEBYTE Bopc_C8

opc_C9: // Opcode C9 - CMP #$00
   DOUBLEBYTESETFIRSTBYTE Bopc_C9

opc_CA: // Opcode CA - DEX
   SINGLEBYTE Bopc_CA

opc_CC: // Opcode CC - CPY $0000
   TRIPLEBYTEABS Bopc_CC

opc_CD: // Opcode CD - CMP $0000
   TRIPLEBYTEABS Bopc_CD

opc_CE: // Opcode CE - DEC $0000
   TRIPLEBYTEABS Bopc_CE

opc_CF: // Opcode CF - BBS4
   BBRS Bopc_CF bbsreturn

opc_D0: // Opcode D0 - BNE
   BRANCH BNEINSTRUCTION

opc_D1: // Opcode D1 - CMP ($00),Y
   DOUBLEBYTET2 Bopc_D1

opc_D2: // Opcode D2 - CMP ($00)
   DOUBLEBYTET2 Bopc_D2

opc_D5: // Opcode D5 - CMP $00,X
   DOUBLEBYTESETFIRSTBYTE Bopc_D5

opc_D6: // Opcode D6 - DEC $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_D6 storeXflags 12

opc_D7: // Opcode D7 - SMB5 $00
   DOUBLEBYTESETTWOBYTES Bopc_D7 12

opc_D8: // Opcode D8 - CLD
   SINGLEBYTE Bopc_D8

opc_D9: // Opcode D9 - CMP $0000,Y
   TRIPLEBYTEABS Bopc_D9

opc_DA: // Opcode DA - PHX
   SINGLEOPCODEBLOUT Bopc_DA pushtemp 4

opc_DD: // Opcode DD - CMP $0000,X
   TRIPLEBYTEABS Bopc_DD

opc_DE: // Opcode DE - DEX $0000,X
   TRIPLEBYTEABSBLOUT Bopc_DE tempflags 20

opc_DF: // Opcode DF - BBS5
   BBRS Bopc_DF bbsreturn

opc_E0: // Opcode E0 - CPX #$00
   DOUBLEBYTESETFIRSTBYTE Bopc_E0

opc_E1: // Opcode E1 - SBC ($00,X)
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_E1 SBCload 12

opc_E3: // Opcode E3 - NOP6
   SINGLEOPCODEBLOUT Bopc_E3 debugopcode 4

opc_E4: // Opcode E4 - CPX $00
   DOUBLEBYTESETFIRSTBYTE Bopc_E4

opc_E5: // Opcode E5 - SBC $00
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_E5 SBC2 12

opc_E6: // Opcode E6 - INC $00
   COPY3 Bopc_E6
   SETBYTE
   strb temp0,[r6,#8]
   ldr temp0,=tempflags
   add temp1,r6,#12
   BL MAKEBRANCHLINK
   B jitend2byte

opc_E7: // Opcode E7 - SMB6 $00
   DOUBLEBYTESETTWOBYTES Bopc_E7 12

opc_E8: // Opcode E8 - INX
   SINGLEBYTE Bopc_E8

opc_E9: // Opcode E9 - SBC #$00
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_E9 SBC2 12

opc_EB: // Opcode EB - NOP6 ( Borrowed for instruction timing)
   SINGLEBYTE Bopc_EB

opc_EC: // Opcode EC - CPX $0000
   TRIPLEBYTEABS Bopc_EC

opc_ED: // Opcode ED - SBC $0000
   TRIPLEBYTEABSBLOUT Bopc_ED SBC3 20

opc_EE: // Opcode EE - INC $0000
   COPY5 Bopc_EE
   SETABS 0 4

   LDR temp0,=incstore
   add temp1,r6,#8
   BL MAKEBRANCHLINK

   mov temp,temp2,LSR #JITLETSHIFT
   ldrh temp0,[temp,#1]
   add temp1,r6,#20
   mov temp0,temp0, LSL #2
   add temp0,temp0,#JITTEDTABLE16
   BL MAKEBRANCHLINK
   B  jitend3byte

opc_EF: // Opcode EF - BBS6
   BBRS Bopc_EF bbsreturn

Bopc_E0:
   cmp     regX, #0xFF<<24
   nop
   nop
   nop
Bopc_E1:
   JITINDX
Bopc_E3:
   mov temp,#1
Bopc_E4:
   ldrb temp,[ram6502,#0x00]
   nop
   nop
   cmp regX, temp, lsl #24
Bopc_E5:
   JITADCSBCZP
Bopc_E6:
   ldrb temp,[ram6502,#0xFF]
   add  temp, temp, #1
   strb temp,[ram6502,#0xFF]
Bopc_E7:
   JITSMB 6
Bopc_E8:
   add     regX, regX, #0x1000000
   teq     regX, #0
Bopc_E9:
   JITADCSBCIMM
Bopc_EB:
#if defined(TIMING_INSTRUCTION)
#if defined(RPI2)|| defined(RPI3) || defined(RPI4)
   mrc     p15, 0, temp0, c9, c13, 0
#else
   MRC     p15, 0, temp0, c15, c12, 1    // Read Cycle Counter Register
#endif
   str     temp0,[ram6502,#0xE0]
#else
   nop
   nop
#endif
Bopc_EC:
   JITABSLDRB
   nop
   nop
   nop
   cmp regX,temp, lsl #24
Bopc_ED:
   JITADCSBCABS
Bopc_EE:
   JITABSLDRBWB
   BL .+8
   sxtb temp, temp
   teq  temp, #0
Bopc_EF:
   JITBBS 6
Bopc_F1:
   JITADCSBCINDY
Bopc_F2:
   JITADCSBCIND
Bopc_F3:
   nop
Bopc_F5:
   JITADCSBCZPX
Bopc_F6:
   add temp1,regX,#0xFF<<24
   ldrb temp,[ram6502,temp1,LSR#24]
   add  temp, temp, #1
Bopc_F7:
   JITSMB 7
Bopc_F8:
   nop
   orr     flags, flags, #D_FLAG6502
Bopc_F9:
   JITADCSBCABSY
Bopc_FA:
   mov temp,#1
Bopc_FB:
#if defined(TIMING_INSTRUCTION)
#if defined(RPI2) || defined(RPI3) || defined(RPI4)
   mrc     p15, 0, temp0, c9, c13, 0
#else
   MRC     p15, 0, temp0, c15, c12, 1    // Read Cycle Counter Register
#endif
   str     temp0,[ram6502,#0xE4]
#else
   nop
   nop
#endif
Bopc_FD:
   JITADCSBCABSX
Bopc_FE:
   JITABS
   ldrb temp,[temp1, regX, lsr #24]!
   BL .+8
   sxtb temp, temp
   teq  temp, #0
Bopc_FF:
   JITBBS 7

opc_F0: // Opcode F0 - BEQ
   BRANCH BEQINSTRUCTION

opc_F1: // Opcode F1 - SBC ($00),Y
   DOUBLEBYTEBYTET2BLOUT Bopc_F1 SBC1 12

opc_F2: // Opcode F2 - SBC ($00)
   DOUBLEBYTEBYTET2BLOUT Bopc_F2 SBC1 12

opc_F3: // Opcode F3 - NOP6
   //NOP6
   SINGLEOPCODEBLOUT Bopc_F3 putcharopcode 4

opc_F5: // Opcode F5 - SBC $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_F5 SBC1 12

opc_F6: // Opcode F6 - INC $00,X
   DOUBLEBYTESETFIRSTBYTEBLOUT Bopc_F6 storeXflags 12

opc_F7: // Opcode F7 - SMB7 $00
   DOUBLEBYTESETTWOBYTES Bopc_F7 12

opc_F8: // Opcode F8 - SED
   SINGLEBYTE Bopc_F8

opc_F9: // Opcode F9 - SBC $0000,Y
   TRIPLEBYTEABSBLOUT Bopc_F9 SBC2 20

opc_FA: // Opcode FA - PLX
   SINGLEOPCODEBLOUT Bopc_FA plx 4

opc_FB: // Opcode FB - NOP6
   SINGLEBYTE Bopc_FB

opc_FD: // Opcode FD - SBC $0000,X
   TRIPLEBYTEABSBLOUT Bopc_FD SBC2 20

opc_FE: // Opcode FE - INC $0000,X
   TRIPLEBYTEABSBLOUT Bopc_FE incstore 12

opc_FF: // Opcode FF - BBS7
   BBRS Bopc_FF bbsreturn

.ltorg

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
   mov   regSP, #0x1F0

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

   ldr   temp2,=dojit-JITLET-8
   mov   temp2,temp2,LSR#2
   add   temp2,temp2,#BLINSTRUCTION
   mov   r0,#JITLET

// setup JITTEDTABLE16 with 64K x mov pc,r14

   ldr   temp,=MOVPCR14INSTRUCTION
   mov   r4,#JITTEDTABLE16

   mov   r3,#0x10000 // 64K
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
#else
   mov temp,#0
   MCR p15, 0, temp, c7, c10, 0 //; clean data cache
#endif

   mov temp,#0
   MCR p15, 0, temp, c7, c5, 0 //; invalidate I cache
   MCR p15, 0, temp, c7, c5, 6 //; invalidate all of the BTB

#if defined(RPI2) || defined(RPI3) || defined(RPI4)
   DSB
   ISB
#endif
   orr   flags, flags, #I_FLAG6502   // Set the I bit
   bic   flags, flags, #D_FLAG6502   // Clear the D bit

   mov temp,#1
   str temp,debugflag

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

// Enable jit tracing

   mov temp,#0xe3
   ldr temp1,= 0x7FFE
   strb temp,[temp1],#1
   mov temp,#0x60
   strb temp,[temp1]

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

.ltorg

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
   ldr     temp0, =tube_irq             // Get IRQ status
   ldr     temp0, [temp0]
   //mrs     temp, CPSR                  // Preserve the 6502 flags
   tst     flags, #I_FLAG6502        // Test whether interrupts are enabled (bit 2 = 0)
   eoreq   temp0, temp0, #1
   tsteq   temp0, #1                    // Test for IRQ
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

// one byte instruction
dejit16bit1: //temp1 pointer to address to dejit
   ldr temp2,=dojit-JITLET-8
   ldr temp0,=MOVPCR14INSTRUCTION
   sub temp2,temp2,temp1,LSL # JITLETSHIFT
   mov temp2,temp2,LSR#2
   add temp2,temp2,#BLINSTRUCTION

   DEJITCLEAN temp2 JITLET,JITLETSHIFT
   DEJITCLEAN temp0 JITTEDTABLE16
#if defined(RPI4)|| defined(RPI3)
   ISB
#endif
   bx lr

// two byte instruction
dejit16bit21: // temp1 , temp1+4

   ldr temp2,=dojit-JITLET-8
   ldr temp0,=MOVPCR14INSTRUCTION
   sub temp2,temp2,temp1,LSL # JITLETSHIFT
   mov temp2,temp2,LSR#2
   add temp2,temp2,#BLINSTRUCTION

   DEJITCLEAN temp2 JITLET ,JITLETSHIFT
   DEJITCLEAN temp0 JITTEDTABLE16
   add temp1,temp1,#1
   sub temp2,temp2,#(NEXTJITLET>>2)
   DEJITCLEAN temp2 JITLET,JITLETSHIFT
   DEJITCLEAN temp0 JITTEDTABLE16
#if defined(RPI4)|| defined(RPI3)
   ISB
#endif
   bx lr

dejit16bit22: // temp1 , temp1-4
   ldr temp2,=(dojit-JITLET-8)
   ldr temp0,=MOVPCR14INSTRUCTION
   sub temp2,temp2,temp1,LSL # JITLETSHIFT
   mov temp2,temp2,LSR#2
   add temp2,temp2,#BLINSTRUCTION

   DEJITCLEAN temp2 JITLET ,JITLETSHIFT
   DEJITCLEAN temp0 JITTEDTABLE16

   sub temp1,temp1,#1
   add temp2,temp2,#(NEXTJITLET>>2)
   DEJITCLEAN temp2 JITLET, JITLETSHIFT
   DEJITCLEAN temp0 JITTEDTABLE16
#if defined(RPI4)|| defined(RPI3)
   ISB
#endif
   bx lr

// three byte instruction
dejit16bit31: // temp1 , temp1+4, temp1 + 8
   ldr temp2,=dojit-JITLET-8
   ldr temp0,=MOVPCR14INSTRUCTION
   sub temp2,temp2,temp1,LSL # JITLETSHIFT
   mov temp2,temp2,LSR#2
   add temp2,temp2,#BLINSTRUCTION

   DEJITCLEAN temp2 JITLET, JITLETSHIFT
   DEJITCLEAN temp0 JITTEDTABLE16
   add temp1,temp1,#1
   sub temp2,temp2,#NEXTJITLET>>2
   DEJITCLEANMID temp2 JITLET, JITLETSHIFT
   DEJITCLEANMID temp0 JITTEDTABLE16
   add temp1,temp1,#1
   sub temp2,temp2,#NEXTJITLET>>2
   DEJITCLEAN temp2 JITLET ,JITLETSHIFT
   DEJITCLEAN temp0 JITTEDTABLE16
#if defined(RPI4)|| defined(RPI3)
   ISB
#endif
   bx lr

dejit16bit32: // temp1 , temp1-4, temp1 + 4

   ldr temp2,=dojit-JITLET-8
   ldr temp0,=MOVPCR14INSTRUCTION
   sub temp2,temp2,temp1,LSL # JITLETSHIFT
   mov temp2,temp2,LSR#2
   add temp2,temp2,#BLINSTRUCTION

   DEJITCLEANMID temp2 JITLET, JITLETSHIFT
   DEJITCLEANMID temp0 JITTEDTABLE16
   add temp1,temp1,#-1
   sub temp2,temp2,#-(NEXTJITLET>>2)
   DEJITCLEAN temp2 JITLET ,JITLETSHIFT
   DEJITCLEAN temp0 JITTEDTABLE16
   add temp1,temp1,#2
   sub temp2,temp2,#(NEXTJITLET>>2)*2
   DEJITCLEAN temp2 JITLET ,JITLETSHIFT
   DEJITCLEAN temp0 JITTEDTABLE16
#if defined(RPI4)|| defined(RPI3)
   ISB
#endif
   bx lr

dejit16bit33: // temp1 , temp1-4, temp1 - 8
   ldr temp2,=dojit-JITLET-8
   ldr temp0,=MOVPCR14INSTRUCTION
   sub temp2,temp2,temp1,LSL # JITLETSHIFT
   mov temp2,temp2,LSR#2
   add temp2,temp2,#BLINSTRUCTION

   DEJITCLEAN temp2 JITLET ,JITLETSHIFT
   DEJITCLEAN temp0 JITTEDTABLE16
   add temp1,temp1,#-1
   sub temp2,temp2,#-(NEXTJITLET>>2)
   DEJITCLEANMID temp2 JITLET ,JITLETSHIFT
   DEJITCLEANMID temp0 JITTEDTABLE16
   add temp1,temp1,#-1
   sub temp2,temp2,#-(NEXTJITLET>>2)
   DEJITCLEAN temp2 JITLET, JITLETSHIFT
   DEJITCLEAN temp0 JITTEDTABLE16
#if defined(RPI4)|| defined(RPI3)
   ISB
#endif
   bx lr

// *************

// **** jit up to a branch or jsr rts rti
dojit:
// r14 has address of instruction
   mrs r4, CPSR                   // Save interrupt state (restored by dojitexit)
   CPSID   if                     // Disable ARM FIQ and IRQ interrupts
   sub r14,r14,#4
   push {r0-r12,r14}

   mov temp3,R14 // return address
   sub temp2,R14,#JITLET
   b jitentry

dojitloop:
#ifdef JITDEBUG
   ldr temp,debugflag
   movs temp,temp
   BEQ 1f

   push {r0-r12}
   mov r0,r6
   mov r1,r2
   BL disarm
   pop {r0-r12}
1:
#endif
jitentry:

   add r6,temp2,#JITLET
   mov temp,temp2,LSR#JITLETSHIFT
//debug
#ifdef JITDEBUG
   ldr r0,debugflag
   movs r0,r0
   BEQ 1f

   push {r0-r12}
   mov r0,temp
   BL dis6502
   pop {r0-r12}
1:
#endif
   ldrb temp,[temp]
   adr temp0,opcode_table
   ldr pc,[temp0,temp,LSL#2]

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

.ltorg

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
   // r2 has last address
   ldr r12,cacheline
   sub r4,r3,#JITLET
   add r2,r2,#JITLET
   mov r4,r4,LSR #(JITLETSHIFT-2)
   add r4,r4,#JITTEDTABLE16

   add r1,r2,r12
   mov r5,r3

   sub r0,r12,#1  // this appears to fix an A72 issue where the cache wasn't being
   bic  r3,r3,r0  // flushed if the address was the last word in a cache line
   bic  r4,r4,r0  // and it was being flushed for the second time !!!!

cacheflushloop:
   mcr p15,0,r3,cr7,cr11,1 // data JITLET (NB 2xJITLET compared to tables)
   MCR p15,0,r3, c7, c5, 1 // instruction
   add r3,r3,r12
   mcr p15,0,r3,cr7,cr11,1 // data JITLET
   MCR p15,0,r3, c7, c5, 1 // instruction
   add r3,r3,r12
   mcr p15,0,r4,cr7,cr11,1 // data JITTEDTABLE16
   MCR p15,0,r4, c7, c5, 1 // instruction
   add r4,r4,r12
   cmp r1,r3
   bhi cacheflushloop

   DSB
   mov temp0,#0
   MCR p15, 0, temp0, c7, c5, 6 //; invalidate all of the BTB
   DSB
   ISB

#else
#if defined(RPI2)
   push {r3}
   // Clean data cache both JITLET
   // r3 has the first address of JITLET instruction
   // r6 has last address ( possibly needs to be +8
   mov r0,r3
   add r1,r6,#8
   sub r1,r1,r0
   mov r4,r0
   mov r5,r1
   BL _clean_cache_area

   pop {r3}
   DSB
   mov temp,#0
   MCR p15, 0, temp, c7, c5, 0 //; invalidate I cache
   MCR p15, 0, temp, c7, c5, 6 //; invalidate all of the BTB
   DSB
   ISB
#else
   mov temp,#0
   MCR p15, 0, temp, c7, c10, 0 //; clean data cache line
   MCR p15, 0, temp, c7, c5, 0 //; invalidate I cache
   MCR p15, 0, temp, c7, c5, 6 //; invalidate all of the BTB

#endif
#endif

   pop {r0-r12,r14}
   msr CPSR, r4  // Restore the 6502 flags, and interrupt state present on entry

   bx r14

jitend1byteexit:
   FILLINJITTEDTABLES dejit16bit1
   B dojitexit

jitend1bytestore:
   str temp,[r6,#4]
jitend1byte:
   FILLINJITTEDTABLES dejit16bit1
   B dojitloop

savelastwordjitend2byte:
   str temp,[r6,#12]

jitend2byte:
   FILLINJITTEDTABLES dejit16bit21
   add temp0,temp0,#(dejit16bit22-dejit16bit21-4)>>2
   add temp2,temp2,#NEXTJITLET
   str temp0,[temp1],#4
  // FILLINJITTEDTABLES dejit16bit22
   B dojitloop

savelastwordjitend2byteexit:
   str temp,[r6,#12]

jitend2byteexit:
   FILLINJITTEDTABLES dejit16bit21
   add temp0,temp0,#(dejit16bit22-dejit16bit21-4)>>2
   add temp2,temp2,#NEXTJITLET
   str temp0,[temp1],#4
  // FILLINJITTEDTABLES dejit16bit22
   B dojitexit

savelastwordjitend3byte:
   str temp,[r6,#20]
jitend3byte:
   FILLINJITTEDTABLES dejit16bit31
   add temp0,temp0,#(dejit16bit32-dejit16bit31-4)>>2
   add temp2,temp2,#NEXTJITLET*2
   str temp0,[temp1],#4
   add temp0,temp0,#(dejit16bit33-dejit16bit32-4)>>2
   str temp0,[temp1],#4
   //FILLINJITTEDTABLES dejit16bit32
   //FILLINJITTEDTABLES dejit16bit33
   B dojitloop

savelastwordjitend3byteexit:
   str temp,[r6,#20]
jitend3byteexit:
   FILLINJITTEDTABLES dejit16bit31
   add temp0,temp0,#(dejit16bit32-dejit16bit31-4)>>2
   add temp2,temp2,#NEXTJITLET*2
   str temp0,[temp1],#4
   add temp0,temp0,#(dejit16bit33-dejit16bit32-4)>>2
   str temp0,[temp1],#4
   //FILLINJITTEDTABLES dejit16bit32
   //FILLINJITTEDTABLES dejit16bit33
   B dojitexit

MAKEBRANCHFILLINJITTEDTABLES:
   mov temp,temp2,LSR #JITLETSHIFT-2
   add temp1,temp,#JITTEDTABLE16
   sub temp0,temp0,temp1
   sub temp0,temp0,#8
   add temp2,temp2,#NEXTJITLET
   mov temp0,temp0,LSR #2
   //BIC temp0,temp0,#0xFF000000  // branch should be forward
   ORR temp0,temp0,#BINSTRUCTION
   str temp0,[temp1],#4
   bx lr

// entry temp0 address to goto (JITLET)
//       temp1 address to place the branch

MAKEBRANCHTOJITLET:
   add temp0,temp0,#JITLET

MAKEBRANCH:
   sub temp0,temp0,temp1
   sub temp0,temp0,#8
   mov temp0,temp0,LSR #2
   BIC temp0,temp0,#0xFF000000
   ORR temp0,temp0,#BINSTRUCTION
   str temp0,[temp1],#4
   bx lr

MAKEBRANCHLINK:
   sub temp0,temp0,temp1
   sub temp0,temp0,#8
   mov temp0,temp0,LSR #2
   BIC temp0,temp0,#0xFF000000
   ORR temp0,temp0,#BLINSTRUCTION
   str temp0,[temp1],#4
   bx lr

// Always gets the first byte operand byte as that is the LSB
//
fixuptype2access:
   // need to look at the instruction to get the operand.
   mov temp,temp2,LSR #JITLETSHIFT

   ldrb temp,[temp,#1]
   ldr temp0,[temp4]
   and jittedtable16ptr,temp,#0xF0
   and temp,temp,#0x0F
   orr temp0,temp0,jittedtable16ptr,LSL #4
   orr temp0,temp0,temp
   mov jittedtable16ptr,#JITTEDTABLE16  // recover jittedtable16ptr
   str temp0,[temp4]
   bx lr

ioload:
   push    {ram6502,r12,lr}            // ram6502 (r6) is used as a working register
   mrs     ram6502, CPSR               // Save 6502 flags and current FIQ/IRQ bits
   CPSID   if                          // Disable FIQ/IRQ at the same time
   bl      tube_parasite_read          // Call up to C code to handle parasite read
   msr     CPSR, ram6502               // Restore 6502 flags and FIQ/IRQ bits
   pop     {ram6502,r12,pc}            // Restore registers and return

iostore:
   push    {ram6502,r12,lr}            // ram6502 (r6) is used as a working register
   mrs     ram6502, CPSR               // Save 6502 flags and current FIQ/IRQ bits
   CPSID   if                          // Disable FIQ/IRQ at the same time
   bl      tube_parasite_write_banksel // Call up to C code to handle parasite write
   msr     CPSR, ram6502               // Restore 6502 flags and FIQ/IRQ bits
   pop     {ram6502,r12,pc}            // Restore registers and return

// Jits overflow

brk:
   sub temp1,r14,#JITLET
   mov temp1,temp1,LSR#JITLETSHIFT
   add temp1,temp1,#1
   INTR    -2, 1

rti:
   PLP
   ldrh    temp1, [regSP, #1]

   mov     temp2, #2
   UADD8   regSP, regSP, temp2

   mov temp2,#0xFF
   orr temp2,temp2,#0xff<<8
   cmp temp1,temp2

   // so we have an irq that happened during execution
   beq noirq

   ldr     temp0, =tube_irq             // Get IRQ status
   ldr     temp0, [temp0]
   //mrs     temp, CPSR                  // Preserve the 6502 flags
   tst     flags, #I_FLAG6502        // Test whether interrupts are enabled (bit 2 = 0)
   eoreq   temp0, temp0, #1
   tsteq   temp0, #1                    // Test for IRQ
   beq     handle_irqrti
   msr     CPSR_flg, temp              // Restore the 6502 flags
   mov     temp1,temp1,LSL #JITLETSHIFT
   add   temp1,temp1,#JITLET
   bx    temp1

handle_irqrti:

   msr  CPSR_flg, temp     // Restore the 6502 flags
   INTR -2, 0


cli:
   mrs temp,CPSR
   B check_irq_plp

plp:
   PLP
// CHECK_IRQ
check_irq_plp:
   ldr     temp0, =tube_irq             // Get IRQ status
   ldr     temp0, [temp0]
   //mrs     temp, CPSR                  // Preserve the 6502 flags
   tst     flags, #I_FLAG6502        // Test whether interrupts are enabled (bit 2 = 0)
   eoreq   temp0, temp0, #1
   tsteq   temp0, #1                    // Test for IRQ
   beq     handle_irqplp
   msr     CPSR_flg, temp              // Restore the 6502 flags
   bx lr

handle_irqplp:
   sub temp1,r14,#JITLET
   mov temp1,temp1,LSR#JITLETSHIFT

   msr  CPSR_flg, temp     // Restore the 6502 flags
   INTR -2, 0

putcharopcode:
   push    {regSP, r12,r14}
   mrs     regSP, CPSR               // Save 6502 flags
   ldr temp,=putchar
   mov r0,regA
   BLX temp
   msr     CPSR_flg, regSP
   pop     {regSP,r12,pc}

debugopcode:
//   ldr temp1,=debugflag
//   str temp,[temp1]
   bx lr

invertcarry:
   mrs   temp0, CPSR
   eor   temp0, temp0, #C_FLAG
   msr   CPSR_flg, temp0
   bx lr

testvflagC:
   mrs     temp1, CPSR
   tst     flags, #V_FLAG6502
   addne pc,r14,#8 // return to jit
   bx lr

testvflagS:
   mrs     temp1, CPSR
   tst     flags, #V_FLAG6502
   addeq pc,r14,#8 // return to jit
   bx lr

bbrreturn:
   addne pc,r14,#4 // return to jit
   msr     CPSR_flg, temp1
   bx lr

bbsreturn:
   addeq pc,r14,#4 // return to jit
   msr     CPSR_flg, temp1
   bx lr

bitimm:
   bicne   temp1, temp1, #Z_FLAG
   orreq   temp1, temp1, #Z_FLAG
   msr     CPSR_flg, temp1
   bx lr

jitstxytable:
   add temp,jittedtable16ptr,temp1,LSR #24-2
   BX temp

jitstatable:
   strb regA,[temp1]
   add temp,jittedtable16ptr, temp1,LSL #2
   BX temp

storeXflags:
   strb temp,[ram6502,temp1,LSR#24]
tempflags:
   sxtb temp, temp
   teq  temp, #0
   bx lr

incstore:
   add  temp, temp, #1
   strb temp,[temp1]
   bx lr

decstore:
   sub  temp, temp, #1
   strb temp,[temp1]
   bx lr

plx:
   uadd8 regX,regSP,temp
   ldrb  regX, [regX]
   uadd8 regSP,regSP,temp
   mov   regX, regX, LSL #24
   teq   regX, #0
   bx lr

ply:
   uadd8 regY,regSP,temp
   ldrb  regY, [regY]
   uadd8 regSP,regSP,temp
   mov   regY, regY, LSL #24
   teq   regY, #0
   bx lr

pla:
   uadd8 regA,regSP,temp
   ldrsb regA, [regA]
   uadd8 regSP,regSP,temp
   teq   regA, #0
   bx lr

rts:

   mov temp1, #2
   mov temp,#JITLET
   UADD8 regSP, regSP, temp1
   add  temp1,temp,#NEXTJITLET
   add  temp1,temp1,temp0,LSL #JITLETSHIFT

   bx temp1

rora:
   subcs   regA, regA, #0x100   // if Carry set all other bits ie sign extend
   rrxs    regA, regA
   bx lr

ror:
   ROR6
   bx lr

rol:
   adc  temp, temp, temp
   strb temp,[temp1]
   lsls    temp, temp, #24
   bx lr

rola:
   lsls    temp1, regA, #24
   sxtb    regA, regA
   bx lr

bitioload: //2c
   push {r14}
   BL      ioload
   pop  {r14}

bitadjustr14:
   add temp2,r14,#16
   bic     flags, flags, #V_FLAG6502
   tst     r0, regA  // This clears N flag and sets up the Z flag
   mrs     temp, CPSR
   and     temp1, r0, #N_FLAG6502
   and     r0, r0, #V_FLAG6502
   orr     flags, flags, r0
   orr     temp, temp, temp1, LSL #24
   msr     CPSR_flg, temp
   bx  temp2

bit: // 34
   tst     temp, regA  // This clears N flag and sets up the Z flag
bit2: // 24 3c
   mrs     temp0, CPSR
   and     temp1, temp, #N_FLAG6502
   and     temp, temp, #V_FLAG6502
   orr     flags, flags, temp
   orr     temp0, temp0, temp1, LSL #24
   msr     CPSR_flg, temp0
   bx lr

cmpa:
   ldrb temp,[temp1]
   mov temp1,regA,LSL #24
   cmp temp1,temp,LSL#24
   bx lr

loada:
   ldrsb regA,[temp]
   teq regA,#0
   bx lr

aflags:
   sxtb    regA, regA
   teq     regA, #0
   bx lr

asl:
   ASL
   bx lr

php:
   orrmi temp, temp, #N_FLAG6502
   orrcs temp, temp, #C_FLAG6502
   orreq temp, temp, #Z_FLAG6502
pushtemp:
   SPUSH temp
   bx lr

//   mrs temp0, CPSR
trb:
   bic temp0, temp0, #Z_FLAG
   tst temp, regA
trb2:
   bic temp, temp, regA
   strb temp,[temp1]
   orreq   temp0, temp0, #Z_FLAG
   msr     CPSR_flg, temp0
   bx lr

//   mrs temp0, CPSR
tsb:
   bic temp0, temp0, #Z_FLAG
   tst temp, regA
tsb2:
   orr temp, temp, regA
   strb temp,[temp1]
   orreq   temp0, temp0, #Z_FLAG
   msr     CPSR_flg, temp0
   bx lr

//   mov temp1,temp1,LSR #24
ldrhldrsb:
   ldrh temp,[temp1]
   ldrsb temp,[temp]
   bx lr

//
// Decimal mode functions
//
// TODO **** sort out registers *****
ADCload:
        ldrb   temp,[temp1]
ADC0:
        tst     flags, #D_FLAG6502
ADC1:
        bic     flags, #V_FLAG6502
ADC2:
        lsl     regA, regA, #24
ADC3:
        subcs   temp, temp ,#0x100    // if carry set then set top 24 bits
        bne     6f
        adcs    regA, regA, temp, ror #8
        orrvs   flags, #V_FLAG6502
        SXTB    regA, regA, ROR #24
        bx lr
6:
decimal_mode_adc:
        // if carry is set the upper 24 bits of temp are set too

        and     temp0, regA, #0x0F<<24
        and     temp1, temp, #15

        adc     temp1, temp1, temp0, lsr #24
        cmp     temp1, #10
        subcs   temp1, temp1, #10
        and     temp1, temp1, #0x0F
        orrcs   temp, temp ,#0x0F
        and     regA, regA, #0xF0<<24
        orrcs   regA, regA, #0x0F<<24
        adcs    regA, regA, temp , ror #8
        orrvs   flags, #V_FLAG6502
        cmpcc   regA, #10<<28
        subcs   regA, regA, #10<<28
        and     regA, regA, #0xF0<<24
        orr     regA, regA, temp1, lsl #24
        sxtb    regA, regA, ror #24
        teq     regA, #0
        bx lr

SBCload:
        ldrb    temp,[temp1]
SBC0:
        tst     flags, #D_FLAG6502
SBC1:
        bic     flags, #V_FLAG6502
SBC2:
        LSL     regA, regA, #24
SBC3:
        bne     6f
        // probably can remove this with some thought
        subcc   temp, temp ,#0x100        // if carry set then set top 24 bits
        sbcs    regA, regA, temp, ror #8
        orrvs   flags, #V_FLAG6502
        SXTB    regA, regA, ROR #24
        bx lr
6:

decimal_mode_sbc:
        bic     temp0, temp,# 0xf0
        and     temp1, regA, #0xf000000
        sbc     temp1, temp1, temp0, ror #8
        sbcs    regA, regA, temp, ror #8
        orrvs   flags, #V_FLAG6502
        subcc   regA, regA,#0x60<<24
        teq     temp1,#0
        submi   regA, regA, #0x6<<24
        sxtb    regA, regA, ror #24
        teq     regA, #0
        bx lr

.ltorg

immediatesignextenedtable:
.set imm,0
.rept 128
   mov temp,#imm
   .set imm,imm+1
.endr
.rept 128
   mov temp,#0xFFFFFF00+imm
   .set imm,imm+1
.endr

bitabstable:
.set imm,0
.rept 256
   ldrb temp0,[temp1,#imm]
   B bitadjustr14
   .set imm,imm+1
.endr
