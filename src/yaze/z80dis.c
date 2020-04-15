#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "mem_mmu.h"
//#include "simz80.h"
#include "z80dis.h"

#define IBUF_SIZE 20

static const char *reg[8]   = { "B", "C", "D", "E", "H", "L", "(HL)", "A"};
static const char *dreg1[4]  = { "BC", "DE", "HL", "SP"};
static const char *dreg2[4]  = { "BC", "DE", "HL", "AF"};
static const char *cond[8]  = { "NZ", "Z", "NC", "C", "PO", "PE", "P", "M"};
static const char *arith[8] = { "ADD   A,", "ADC   A,", "SUB   ", "SBC   A,",
                                "AND   ", "XOR   ", "OR    ","CP    "};
static const char *ins1[8]  = { "RLCA","RRCA","RLA","RRA","DAA","CPL","SCF","CCF"};
static const char *ins2[4]  = { "RET", "EXX", "JP    (HL)", "LD    SP,HL" };
static const char *ins3[8]  = { "RLC","RRC","RL ","RR ","SLA","SRA","???","SRL"};
static const char *ins4[8]  = { "NEG","???","???","???","???","???","???","???"};
static const char *ins5[8]  = { "RETN","RETI","???","???","???","???","???","???"};
static const char *ins6[8]  = { "LD   I,A","???","LD   A,I","???","RRD","RLD","???","???"};
static const char *ins7[32] = { "LDI","CPI","INI","OUTI","???","???","???","???",
                                "LDD","CPD","IND","OUTD","???","???","???","???",
                                "LDIR","CPIR","INIR","OTIR","???","???","???","???",
                                "LDDR","CPDR","INDR","OTDR","???","???","???","???"};
static const char *ins8[8]  = { "RLC","RRC","RL","RR","SLA","SRA","???","SRL"};

static inline uint32_t unp_misc1(uint32_t addr, uint8_t a, uint8_t d, uint8_t e, const char **ptr, char *ibuf) {
    uint16_t opaddr;

    switch(e) {
        case 0x00: // relative jumps and assorted.
            switch(d) {
                case 0x00:
                    *ptr = "NOP";
                    break;
                case 0x01:
                    *ptr = "EX    AF,AF'";
                    break;
                case 0x02:
                    opaddr = addr + 1;
                    opaddr += (signed char)GetBYTE(addr++);
                    snprintf(ibuf, IBUF_SIZE, "DJNZ  %4.4Xh", opaddr);
                    break;
                case 0x03:
                    opaddr = addr + 1;
                    opaddr += (signed char)GetBYTE(addr++);
                    snprintf(ibuf, IBUF_SIZE, "JR    %4.4Xh", opaddr);
                    break;
                default:
                    opaddr = addr + 1;
                    opaddr += (signed char)GetBYTE(addr++);
                    snprintf(ibuf, IBUF_SIZE, "JR    %s,%4.4Xh", cond[d & 3], opaddr);
                    break;
            }
            break;
        case 0x01: // 16-=bit load immediate/add
            if (a & 0x08) {
                snprintf(ibuf, IBUF_SIZE, "ADD   HL,%s", dreg1[d >> 1]);
            } else {
                opaddr = GetBYTE(addr++);
                opaddr |= (GetBYTE(addr++)<<8);
                snprintf(ibuf, IBUF_SIZE, "LD    %s,%4.4Xh", dreg1[d >> 1], opaddr);
            }
            break;
        case 0x02: // indirect load.
            switch(d) {
                case 0x00:
                    *ptr = "LD    (BC),A";
                    break;
                case 0x01:
                    *ptr = "LD    A,(BC)";
                    break;
                case 0x02:
                    *ptr = "LD    (DE),A";
                    break;
                case 0x03:
                    *ptr = "LD    A,(DE)";
                    break;
                case 0x04:
                    opaddr = GetBYTE(addr++);
                    opaddr |= (GetBYTE(addr++)<<8);
                    snprintf(ibuf, IBUF_SIZE, "LD    (%4.4Xh),HL", opaddr);
                    break;
                case 0x05:
                    opaddr = GetBYTE(addr++);
                    opaddr |= (GetBYTE(addr++)<<8);
                    snprintf(ibuf, IBUF_SIZE, "LD    HL,(%4.4Xh)", opaddr);
                    break;
                case 0x06:
                    opaddr = GetBYTE(addr++);
                    opaddr |= (GetBYTE(addr++)<<8);
                    snprintf(ibuf, IBUF_SIZE, "LD    (%4.4Xh),A", opaddr);
                    break;
                case 0x07:
                    opaddr = GetBYTE(addr++);
                    opaddr |= (GetBYTE(addr++)<<8);
                    snprintf(ibuf, IBUF_SIZE, "LD    A,(%4.4Xh)", opaddr);
                    break;
            }
            break;
        case 0x03:
            if(a & 0x08)
                snprintf(ibuf, IBUF_SIZE, "DEC   %s", dreg1[d >> 1]);
            else
                snprintf(ibuf, IBUF_SIZE, "INC   %s", dreg1[d >> 1]);
            break;
        case 0x04:
            snprintf(ibuf, IBUF_SIZE, "INC   %s", reg[d]);
            break;
        case 0x05:
            snprintf(ibuf, IBUF_SIZE, "DEC   %s", reg[d]);
            break;
        case 0x06:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "LD    %s,%2.2Xh", reg[d], opaddr);
            break;
        case 0x07:
            *ptr = ins1[d];
            break;
    }
    return addr;
}

static inline uint32_t pfx_cb(uint32_t addr, const char **ptr, char *ibuf) {
    uint8_t a, d, e;

    a = GetBYTE(addr++);
    d = (a >> 3) & 7;
    e = a & 7;
    switch(a & 0xC0) {
        case 0x00:
            snprintf(ibuf, IBUF_SIZE, "%s   %s", ins3[d], reg[e]);
            break;
        case 0x40:
            snprintf(ibuf, IBUF_SIZE, "BIT   %d,%s", d, reg[e]);
            break;
        case 0x80:
            snprintf(ibuf, IBUF_SIZE, "RES   %d,%s", d, reg[e]);
            break;
        case 0xC0:
            snprintf(ibuf, IBUF_SIZE, "SET   %d,%s", d, reg[e]);
            break;
    }
    return addr;
}

static inline uint32_t pfx_ed(uint32_t addr, const char **ptr, char *ibuf) {
    uint8_t a, d, e;
    uint16_t opaddr;

    a = GetBYTE(addr++);
    d = (a >> 3) & 7;
    e = a & 7;
    switch (a & 0xC0) {
        case 0x40:
            switch (e) {
                case 0x00:
                    snprintf(ibuf, IBUF_SIZE, "IN    %s,(C)", reg[d]);
                    break;
                case 0x01:
                    snprintf(ibuf, IBUF_SIZE, "OUT   (C),%s", reg[d]);
                    break;
                case 0x02:
                    if (d & 1)
                        snprintf(ibuf, IBUF_SIZE, "ADC   HL,%s", dreg1[d >> 1]);
                    else
                        snprintf(ibuf, IBUF_SIZE, "SBC   HL,%s", dreg1[d >> 1]);
                    break;
                case 0x03:
                    opaddr = GetBYTE(addr++);
                    opaddr |= (GetBYTE(addr++)<<8);
                    if (d & 1)
                        snprintf(ibuf, IBUF_SIZE, "LD    %s,(%4.4Xh)", dreg1[d >> 1], opaddr);
                    else
                        snprintf(ibuf, IBUF_SIZE, "LD    (%4.4Xh),%s", opaddr, dreg1[d >> 1]);
                    break;
                case 0x04:
                    *ptr = ins4[d];
                    break;
                case 0x05:
                    *ptr = ins5[d];
                    break;
                case 0x06:
                    switch(d) {
                        case 0:
                            *ptr = "IM    0";
                            break;
                        case 1:
                            *ptr = "IM    0/1";
                            break;
                        default:
                            snprintf(ibuf, IBUF_SIZE, "IM    %d", d-1);
                    }
                    break;
                case 0x07:
                    *ptr = ins6[d];
                    break;
            }
            break;
        case 0x80:
            *ptr = ins7[a & 0x1F];
            break;
    }
    return addr;
}

static inline uint32_t pfx_ireg(uint32_t addr, uint8_t a, const char **ptr, char *ibuf) {
    uint8_t d;
    uint16_t opaddr, opadd2;
    const char *ireg;

    // 0x01 (0xDD) = IX, 0x03 (0xFD) = IY
    ireg = (a & 0x20) ? "IY" : "IX";
    a = GetBYTE(addr++);
    switch(a) {
        case 0x09:
            snprintf(ibuf, IBUF_SIZE, "ADD   %s,BC", ireg);
            break;
        case 0x19:
            snprintf(ibuf, IBUF_SIZE, "ADD   %s,DE", ireg);
            break;
        case 0x21:
            opaddr = GetBYTE(addr++);
            opaddr |= (GetBYTE(addr++)<<8);
            snprintf(ibuf, IBUF_SIZE, "LD    %s,%4.4Xh", ireg, opaddr);
            break;
        case 0x22:
            opaddr = GetBYTE(addr++);
            opaddr |= (GetBYTE(addr++)<<8);
            snprintf(ibuf, IBUF_SIZE, "LD    (%4.4Xh),%s", opaddr, ireg);
            break;
        case 0x23:
            snprintf(ibuf, IBUF_SIZE, "INC   %s", ireg);
            break;
        case 0x29:
            snprintf(ibuf, IBUF_SIZE, "ADD   %s,%s", ireg, ireg);
            break;
        case 0x2A:
            opaddr = GetBYTE(addr++);
            opaddr |= (GetBYTE(addr++)<<8);
            snprintf(ibuf, IBUF_SIZE, "LD    %s,(%4.4Xh)", ireg, opaddr);
            break;
        case 0x2B:
            snprintf(ibuf, IBUF_SIZE, "DEC   %s", ireg);
            break;
        case 0x34:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "INC   (%s+%2.2Xh)", ireg, opaddr);
            break;
        case 0x35:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "DEC   (%s+%2.2Xh)", ireg, opaddr);
            break;
        case 0x36:
            opaddr = GetBYTE(addr++);
            opadd2 = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "LD    (%s+%2.2Xh),%2.2Xh", ireg, (uint8_t) opaddr, (uint8_t) opadd2);
            break;
        case 0x39:
            snprintf(ibuf, IBUF_SIZE, "ADD   %s,SP", ireg);
            break;
        case 0x46:
        case 0x4E:
        case 0x56:
        case 0x5E:
        case 0x66:
        case 0x6E:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "LD    %s,(%s+%2.2Xh)", reg[(a>>3)&7], ireg, opaddr);
            break;
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x77:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "LD    (%s+%2.2Xh),%s", ireg, opaddr, reg[a & 7]);
            break;
        case 0x7E:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "LD    A,(%s+%2.2Xh)", ireg, opaddr);
            break;
        case 0x86:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "ADD   A,(%s+%2.2Xh)", ireg, opaddr);
            break;
        case 0x8E:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "ADC   A,(%s+%2.2Xh)", ireg, opaddr);
            break;
        case 0x96:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "SUB   (%s+%2.2Xh)", ireg, opaddr);
            break;
        case 0x9E:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "SBC   A,(%s+%2.2Xh)", ireg, opaddr);
            break;
        case 0xA6:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "AND   A,(%s+%2.2Xh)", ireg, opaddr);
            break;
        case 0xAE:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "XOR   A,(%s+%2.2Xh)", ireg, opaddr);
            break;
        case 0xB6:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "OR    A,(%s+%2.2Xh)", ireg, opaddr);
            break;
        case 0xBE:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "CP    A,(%s+%2.2Xh)", ireg, opaddr);
            break;
        case 0xE1:
            snprintf(ibuf, IBUF_SIZE, "POP   %s", ireg);
            break;
        case 0xE3:
            snprintf(ibuf, IBUF_SIZE, "EX    (SP),%s", ireg);
            break;
        case 0xE5:
            snprintf(ibuf, IBUF_SIZE, "PUSH  %s", ireg);
            break;
        case 0xE9:
            snprintf(ibuf, IBUF_SIZE, "JP    (%s)", ireg);
            break;
        case 0xF9:
            snprintf(ibuf, IBUF_SIZE, "LD    SP,%s", ireg);
            break;
        case 0xCB:
            opaddr = GetBYTE(addr++);
            a = GetBYTE(addr++);
            d = (a >> 3) & 7;
            switch(a & 0xC0) {
                case 0x00:
                    snprintf(ibuf, IBUF_SIZE, "%s   (%s+%2.2Xh)", ins8[d], ireg, opaddr);
                    break;
                case 0x40:
                    snprintf(ibuf, IBUF_SIZE, "BIT   %d,(%s+%2.2Xh)", d, ireg, opaddr);
                    break;
                case 0x80:
                    snprintf(ibuf, IBUF_SIZE, "RES   %d,(%s+%2.2Xh)", d, ireg, opaddr);
                    break;
                case 0xC0:
                    snprintf(ibuf, IBUF_SIZE, "SET   %d,(%s+%2.2Xh)", d, ireg, opaddr);
                    break;
            }
            break;
    }
    return addr;
}
    
static uint32_t unp_misc2(uint32_t addr, uint8_t a, uint8_t d, uint8_t e, const char **ptr, char *ibuf) {
    uint16_t opaddr;

    switch(e) {
        case 0x00:
            snprintf(ibuf, IBUF_SIZE, "RET   %s", cond[d]);
            break;
        case 0x01:
            if(d & 1)
                *ptr = ins2[d >> 1];
            else
                snprintf(ibuf, IBUF_SIZE, "POP   %s", dreg2[d >> 1]);
            break;
        case 0x02:
            opaddr = GetBYTE(addr++);
            opaddr |= (GetBYTE(addr++)<<8);
            snprintf(ibuf, IBUF_SIZE, "JP    %s,%4.4Xh", cond[d], opaddr);
            break;
        case 0x03:
            switch(d) {
                case 0x00:
                    opaddr = GetBYTE(addr++);
                    opaddr |= (GetBYTE(addr++)<<8);
                    snprintf(ibuf, IBUF_SIZE, "JP    %4.4Xh", opaddr);
                    break;
                case 0x01:
                    addr = pfx_cb(addr, ptr, ibuf);
                    break;
                case 0x02:
                    opaddr = GetBYTE(addr++);
                    snprintf(ibuf, IBUF_SIZE, "OUT   (%2.2Xh),A", opaddr);
                    break;
                case 0x03:
                    opaddr = GetBYTE(addr++);
                    snprintf(ibuf, IBUF_SIZE, "IN    A,(%2.2Xh)", opaddr);
                    break;
                case 0x04:
                    *ptr = "EX    (SP),HL";
                    break;
                case 0x05:
                    *ptr = "EX    DE,HL";
                    break;
                case 0x06:
                    *ptr = "DI";
                    break;
                case 0x07:
                    *ptr = "EI";
                    break;
                }
            break;
        case 0x04:
            opaddr = GetBYTE(addr++);
            opaddr |= (GetBYTE(addr++)<<8);
            snprintf(ibuf, IBUF_SIZE, "CALL  %s,%4.4Xh", cond[d], opaddr);
            break;
        case 0x05:
            if (d & 1) {
                switch(d >> 1) {
                    case 0x00:
                        opaddr = GetBYTE(addr++);
                        opaddr |= (GetBYTE(addr++)<<8);
                        snprintf(ibuf, IBUF_SIZE, "CALL  %4.4Xh", opaddr);
                        break;
                    case 0x02:
                        addr = pfx_ed(addr, ptr, ibuf);
                        break;
                    default:
                        addr = pfx_ireg(addr, a, ptr, ibuf);
                        break;
                }
            } else
                snprintf(ibuf, IBUF_SIZE, "PUSH  %s", dreg2[d >> 1]);
            break;
        case 0x06:
            opaddr = GetBYTE(addr++);
            snprintf(ibuf, IBUF_SIZE, "%s%2.2Xh", arith[d], opaddr);
            break;
        case 0x07:
            snprintf(ibuf, IBUF_SIZE, "RST   %2.2Xh", a & 0x38);
            break;
    }
    return addr;
}

static uint32_t disassemble(uint32_t addr, const char **ptr, char *ibuf) {
    uint8_t a = GetBYTE(addr++);
    uint8_t d = (a >> 3) & 7;
    uint8_t e = a & 7;

    if (a & 0x80) {
        if (a & 0x40)
            addr = unp_misc2(addr, a, d, e, ptr, ibuf);
        else
            snprintf(ibuf, IBUF_SIZE, "%s%s", arith[d], reg[e]);
    } else {
        if (a & 0x40) {
            if (d == 6 && e == 6) {
                *ptr = "HALT";
            } else {
                snprintf(ibuf, IBUF_SIZE, "LD    %s,%s", reg[d], reg[e]);
            }
        } else
            addr = unp_misc1(addr, a, d, e, ptr, ibuf);
    }
    return addr;
}

uint32_t z80_disassemble(uint32_t addr, char *buf, size_t bufsize) {
    uint32_t naddr, oaddr;
    const int width=18;
    char ibuf[IBUF_SIZE];
    const char *iptr = ibuf;
    uint8_t b1, b2, b3, b4;

    naddr = disassemble(addr, &iptr, ibuf);
    oaddr = addr;
    switch(naddr-addr) {
        case 1:
            b1 = GetBYTE(oaddr++);
            snprintf(buf, bufsize, "%04"PRIX32" %02X          %-*s", addr, b1, width, iptr);
            break;
        case 2:
            b1 = GetBYTE(oaddr++);
            b2 = GetBYTE(oaddr++);
            snprintf(buf, bufsize, "%04"PRIX32" %02X %02X       %-*s", addr, b1, b2, width, iptr);
            break;
        case 3:
            b1 = GetBYTE(oaddr++);
            b2 = GetBYTE(oaddr++);
            b3 = GetBYTE(oaddr++);
            snprintf(buf, bufsize, "%04"PRIX32" %02X %02X %02X    %-*s", addr, b1, b2, b3, width, iptr);
            break;
        default:
            b1 = GetBYTE(oaddr++);
            b2 = GetBYTE(oaddr++);
            b3 = GetBYTE(oaddr++);
            b4 = GetBYTE(oaddr++);
            snprintf(buf, bufsize, "%04"PRIX32" %02X %02X %02X %02X %-*s", addr, b1, b2, b3, b4, width, iptr);
    }
    return naddr;
}
