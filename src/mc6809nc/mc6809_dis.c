/* dis6809.c -- 6809 disassembler
   Copyright (C) 1998 Jerome Thoen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdarg.h>
#include "mc6809.h"

typedef unsigned char tt_u8;
typedef signed char tt_s8;
typedef unsigned short tt_u16;
typedef signed short tt_s16;

static inline unsigned int get_memw(unsigned int addr) {
   return (read8(addr) << 8) + read8(addr + 1);
}

static inline unsigned char get_memb(unsigned int addr) {
   return read8(addr);
}

enum opcodes {
   OP_UU  ,
   OP_XX  ,
   OP_ABX ,
   OP_ADCA,
   OP_ADCB,
   OP_ADDA,
   OP_ADDB,
   OP_ADDD,
   OP_ANDA,
   OP_ANDB,
   OP_ANDC,
   OP_ASL ,
   OP_ASLA,
   OP_ASR ,
   OP_ASRA,
   OP_ASRB,
   OP_BCC ,
   OP_BEQ ,
   OP_BGE ,
   OP_BGT ,
   OP_BHI ,
   OP_BITA,
   OP_BITB,
   OP_BLE ,
   OP_BLO ,
   OP_BLS ,
   OP_BLT ,
   OP_BMI ,
   OP_BNE ,
   OP_BPL ,
   OP_BRA ,
   OP_BRN ,
   OP_BSR ,
   OP_BVC ,
   OP_BVS ,
   OP_CLR ,
   OP_CLRA,
   OP_CLRB,
   OP_CMPA,
   OP_CMPB,
   OP_CMPD,
   OP_CMPS,
   OP_CMPU,
   OP_CMPX,
   OP_CMPY,
   OP_COM ,
   OP_COMA,
   OP_COMB,
   OP_CWAI,
   OP_DAA ,
   OP_DEC ,
   OP_DECA,
   OP_DECB,
   OP_EORA,
   OP_EORB,
   OP_EXG ,
   OP_INC ,
   OP_INCA,
   OP_INCB,
   OP_JMP ,
   OP_JSR ,
   OP_LBCC,
   OP_LBEQ,
   OP_LBGE,
   OP_LBGT,
   OP_LBHI,
   OP_LBLE,
   OP_LBLO,
   OP_LBLS,
   OP_LBLT,
   OP_LBMI,
   OP_LBNE,
   OP_LBPL,
   OP_LBRA,
   OP_LBRN,
   OP_LBSR,
   OP_LBVC,
   OP_LBVS,
   OP_LDA ,
   OP_LDB ,
   OP_LDD ,
   OP_LDS ,
   OP_LDU ,
   OP_LDX ,
   OP_LDY ,
   OP_LEAS,
   OP_LEAU,
   OP_LEAX,
   OP_LEAY,
   OP_LSLB,
   OP_LSR ,
   OP_LSRA,
   OP_LSRB,
   OP_MUL ,
   OP_NEG ,
   OP_NEGA,
   OP_NEGB,
   OP_NOP ,
   OP_ORA ,
   OP_ORB ,
   OP_ORCC,
   OP_PSHS,
   OP_PSHU,
   OP_PULS,
   OP_PULU,
   OP_ROL ,
   OP_ROLA,
   OP_ROLB,
   OP_ROR ,
   OP_RORA,
   OP_RORB,
   OP_RTI ,
   OP_RTS ,
   OP_SBCA,
   OP_SBCB,
   OP_SEX ,
   OP_STA ,
   OP_STB ,
   OP_STD ,
   OP_STS ,
   OP_STU ,
   OP_STX ,
   OP_STY ,
   OP_SUBA,
   OP_SUBB,
   OP_SUBD,
   OP_SWI ,
   OP_SWI2,
   OP_SWI3,
   OP_SYNC,
   OP_TFR ,
   OP_TST ,
   OP_TSTA,
   OP_TSTB
};

static const char inst[] = "\
--  \
??  \
ABX \
ADCA\
ADCB\
ADDA\
ADDB\
ADDD\
ANDA\
ANDB\
ANDC\
ASL \
ASLA\
ASR \
ASRA\
ASRB\
BCC \
BEQ \
BGE \
BGT \
BHI \
BITA\
BITB\
BLE \
BLO \
BLS \
BLT \
BMI \
BNE \
BPL \
BRA \
BRN \
BSR \
BVC \
BVS \
CLR \
CLRA\
CLRB\
CMPA\
CMPB\
CMPD\
CMPS\
CMPU\
CMPX\
CMPY\
COM \
COMA\
COMB\
CWAI\
DAA \
DEC \
DECA\
DECB\
EORA\
EORB\
EXG \
INC \
INCA\
INCB\
JMP \
JSR \
LBCC\
LBEQ\
LBGE\
LBGT\
LBHI\
LBLE\
LBLO\
LBLS\
LBLT\
LBMI\
LBNE\
LBPL\
LBRA\
LBRN\
LBSR\
LBVC\
LBVS\
LDA \
LDB \
LDD \
LDS \
LDU \
LDX \
LDY \
LEAS\
LEAU\
LEAX\
LEAY\
LSLB\
LSR \
LSRA\
LSRB\
MUL \
NEG \
NEGA\
NEGB\
NOP \
ORA \
ORB \
ORCC\
PSHS\
PSHU\
PULS\
PULU\
ROL \
ROLA\
ROLB\
ROR \
RORA\
RORB\
RTI \
RTS \
SBCA\
SBCB\
SEX \
STA \
STB \
STD \
STS \
STU \
STX \
STY \
SUBA\
SUBB\
SUBD\
SWI \
SWI2\
SWI3\
SYNC\
TFR \
TST \
TSTA\
TSTB";

// The first byte is the opcode index
// The second byte is <length><mode>
//      modes:
//      1 immediate
//      2 direct    
//      3 indexed   
//      4 extended  
//      5 inherent
//      6 relative

static const unsigned char map0[] = {
   OP_NEG , 0x22,
   OP_XX  , 0x22,
   OP_XX  , 0x12,
   OP_COM , 0x22,
   OP_LSR , 0x22,
   OP_XX  , 0x12,
   OP_ROR , 0x22,
   OP_ASR , 0x22,
   OP_ASL , 0x22,
   OP_ROL , 0x22,
   OP_DEC , 0x22,
   OP_XX  , 0x12,
   OP_INC , 0x22,
   OP_TST , 0x22,
   OP_JMP , 0x22,
   OP_CLR , 0x22,
   OP_UU  , 0x10,
   OP_UU  , 0x10,
   OP_NOP , 0x15,
   OP_SYNC, 0x15,
   OP_XX  , 0x10,
   OP_XX  , 0x10,
   OP_LBRA, 0x36,
   OP_LBSR, 0x36,
   OP_XX  , 0x10,
   OP_DAA , 0x15,
   OP_ORCC, 0x25,
   OP_XX  , 0x10,
   OP_ANDC, 0x25,
   OP_SEX , 0x15,
   OP_EXG , 0x25,
   OP_TFR , 0x25,
   OP_BRA , 0x26,
   OP_BRN , 0x26,
   OP_BHI , 0x26,
   OP_BLS , 0x26,
   OP_BCC , 0x26,
   OP_BLO , 0x26,
   OP_BNE , 0x26,
   OP_BEQ , 0x26,
   OP_BVC , 0x26,
   OP_BVS , 0x26,
   OP_BPL , 0x26,
   OP_BMI , 0x26,
   OP_BGE , 0x26,
   OP_BLT , 0x26,
   OP_BGT , 0x26,
   OP_BLE , 0x26,
   OP_LEAX, 0x23,
   OP_LEAY, 0x23,
   OP_LEAS, 0x23,
   OP_LEAU, 0x23,
   OP_PSHS, 0x25,
   OP_PULS, 0x25,
   OP_PSHU, 0x25,
   OP_PULU, 0x25,
   OP_XX  , 0x15,
   OP_RTS , 0x15,
   OP_ABX , 0x15,
   OP_RTI , 0x15,
   OP_CWAI, 0x25,
   OP_MUL , 0x15,
   OP_XX  , 0x15,
   OP_SWI , 0x15,
   OP_NEGA, 0x15,
   OP_XX  , 0x15,
   OP_XX  , 0x15,
   OP_COMA, 0x15,
   OP_LSRA, 0x15,
   OP_XX  , 0x15,
   OP_RORA, 0x15,
   OP_ASRA, 0x15,
   OP_ASLA, 0x15,
   OP_ROLA, 0x15,
   OP_DECA, 0x15,
   OP_XX  , 0x15,
   OP_INCA, 0x15,
   OP_TSTA, 0x15,
   OP_XX  , 0x15,
   OP_CLRA, 0x15,
   OP_NEGB, 0x15,
   OP_XX  , 0x15,
   OP_XX  , 0x15,
   OP_COMB, 0x15,
   OP_LSRB, 0x15,
   OP_XX  , 0x15,
   OP_RORB, 0x15,
   OP_ASRB, 0x15,
   OP_LSLB, 0x15,
   OP_ROLB, 0x15,
   OP_DECB, 0x15,
   OP_XX  , 0x15,
   OP_INCB, 0x15,
   OP_TSTB, 0x15,
   OP_XX  , 0x15,
   OP_CLRB, 0x15,
   OP_NEG , 0x23,
   OP_XX  , 0x13,
   OP_XX  , 0x13,
   OP_COM , 0x23,
   OP_LSR , 0x23,
   OP_XX  , 0x13,
   OP_ROR , 0x23,
   OP_ASR , 0x23,
   OP_ASL , 0x23,
   OP_ROL , 0x23,
   OP_DEC , 0x23,
   OP_XX  , 0x13,
   OP_INC , 0x23,
   OP_TST , 0x23,
   OP_JMP , 0x23,
   OP_CLR , 0x23,
   OP_NEG , 0x34,
   OP_XX  , 0x14,
   OP_XX  , 0x14,
   OP_COM , 0x34,
   OP_LSR , 0x34,
   OP_XX  , 0x14,
   OP_ROR , 0x34,
   OP_ASR , 0x34,
   OP_ASL , 0x34,
   OP_ROL , 0x34,
   OP_DEC , 0x34,
   OP_XX  , 0x14,
   OP_INC , 0x34,
   OP_TST , 0x34,
   OP_JMP , 0x34,
   OP_CLR , 0x34,
   OP_SUBA, 0x21,
   OP_CMPA, 0x21,
   OP_SBCA, 0x21,
   OP_SUBD, 0x31,
   OP_ANDA, 0x21,
   OP_BITA, 0x21,
   OP_LDA , 0x21,
   OP_XX  , 0x11,
   OP_EORA, 0x21,
   OP_ADCA, 0x21,
   OP_ORA , 0x21,
   OP_ADDA, 0x21,
   OP_CMPX, 0x31,
   OP_BSR , 0x26,
   OP_LDX , 0x31,
   OP_XX  , 0x10,
   OP_SUBA, 0x22,
   OP_CMPA, 0x22,
   OP_SBCA, 0x22,
   OP_SUBD, 0x22,
   OP_ANDA, 0x22,
   OP_BITA, 0x22,
   OP_LDA , 0x22,
   OP_STA , 0x22,
   OP_EORA, 0x22,
   OP_ADCA, 0x22,
   OP_ORA , 0x22,
   OP_ADDA, 0x22,
   OP_CMPX, 0x22,
   OP_JSR , 0x22,
   OP_LDX , 0x22,
   OP_STX , 0x22,
   OP_SUBA, 0x23,
   OP_CMPA, 0x23,
   OP_SBCA, 0x23,
   OP_SUBD, 0x23,
   OP_ANDA, 0x23,
   OP_BITA, 0x23,
   OP_LDA , 0x23,
   OP_STA , 0x23,
   OP_EORA, 0x23,
   OP_ADCA, 0x23,
   OP_ORA , 0x23,
   OP_ADDA, 0x23,
   OP_CMPX, 0x23,
   OP_JSR , 0x23,
   OP_LDX , 0x23,
   OP_STX , 0x23,
   OP_SUBA, 0x34,
   OP_CMPA, 0x34,
   OP_SBCA, 0x34,
   OP_SUBD, 0x34,
   OP_ANDA, 0x34,
   OP_BITA, 0x34,
   OP_LDA , 0x34,
   OP_STA , 0x34,
   OP_EORA, 0x34,
   OP_ADCA, 0x34,
   OP_ORA , 0x34,
   OP_ADDA, 0x34,
   OP_CMPX, 0x34,
   OP_JSR , 0x34,
   OP_LDX , 0x34,
   OP_STX , 0x34,
   OP_SUBB, 0x21,
   OP_CMPB, 0x21,
   OP_SBCB, 0x21,
   OP_ADDD, 0x31,
   OP_ANDB, 0x21,
   OP_BITB, 0x21,
   OP_LDB , 0x21,
   OP_XX  , 0x11,
   OP_EORB, 0x21,
   OP_ADCB, 0x21,
   OP_ORB , 0x21,
   OP_ADDB, 0x21,
   OP_LDD , 0x31,
   OP_XX  , 0x11,
   OP_LDU , 0x31,
   OP_XX  , 0x10,
   OP_SUBB, 0x22,
   OP_CMPB, 0x22,
   OP_SBCB, 0x22,
   OP_ADDD, 0x22,
   OP_ANDB, 0x22,
   OP_BITB, 0x22,
   OP_LDB , 0x22,
   OP_STB , 0x22,
   OP_EORB, 0x22,
   OP_ADCB, 0x22,
   OP_ORB , 0x22,
   OP_ADDB, 0x22,
   OP_LDD , 0x22,
   OP_STD , 0x22,
   OP_LDU , 0x22,
   OP_STU , 0x22,
   OP_SUBB, 0x23,
   OP_CMPB, 0x23,
   OP_SBCB, 0x23,
   OP_ADDD, 0x23,
   OP_ANDB, 0x23,
   OP_BITB, 0x23,
   OP_LDB , 0x23,
   OP_STB , 0x23,
   OP_EORB, 0x23,
   OP_ADCB, 0x23,
   OP_ORB , 0x23,
   OP_ADDB, 0x23,
   OP_LDD , 0x23,
   OP_STD , 0x23,
   OP_LDU , 0x23,
   OP_STU , 0x23,
   OP_SUBB, 0x34,
   OP_CMPB, 0x34,
   OP_SBCB, 0x34,
   OP_ADDD, 0x34,
   OP_ANDB, 0x34,
   OP_BITB, 0x34,
   OP_LDB , 0x34,
   OP_STB , 0x34,
   OP_EORB, 0x34,
   OP_ADCB, 0x34,
   OP_ORB , 0x34,
   OP_ADDB, 0x34,
   OP_LDD , 0x34,
   OP_STD , 0x34,
   OP_LDU , 0x34,
   OP_STU , 0x34,
};

static const unsigned char map1[] = {
   33, OP_LBRN, 0x46,
   34, OP_LBHI, 0x46,
   35, OP_LBLS, 0x46,
   36, OP_LBCC, 0x46,
   37, OP_LBLO, 0x46,
   38, OP_LBNE, 0x46,
   39, OP_LBEQ, 0x46,
   40, OP_LBVC, 0x46,
   41, OP_LBVS, 0x46,
   42, OP_LBPL, 0x46,
   43, OP_LBMI, 0x46,
   44, OP_LBGE, 0x46,
   45, OP_LBLT, 0x46,
   46, OP_LBGT, 0x46,
   47, OP_LBLE, 0x46,
   63, OP_SWI2, 0x25,
   131, OP_CMPD, 0x41,
   140, OP_CMPY, 0x41,
   142, OP_LDY , 0x41,
   147, OP_CMPD, 0x32,
   156, OP_CMPY, 0x32,
   158, OP_LDY , 0x32,
   159, OP_STY , 0x32,
   163, OP_CMPD, 0x33,
   172, OP_CMPY, 0x33,
   174, OP_LDY , 0x33,
   175, OP_STY , 0x33,
   179, OP_CMPD, 0x44,
   188, OP_CMPY, 0x44,
   190, OP_LDY , 0x44,
   191, OP_STY , 0x44,
   206, OP_LDS , 0x41,
   222, OP_LDS , 0x32,
   223, OP_STS , 0x32,
   238, OP_LDS , 0x33,
   239, OP_STS , 0x33,
   254, OP_LDS , 0x44,
   255, OP_STS , 0x44,
};

static const unsigned char map2[] = {
   63, OP_SWI3, 0x25,
   131, OP_CMPU, 0x41,
   140, OP_CMPS, 0x41,
   147, OP_CMPU, 0x32,
   156, OP_CMPS, 0x32,
   163, OP_CMPU, 0x33,
   172, OP_CMPS, 0x33,
   179, OP_CMPU, 0x44,
   188, OP_CMPS, 0x44,
   255, OP_XX  , 0x10
};

static const char regi[] = { 'X', 'Y', 'U', 'S' };

static const char *exgi[] = { "D", "X", "Y", "U", "S", "PC", "??", "??", "A",
                              "B", "CC", "DP", "??", "??", "??", "??" };

static const char *pshsregi[] = { "PC", "U", "Y", "X", "DP", "B", "A", "CC" };
static const char *pshuregi[] = { "PC", "S", "Y", "X", "DP", "B", "A", "CC" };

static char *str_buf;
static size_t str_bufsize;

static void stringInit(char *buf, size_t bufsize) {
   str_buf = buf;
   str_bufsize = bufsize;
}

static void stringAppend(const char *fmt, ...) {
   int len;
   va_list argptr;
   va_start(argptr, fmt);
   len = vsnprintf(str_buf, str_bufsize, fmt, argptr);
   str_buf += len;
   str_bufsize -= len;
   va_end(argptr);
}

/* disassemble one instruction at adress adr and return its size */

static char hexdigit(tt_u16 v)
{
   v &= 0xf;
   if (v <= 9)
      return '0' + v;
   else
      return 'A' - 10 + v;
}

static char *hex8str(tt_u8 v)
{
   static char tmpbuf[3] = "  ";
   
   tmpbuf[1] = hexdigit(v);
   tmpbuf[0] = hexdigit(v >> 4);
   
   return tmpbuf;
}

static char *hex16str(tt_u16 v)
{
   static char tmpbuf[5] = "    ";
   
   tmpbuf[3] = hexdigit(v);
   v >>= 4;
   tmpbuf[2] = hexdigit(v);
   v >>= 4;
   tmpbuf[1] = hexdigit(v);
   v >>= 4;
   tmpbuf[0] = hexdigit(v);
   
   return tmpbuf;
}

const char *statusString = "EFHINZVC";

static char *ccstr(tt_u8 val)
{
   static char tempbuf[9] = "        ";
   int i;
   
   for (i = 0; i < 8; i++) {
      if (val & 0x80)
         tempbuf[i] = statusString[i];
      else
         tempbuf[i] = '.';
      val <<= 1;
   }
   
   return tempbuf;
}

static unsigned int disassemble(unsigned int addr)
{
   int d = get_memb(addr);
   int s, i;
   tt_u8 pb;
   char reg;
   const unsigned char *map = NULL;
   
   // Default for most undefined opcodes
   unsigned char sm = 0x10; // size_mode byte
   unsigned char oi = OP_XX; // opcode index
   
   if (d == 0x10) {
      d = get_memb(addr + 1); 
      map = map1;
   }
   
   if (d == 0x11) {
      d = get_memb(addr + 1);
      map = map2;
   }
   
   if (map) {
      // Search for the opcode in map1 or map2
      map -= 3;
      do {
         map += 3;
         if (*map == d) {
            oi = *(++map);
            sm = *(++map);
            break;
         }
      } while (*map < 255);
   } else {
      // Lookup directly in map0
      map = map0 + 2 * d;
      oi = *(map++);
      sm = *(map++);
   }
   
   s = sm >> 4;
   
   stringAppend("%04X ", addr);
   
   for (i = 0; i < s; i++) {
      stringAppend("%s", hex8str(get_memb(addr + i)));
      stringAppend("%c", ' ');
   }
   for (i = s; i < 4; i++) {
      stringAppend("%s", "   ");
   }
   
   const char *ip = inst + oi * 4; 
   for (i = 0; i < 4; i++)
      stringAppend("%c", *(ip++));
   
   stringAppend("%s", "  ");
   
   
   switch(sm & 15) {
   case 1:             /* immediate */
      stringAppend("%s", "#$");
      if (s == 2)
         stringAppend("%s", hex8str(get_memb(addr + 1)));
      else
         stringAppend("%s", hex16str(get_memw(addr + s - 2)));
      break;
   case 2:             /* direct */
      stringAppend("%s", "$");
      stringAppend("%s", hex8str(get_memb(addr + s - 1)));
      break;
   case 3:             /* indexed */
      pb = get_memb(addr + s - 1);
      reg = regi[(pb >> 5) & 0x03];
      
      if (!(pb & 0x80)) {       /* n4,R */
         if (pb & 0x10)
            stringAppend("-$%s,%c", hex8str(((pb & 0x0f) ^ 0x0f) + 1), reg);
         else
            stringAppend("$%s,%c", hex8str(pb & 0x0f), reg);
      }
      else {
         if (pb & 0x10)
            stringAppend("%c", '[');
         switch (pb & 0x0f) {
         case 0:                 /* ,R+ */
            stringAppend(",%c+", reg);
            break;
         case 1:                 /* ,R++ */
            stringAppend(",%c++", reg);
            break;
         case 2:                 /* ,-R */
            stringAppend(",-%c", reg);
            break;
         case 3:                 /* ,--R */
            stringAppend(",--%c", reg);
            break;
         case 4:                 /* ,R */
            stringAppend(",%c", reg);
            break;
         case 5:                 /* B,R */
            stringAppend("B,%c", reg);
            break;
         case 6:                 /* A,R */
            stringAppend("A,%c", reg);
            break;
         case 8:                 /* n7,R */
            s += 1;
            stringAppend("$%s,%c", hex8str(get_memb(addr + s - 1)), reg);
            break;
         case 9:                 /* n15,R */
            s += 2;
            stringAppend("$%s,%c", hex16str(get_memw(addr + s - 2)), reg);
            break;
         case 11:                /* D,R */
            stringAppend("D,%c", reg);
            break;
         case 12:                /* n7,PCR */
            s += 1;
            stringAppend("$%s,PCR", hex8str(get_memb(addr + s - 1)));
            break;
         case 13:                /* n15,PCR */
            s += 2;
            stringAppend("$%s,PCR", hex16str(get_memw(addr + s - 2)));
            break;
         case 15:                /* [n] */
            s += 2;
            stringAppend("$%s", hex16str(get_memw(addr + s - 2)));
            break;
         default:
            stringAppend("%s", "??");
            break; }
         if (pb & 0x10)
            stringAppend("%c", ']');
      }
      break;
   case 4:          /* extended */
      stringAppend("$%s", hex16str(get_memw(addr + s - 2)));
      break;
   case 5:          /* inherent */
      pb = get_memb(addr + 1);
      switch (d) {
      case 0x1e: case 0x1f:              /* exg tfr */
         stringAppend("%s,%s", exgi[(pb >> 4) & 0x0f], exgi[pb & 0x0f]);
         break; 
      case 0x1a: case 0x1c: case 0x3c:   /* orcc andcc cwai */
         stringAppend("#$%s=%s", hex8str(pb), ccstr(pb));
         break;
      case 0x34:                         /* pshs */
      {
         int p = 0;
         
         for (i = 0; i < 8; i++) {
            if (pb & 0x80) {
               if (p)
                  stringAppend("%c", ',');
               stringAppend("%s", pshsregi[i]);
               p = 1;
            }
            pb <<= 1;
         }
      }
      break;
      case 0x35:                         /* puls */
      {
         int p = 0;
         
         for (i = 7; i >= 0; i--) {
            if (pb & 0x01) {
               if (p)
                  stringAppend("%c", ',');
               stringAppend("%s", pshsregi[i]);
               p = 1;
            }
            pb >>= 1;
         }
      }
      break;
      case 0x36:                         /* pshu */
      {
         int p = 0;
         
         for (i = 0; i < 8; i++) {
            if (pb & 0x80) {
               if (p)
                  stringAppend("%c", ',');
               stringAppend("%s", pshuregi[i]);
               p = 1;
            }
            pb <<= 1;
         }
      }
      break;
      case 0x37:                         /* pulu */
      {
         int p = 0;
         
         for (i = 7; i >= 0; i--) {
            if (pb & 0x01) {
               if (p)
                  stringAppend("%c", ',');
               stringAppend("%s", pshuregi[i]);
               p = 1;
            }
            pb >>= 1;
         }
      }
      break;
      }
      break; 
   case 6:             /* relative */
   {
      tt_s16 v;
      
      if (s == 2)
         v = (tt_s16)(tt_s8)get_memb(addr + 1);
      else
         v = (tt_s16)get_memw(addr + s - 2);
      stringAppend("$%s", hex16str(addr + (tt_u16)s + v));
      break;
   }
   }
   
   return addr + s;
}

uint32_t mc6809_disassemble(uint32_t addr, char *buf, size_t bufsize)
{
   stringInit(buf, bufsize);
   return disassemble(addr);
}

