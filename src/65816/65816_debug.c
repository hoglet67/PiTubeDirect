/* -*- mode: c; c-basic-offset: 4 -*- */

#include <stdio.h>
#include "65816.h"
#include "65816_debug.h"

typedef enum {
    IMP,    // Implied.
    IMPA,   // Implied with A as the implied operand.
    IMM,    // Immediate, 8 bit
    IMV,    // Immediate, 8 or 16 bit depending accumulator mode.
    IMX,    // Immediate, 8 or 16 bit depending on index register mode.
    ZP,     // Zero page, known as Direct Page on the 65816.
    ZPX,    // Zero (direct) page indexed by X.
    ZPY,    // Zero (direct) page indexed by Y (for LDX).
    INDX,   // Zero (direct) page indexed (by X) indirect.
    INDY,   // Zero (direct) page indirect indexed (by Y).
    INDYL,  // Direct page indirect long indexed (by Y).  65816 only.
    IND,    // Zero (direct) page indirect.
    INDL,   // Direct page indirect long, 24 bit (65816 only)
    ABS,    // Absolute.
    ABSL,   // Absolute long, 24 bit (65816 only)
    ABSX,   // Absolute indexed by X
    ABSXL,  // Absolute indexed by X, long
    ABSY,   // Absolute indexed by Y
    IND16,  // Indirect 16bit (for JMP).
    IND1X,  // Indexed (by X) indirect (for JMP)
    PCR,    // PC-relative.  8bit signed offset from PC for branch instructions.
    PCRL,   // PC-relative.  16bit signed offset from PC.
    SR,     // Stack relative (65816 only)
    SRY,    // Stack relative indirect indexed (by Y).
    BM,     // Block moves (65816 only)
    BITC,   // Bit change (set/reset) as used by RMB, SMB
    BITB    // Branch on bit set/reset (BBR, BBS.)
} addr_mode_t;

typedef enum {
    UND,   ADC,   ANC,   AND,   ANE,   ARR,   ASL,   ASR,   BCC,   BCS,   BEQ,
    BIT,   BMI,   BNE,   BPL,   BRA,   BRK,   BRL,   BVC,   BVS,   CLC,   CLD,
    CLI,   CLV,   CMP,   COP,   CPX,   CPY,   DCP,   DEC,   DEX,   DEY,   EOR,
    HLT,   INC,   INX,   INY,   ISB,   JML,   JMP,   JSL,   JSR,   LAS,   LAX,
    LDA,   LDX,   LDY,   LSR,   LXA,   MVN,   MVP,   NOP,   ORA,   PEA,   PEI,
    PER,   PHA,   PHB,   PHD,   PHK,   PHP,   PHX,   PHY,   PLA,   PLB,   PLD,
    PLP,   PLX,   PLY,   REP,   RLA,   ROL,   ROR,   RRA,   RTI,   RTL,   RTS,
    SAX,   SBC,   SBX,   SEC,   SED,   SEI,   SEP,   SHA,   SHS,   SHX,   SHY,
    SLO,   SRE,   STA,   STP,   STX,   STY,   STZ,   TAX,   TAY,   TCD,   TCS,
    TDC,   TRB,   TSB,   TSC,   TSX,   TXA,   TXS,   TXY,   TYA,   TYX,   WAI,
    WDM,   XBA,   XCE,   RMB,   SMB,   BBR,   BBS
} op_t;

static const char op_names[117][4] = {
    "---", "ADC", "ANC", "AND", "ANE", "ARR", "ASL", "ASR", "BCC", "BCS", "BEQ",
    "BIT", "BMI", "BNE", "BPL", "BRA", "BRK", "BRL", "BVC", "BVS", "CLC", "CLD",
    "CLI", "CLV", "CMP", "COP", "CPX", "CPY", "DCP", "DEC", "DEX", "DEY", "EOR",
    "HLT", "INC", "INX", "INY", "ISB", "JML", "JMP", "JSL", "JSR", "LAS", "LAX",
    "LDA", "LDX", "LDY", "LSR", "LXA", "MVN", "MVP", "NOP", "ORA", "PEA", "PEI",
    "PER", "PHA", "PHB", "PHD", "PHK", "PHP", "PHX", "PHY", "PLA", "PLB", "PLD",
    "PLP", "PLX", "PLY", "REP", "RLA", "ROL", "ROR", "RRA", "RTI", "RTL", "RTS",
    "SAX", "SBC", "SBX", "SEC", "SED", "SEI", "SEP", "SHA", "SHS", "SHX", "SHY",
    "SLO", "SRE", "STA", "STP", "STX", "STY", "STZ", "TAX", "TAY", "TCD", "TCS",
    "TDC", "TRB", "TSB", "TSC", "TSX", "TXA", "TXS", "TXY", "TYA", "TYX", "WAI",
    "WDM", "XBA", "XCE", "RMB", "SMB", "BBR", "BBS"
};

static const uint8_t op_816[256] =
{
/*       0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F */
/*00*/  BRK,  ORA,  COP,  ORA,  TSB,  ORA,  ASL,  ORA,  PHP,  ORA,  ASL,  PHD,  TSB,  ORA,  ASL,  ORA,
/*10*/  BPL,  ORA,  ORA,  ORA,  TRB,  ORA,  ASL,  ORA,  CLC,  ORA,  INC,  TCS,  TRB,  ORA,  ASL,  ORA,
/*20*/  JSR,  AND,  JSL,  AND,  BIT,  AND,  ROL,  AND,  PLP,  AND,  ROL,  PLD,  BIT,  AND,  ROL,  AND,
/*30*/  BMI,  AND,  AND,  AND,  BIT,  AND,  ROL,  AND,  SEC,  AND,  DEC,  TSC,  BIT,  AND,  ROL,  AND,
/*40*/  RTI,  EOR,  WDM,  EOR,  MVP,  EOR,  LSR,  EOR,  PHA,  EOR,  LSR,  PHK,  JMP,  EOR,  LSR,  EOR,
/*50*/  BVC,  EOR,  EOR,  EOR,  MVN,  EOR,  LSR,  EOR,  CLI,  EOR,  PHY,  TCD,  JMP,  EOR,  LSR,  EOR,
/*60*/  RTS,  ADC,  PER,  ADC,  STZ,  ADC,  ROR,  ADC,  PLA,  ADC,  ROR,  RTL,  JMP,  ADC,  ROR,  ADC,
/*70*/  BVS,  ADC,  ADC,  ADC,  STZ,  ADC,  ROR,  ADC,  SEI,  ADC,  PLY,  TDC,  JMP,  ADC,  ROR,  ADC,
/*80*/  BRA,  STA,  BRL,  STA,  STY,  STA,  STX,  STA,  DEY,  BIT,  TXA,  PHB,  STY,  STA,  STX,  STA,
/*90*/  BCC,  STA,  STA,  STA,  STY,  STA,  STX,  STA,  TYA,  STA,  TXS,  TXY,  STZ,  STA,  STZ,  STA,
/*A0*/  LDY,  LDA,  LDX,  LDA,  LDY,  LDA,  LDX,  LDA,  TAY,  LDA,  TAX,  PLB,  LDY,  LDA,  LDX,  LDA,
/*B0*/  BCS,  LDA,  LDA,  LDA,  LDY,  LDA,  LDX,  LDA,  CLV,  LDA,  TSX,  TYX,  LDY,  LDA,  LDX,  LDA,
/*C0*/  CPY,  CMP,  REP,  CMP,  CPY,  CMP,  DEC,  CMP,  INY,  CMP,  DEX,  WAI,  CPY,  CMP,  DEC,  CMP,
/*D0*/  BNE,  CMP,  CMP,  CMP,  PEI,  CMP,  DEC,  CMP,  CLD,  CMP,  PHX,  STP,  JML,  CMP,  DEC,  CMP,
/*E0*/  CPX,  SBC,  SEP,  SBC,  CPX,  SBC,  INC,  SBC,  INX,  SBC,  NOP,  XBA,  CPX,  SBC,  INC,  SBC,
/*F0*/  BEQ,  SBC,  SBC,  SBC,  PEA,  SBC,  INC,  SBC,  SED,  SBC,  PLX,  XCE,  JSR,  SBC,  INC,  SBC
};

static const uint8_t am_816[256]=
{
/*       0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F */
/*00*/  IMP,  INDX, IMM,  SR,   ZP,   ZP,   ZP,   INDL, IMP,  IMV,  IMPA, IMP,  ABS,  ABS,  ABS,  ABSL,
/*10*/  PCR,  INDY, IND,  SRY,  ZP,   ZPX,  ZPX,  INDYL,IMP,  ABSY, IMPA, IMP,  ABS,  ABSX, ABSX, ABSXL,
/*20*/  ABS,  INDX, ABSL, SR,   ZP,   ZP,   ZP,   INDL, IMP,  IMV,  IMPA, IMP,  ABS,  ABS,  ABS,  ABSL,
/*30*/  PCR,  INDY, IND,  SRY,  ZPX,  ZPX,  ZPX,  INDYL,IMP,  ABSY, IMPA, IMP,  ABSX, ABSX, ABSX, ABSXL,
/*40*/  IMP,  INDX, IMP,  SR,   BM,   ZP,   ZP,   INDL, IMP,  IMV,  IMPA, IMP,  ABS,  ABS,  ABS,  ABSL,
/*50*/  PCR,  INDY, IND,  SRY,  BM,   ZPX,  ZPX,  INDYL,IMP,  ABSY, IMP,  IMP,  ABSL, ABSX, ABSX, ABSXL,
/*60*/  IMP,  INDX, PCRL, SR,   ZP,   ZP,   ZP,   INDL, IMP,  IMV,  IMPA, IMP,  IND16,ABS,  ABS,  ABSL,
/*70*/  PCR,  INDY, IND,  SRY,  ZPX,  ZPX,  ZPX,  INDYL,IMP,  ABSY, IMP,  IMP,  IND1X,ABSX, ABSX, ABSXL,
/*80*/  PCR,  INDX, PCRL, SR,   ZP,   ZP,   ZP,   INDL, IMP,  IMV,  IMP,  IMP,  ABS,  ABS,  ABS,  ABSL,
/*90*/  PCR,  INDY, IND,  SRY,  ZPX,  ZPX,  ZPY,  INDYL,IMP,  ABSY, IMP,  IMP,  ABS,  ABSX, ABSX, ABSXL,
/*A0*/  IMX,  INDX, IMX,  SR,   ZP,   ZP,   ZP,   INDL, IMP,  IMV,  IMP,  IMP,  ABS,  ABS,  ABS,  ABSL,
/*B0*/  PCR,  INDY, IND,  SRY,  ZPX,  ZPX,  ZPY,  INDYL,IMP,  ABSY, IMP,  IMP,  ABSX, ABSX, ABSY, ABSXL,
/*C0*/  IMX,  INDX, IMV,  SR,   ZP,   ZP,   ZP,   INDL, IMP,  IMV,  IMP,  IMP,  ABS,  ABS,  ABS,  ABSL,
/*D0*/  PCR,  INDY, IND,  SRY,  IMP,  ZPX,  ZPX,  INDYL,IMP,  ABSY, IMP,  IMP,  ABSL, IND16,ABSX, ABSXL,
/*E0*/  IMX,  INDX, IMV,  SR,   ZP,   ZP,   ZP,   INDL, IMP,  IMV,  IMP,  IMP,  ABS,  ABS,  ABS,  ABSL,
/*F0*/  PCR,  INDY, IND,  SRY,  IMP,  ZPX,  ZPX,  INDYL,IMP,  ABSY, IMP,  IMP,  ABSX, ABSX, ABSX, ABSXL
};

uint32_t dbg65816_disassemble(cpu_debug_t *cpu, uint32_t addr, char *buf, size_t bufsize)
{
    uint8_t op, ni, p1, p2, p3;
    uint16_t temp;
    const char *op_name;
    size_t len;
    addr_mode_t addr_mode;

    op = cpu->memread(addr);

    ni = op_816[op];
    addr_mode = am_816[op];

    op_name = op_names[ni];
    len = snprintf(buf, bufsize, "%06"PRIX32": %02x ", addr, op);

    buf += len;
    bufsize -= len;
    addr++;

    switch (addr_mode)
    {
        case IMP:
            snprintf(buf, bufsize, "         %s         ", op_name);
            break;
        case IMPA:
            snprintf(buf, bufsize, "         %s A       ", op_name);
            break;
        case IMM:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s #%02X     ", p1, op_name, p1);
            break;
        case IMV:
            p1 = cpu->memread(addr++);
            if (w65816p.m || w65816p.e)
                snprintf(buf, bufsize, "%02X       %s #%02X     ", p1, op_name, p1);
            else {
                p2 = cpu->memread(addr++);
                snprintf(buf, bufsize, "%02X %02X    %s #%02X%02X     ", p1, p2, op_name, p1, p2);
            }
            break;
        case IMX:
            p1 = cpu->memread(addr++);
            if (w65816p.ex || w65816p.e)
                snprintf(buf, bufsize, "%02X       %s #%02X     ", p1, op_name, p1);
            else {
                p2 = cpu->memread(addr++);
                snprintf(buf, bufsize, "%02X %02X    %s #%02X%02X     ", p1, p2, op_name, p1, p2);
            }
            break;
        case ZP:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s %02X      ", p1, op_name, p1);
            break;
        case ZPX:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s %02X,X    ", p1, op_name, p1);
            break;
        case ZPY:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s %02X,Y    ", p1, op_name, p1);
            break;
        case IND:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s (%02X)    ", p1, op_name, p1);
            break;
        case INDL:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s [%02X]    ", p1, op_name, p1);
            break;
        case INDX:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s (%02X,X)  ", p1, op_name, p1);
            break;
        case INDY:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s (%02X),Y  ", p1, op_name, p1);
            break;
        case INDYL:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s [%02X],Y  ", p1, op_name, p1);
            break;
        case SR:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s (%02X,S)  ", p1, op_name, p1);
            break;
        case SRY:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s (%02X,S),Y", p1, op_name, p1);
            break;
        case ABS:
            p1 = cpu->memread(addr++);
            p2 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X %02X    %s %02X%02X    ", p1, p2, op_name, p2, p1);
            break;
        case ABSL:
            p1 = cpu->memread(addr++);
            p2 = cpu->memread(addr++);
            p3 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X %02X %02X %s %02X%02X%02X  ", p1, p2, p3, op_name, p2, p1, p3);
            break;
        case ABSX:
            p1 = cpu->memread(addr++);
            p2 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X %02X    %s %02X%02X,X  ", p1, p2, op_name, p2, p1);
            break;
        case ABSY:
            p1 = cpu->memread(addr++);
            p2 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X %02X    %s %02X%02X,Y  ", p1, p2, op_name, p2, p1);
            break;
        case ABSXL:
            p1 = cpu->memread(addr++);
            p2 = cpu->memread(addr++);
            p3 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X %02X %02X  %s %02X%02X%02X,X  ", p1, p2, p3, op_name, p2, p1, p3);
            break;
        case BM:
            p1 = cpu->memread(addr++);
            p2 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X %02X    %s %02X,%02X   ", p1, p2, op_name, p1, p2);
            break;
        case IND16:
            p1 = cpu->memread(addr++);
            p2 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X %02X    %s (%02X%02X)  ", p1, p2, op_name, p2, p1);
            break;
        case IND1X:
            p1 = cpu->memread(addr++);
            p2 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X %02X    %s (%02X%02X,X)", p1, p2, op_name, p2, p1);
            break;
        case PCR:
            p1 = cpu->memread(addr++);
            temp = (signed char)p1;
            temp += addr;
            snprintf(buf, bufsize, "%02X       %s %04X    ", p1, op_name, temp);
            break;
        case PCRL:
            p1 = cpu->memread(addr++);
            p2 = cpu->memread(addr++);
            temp = (int16_t)((uint16_t)p1 | (uint16_t)p2 <<8);
            temp += addr;
            snprintf(buf, bufsize, "%02X %02X     %s %04X    ", p1, p2, op_name, temp);
            break;
        case BITC:
            p1 = cpu->memread(addr++);
            snprintf(buf, bufsize, "%02X       %s%x %02X", p1, op_name, (op & 0x70) >> 4, p1);
            break;
        case BITB:
            p1 = cpu->memread(addr++);
            p2 = cpu->memread(addr++);
            temp = (signed char)p2;
            temp += addr;
            snprintf(buf, bufsize, "%02X %02X     %s%x %02X,%04X", p1, p2, op_name, (op & 0x70) >> 4, p1, temp);
            break;
    }

    return addr;
}
