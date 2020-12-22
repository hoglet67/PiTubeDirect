/* -*- mode: c; c-basic-offset: 4 -*- */

/*B-em v2.2 by Tom Walker
  65816 parasite CPU emulation
  Originally from Snem, with some bugfixes*/

#include <stdio.h>
#include "65816.h"

#include "../tube-ula.h"
#include "../tube.h"
#include "../tube-client.h"

#ifdef INCLUDE_DEBUGGER
#include "65816_debug.h"
#endif

#define W65816_ROM_SIZE  0x8000
#define W65816_RAM_SIZE 0x80000

static uint8_t *w65816ram, *w65816rom;

// The bank number to load any native vectors from
static uint8_t w65816nvb = 0x00;

/*Registers*/
typedef union {
    uint16_t w;
    struct {
        uint8_t l, h;
    } b;
} reg;

static reg w65816a, w65816x, w65816y, w65816s;
static uint32_t pbr, dbr;
static uint16_t w65816pc, dp;

static uint32_t wins = 0;

w65816p_t w65816p;

static int inwai;

/*CPU modes : 0 = X1M1
              1 = X1M0
              2 = X0M1
              3 = X0M0
              4 = emulation*/

static int cpumode;
static void (**modeptr)(void);

/*Current opcode*/
static uint8_t w65816opcode;

#define a w65816a
#define x w65816x
#define y w65816y
#define s w65816s
#define pc w65816pc
#define p w65816p

#define opcode w65816opcode

static int cycles = 0;

static int def = 1, divider = 0, banking = 0, banknum = 0;
static uint32_t w65816mask = 0xFFFF;
static uint16_t toldpc;

#ifdef INCLUDE_DEBUGGER

static const char *dbg65816_reg_names[] = { "AB", "X", "Y", "S", "P", "PC", "DP", "DB", "PB", NULL };

static int dbg_w65816 = 0;

static int dbg_debug_enable(int newvalue)
{
    int oldvalue = dbg_w65816;
    dbg_w65816 = newvalue;
    return oldvalue;
};

#endif

static inline uint8_t pack_flags(void)
{
    uint8_t flags = 0;

    if (p.c)
        flags |= 0x01;
    if (p.z)
        flags |= 0x02;
    if (p.i)
        flags |= 0x04;
    if (p.d)
        flags |= 0x08;
    if (p.ex)
        flags |= 0x10;
    if (p.m)
        flags |= 0x20;
    if (p.v)
        flags |= 0x40;
    if (p.n)
        flags |= 0x80;
    return flags;
}

static inline uint8_t pack_flags_em(uint8_t flags)
{
    if (p.c)
        flags |= 0x01;
    if (p.z)
        flags |= 0x02;
    if (p.i)
        flags |= 0x4;
    if (p.d)
        flags |= 0x08;
    if (p.v)
        flags |= 0x40;
    if (p.n)
        flags |= 0x80;
    return flags;
}

static inline void unpack_flags(uint8_t flags)
{
    p.c = flags & 0x01;
    p.z = flags & 0x02;
    p.i = flags & 0x04;
    p.d = flags & 0x08;
    p.ex = flags & 0x10;
    p.m = flags & 0x20;
    p.v = flags & 0x40;
    p.n = flags & 0x80;
}

static inline void unpack_flags_em(uint8_t flags)
{
    p.c = flags & 0x01;
    p.z = flags & 0x02;
    p.i = flags & 0x04;
    p.d = flags & 0x08;
    p.ex = p.m = 0;
    p.v = flags & 0x40;
    p.n = flags & 0x80;
}

#ifdef INCLUDE_DEBUGGER

static uint32_t dbg_reg_get(int which)
{
    switch (which) {
        case REG_A:
            return w65816a.w;
        case REG_X:
            return w65816x.w;
        case REG_Y:
            return w65816y.w;
        case REG_S:
            return w65816s.w;
        case REG_P:
            return pack_flags();
        case REG_PC:
            return pc;
        case REG_DP:
            return dp;
        case REG_DB:
            return dbr;
        case REG_PB:
            return pbr;
        default:
            LOG_WARN("65816: attempt to get non-existent register");
            return 0;
    }
}

static void dbg_reg_set(int which, uint32_t value)
{
    switch (which) {
        case REG_A:
            w65816a.w = value;
            break;
        case REG_X:
            w65816x.w = value;
            break;
        case REG_Y:
            w65816y.w = value;
            break;
        case REG_S:
            w65816s.w = value;
            break;
        case REG_P:
            unpack_flags(value);
            break;
        case REG_PC:
            pc = value;
            break;
        case REG_DP:
            dp = value;
            break;
        case REG_DB:
            dbr = value;
            break;
        case REG_PB:
            pbr = value;
            break;
        default:
            LOG_WARN("65816: attempt to set non-existent register");
    }
}

size_t dbg65816_print_flags(char *buf, size_t bufsize)
{
    if (bufsize >= 10) {
        *buf++ = p.n  ? 'N' : ' ';
        *buf++ = p.v  ? 'V' : ' ';
        if (p.e) {
            *buf++ = '-';
            *buf++ = 'B';
        } else {
            *buf++ = p.m  ? 'M' : ' ';
            *buf++ = p.ex ? 'X' : ' ';
        }
        *buf++ = p.d  ? 'D' : ' ';
        *buf++ = p.i  ? 'I' : ' ';
        *buf++ = p.z  ? 'Z' : ' ';
        *buf++ = p.c  ? 'C' : ' ';
        *buf++ = p.e  ? 'E' : ' ';
        // Also add a terminator, as snprintf would
        *buf++ = 0;
        return 9;
    }
    return 0;
}

static size_t dbg_reg_print(int which, char *buf, size_t bufsize)
{
    switch (which) {
        case REG_P:
            return dbg65816_print_flags(buf, bufsize);
        default:
            return snprintf(buf, bufsize, "%04"PRIX32, dbg_reg_get(which));
    }
}

static void dbg_reg_parse(int which, const char *str)
{
    uint32_t value = strtol(str, NULL, 16);
    dbg_reg_set(which, value);
}

static uint32_t do_readmem65816(uint32_t addr);
static void do_writemem65816(uint32_t addr, uint32_t val);
static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize);

static uint32_t dbg_get_instr_addr(void)
{
    return toldpc;
}

cpu_debug_t w65816_cpu_debug = {
    .cpu_name       = "65816",
    .debug_enable   = dbg_debug_enable,
    .memread        = do_readmem65816,
    .memwrite       = do_writemem65816,
    .disassemble    = dbg_disassemble,
    .reg_names      = dbg65816_reg_names,
    .reg_get        = dbg_reg_get,
    .reg_set        = dbg_reg_set,
    .reg_print      = dbg_reg_print,
    .reg_parse      = dbg_reg_parse,
    .get_instr_addr = dbg_get_instr_addr
    //    .print_addr     = debug_print_addr16
};

static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize)
{
    return dbg65816_disassemble(&w65816_cpu_debug, addr, buf, bufsize);
}

#endif

static uint32_t do_readmem65816(uint32_t addr)
{
    uint8_t temp;
    addr &= w65816mask;
    if ((addr & ~7) == 0xFEF8) {
        temp = tube_parasite_read(addr);
        return temp;
    }
    if ((addr & 0x78000) == 0x8000 && (def || (banking & 8)))
        return w65816rom[addr & 0x7FFF];
    if ((addr & 0x7C000) == 0x4000 && !def && (banking & 1))
        return w65816ram[(addr & 0x3FFF) | ((banknum & 7) << 14)];
    if ((addr & 0x7C000) == 0x8000 && !def && (banking & 2))
        return w65816ram[(addr & 0x3FFF) | (((banknum >> 3) & 7) << 14)];
    return w65816ram[addr];
}

static uint8_t readmem65816(uint32_t addr)
{
    uint32_t value = do_readmem65816(addr);
    cycles--;
#ifdef INCLUDE_DEBUGGER
    if (dbg_w65816)
        debug_memread(&w65816_cpu_debug, addr, value, 1);
#endif
    return value;
}

static uint16_t readmemw65816(uint32_t a)
{
    uint16_t value;

    a &= w65816mask;
    value = do_readmem65816(a) | (do_readmem65816(a + 1) << 8);
#ifdef INCLUDE_DEBUGGER
    if (dbg_w65816)
        debug_memread(&w65816_cpu_debug, a, value, 2);
#endif
    return value;
}

static int endtimeslice;

static void do_writemem65816(uint32_t addr, uint32_t val)
{
    addr &= w65816mask;
    if ((addr & ~7) == 0xFEF0) {
        switch (val & 7) {
            case 0:
            case 1:
                def = val & 1;
                break;
            case 2:
            case 3:
                divider = (divider >> 1) | ((val & 1) << 3);
                break;
            case 4:
            case 5:
                banking = (banking >> 1) | ((val & 1) << 3);
                break;
            case 6:
            case 7:
                banknum = (banknum >> 1) | ((val & 1) << 5);
                break;
        }
        if (def || !(banking & 4))
            w65816mask = 0xFFFF;
        else
            w65816mask = 0x7FFFF;
        printf("def=%x divider=%x banking=%x banknum=%x mask=%"PRIX32"\r\n", def, divider, banking, banknum, w65816mask);
        return;
    }
    if ((addr & ~7) == 0xFEF8) {
        tube_parasite_write(addr, val);
        endtimeslice = 1;
        return;
    }
    if ((addr & 0x7C000) == 0x4000 && !def && (banking & 1)) {
        w65816ram[(addr & 0x3FFF) | ((banknum & 7) << 14)] = val;
        return;
    }
    if ((addr & 0x7C000) == 0x8000 && !def && (banking & 2)) {
        w65816ram[(addr & 0x3FFF) | (((banknum >> 3) & 7) << 14)] = val;
        return;
    }
    w65816ram[addr] = val;
}

static void writemem65816(uint32_t addr, uint8_t val)
{
#ifdef INCLUDE_DEBUGGER
    if (dbg_w65816)
        debug_memwrite(&w65816_cpu_debug, addr, val, 1);
#endif
    cycles--;
    do_writemem65816(addr, val);
}

static void writememw65816(uint32_t a, uint16_t v)
{
#ifdef INCLUDE_DEBUGGER
    if (dbg_w65816)
        debug_memwrite(&w65816_cpu_debug, a, v, 2);
#endif
    a &= w65816mask;
    cycles -= 2;
    do_writemem65816(a, v);
    do_writemem65816(a + 1, v >> 8);
}

#define readmem(a)     readmem65816(a)
#define readmemw(a)    readmemw65816(a)
#define writemem(a,v)  writemem65816(a,v)
#define writememw(a,v) writememw65816(a,v)

#define clockspc(c)

static void updatecpumode(void);
static int inwai = 0;

/*Addressing modes*/
static inline uint32_t absolute(void)
{
    uint32_t temp = readmemw(pbr | pc);
    pc += 2;
    return temp | dbr;
}

static uint32_t absolutex(void)
{
    uint32_t addr1 = dbr + readmemw(pbr | pc);
    uint32_t addr2 = addr1 + x.w;
    if (!p.ex || (addr1 & 0xffff00) ^ (addr2 & 0xffff00))
        --cycles;
    pc += 2;
    return addr2;
}

static uint32_t absolutey(void)
{
    uint32_t addr1 = dbr + readmemw(pbr | pc);
    uint32_t addr2 = addr1 + y.w;
    if (!p.ex || (addr1 & 0xffff00) ^ (addr2 & 0xffff00))
        --cycles;
    pc += 2;
    return addr2;
}

static inline uint32_t absolutelong(void)
{
    uint32_t temp = readmemw(pbr | pc);
    pc += 2;
    temp |= (readmem(pbr | pc) << 16);
    pc++;
    return temp;
}

static inline uint32_t absolutelongx(void)
{
    uint32_t temp = (readmemw(pbr | pc)) + x.w;
    pc += 2;
    temp += (readmem(pbr | pc) << 16);
    pc++;
    return temp;
}

static inline uint32_t zeropage(void)
{
    /* It's actually direct page, but I'm used to calling it zero page */
    uint32_t temp = readmem(pbr | pc);
    pc++;
    temp += dp;
    if (dp & 0xFF) {
        cycles--;
        clockspc(6);
    }
    return temp & 0xFFFF;
}

static inline uint32_t zeropagex(void)
{
    uint32_t temp = readmem(pbr | pc) + x.w;
    pc++;
    if (p.e)
        temp &= 0xFF;
    temp += dp;
    if (dp & 0xFF) {
        cycles--;
        clockspc(6);
    }
    return temp & 0xFFFF;
}

static inline uint32_t zeropagey(void)
{
    uint32_t temp = readmem(pbr | pc) + y.w;
    pc++;
    if (p.e)
        temp &= 0xFF;
    temp += dp;
    if (dp & 0xFF) {
        cycles--;
        clockspc(6);
    }
    return temp & 0xFFFF;
}

static inline uint32_t stack(void)
{
    uint32_t temp = readmem(pbr | pc);
    pc++;
    temp += s.w;
    return temp & 0xFFFF;
}

static inline uint32_t indirect(void)
{
    uint32_t temp = (readmem(pbr | pc) + dp) & 0xFFFF;
    pc++;
    return (readmemw(temp)) + dbr;
}

static inline uint32_t indirectx(void)
{
    uint32_t addr = (readmem(pbr | pc++) + dp + x.w) & 0xFFFF;
    return (readmemw(addr)) + dbr;
}

static inline uint32_t indirectxE(void)
{
    uint32_t addr = (readmem(pbr | pc++) + dp + x.b.l) & 0xff;
    return (readmemw(addr)) + dbr;
}

static inline uint32_t jindirectx(void)
{
    /* JSR (,x) uses PBR instead of DBR, and 2 byte address insted of 1 + dp */
    uint32_t temp = (readmem(pbr | pc) + (readmem((pbr | pc) + 1) << 8) + x.w) + pbr;
    pc += 2;
    return temp;
}

static inline uint32_t indirecty(void)
{
    uint32_t addr = (readmem(pbr | pc++) + dp) & 0xFFFF;
    return (readmemw(addr)) + y.w + dbr;
}

static uint32_t indirectyE(void)
{
    uint32_t addr1, addr2;
    uint8_t imm = readmem(pbr | pc++);
    if (imm == 0xff) {
        /* Fetching the two bytes of the indirect address wraps within the page */
        addr1 = readmem((dp + imm) & 0xffff);
        addr1 |= readmem(dp) << 8;
    }
    else {
        /* The indirect address is two bytes fetched normally. */
        addr1 = (imm + dp) & 0xFFFF;
        addr1 = readmemw(addr1);
    }
    addr1 += dbr;
    addr2 = addr1 + y.w;
    if (!p.ex || (addr1 & 0xffff00) ^ (addr2 & 0xffff00))
        --cycles;
    return addr2 & 0xffff;
}

static inline uint32_t sindirecty(void)
{
    uint32_t temp = (readmem(pbr | pc) + s.w) & 0xFFFF;
    pc++;
    return (readmemw(temp)) + y.w + dbr;
}

static inline uint32_t indirectl(void)
{
    uint32_t temp, addr;
    temp = (readmem(pbr | pc) + dp) & 0xFFFF;
    pc++;
    addr = readmemw(temp) | (readmem(temp + 2) << 16);
    return addr;
}

static inline uint32_t indirectly(void)
{
    uint32_t temp, addr;
    temp = (readmem(pbr | pc) + dp) & 0xFFFF;
    pc++;
    addr = (readmemw(temp) | (readmem(temp + 2) << 16)) + y.w;
    return addr;
}

/*Flag setting*/

static inline void setzn8(uint8_t v)
{
    p.z=!(v);
    p.n=(v)&0x80;
}

static inline void setzn16(uint16_t v)
{
    p.z=!(v);
    p.n=(v)&0x8000;
}

/*ADC/SBC*/

static inline void adcbin8(uint8_t temp)
{
    uint16_t tempw=a.b.l + temp + ((p.c) ? 1 : 0);
    p.v = (!((a.b.l ^ temp) & 0x80) && ((a.b.l ^ tempw) & 0x80));
    a.b.l = tempw & 0xFF;
    setzn8(a.b.l);
    p.c = tempw & 0x100;
}

static inline void adcbcd8(uint8_t temp)
{
    int al, ah;

    ah = 0;
    al = (a.b.l & 0xF) + (temp & 0xF) + (p.c ? 1 : 0);
    if (al > 9) {
        al -= 10;
        al &= 0xF;
        ah = 1;
    }
    ah += ((a.b.l >> 4) + (temp >> 4));
    p.v = (((ah << 4) ^ a.b.l) & 0x80) && !((a.b.l ^ temp) & 0x80);
    p.c = 0;
    if (ah > 9) {
        p.c = 1;
        ah -= 10;
        ah &= 0xF;
    }
    a.b.l = (al & 0xF) | (ah << 4);
    setzn8(a.b.l);
    cycles--;
    clockspc(6);
}

static inline void adc8(uint8_t temp)
{
    if (p.d)
        adcbcd8(temp);
    else
        adcbin8(temp);
}

static inline void adcbin16(uint16_t tempw)
{
    uint32_t templ = a.w+tempw + ((p.c) ? 1 : 0);
    p.v = (!((a.w ^ tempw) & 0x8000) && ((a.w ^ templ) & 0x8000));
    a.w = templ & 0xFFFF;
    setzn16(a.w);
    p.c = templ & 0x10000;
}

static inline void adcbcd16(uint16_t tempw)
{
    uint32_t templ = (a.w & 0xF) + (tempw & 0xF) + (p.c ? 1 : 0);
    if (templ > 9)
        templ += 6;
    templ += ((a.w & 0xF0) + (tempw & 0xF0));
    if (templ > 0x9F)
        templ += 0x60;
    templ += ((a.w & 0xF00) + (tempw & 0xF00));
    if (templ > 0x9FF)
        templ += 0x600;
    templ += ((a.w & 0xF000) + (tempw & 0xF000));
    if (templ > 0x9FFF)
        templ += 0x6000;
    p.v= (!((a.w ^ tempw) & 0x8000) && ((a.w ^ templ) & 0x8000));
    a.w = templ & 0xFFFF;
    setzn16(a.w);
    p.c=templ > 0xFFFF;
    cycles--;
    clockspc(6);
}

static inline void adc16(uint16_t temp)
{
    if (p.d)
        adcbcd16(temp);
    else
        adcbin16(temp);
}

static inline void sbcbin8(uint8_t temp)
{
    uint16_t tempw = a.b.l - temp - ((p.c) ? 0 : 1);
    p.v = (((a.b.l ^ temp) & 0x80) && ((a.b.l ^ tempw) & 0x80));
    a.b.l = tempw & 0xFF;
    setzn8(a.b.l);
    p.c = tempw <= 0xFF;
}

static inline void sbcbcd8(uint8_t temp)
{
    int al;
    int16_t tempw;
    int32_t tempv;

    al = (a.b.l & 0x0f) - (temp & 0x0f) - (p.c ? 0 : 1);
    tempw = a.b.l - temp - (p.c ? 0 : 1);
    tempv = (signed char)a.b.l - (signed char)temp - (p.c ? 0 : 1);
    p.v = ((tempw & 0x80) > 0) ^ ((tempv & 0x100) != 0);
    p.c = tempw >= 0;
    if (tempw < 0)
       tempw -= 0x60;
    if (al < 0)
       tempw -= 0x06;
    a.b.l = tempw & 0xFF;
    setzn8(a.b.l);
    cycles--;
    clockspc(6);
}

static inline void sbc8(uint8_t temp)
{
    if (p.d)
        sbcbcd8(temp);
    else
        sbcbin8(temp);
}

static inline void sbcbin16(uint16_t tempw)
{
    uint32_t templ = a.w - tempw - ((p.c) ? 0 : 1);
    p.v = (((a.w ^ tempw) & (a.w ^ templ)) & 0x8000);
    a.w = templ & 0xFFFF;
    setzn16(a.w);
    p.c=templ<=0xFFFF;
}

static inline void sbcbcd16(uint16_t tempw)
{
    uint32_t templ = (a.w & 0xF) - (tempw & 0xF) - (p.c ? 0 : 1);
    if (templ > 9)
        templ -= 6;
    templ += ((a.w & 0xF0) - (tempw & 0xF0));
    if (templ > 0x9F)
        templ -= 0x60;
    templ += ((a.w & 0xF00) - (tempw & 0xF00));
    if (templ > 0x9FF)
        templ -= 0x600;
    templ += ((a.w & 0xF000) - (tempw & 0xF000));
    if (templ > 0x9FFF)
        templ -= 0x6000;
    p.v = (((a.w ^ tempw) & 0x8000) && ((a.w ^ templ) & 0x8000));
    a.w = templ & 0xFFFF;
    setzn16(a.w);
    p.c=templ <= 0xFFFF;
    cycles--;
    clockspc(6);
}

static inline void sbc16(uint16_t temp)
{
    if (p.d)
        sbcbcd16(temp);
    else
        sbcbin16(temp);
}

/*Instructions*/
static void inca8(void)
{
    readmem(pbr | pc);
    a.b.l++;
    setzn8(a.b.l);
}

static void inca16(void)
{
    readmem(pbr | pc);
    a.w++;
    setzn16(a.w);
}

static void inx8(void)
{
    readmem(pbr | pc);
    x.b.l++;
    setzn8(x.b.l);
}

static void inx16(void)
{
    readmem(pbr | pc);
    x.w++;
    setzn16(x.w);
}

static void iny8(void)
{
    readmem(pbr | pc);
    y.b.l++;
    setzn8(y.b.l);
}

static void iny16(void)
{
    readmem(pbr | pc);
    y.w++;
    setzn16(y.w);
}

static void deca8(void)
{
    readmem(pbr | pc);
    a.b.l--;
    setzn8(a.b.l);
}

static void deca16(void)
{
    readmem(pbr | pc);
    a.w--;
    setzn16(a.w);
}

static void dex8(void)
{
    readmem(pbr | pc);
    x.b.l--;
    setzn8(x.b.l);
}

static void dex16(void)
{
    readmem(pbr | pc);
    x.w--;
    setzn16(x.w);
}

static void dey8(void)
{
    readmem(pbr | pc);
    y.b.l--;
    setzn8(y.b.l);
}

static void dey16(void)
{
    readmem(pbr | pc);
    y.w--;
    setzn16(y.w);
}

/*INC group*/
static void incZp8(void)
{
    uint32_t addr = zeropage();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    temp++;
    setzn8(temp);
    writemem(addr, temp);
}

static void incZp16(void)
{
    uint32_t addr = zeropage();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    temp++;
    setzn16(temp);
    writememw(addr, temp);
}

static void incZpx8(void)
{
    uint32_t addr = zeropagex();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    temp++;
    setzn8(temp);
    writemem(addr, temp);
}

static void incZpx16(void)
{
    uint32_t addr = zeropagex();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    temp++;
    setzn16(temp);
    writememw(addr, temp);
}

static void incAbs8(void)
{
    uint32_t addr = absolute();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    temp++;
    setzn8(temp);
    writemem(addr, temp);
}

static void incAbs16(void)
{
    uint32_t addr = absolute();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    temp++;
    setzn16(temp);
    writememw(addr, temp);
}

static void incAbsx8(void)
{
    uint32_t addr = absolutex();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    temp++;
    setzn8(temp);
    writemem(addr, temp);
}

static void incAbsx16(void)
{
    uint32_t addr = absolutex();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    temp++;
    setzn16(temp);
    writememw(addr, temp);
}

/*DEC group*/
static void decZp8(void)
{
    uint32_t addr = zeropage();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    temp--;
    setzn8(temp);
    writemem(addr, temp);
}

static void decZp16(void)
{
    uint32_t addr = zeropage();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    temp--;
    setzn16(temp);
    writememw(addr, temp);
}

static void decZpx8(void)
{
    uint32_t addr = zeropagex();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    temp--;
    setzn8(temp);
    writemem(addr, temp);
}

static void decZpx16(void)
{
    uint32_t addr = zeropagex();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    temp--;
    setzn16(temp);
    writememw(addr, temp);
}

static void decAbs8(void)
{
    uint32_t addr = absolute();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    temp--;
    setzn8(temp);
    writemem(addr, temp);
}

static void decAbs16(void)
{
    uint32_t addr = absolute();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    temp--;
    setzn16(temp);
    writememw(addr, temp);
}

static void decAbsx8(void)
{
    uint32_t addr = absolutex();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    temp--;
    setzn8(temp);
    writemem(addr, temp);
}

static void decAbsx16(void)
{
    uint32_t addr = absolutex();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    temp--;
    setzn16(temp);
    writememw(addr, temp);
}

/*Flag group*/
static void clc(void)
{
    readmem(pbr | pc);
    p.c = 0;
}

static void cld(void)
{
    readmem(pbr | pc);
    p.d = 0;
}

static void cli(void)
{
    readmem(pbr | pc);
    p.i = 0;
}

static void clv(void)
{
    readmem(pbr | pc);
    p.v = 0;
}

static void sec(void)
{
    readmem(pbr | pc);
    p.c = 1;
}

static void sed(void)
{
    readmem(pbr | pc);
    p.d = 1;
}

static void sei(void)
{
    readmem(pbr | pc);
    p.i = 1;
}

static void xce(void)
{
    int temp = p.c;
    p.c = p.e;
    p.e = temp;
    readmem(pbr | pc);
    // When re-entering native mode, the X and M flags will be 1 (8-bits)
    // (i.e. these flags are not preserved in emulation mode)
    if (p.c && !p.e) {
        p.ex = p.m = 1;
    }
    updatecpumode();
}

static void sep(void)
{
    uint8_t temp = readmem(pbr | pc++);
    if (temp & 1)
        p.c = 1;
    if (temp & 2)
        p.z = 1;
    if (temp & 4)
        p.i = 1;
    if (temp & 8)
        p.d = 1;
    if (temp & 0x40)
        p.v = 1;
    if (temp & 0x80)
        p.n = 1;
    if (!p.e) {
        if (temp & 0x10)
            p.ex = 1;
        if (temp & 0x20)
            p.m = 1;
        updatecpumode();
    }
}

static void rep65816(void)
{
    uint8_t temp = readmem(pbr | pc++);
    if (temp & 1)
        p.c = 0;
    if (temp & 2)
        p.z = 0;
    if (temp & 4)
        p.i = 0;
    if (temp & 8)
        p.d = 0;
    if (temp & 0x40)
        p.v = 0;
    if (temp & 0x80)
        p.n = 0;
    if (!p.e) {
        if (temp & 0x10)
            p.ex = 0;
        if (temp & 0x20)
            p.m = 0;
        updatecpumode();
    }
}

/*Transfer group*/
static void tax8(void)
{
    readmem(pbr | pc);
    x.b.l = a.b.l;
    setzn8(x.b.l);
}

static void tay8(void)
{
    readmem(pbr | pc);
    y.b.l = a.b.l;
    setzn8(y.b.l);
}

static void txa8(void)
{
    readmem(pbr | pc);
    a.b.l = x.b.l;
    setzn8(a.b.l);
}

static void tya8(void)
{
    readmem(pbr | pc);
    a.b.l = y.b.l;
    setzn8(a.b.l);
}

static void tsx8(void)
{
    readmem(pbr | pc);
    x.b.l = s.b.l;
    setzn8(x.b.l);
}

static void txs8(void)
{
    readmem(pbr | pc);
    s.b.l = x.b.l;
}

static void txy8(void)
{
    readmem(pbr | pc);
    y.b.l = x.b.l;
    setzn8(y.b.l);
}

static void tyx8(void)
{
    readmem(pbr | pc);
    x.b.l = y.b.l;
    setzn8(x.b.l);
}

static void tax16(void)
{
    readmem(pbr | pc);
    x.w = a.w;
    setzn16(x.w);
}

static void tay16(void)
{
    readmem(pbr | pc);
    y.w = a.w;
    setzn16(y.w);
}

static void txa16(void)
{
    readmem(pbr | pc);
    a.w = x.w;
    setzn16(a.w);
}

static void tya16(void)
{
    readmem(pbr | pc);
    a.w = y.w;
    setzn16(a.w);
}

static void tsx16(void)
{
    readmem(pbr | pc);
    x.w = s.w;
    setzn16(x.w);
}

static void txs16(void)
{
    readmem(pbr | pc);
    s.w = x.w;
}

static void txy16(void)
{
    readmem(pbr | pc);
    y.w = x.w;
    setzn16(y.w);
}

static void tyx16(void)
{
    readmem(pbr | pc);
    x.w = y.w;
    setzn16(x.w);
}

/*LDX group*/
static void ldxImm8(void)
{
    x.b.l = readmem(pbr | pc);
    pc++;
    setzn8(x.b.l);
}

static void ldxZp8(void)
{
    x.b.l = readmem(zeropage());
    setzn8(x.b.l);
}

static void ldxZpy8(void)
{
    x.b.l = readmem(zeropagey());
    setzn8(x.b.l);
}

static void ldxAbs8(void)
{
    x.b.l = readmem(absolute());
    setzn8(x.b.l);
}

static void ldxAbsy8(void)
{
    x.b.l = readmem(absolutey());
    setzn8(x.b.l);
}

static void ldxImm16(void)
{
    x.w = readmemw(pbr | pc);
    pc += 2;
    setzn16(x.w);
}

static void ldxZp16(void)
{
    x.w = readmemw(zeropage());
    setzn16(x.w);
}

static void ldxZpy16(void)
{
    x.w = readmemw(zeropagey());
    setzn16(x.w);
}

static void ldxAbs16(void)
{
    x.w = readmemw(absolute());
    setzn16(x.w);
}

static void ldxAbsy16(void)
{
    x.w = readmemw(absolutey());
    setzn16(x.w);
}

/*LDY group*/
static void ldyImm8(void)
{
    y.b.l = readmem(pbr | pc);
    pc++;
    setzn8(y.b.l);
}

static void ldyZp8(void)
{
    y.b.l = readmem(zeropage());
    setzn8(y.b.l);
}

static void ldyZpx8(void)
{
    y.b.l = readmem(zeropagex());
    setzn8(y.b.l);
}

static void ldyAbs8(void)
{
    y.b.l = readmem(absolute());
    setzn8(y.b.l);
}

static void ldyAbsx8(void)
{
    y.b.l = readmem(absolutex());
    setzn8(y.b.l);
}

static void ldyImm16(void)
{
    y.w = readmemw(pbr | pc);
    pc += 2;
    setzn16(y.w);
}

static void ldyZp16(void)
{
    y.w = readmemw(zeropage());
    setzn16(y.w);
}

static void ldyZpx16(void)
{
    y.w = readmemw(zeropagex());
    setzn16(y.w);
}

static void ldyAbs16(void)
{
    y.w = readmemw(absolute());
    setzn16(y.w);
}

static void ldyAbsx16(void)
{
    y.w = readmemw(absolutex());
    setzn16(y.w);
}

/*LDA group*/
static void ldaImm8(void)
{
    a.b.l = readmem(pbr | pc);
    pc++;
    setzn8(a.b.l);
}

static void ldaZp8(void)
{
    a.b.l = readmem(zeropage());
    setzn8(a.b.l);
}

static void ldaZpx8(void)
{
    a.b.l = readmem(zeropagex());
    setzn8(a.b.l);
}

static void ldaSp8(void)
{
    a.b.l = readmem(stack());
    setzn8(a.b.l);
}

static void ldaSIndirecty8(void)
{
    a.b.l = readmem(sindirecty());
    setzn8(a.b.l);
}

static void ldaAbs8(void)
{
    a.b.l = readmem(absolute());
    setzn8(a.b.l);
}

static void ldaAbsx8(void)
{
    a.b.l = readmem(absolutex());
    setzn8(a.b.l);
}

static void ldaAbsy8(void)
{
    a.b.l = readmem(absolutey());
    setzn8(a.b.l);
}

static void ldaLong8(void)
{
    a.b.l = readmem(absolutelong());
    setzn8(a.b.l);
}

static void ldaLongx8(void)
{
    a.b.l = readmem(absolutelongx());
    setzn8(a.b.l);
}

static void ldaIndirect8(void)
{
    a.b.l = readmem(indirect());
    setzn8(a.b.l);
}

static void ldaIndirectx8(void)
{
    a.b.l = readmem(indirectx());
    setzn8(a.b.l);
}

static void ldaIndirectxE(void)
{
    a.b.l = readmem(indirectxE());
    setzn8(a.b.l);
}

static void ldaIndirecty8(void)
{
    a.b.l = readmem(indirecty());
    setzn8(a.b.l);
}

static void ldaIndirectyE(void)
{
    a.b.l = readmem(indirectyE());
    setzn8(a.b.l);
}

static void ldaIndirectLong8(void)
{
    a.b.l = readmem(indirectl());
    setzn8(a.b.l);
}

static void ldaIndirectLongy8(void)
{
    a.b.l = readmem(indirectly());
    setzn8(a.b.l);
}

static void ldaImm16(void)
{
    a.w = readmemw(pbr | pc);
    pc += 2;
    setzn16(a.w);
}

static void ldaZp16(void)
{
    a.w = readmemw(zeropage());
    setzn16(a.w);
}

static void ldaZpx16(void)
{
    a.w = readmemw(zeropagex());
    setzn16(a.w);
}

static void ldaSp16(void)
{
    a.w = readmemw(stack());
    setzn16(a.w);
}

static void ldaSIndirecty16(void)
{
    a.w = readmemw(sindirecty());
    setzn16(a.w);
}

static void ldaAbs16(void)
{
    a.w = readmemw(absolute());
    setzn16(a.w);
}

static void ldaAbsx16(void)
{
    a.w = readmemw(absolutex());
    setzn16(a.w);
}

static void ldaAbsy16(void)
{
    a.w = readmemw(absolutey());
    setzn16(a.w);
}

static void ldaLong16(void)
{
    a.w = readmemw(absolutelong());
    setzn16(a.w);
}

static void ldaLongx16(void)
{
    a.w = readmemw(absolutelongx());
    setzn16(a.w);
}

static void ldaIndirect16(void)
{
    a.w = readmemw(indirect());
    setzn16(a.w);
}

static void ldaIndirectx16(void)
{
    a.w = readmemw(indirectx());
    setzn16(a.w);
}

static void ldaIndirecty16(void)
{
    a.w = readmemw(indirecty());
    setzn16(a.w);
}

static void ldaIndirectLong16(void)
{
    a.w = readmemw(indirectl());
    setzn16(a.w);
}

static void ldaIndirectLongy16(void)
{
    a.w = readmemw(indirectly());
    setzn16(a.w);
}

/*STA group*/
static void staZp8(void)
{
    writemem(zeropage(), a.b.l);
}

static void staZpx8(void)
{
    writemem(zeropagex(), a.b.l);
}

static void staAbs8(void)
{
    writemem(absolute(), a.b.l);
}

static void staAbsx8(void)
{
    writemem(absolutex(), a.b.l);
}

static void staAbsy8(void)
{
    writemem(absolutey(), a.b.l);
}

static void staLong8(void)
{
    writemem(absolutelong(), a.b.l);
}

static void staLongx8(void)
{
    writemem(absolutelongx(), a.b.l);
}

static void staIndirect8(void)
{
    writemem(indirect(), a.b.l);
}

static void staIndirectx8(void)
{
    writemem(indirectx(), a.b.l);
}

static void staIndirectxE(void)
{
    writemem(indirectxE(), a.b.l);
}

static void staIndirecty8(void)
{
    writemem(indirecty(), a.b.l);
}

static void staIndirectyE(void)
{
    writemem(indirectyE(), a.b.l);
}

static void staIndirectLong8(void)
{
    writemem(indirectl(), a.b.l);
}

static void staIndirectLongy8(void)
{
    writemem(indirectly(), a.b.l);
}

static void staSp8(void)
{
    writemem(stack(), a.b.l);
}

static void staSIndirecty8(void)
{
    writemem(sindirecty(), a.b.l);
}

static void staZp16(void)
{
    writememw(zeropage(), a.w);
}

static void staZpx16(void)
{
    writememw(zeropagex(), a.w);
}

static void staAbs16(void)
{
    writememw(absolute(), a.w);
}

static void staAbsx16(void)
{
    writememw(absolutex(), a.w);
}

static void staAbsy16(void)
{
    writememw(absolutey(), a.w);
}

static void staLong16(void)
{
    writememw(absolutelong(), a.w);
}

static void staLongx16(void)
{
    writememw(absolutelongx(), a.w);
}

static void staIndirect16(void)
{
    writememw(indirect(), a.w);
}

static void staIndirectx16(void)
{
    writememw(indirectx(), a.w);
}

static void staIndirecty16(void)
{
    writememw(indirecty(), a.w);
}

static void staIndirectLong16(void)
{
    writememw(indirectl(), a.w);
}

static void staIndirectLongy16(void)
{
    writememw(indirectly(), a.w);
}

static void staSp16(void)
{
    writememw(stack(), a.w);
}

static void staSIndirecty16(void)
{
    writememw(sindirecty(), a.w);
}

/*STX group*/
static void stxZp8(void)
{
    writemem(zeropage(), x.b.l);
}

static void stxZpy8(void)
{
    writemem(zeropagey(), x.b.l);
}

static void stxAbs8(void)
{
    writemem(absolute(), x.b.l);
}

static void stxZp16(void)
{
    writememw(zeropage(), x.w);
}

static void stxZpy16(void)
{
    writememw(zeropagey(), x.w);
}

static void stxAbs16(void)
{
    writememw(absolute(), x.w);
}

/*STY group*/
static void styZp8(void)
{
    writemem(zeropage(), y.b.l);
}

static void styZpx8(void)
{
    writemem(zeropagex(), y.b.l);
}

static void styAbs8(void)
{
    writemem(absolute(), y.b.l);
}

static void styZp16(void)
{
    writememw(zeropage(), y.w);
}

static void styZpx16(void)
{
    writememw(zeropagex(), y.w);
}

static void styAbs16(void)
{
    writememw(absolute(), y.w);
}

/*STZ group*/
static void stzZp8(void)
{
    writemem(zeropage(), 0);
}

static void stzZpx8(void)
{
    writemem(zeropagex(), 0);
}

static void stzAbs8(void)
{
    writemem(absolute(), 0);
}

static void stzAbsx8(void)
{
    writemem(absolutex(), 0);
}

static void stzZp16(void)
{
    writememw(zeropage(), 0);
}

static void stzZpx16(void)
{
    writememw(zeropagex(), 0);
}

static void stzAbs16(void)
{
    writememw(absolute(), 0);
}

static void stzAbsx16(void)
{
    writememw(absolutex(), 0);
}

/*ADC group*/
static void adcImm8(void)
{
    adc8(readmem(pbr | pc++));
}

static void adcZp8(void)
{
    adc8(readmem(zeropage()));
}

static void adcZpx8(void)
{
    adc8(readmem(zeropagex()));
}

static void adcSp8(void)
{
    adc8(readmem(stack()));
}

static void adcAbs8(void)
{
    adc8(readmem(absolute()));
}

static void adcAbsx8(void)
{
    adc8(readmem(absolutex()));
}

static void adcAbsy8(void)
{
    adc8(readmem(absolutey()));
}

static void adcLong8(void)
{
    adc8(readmem(absolutelong()));
}

static void adcLongx8(void)
{
    adc8(readmem(absolutelongx()));
}

static void adcIndirect8(void)
{
    adc8(readmem(indirect()));
}

static void adcIndirectx8(void)
{
    adc8(readmem(indirectx()));
}

static void adcIndirectxE(void)
{
    adc8(readmem(indirectxE()));
}

static void adcIndirecty8(void)
{
    adc8(readmem(indirecty()));
}

static void adcIndirectyE(void)
{
    adc8(readmem(indirectyE()));
}

static void adcsIndirecty8(void)
{
    adc8(readmem(sindirecty()));
}

static void adcIndirectLong8(void)
{
    adc8(readmem(indirectl()));
}

static void adcIndirectLongy8(void)
{
    adc8(readmem(indirectly()));
}

static void adcImm16(void)
{
    adc16(readmemw(pbr | pc));
    pc += 2;
}

static void adcZp16(void)
{
    adc16(readmemw(zeropage()));
}

static void adcZpx16(void)
{
    adc16(readmemw(zeropagex()));
}

static void adcSp16(void)
{
    adc16(readmemw(stack()));
}

static void adcAbs16(void)
{
    adc16(readmemw(absolute()));
}

static void adcAbsx16(void)
{
    adc16(readmemw(absolutex()));
}

static void adcAbsy16(void)
{
    adc16(readmemw(absolutey()));
}

static void adcLong16(void)
{
    adc16(readmemw(absolutelong()));
}

static void adcLongx16(void)
{
    adc16(readmemw(absolutelongx()));
}

static void adcIndirect16(void)
{
    adc16(readmemw(indirect()));
}

static void adcIndirectx16(void)
{
    adc16(readmemw(indirectx()));
}

static void adcIndirecty16(void)
{
    adc16(readmemw(indirecty()));
}

static void adcsIndirecty16(void)
{
    adc16(readmemw(sindirecty()));
}

static void adcIndirectLong16(void)
{
    adc16(readmemw(indirectl()));
}

static void adcIndirectLongy16(void)
{
    adc16(readmemw(indirectly()));
}

/*SBC group*/
static void sbcImm8(void)
{
    sbc8(readmem(pbr | pc++));
}

static void sbcZp8(void)
{
    sbc8(readmem(zeropage()));
}

static void sbcZpx8(void)
{
    sbc8(readmem(zeropagex()));
}

static void sbcSp8(void)
{
    sbc8(readmem(stack()));
}

static void sbcAbs8(void)
{
    sbc8(readmem(absolute()));
}

static void sbcAbsx8(void)
{
    sbc8(readmem(absolutex()));
}

static void sbcAbsy8(void)
{
    sbc8(readmem(absolutey()));
}

static void sbcLong8(void)
{
    sbc8(readmem(absolutelong()));
}

static void sbcLongx8(void)
{
    sbc8(readmem(absolutelongx()));
}

static void sbcIndirect8(void)
{
    sbc8(readmem(indirect()));
}

static void sbcIndirectx8(void)
{
    sbc8(readmem(indirectx()));
}

static void sbcIndirectxE(void)
{
    sbc8(readmem(indirectxE()));
}

static void sbcIndirecty8(void)
{
    sbc8(readmem(indirecty()));
}

static void sbcIndirectyE(void)
{
    sbc8(readmem(indirectyE()));
}

static void sbcsIndirecty8(void)
{
    sbc8(readmem(sindirecty()));
}

static void sbcIndirectLong8(void)
{
    sbc8(readmem(indirectl()));
}

static void sbcIndirectLongy8(void)
{
    sbc8(readmem(indirectly()));
}

static void sbcImm16(void)
{
    sbc16(readmemw(pbr | pc));
    pc += 2;
}

static void sbcZp16(void)
{
    sbc16(readmemw(zeropage()));
}

static void sbcZpx16(void)
{
    sbc16(readmemw(zeropagex()));
}

static void sbcSp16(void)
{
    sbc16(readmemw(stack()));
}

static void sbcAbs16(void)
{
    sbc16(readmemw(absolute()));
}

static void sbcAbsx16(void)
{
    sbc16(readmemw(absolutex()));
}

static void sbcAbsy16(void)
{
    sbc16(readmemw(absolutey()));
}

static void sbcLong16(void)
{
    sbc16(readmemw(absolutelong()));
}

static void sbcLongx16(void)
{
    sbc16(readmemw(absolutelongx()));
}

static void sbcIndirect16(void)
{
    sbc16(readmemw(indirect()));
}

static void sbcIndirectx16(void)
{
    sbc16(readmemw(indirectx()));
}

static void sbcIndirecty16(void)
{
    sbc16(readmemw(indirecty()));
}

static void sbcsIndirecty16(void)
{
    sbc16(readmemw(sindirecty()));
}

static void sbcIndirectLong16(void)
{
    sbc16(readmemw(indirectl()));
}

static void sbcIndirectLongy16(void)
{
    sbc16(readmemw(indirectly()));
}

/*EOR group*/
static void eorImm8(void)
{
    a.b.l ^= readmem(pbr | pc);
    pc++;
    setzn8(a.b.l);
}

static void eorZp8(void)
{
    a.b.l ^= readmem(zeropage());
    setzn8(a.b.l);
}

static void eorZpx8(void)
{
    a.b.l ^= readmem(zeropagex());
    setzn8(a.b.l);
}

static void eorSp8(void)
{
    a.b.l ^= readmem(stack());
    setzn8(a.b.l);
}

static void eorAbs8(void)
{
    a.b.l ^= readmem(absolute());
    setzn8(a.b.l);
}

static void eorAbsx8(void)
{
    a.b.l ^= readmem(absolutex());
    setzn8(a.b.l);
}

static void eorAbsy8(void)
{
    a.b.l ^= readmem(absolutey());
    setzn8(a.b.l);
}

static void eorLong8(void)
{
    a.b.l ^= readmem(absolutelong());
    setzn8(a.b.l);
}

static void eorLongx8(void)
{
    a.b.l ^= readmem(absolutelongx());
    setzn8(a.b.l);
}

static void eorIndirect8(void)
{
    a.b.l ^= readmem(indirect());
    setzn8(a.b.l);
}

static void eorIndirectx8(void)
{
    a.b.l ^= readmem(indirectx());
    setzn8(a.b.l);
}

static void eorIndirectxE(void)
{
    a.b.l ^= readmem(indirectxE());
    setzn8(a.b.l);
}

static void eorIndirecty8(void)
{
    a.b.l ^= readmem(indirecty());
    setzn8(a.b.l);
}

static void eorIndirectyE(void)
{
    a.b.l ^= readmem(indirectyE());
    setzn8(a.b.l);
}

static void eorsIndirecty8(void)
{
    a.b.l ^= readmem(sindirecty());
    setzn8(a.b.l);
}

static void eorIndirectLong8(void)
{
    a.b.l ^= readmem(indirectl());
    setzn8(a.b.l);
}

static void eorIndirectLongy8(void)
{
    a.b.l ^= readmem(indirectly());
    setzn8(a.b.l);
}

static void eorImm16(void)
{
    a.w ^= readmemw(pbr | pc);
    pc += 2;
    setzn16(a.w);
}

static void eorZp16(void)
{
    a.w ^= readmemw(zeropage());
    setzn16(a.w);
}

static void eorZpx16(void)
{
    a.w ^= readmemw(zeropagex());
    setzn16(a.w);
}

static void eorSp16(void)
{
    a.w ^= readmemw(stack());
    setzn16(a.w);
}

static void eorAbs16(void)
{
    a.w ^= readmemw(absolute());
    setzn16(a.w);
}

static void eorAbsx16(void)
{
    a.w ^= readmemw(absolutex());
    setzn16(a.w);
}

static void eorAbsy16(void)
{
    a.w ^= readmemw(absolutey());
    setzn16(a.w);
}

static void eorLong16(void)
{
    uint32_t addr = absolutelong();
    a.w ^= readmemw(addr);
    setzn16(a.w);
}

static void eorLongx16(void)
{
    a.w ^= readmemw(absolutelongx());
    setzn16(a.w);
}

static void eorIndirect16(void)
{
    a.w ^= readmemw(indirect());
    setzn16(a.w);
}

static void eorIndirectx16(void)
{
    a.w ^= readmemw(indirectx());
    setzn16(a.w);
}

static void eorIndirecty16(void)
{
    a.w ^= readmemw(indirecty());
    setzn16(a.w);
}

static void eorsIndirecty16(void)
{
    a.w ^= readmemw(sindirecty());
    setzn16(a.w);
}

static void eorIndirectLong16(void)
{
    a.w ^= readmemw(indirectl());
    setzn16(a.w);
}

static void eorIndirectLongy16(void)
{
    a.w ^= readmemw(indirectly());
    setzn16(a.w);
}

/*AND group*/
static void andImm8(void)
{
    a.b.l &= readmem(pbr | pc++);
    setzn8(a.b.l);
}

static void andZp8(void)
{
    a.b.l &= readmem(zeropage());
    setzn8(a.b.l);
}

static void andZpx8(void)
{
    a.b.l &= readmem(zeropagex());
    setzn8(a.b.l);
}

static void andSp8(void)
{
    a.b.l &= readmem(stack());
    setzn8(a.b.l);
}

static void andAbs8(void)
{
    a.b.l &= readmem(absolute());
    setzn8(a.b.l);
}

static void andAbsx8(void)
{
    a.b.l &= readmem(absolutex());
    setzn8(a.b.l);
}

static void andAbsy8(void)
{
    a.b.l &= readmem(absolutey());
    setzn8(a.b.l);
}

static void andLong8(void)
{
    a.b.l &= readmem(absolutelong());
    setzn8(a.b.l);
}

static void andLongx8(void)
{
    a.b.l &= readmem(absolutelongx());
    setzn8(a.b.l);
}

static void andIndirect8(void)
{
    a.b.l &= readmem(indirect());
    setzn8(a.b.l);
}

static void andIndirectx8(void)
{
    a.b.l &= readmem(indirectx());
    setzn8(a.b.l);
}

static void andIndirectxE(void)
{
    a.b.l &= readmem(indirectxE());
    setzn8(a.b.l);
}

static void andIndirecty8(void)
{
    a.b.l &= readmem(indirecty());
    setzn8(a.b.l);
}

static void andIndirectyE(void)
{
    uint32_t addr = indirectyE();
    a.b.l &= readmem(addr);
    setzn8(a.b.l);
}

static void andsIndirecty8(void)
{
    a.b.l &= readmem(sindirecty());
    setzn8(a.b.l);
}

static void andIndirectLong8(void)
{
    a.b.l &= readmem(indirectl());
    setzn8(a.b.l);
}

static void andIndirectLongy8(void)
{
    a.b.l &= readmem(indirectly());
    setzn8(a.b.l);
}

static void andImm16(void)
{
    a.w &= readmemw(pbr | pc);
    pc += 2;
    setzn16(a.w);
}

static void andZp16(void)
{
    a.w &= readmemw(zeropage());
    setzn16(a.w);
}

static void andZpx16(void)
{
    a.w &= readmemw(zeropagex());
    setzn16(a.w);
}

static void andSp16(void)
{
    a.w &= readmemw(stack());
    setzn16(a.w);
}

static void andAbs16(void)
{
    a.w &= readmemw(absolute());
    setzn16(a.w);
}

static void andAbsx16(void)
{
    a.w &= readmemw(absolutex());
    setzn16(a.w);
}

static void andAbsy16(void)
{
    a.w &= readmemw(absolutey());
    setzn16(a.w);
}

static void andLong16(void)
{
    a.w &= readmemw(absolutelong());
    setzn16(a.w);
}

static void andLongx16(void)
{
    a.w &= readmemw(absolutelongx());
    setzn16(a.w);
}

static void andIndirect16(void)
{
    a.w &= readmemw(indirect());
    setzn16(a.w);
}

static void andIndirectx16(void)
{
    a.w &= readmemw(indirectx());
    setzn16(a.w);
}

static void andIndirecty16(void)
{
    a.w &= readmemw(indirecty());
    setzn16(a.w);
}

static void andsIndirecty16(void)
{
    a.w &= readmemw(sindirecty());
    setzn16(a.w);
}

static void andIndirectLong16(void)
{
    a.w &= readmemw(indirectl());
    setzn16(a.w);
}

static void andIndirectLongy16(void)
{
    a.w &= readmemw(indirectly());
    setzn16(a.w);
}

/*ORA group*/
static void oraImm8(void)
{
    a.b.l |= readmem(pbr | pc++);
    setzn8(a.b.l);
}

static void oraZp8(void)
{
    a.b.l |= readmem(zeropage());
    setzn8(a.b.l);
}

static void oraZpx8(void)
{
    a.b.l |= readmem(zeropagex());
    setzn8(a.b.l);
}

static void oraSp8(void)
{
    a.b.l |= readmem(stack());
    setzn8(a.b.l);
}

static void oraAbs8(void)
{
    a.b.l |= readmem(absolute());
    setzn8(a.b.l);
}

static void oraAbsx8(void)
{
    a.b.l |= readmem(absolutex());
    setzn8(a.b.l);
}

static void oraAbsy8(void)
{
    a.b.l |= readmem(absolutey());
    setzn8(a.b.l);
}

static void oraLong8(void)
{
    a.b.l |= readmem(absolutelong());
    setzn8(a.b.l);
}

static void oraLongx8(void)
{
    a.b.l |= readmem(absolutelongx());
    setzn8(a.b.l);
}

static void oraIndirect8(void)
{
    a.b.l |= readmem(indirect());
    setzn8(a.b.l);
}

static void oraIndirectx8(void)
{
    a.b.l |= readmem(indirectx());
    setzn8(a.b.l);
}

static void oraIndirectxE(void)
{
    a.b.l |= readmem(indirectxE());
    setzn8(a.b.l);
}

static void oraIndirecty8(void)
{
    a.b.l |= readmem(indirecty());
    setzn8(a.b.l);
}

static void oraIndirectyE(void)
{
    a.b.l |= readmem(indirectyE());
    setzn8(a.b.l);
}

static void orasIndirecty8(void)
{
    a.b.l |= readmem(sindirecty());
    setzn8(a.b.l);
}

static void oraIndirectLong8(void)
{
    a.b.l |= readmem(indirectl());
    setzn8(a.b.l);
}

static void oraIndirectLongy8(void)
{
    a.b.l |= readmem(indirectly());
    setzn8(a.b.l);
}

static void oraImm16(void)
{
    a.w |= readmemw(pbr | pc);
    pc += 2;
    setzn16(a.w);
}

static void oraZp16(void)
{
    a.w |= readmemw(zeropage());
    setzn16(a.w);
}

static void oraZpx16(void)
{
    a.w |= readmemw(zeropagex());
    setzn16(a.w);
}

static void oraSp16(void)
{
    a.w |= readmemw(stack());
    setzn16(a.w);
}

static void oraAbs16(void)
{
    a.w |= readmemw(absolute());
    setzn16(a.w);
}

static void oraAbsx16(void)
{
    a.w |= readmemw(absolutex());
    setzn16(a.w);
}

static void oraAbsy16(void)
{
    a.w |= readmemw(absolutey());
    setzn16(a.w);
}

static void oraLong16(void)
{
    a.w |= readmemw(absolutelong());
    setzn16(a.w);
}

static void oraLongx16(void)
{
    a.w |= readmemw(absolutelongx());
    setzn16(a.w);
}

static void oraIndirect16(void)
{
    a.w |= readmemw(indirect());
    setzn16(a.w);
}

static void oraIndirectx16(void)
{
    a.w |= readmemw(indirectx());
    setzn16(a.w);
}

static void oraIndirecty16(void)
{
    a.w |= readmemw(indirecty());
    setzn16(a.w);
}

static void orasIndirecty16(void)
{
    a.w |= readmem(sindirecty());
    setzn16(a.w);
}

static void oraIndirectLong16(void)
{
    a.w |= readmemw(indirectl());
    setzn16(a.w);
}

static void oraIndirectLongy16(void)
{
    a.w |= readmemw(indirectly());
    setzn16(a.w);
}

/*BIT group*/
static void bitImm8(void)
{
    p.z = !(readmem(pbr | pc++) & a.b.l);
}

static void bitImm16(void)
{
    p.z = !(readmemw(pbr | pc) & a.w);
    pc += 2;
}

static void bitZp8(void)
{
    uint8_t temp = readmem(zeropage());
    p.z = !(temp & a.b.l);
    p.v = temp & 0x40;
    p.n = temp & 0x80;
}

static void bitZp16(void)
{
    uint16_t temp = readmemw(zeropage());
    p.z = !(temp & a.w);
    p.v = temp & 0x4000;
    p.n = temp & 0x8000;
}

static void bitZpx8(void)
{
    uint8_t temp = readmem(zeropagex());
    p.z = !(temp & a.b.l);
    p.v = temp & 0x40;
    p.n = temp & 0x80;
}

static void bitZpx16(void)
{
    uint16_t temp = readmemw(zeropagex());
    p.z = !(temp & a.w);
    p.v = temp & 0x4000;
    p.n = temp & 0x8000;
}

static void bitAbs8(void)
{
    uint8_t temp = readmem(absolute());
    p.z = !(temp & a.b.l);
    p.v = temp & 0x40;
    p.n = temp & 0x80;
}

static void bitAbs16(void)
{
    uint16_t temp = readmemw(absolute());
    p.z = !(temp & a.w);
    p.v = temp & 0x4000;
    p.n = temp & 0x8000;
}

static void bitAbsx8(void)
{
    uint8_t temp = readmem(absolutex());
    p.z = !(temp & a.b.l);
    p.v = temp & 0x40;
    p.n = temp & 0x80;
}

static void bitAbsx16(void)
{
    uint16_t temp = readmemw(absolutex());
    p.z = !(temp & a.w);
    p.v = temp & 0x4000;
    p.n = temp & 0x8000;
}

/*CMP group*/
static void cmpImm8(void)
{
    uint8_t temp = readmem(pbr | pc++);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpZp8(void)
{
    uint8_t temp = readmem(zeropage());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpZpx8(void)
{
    uint8_t temp = readmem(zeropagex());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpSp8(void)
{
    uint8_t temp = readmem(stack());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpAbs8(void)
{
    uint8_t temp = readmem(absolute());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpAbsx8(void)
{
    uint8_t temp = readmem(absolutex());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpAbsy8(void)
{
    uint8_t temp = readmem(absolutey());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpLong8(void)
{
    uint8_t temp = readmem(absolutelong());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpLongx8(void)
{
    uint8_t temp = readmem(absolutelongx());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpIndirect8(void)
{
    uint8_t temp = readmem(indirect());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpIndirectx8(void)
{
    uint8_t temp = readmem(indirectx());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpIndirectxE(void)
{
    uint8_t temp = readmem(indirectxE());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpIndirecty8(void)
{
    uint8_t temp = readmem(indirecty());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpIndirectyE(void)
{
    uint8_t temp = readmem(indirectyE());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpsIndirecty8(void)
{
    uint8_t temp = readmem(sindirecty());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpIndirectLong8(void)
{
    uint8_t temp = readmem(indirectl());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpIndirectLongy8(void)
{
    uint8_t temp = readmem(indirectly());
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}

static void cmpImm16(void)
{
    uint16_t temp = readmemw(pbr | pc);
    pc += 2;
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpZp16(void)
{
    uint16_t temp = readmemw(zeropage());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpSp16(void)
{
    uint16_t temp = readmemw(stack());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpZpx16(void)
{
    uint16_t temp = readmemw(zeropagex());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpAbs16(void)
{
    uint16_t temp = readmemw(absolute());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpAbsx16(void)
{
    uint16_t temp = readmemw(absolutex());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpAbsy16(void)
{
    uint16_t temp = readmemw(absolutey());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpLong16(void)
{
    uint16_t temp = readmemw(absolutelong());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpLongx16(void)
{
    uint16_t temp = readmemw(absolutelongx());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpIndirect16(void)
{
    uint16_t temp = readmemw(indirect());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpIndirectx16(void)
{
    uint16_t temp = readmemw(indirectx());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpIndirecty16(void)
{
    uint16_t temp = readmemw(indirecty());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpsIndirecty16(void)
{
    uint16_t temp = readmemw(sindirecty());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpIndirectLong16(void)
{
    uint16_t temp = readmemw(indirectl());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

static void cmpIndirectLongy16(void)
{
    uint16_t temp = readmemw(indirectly());
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}

/*Stack Group*/
static void phb(void)
{
    readmem(pbr | pc);
    writemem(s.w, dbr >> 16);
    s.w--;
}

static void phbe(void)
{
    readmem(pbr | pc);
    writemem(s.w, dbr >> 16);
    s.b.l--;
}

static void phk(void)
{
    readmem(pbr | pc);
    writemem(s.w, pbr >> 16);
    s.w--;
}

static void phke(void)
{
    readmem(pbr | pc);
    writemem(s.w, pbr >> 16);
    s.b.l--;
}

static void pea(void)
{
    uint32_t addr = readmemw(pbr | pc);
    pc += 2;
    writemem(s.w, addr >> 8);
    s.w--;
    writemem(s.w, addr & 0xFF);
    s.w--;
}

static void pei(void)
{
    uint32_t addr = indirect();
    writemem(s.w, addr >> 8);
    s.w--;
    writemem(s.w, addr & 0xFF);
    s.w--;
}

static void per(void)
{
    uint32_t addr = readmemw(pbr | pc);
    pc += 2;
    addr += pc;
    writemem(s.w, addr >> 8);
    s.w--;
    writemem(s.w, addr & 0xFF);
    s.w--;
}

static void phd(void)
{
    writemem(s.w, dp >> 8);
    s.w--;
    writemem(s.w, dp & 0xFF);
    s.w--;
}

static void pld(void)
{
    readmem(pbr | pc);
    s.w++;
    cycles--;
    clockspc(6);
    dp = readmem(s.w);
    s.w++;
    dp |= (readmem(s.w) << 8);
}

static void pha8(void)
{
    readmem(pbr | pc);
    writemem(s.w, a.b.l);
    s.w--;
}

static void phaE(void)
{
    pha8();
    if (s.w < 0x100)
        s.w = 0x1ff;
}

static void pha16(void)
{
    readmem(pbr | pc);
    writemem(s.w, a.b.h);
    s.w--;
    writemem(s.w, a.b.l);
    s.w--;
}

static void phx8(void)
{
    readmem(pbr | pc);
    writemem(s.w, x.b.l);
    s.w--;
}

static void phxE(void)
{
    phx8();
    if (s.w < 0x100)
        s.w = 0x1ff;
}

static void phx16(void)
{
    readmem(pbr | pc);
    writemem(s.w, x.b.h);
    s.w--;
    writemem(s.w, x.b.l);
    s.w--;
}

static void phy8(void)
{
    readmem(pbr | pc);
    writemem(s.w, y.b.l);
    s.w--;
}

static void phyE(void)
{
    phy8();
    if (s.w < 0x100)
        s.w = 0x1ff;
}

static void phy16(void)
{
    readmem(pbr | pc);
    writemem(s.w, y.b.h);
    s.w--;
    writemem(s.w, y.b.l);
    s.w--;
}

static inline void pla8_tail(void)
{
    readmem(pbr | pc);
    cycles--;
    clockspc(6);
    a.b.l = readmem(s.w);
    setzn8(a.b.l);
}

static void pla8(void)
{
    ++s.w;
    pla8_tail();
}

static void plaE(void)
{
    if (++s.w >= 0x200)
        s.w = 0x100;
    pla8_tail();
}

static void pla16(void)
{
    readmem(pbr | pc);
    s.w++;
    cycles--;
    clockspc(6);
    a.b.l = readmem(s.w);
    s.w++;
    a.b.h = readmem(s.w);
    setzn16(a.w);
}

static inline void plx8_tail(void)
{
    readmem(pbr | pc);
    cycles--;
    clockspc(6);
    x.b.l = readmem(s.w);
    setzn8(x.b.l);
}

static void plx8(void)
{
    ++s.w;
    plx8_tail();
}

static void plxE(void)
{
    if (++s.w >= 0x200)
        s.w = 0x100;
    plx8_tail();
}

static void plx16(void)
{
    readmem(pbr | pc);
    s.w++;
    cycles--;
    clockspc(6);
    x.b.l = readmem(s.w);
    s.w++;
    x.b.h = readmem(s.w);
    setzn16(x.w);
}

static void ply8_tail(void)
{
    readmem(pbr | pc);
    cycles--;
    clockspc(6);
    y.b.l = readmem(s.w);
    setzn8(y.b.l);
}

static void ply8(void)
{
    ++s.w;
    ply8_tail();
}

static void plyE(void)
{
    if (++s.w >= 0x200)
        s.w = 0x100;
    ply8_tail();
}

static void ply16(void)
{
    readmem(pbr | pc);
    s.w++;
    cycles--;
    clockspc(6);
    y.b.l = readmem(s.w);
    s.w++;
    y.b.h = readmem(s.w);
    setzn16(y.w);
}

static void plb(void)
{
    readmem(pbr | pc);
    s.w++;
    cycles--;
    clockspc(6);
    dbr = readmem(s.w) << 16;
}

static void plbe(void)
{
    readmem(pbr | pc);
    s.b.l++;
    cycles--;
    clockspc(6);
    dbr = readmem(s.w) << 16;
}

static void plp(void)
{
    unpack_flags(readmem(s.w + 1));
    s.w++;
    cycles -= 2;
    clockspc(12);
    updatecpumode();
}

static void plpE(void)
{
    s.b.l++;
    unpack_flags_em(readmem(s.w));
    cycles -= 2;
    clockspc(12);
}

static void php(void)
{
    uint8_t flags = pack_flags();
    readmem(pbr | pc);
    writemem(s.w, flags);
    s.w--;
}

static void phpE(void)
{
    readmem(pbr | pc);
    writemem(s.w, pack_flags_em(0x30));
    s.b.l--;
}

/*CPX group*/
static void cpxImm8(void)
{
    uint8_t temp = readmem(pbr | pc++);
    setzn8(x.b.l - temp);
    p.c = (x.b.l >= temp);
}

static void cpxImm16(void)
{
    uint16_t temp = readmemw(pbr | pc);
    pc += 2;
    setzn16(x.w - temp);
    p.c = (x.w >= temp);
}

static void cpxZp8(void)
{
    uint8_t temp;
    uint32_t addr = zeropage();
    temp = readmem(addr);
    setzn8(x.b.l - temp);
    p.c = (x.b.l >= temp);
}

static void cpxZp16(void)
{
    uint16_t temp = readmemw(zeropage());
    setzn16(x.w - temp);
    p.c = (x.w >= temp);
}

static void cpxAbs8(void)
{
    uint8_t temp = readmem(absolute());
    setzn8(x.b.l - temp);
    p.c = (x.b.l >= temp);
}

static void cpxAbs16(void)
{
    uint16_t temp = readmemw(absolute());
    setzn16(x.w - temp);
    p.c = (x.w >= temp);
}

/*CPY group*/
static void cpyImm8(void)
{
    uint8_t temp = readmem(pbr | pc++);
    setzn8(y.b.l - temp);
    p.c = (y.b.l >= temp);
}

static void cpyImm16(void)
{
    uint16_t temp = readmemw(pbr | pc);
    pc += 2;
    setzn16(y.w - temp);
    p.c = (y.w >= temp);
}

static void cpyZp8(void)
{
    uint8_t temp = readmem(zeropage());
    setzn8(y.b.l - temp);
    p.c = (y.b.l >= temp);
}

static void cpyZp16(void)
{
    uint16_t temp = readmemw(zeropage());
    setzn16(y.w - temp);
    p.c = (y.w >= temp);
}

static void cpyAbs8(void)
{
    uint8_t temp = readmem(absolute());
    setzn8(y.b.l - temp);
    p.c = (y.b.l >= temp);
}

static void cpyAbs16(void)
{
    uint16_t temp = readmemw(absolute());
    setzn16(y.w - temp);
    p.c = (y.w >= temp);
}

/*Branch group*/
static void bcc(void)
{
    int8_t temp = (int8_t) readmem(pbr | pc++);

    if (!p.c) {
        pc += temp;
        cycles--;
        clockspc(6);
    }
}

static void bcs(void)
{
    int8_t temp = (int8_t) readmem(pbr | pc++);

    if (p.c) {
        pc += temp;
        cycles--;
        clockspc(6);
    }
}

static void beq(void)
{
    int8_t temp = (int8_t) readmem(pbr | pc++);

    if (p.z) {
        pc += temp;
        cycles--;
        clockspc(6);
    }
}

static void bne(void)
{
    int8_t temp = (int8_t) readmem(pbr | pc++);
    if (!p.z) {
        pc += temp;
        cycles--;
        clockspc(6);
    }
}

static void bpl(void)
{
    int8_t temp = (int8_t) readmem(pbr | pc++);
    if (!p.n) {
        pc += temp;
        cycles--;
        clockspc(6);
    }
}

static void bmi(void)
{
    int8_t temp = (int8_t) readmem(pbr | pc++);
    if (p.n) {
        pc += temp;
        cycles--;
        clockspc(6);
    }
}

static void bvc(void)
{
    int8_t temp = (int8_t) readmem(pbr | pc++);
    if (!p.v) {
        pc += temp;
        cycles--;
        clockspc(6);
    }
}

static void bvs(void)
{
    int8_t temp = (int8_t) readmem(pbr | pc++);
    if (p.v) {
        pc += temp;
        cycles--;
        clockspc(6);
    }
}

static void bra(void)
{
    int8_t temp = (int8_t) readmem(pbr | pc++);
    pc += temp;
    cycles--;
    clockspc(6);
}

static void brl(void)
{
    uint16_t temp = readmemw(pbr | pc);
    pc += 2;
    pc += temp;
    cycles--;
    clockspc(6);
}

/*Jump group*/
static void jmp(void)
{
    pc = readmemw(pbr | pc);
}

static void jmplong(void)
{
    uint32_t addr = readmemw(pbr | pc) | (readmem((pbr | pc) + 2) << 16);
    pc = addr & 0xFFFF;
    pbr = addr & 0xFF0000;
}

static void jmpind(void)
{
    pc = readmemw(readmemw(pbr | pc));
}

static void jmpindx(void)
{
    pc = readmemw((readmemw(pbr | pc)) + x.w + pbr);
}

static void jmlind(void)
{
    uint32_t addr = readmemw(pbr | pc);
    pc = readmemw(addr);
    pbr = readmem(addr + 2) << 16;
}

static void jsr(void)
{
    uint32_t addr = readmemw(pbr | pc++);
    readmem(pbr | pc);
    writemem(s.w, pc >> 8);
    s.w--;
    writemem(s.w, pc & 0xFF);
    s.w--;
    pc = addr;
}

static void jsrE(void)
{
    uint32_t addr = readmemw(pbr | pc++);
    readmem(pbr | pc);
    writemem(s.w, pc >> 8);
    s.b.l--;
    writemem(s.w, pc & 0xFF);
    s.b.l--;
    pc = addr;
}

static void jsrIndx(void)
{
    uint32_t addr = jindirectx();
    pc--;
    writemem(s.w, pc >> 8);
    s.w--;
    writemem(s.w, pc & 0xFF);
    s.w--;
    pc = readmemw(addr);
}

static void jsrIndxE(void)
{
    uint32_t addr = jindirectx();
    pc--;
    writemem(s.w, pc >> 8);
    s.b.l--;
    writemem(s.w, pc & 0xFF);
    s.b.l--;
    pc = readmemw(addr);
}

static void jsl(void)
{
    uint8_t temp;
    uint32_t addr = readmemw(pbr | pc);
    pc += 2;
    temp = readmem(pbr | pc);
    writemem(s.w, pbr >> 16);
    s.w--;
    writemem(s.w, pc >> 8);
    s.w--;
    writemem(s.w, pc & 0xFF);
    s.w--;
    pc = addr;
    pbr = temp << 16;
}

static void jslE(void)
{
    uint8_t temp;
    uint32_t addr = readmemw(pbr | pc);
    pc += 2;
    temp = readmem(pbr | pc);
    writemem(s.w, pbr >> 16);
    s.b.l--;
    writemem(s.w, pc >> 8);
    s.b.l--;
    writemem(s.w, pc & 0xFF);
    s.b.l--;
    pc = addr;
    pbr = temp << 16;
}

static void rtl(void)
{
    cycles -= 3;
    clockspc(18);
    pc = readmemw(s.w + 1);
    s.w += 2;
    pbr = readmem(s.w + 1) << 16;
    s.w++;
    pc++;
}

static void rtlE(void)
{
    cycles -= 3;
    clockspc(18);
    s.b.l++;
    pc = readmem(s.w);
    s.b.l++;
    pc |= (readmem(s.w) << 8);
    s.b.l++;
    pbr = readmem(s.w) << 16;
    pc++;
}

static void rts(void)
{
    cycles -= 3;
    clockspc(18);
    pc = readmemw(s.w + 1);
    s.w += 2;
    pc++;
}

static void rtsE(void)
{
    cycles -= 3;
    clockspc(18);
    s.b.l++;
    pc = readmem(s.w);
    s.b.l++;
    pc |= (readmem(s.w) << 8);
    pc++;
}

static void rti(void)
{
    cycles--;
    s.w++;
    clockspc(6);
    unpack_flags(readmem(s.w));
    s.w++;
    pc = readmem(s.w);
    s.w++;
    pc |= (readmem(s.w) << 8);
    s.w++;
    pbr = readmem(s.w) << 16;
    updatecpumode();
}

static void rtiE(void)
{
    cycles--;
    s.b.l++;
    clockspc(6);
    unpack_flags_em(readmem(s.w));
    s.b.l++;
    pc = readmem(s.w);
    s.b.l++;
    pc |= (readmem(s.w) << 8);
    updatecpumode();
}

/*Shift group*/
static void asla8(void)
{
    readmem(pbr | pc);
    p.c = a.b.l & 0x80;
    a.b.l <<= 1;
    setzn8(a.b.l);
}

static void asla16(void)
{
    readmem(pbr | pc);
    p.c = a.w & 0x8000;
    a.w <<= 1;
    setzn16(a.w);
}

static void aslZp8(void)
{
    uint32_t addr = zeropage();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 0x80;
    temp <<= 1;
    setzn8(temp);
    writemem(addr, temp);
}

static void aslZp16(void)
{
    uint32_t addr = zeropage();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 0x8000;
    temp <<= 1;
    setzn16(temp);
    writememw(addr, temp);
}

static void aslZpx8(void)
{
    uint32_t addr = zeropagex();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 0x80;
    temp <<= 1;
    setzn8(temp);
    writemem(addr, temp);
}

static void aslZpx16(void)
{
    uint32_t addr = zeropagex();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 0x8000;
    temp <<= 1;
    setzn16(temp);
    writememw(addr, temp);
}

static void aslAbs8(void)
{
    uint32_t addr = absolute();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 0x80;
    temp <<= 1;
    setzn8(temp);
    writemem(addr, temp);
}

static void aslAbs16(void)
{
    uint32_t addr = absolute();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 0x8000;
    temp <<= 1;
    setzn16(temp);
    writememw(addr, temp);
}

static void aslAbsx8(void)
{
    uint32_t addr = absolutex();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 0x80;
    temp <<= 1;
    setzn8(temp);
    writemem(addr, temp);
}

static void aslAbsx16(void)
{
    uint32_t addr = absolutex();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 0x8000;
    temp <<= 1;
    setzn16(temp);
    writememw(addr, temp);
}

static void lsra8(void)
{
    readmem(pbr | pc);
    p.c = a.b.l & 1;
    a.b.l >>= 1;
    setzn8(a.b.l);
}

static void lsra16(void)
{
    readmem(pbr | pc);
    p.c = a.w & 1;
    a.w >>= 1;
    setzn16(a.w);
}

static void lsrZp8(void)
{
    uint32_t addr = zeropage();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn8(temp);
    writemem(addr, temp);
}

static void lsrZp16(void)
{
    uint32_t addr = zeropage();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn16(temp);
    writememw(addr, temp);
}

static void lsrZpx8(void)
{
    uint32_t addr = zeropagex();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn8(temp);
    writemem(addr, temp);
}

static void lsrZpx16(void)
{
    uint32_t addr = zeropagex();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn16(temp);
    writememw(addr, temp);
}

static void lsrAbs8(void)
{
    uint32_t addr = absolute();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn8(temp);
    writemem(addr, temp);
}

static void lsrAbs16(void)
{
    uint32_t addr = absolute();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn16(temp);
    writememw(addr, temp);
}

static void lsrAbsx8(void)
{
    uint32_t addr = absolutex();
    uint8_t temp = readmem(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn8(temp);
    writemem(addr, temp);
}

static void lsrAbsx16(void)
{
    uint32_t addr = absolutex();
    uint16_t temp = readmemw(addr);
    cycles--;
    clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn16(temp);
    writememw(addr, temp);
}

static inline uint8_t rol8(uint8_t value)
{
    int tempc = p.c;
    p.c = value & 0x80;
    value <<= 1;
    if (tempc)
        value |= 1;
    setzn8(value);
    return value;
}

static inline uint16_t rol16(uint16_t value)
{
    int tempc = p.c;
    p.c = value & 0x8000;
    value <<= 1;
    if (tempc)
        value |= 1;
    setzn16(a.w);
    return value;
}

static void rola8(void)
{
    readmem(pbr | pc);
    a.b.l = rol8(a.b.l);
}

static void rola16(void)
{
    readmem(pbr | pc);
    a.w = rol16(a.w);
}

static inline void rol8addr(uint32_t addr)
{
    writemem(addr, rol8(readmem(addr)));
    cycles--;
    clockspc(6);
}

static inline void rol16addr(uint32_t addr)
{
    writememw(addr, rol16(readmemw(addr)));
    cycles--;
    clockspc(6);
}

static void rolZp8(void)
{
    rol8addr(zeropage());
}

static void rolZp16(void)
{
    rol16addr(zeropage());
}

static void rolZpx8(void)
{
    rol8addr(zeropagex());
}

static void rolZpx16(void)
{
    rol16addr(zeropagex());
}

static void rolAbs8(void)
{
    rol8addr(absolute());
}

static void rolAbs16(void)
{
    rol16addr(absolute());
}

static void rolAbsx8(void)
{
    rol8addr(absolutex());
}

static void rolAbsx16(void)
{
    rol16addr(absolutex());
}

static inline uint8_t ror8(uint8_t value)
{
    int tempc = p.c;
    p.c = value & 1;
    value >>= 1;
    if (tempc)
        value |= 0x80;
    setzn8(value);
    return value;
}

static inline uint16_t ror16(uint16_t value)
{
    int tempc = p.c;
    p.c = value & 1;
    value >>= 1;
    if (tempc)
        value |= 0x8000;
    setzn16(value);
    return value;
}

static void rora8(void)
{
    readmem(pbr | pc);
    a.b.l = ror8(a.b.l);
}

static void rora16(void)
{
    readmem(pbr | pc);
    a.w = ror16(a.w);
}

static inline void ror8addr(uint32_t addr)
{
    writemem(addr, ror8(readmem(addr)));
    cycles--;
    clockspc(6);
}

static inline void ror16addr(uint32_t addr)
{
    writememw(addr, ror16(readmemw(addr)));
    cycles--;
    clockspc(6);
}

static void rorZp8(void)
{
    ror8addr(zeropage());
}

static void rorZp16(void)
{
    uint32_t addr = zeropage();
    writememw(addr, ror16(readmemw(addr)));
    cycles--;
    clockspc(6);
}

static void rorZpx8(void)
{
    ror8addr(zeropagex());
}

static void rorZpx16(void)
{
    ror16addr(zeropagex());
}

static void rorAbs8(void)
{
    ror8addr(absolute());
}

static void rorAbs16(void)
{
    ror16addr(absolute());
}

static void rorAbsx8(void)
{
    ror8addr(absolutex());
}

static void rorAbsx16(void)
{
    ror16addr(absolutex());
}

/*Misc group*/
static void xba(void)
{
    readmem(pbr | pc);
    a.w = (a.w >> 8) | (a.w << 8);
    setzn8(a.b.l);
}

static void nop(void)
{
    cycles--;
    clockspc(6);
}

static void tcd(void)
{
    readmem(pbr | pc);
    dp = a.w;
    setzn16(dp);
}

static void tdc(void)
{
    readmem(pbr | pc);
    a.w = dp;
    setzn16(a.w);
}

static void tcs(void)
{
    readmem(pbr | pc);
    s.w = a.w;
}

static void tsc(void)
{
    readmem(pbr | pc);
    a.w = s.w;
    setzn16(a.w);
}

static inline void trb8(uint32_t addr)
{
    uint8_t temp = readmem(addr);
    p.z = !(a.b.l & temp);
    temp &= ~a.b.l;
    cycles--;
    clockspc(6);
    writemem(addr, temp);
}

static inline void trb16(uint32_t addr)
{
    uint16_t temp = readmemw(addr);
    p.z = !(a.w & temp);
    temp &= ~a.w;
    cycles--;
    clockspc(6);
    writememw(addr, temp);
}

static void trbZp8(void)
{
    trb8(zeropage());
}

static void trbZp16(void)
{
    trb16(zeropage());
}

static void trbAbs8(void)
{
    trb8(absolute());
}

static void trbAbs16(void)
{
    trb16(absolute());
}

static inline void tsb8(uint32_t addr)
{
    uint16_t temp = readmem(addr);
    p.z = !(a.b.l & temp);
    temp |= a.b.l;
    cycles--;
    clockspc(6);
    writemem(addr, temp);
}

static inline void tsb16(uint32_t addr)
{
    uint16_t temp = readmemw(addr);
    p.z = !(a.w & temp);
    temp |= a.w;
    cycles--;
    clockspc(6);
    writememw(addr, temp);
}

static void tsbZp8(void)
{
    tsb8(zeropage());
}

static void tsbZp16(void)
{
    tsb16(zeropage());
}

static void tsbAbs8(void)
{
    tsb8(absolute());
}

static void tsbAbs16(void)
{
    tsb16(absolute());
}

static void wai(void)
{
    readmem(pbr | pc);
    inwai = 1;
    pc--;
}

static void mvp(void)
{
    uint8_t temp;
    uint32_t addr;
    dbr = (readmem(pbr | pc)) << 16;
    pc++;
    addr = (readmem(pbr | pc)) << 16;
    pc++;
    temp = readmem(addr | x.w);
    writemem(dbr | y.w, temp);
    x.w--;
    y.w--;
    a.w--;
    if (a.w != 0xFFFF)
        pc -= 3;
    cycles -= 2;
    clockspc(12);
}

static void mvn(void)
{
    uint8_t temp;
    uint32_t addr;
    dbr = (readmem(pbr | pc)) << 16;
    pc++;
    addr = (readmem(pbr | pc)) << 16;
    pc++;
    temp = readmem(addr | x.w);
    writemem(dbr | y.w, temp);
    x.w++;
    y.w++;
    a.w--;
    if (a.w != 0xFFFF)
        pc -= 3;
    cycles -= 2;
    clockspc(12);
}

static void op_brk(void)
{
    pc++;
    writemem(s.w--, pbr >> 16);
    writemem(s.w--, pc >> 8);
    writemem(s.w--, pc & 0xFF);
    writemem(s.w--, pack_flags());
    pc = readmemw((w65816nvb << 16) | 0xFFE6);
    pbr = 0;
    p.i = 1;
    p.d = 0;
}

static void brkE(void)
{
    pc++;
    writemem(s.w--, pc >> 8);
    writemem(s.w--, pc & 0xFF);
    writemem(s.w--, pack_flags_em(0x30));
    pc = readmemw(0xFFFE);
    pbr = 0;
    p.i = 1;
    p.d = 0;
}

static void cop(void)
{
    pc++;
    writemem(s.w--, pbr >> 16);
    writemem(s.w--, pc >> 8);
    writemem(s.w--, pc & 0xFF);
    writemem(s.w--, pack_flags());
    pc = readmemw((w65816nvb << 16) | 0xFFE4);
    pbr = 0;
    p.i = 1;
    p.d = 0;
}

static void cope(void)
{
    pc++;
    writemem(s.w--, pc >> 8);
    writemem(s.w--, pc & 0xFF);
    writemem(s.w--, pack_flags_em(0));
    pc = readmemw(0xFFF4);
    pbr = 0;
    p.i = 1;
    p.d = 0;
}

static void wdm(void)
{
    readmem(pc);
    pc++;
}

static void stp(void)
{
    /* No point emulating this properly as the external support circuitry isn't there */
    pc--;
    cycles -= 600;
}

/*Opcode table*/
static void (*opcodes[5][256])() =
{
    {
        op_brk,             /* X1M1 00 */
        oraIndirectx8,      /* X1M1 01 */
        cop,                /* X1M1 02 */
        oraSp8,             /* X1M1 03 */
        tsbZp8,             /* X1M1 04 */
        oraZp8,             /* X1M1 05 */
        aslZp8,             /* X1M1 06 */
        oraIndirectLong8,   /* X1M1 07 */
        php,                /* X1M1 08 */
        oraImm8,            /* X1M1 09 */
        asla8,              /* X1M1 0a */
        phd,                /* X1M1 0b */
        tsbAbs8,            /* X1M1 0c */
        oraAbs8,            /* X1M1 0d */
        aslAbs8,            /* X1M1 0e */
        oraLong8,           /* X1M1 0f */
        bpl,                /* X1M1 10 */
        oraIndirecty8,      /* X1M1 11 */
        oraIndirect8,       /* X1M1 12 */
        orasIndirecty8,     /* X1M1 13 */
        trbZp8,             /* X1M1 14 */
        oraZpx8,            /* X1M1 15 */
        aslZpx8,            /* X1M1 16 */
        oraIndirectLongy8,  /* X1M1 17 */
        clc,                /* X1M1 18 */
        oraAbsy8,           /* X1M1 19 */
        inca8,              /* X1M1 1a */
        tcs,                /* X1M1 1b */
        trbAbs8,            /* X1M1 1c */
        oraAbsx8,           /* X1M1 1d */
        aslAbsx8,           /* X1M1 1e */
        oraLongx8,          /* X1M1 1f */
        jsr,                /* X1M1 20 */
        andIndirectx8,      /* X1M1 21 */
        jsl,                /* X1M1 22 */
        andSp8,             /* X1M1 23 */
        bitZp8,             /* X1M1 24 */
        andZp8,             /* X1M1 25 */
        rolZp8,             /* X1M1 26 */
        andIndirectLong8,   /* X1M1 27 */
        plp,                /* X1M1 28 */
        andImm8,            /* X1M1 29 */
        rola8,              /* X1M1 2a */
        pld,                /* X1M1 2b */
        bitAbs8,            /* X1M1 2c */
        andAbs8,            /* X1M1 2d */
        rolAbs8,            /* X1M1 2e */
        andLong8,           /* X1M1 2f */
        bmi,                /* X1M1 30 */
        andIndirecty8,      /* X1M1 31 */
        andIndirect8,       /* X1M1 32 */
        andsIndirecty8,     /* X1M1 33 */
        bitZpx8,            /* X1M1 34 */
        andZpx8,            /* X1M1 35 */
        rolZpx8,            /* X1M1 36 */
        andIndirectLongy8,  /* X1M1 37 */
        sec,                /* X1M1 38 */
        andAbsy8,           /* X1M1 39 */
        deca8,              /* X1M1 3a */
        tsc,                /* X1M1 3b */
        bitAbsx8,           /* X1M1 3c */
        andAbsx8,           /* X1M1 3d */
        rolAbsx8,           /* X1M1 3e */
        andLongx8,          /* X1M1 3f */
        rti,                /* X1M1 40 */
        eorIndirectx8,      /* X1M1 41 */
        wdm,                /* X1M1 42 */
        eorSp8,             /* X1M1 43 */
        mvp,                /* X1M1 44 */
        eorZp8,             /* X1M1 45 */
        lsrZp8,             /* X1M1 46 */
        eorIndirectLong8,   /* X1M1 47 */
        pha8,               /* X1M1 48 */
        eorImm8,            /* X1M1 49 */
        lsra8,              /* X1M1 4a */
        phk,                /* X1M1 4b */
        jmp,                /* X1M1 4c */
        eorAbs8,            /* X1M1 4d */
        lsrAbs8,            /* X1M1 4e */
        eorLong8,           /* X1M1 4f */
        bvc,                /* X1M1 50 */
        eorIndirecty8,      /* X1M1 51 */
        eorIndirect8,       /* X1M1 52 */
        eorsIndirecty8,     /* X1M1 53 */
        mvn,                /* X1M1 54 */
        eorZpx8,            /* X1M1 55 */
        lsrZpx8,            /* X1M1 56 */
        eorIndirectLongy8,  /* X1M1 57 */
        cli,                /* X1M1 58 */
        eorAbsy8,           /* X1M1 59 */
        phy8,               /* X1M1 5a */
        tcd,                /* X1M1 5b */
        jmplong,            /* X1M1 5c */
        eorAbsx8,           /* X1M1 5d */
        lsrAbsx8,           /* X1M1 5e */
        eorLongx8,          /* X1M1 5f */
        rts,                /* X1M1 60 */
        adcIndirectx8,      /* X1M1 61 */
        per,                /* X1M1 62 */
        adcSp8,             /* X1M1 63 */
        stzZp8,             /* X1M1 64 */
        adcZp8,             /* X1M1 65 */
        rorZp8,             /* X1M1 66 */
        adcIndirectLong8,   /* X1M1 67 */
        pla8,               /* X1M1 68 */
        adcImm8,            /* X1M1 69 */
        rora8,              /* X1M1 6a */
        rtl,                /* X1M1 6b */
        jmpind,             /* X1M1 6c */
        adcAbs8,            /* X1M1 6d */
        rorAbs8,            /* X1M1 6e */
        adcLong8,           /* X1M1 6f */
        bvs,                /* X1M1 70 */
        adcIndirecty8,      /* X1M1 71 */
        adcIndirect8,       /* X1M1 72 */
        adcsIndirecty8,     /* X1M1 73 */
        stzZpx8,            /* X1M1 74 */
        adcZpx8,            /* X1M1 75 */
        rorZpx8,            /* X1M1 76 */
        adcIndirectLongy8,  /* X1M1 77 */
        sei,                /* X1M1 78 */
        adcAbsy8,           /* X1M1 79 */
        ply8,               /* X1M1 7a */
        tdc,                /* X1M1 7b */
        jmpindx,            /* X1M1 7c */
        adcAbsx8,           /* X1M1 7d */
        rorAbsx8,           /* X1M1 7e */
        adcLongx8,          /* X1M1 7f */
        bra,                /* X1M1 80 */
        staIndirectx8,      /* X1M1 81 */
        brl,                /* X1M1 82 */
        staSp8,             /* X1M1 83 */
        styZp8,             /* X1M1 84 */
        staZp8,             /* X1M1 85 */
        stxZp8,             /* X1M1 86 */
        staIndirectLong8,   /* X1M1 87 */
        dey8,               /* X1M1 88 */
        bitImm8,            /* X1M1 89 */
        txa8,               /* X1M1 8a */
        phb,                /* X1M1 8b */
        styAbs8,            /* X1M1 8c */
        staAbs8,            /* X1M1 8d */
        stxAbs8,            /* X1M1 8e */
        staLong8,           /* X1M1 8f */
        bcc,                /* X1M1 90 */
        staIndirecty8,      /* X1M1 91 */
        staIndirect8,       /* X1M1 92 */
        staSIndirecty8,     /* X1M1 93 */
        styZpx8,            /* X1M1 94 */
        staZpx8,            /* X1M1 95 */
        stxZpy8,            /* X1M1 96 */
        staIndirectLongy8,  /* X1M1 97 */
        tya8,               /* X1M1 98 */
        staAbsy8,           /* X1M1 99 */
        txs8,               /* X1M1 9a */
        txy8,               /* X1M1 9b */
        stzAbs8,            /* X1M1 9c */
        staAbsx8,           /* X1M1 9d */
        stzAbsx8,           /* X1M1 9e */
        staLongx8,          /* X1M1 9f */
        ldyImm8,            /* X1M1 a0 */
        ldaIndirectx8,      /* X1M1 a1 */
        ldxImm8,            /* X1M1 a2 */
        ldaSp8,             /* X1M1 a3 */
        ldyZp8,             /* X1M1 a4 */
        ldaZp8,             /* X1M1 a5 */
        ldxZp8,             /* X1M1 a6 */
        ldaIndirectLong8,   /* X1M1 a7 */
        tay8,               /* X1M1 a8 */
        ldaImm8,            /* X1M1 a9 */
        tax8,               /* X1M1 aa */
        plb,                /* X1M1 ab */
        ldyAbs8,            /* X1M1 ac */
        ldaAbs8,            /* X1M1 ad */
        ldxAbs8,            /* X1M1 ae */
        ldaLong8,           /* X1M1 af */
        bcs,                /* X1M1 b0 */
        ldaIndirecty8,      /* X1M1 b1 */
        ldaIndirect8,       /* X1M1 b2 */
        ldaSIndirecty8,     /* X1M1 b3 */
        ldyZpx8,            /* X1M1 b4 */
        ldaZpx8,            /* X1M1 b5 */
        ldxZpy8,            /* X1M1 b6 */
        ldaIndirectLongy8,  /* X1M1 b7 */
        clv,                /* X1M1 b8 */
        ldaAbsy8,           /* X1M1 b9 */
        tsx8,               /* X1M1 ba */
        tyx8,               /* X1M1 bb */
        ldyAbsx8,           /* X1M1 bc */
        ldaAbsx8,           /* X1M1 bd */
        ldxAbsy8,           /* X1M1 be */
        ldaLongx8,          /* X1M1 bf */
        cpyImm8,            /* X1M1 c0 */
        cmpIndirectx8,      /* X1M1 c1 */
        rep65816,           /* X1M1 c2 */
        cmpSp8,             /* X1M1 c3 */
        cpyZp8,             /* X1M1 c4 */
        cmpZp8,             /* X1M1 c5 */
        decZp8,             /* X1M1 c6 */
        cmpIndirectLong8,   /* X1M1 c7 */
        iny8,               /* X1M1 c8 */
        cmpImm8,            /* X1M1 c9 */
        dex8,               /* X1M1 ca */
        wai,                /* X1M1 cb */
        cpyAbs8,            /* X1M1 cc */
        cmpAbs8,            /* X1M1 cd */
        decAbs8,            /* X1M1 ce */
        cmpLong8,           /* X1M1 cf */
        bne,                /* X1M1 d0 */
        cmpIndirecty8,      /* X1M1 d1 */
        cmpIndirect8,       /* X1M1 d2 */
        cmpsIndirecty8,     /* X1M1 d3 */
        pei,                /* X1M1 d4 */
        cmpZpx8,            /* X1M1 d5 */
        decZpx8,            /* X1M1 d6 */
        cmpIndirectLongy8,  /* X1M1 d7 */
        cld,                /* X1M1 d8 */
        cmpAbsy8,           /* X1M1 d9 */
        phx8,               /* X1M1 da */
        stp,                /* X1M1 db */
        jmlind,             /* X1M1 dc */
        cmpAbsx8,           /* X1M1 dd */
        decAbsx8,           /* X1M1 de */
        cmpLongx8,          /* X1M1 df */
        cpxImm8,            /* X1M1 e0 */
        sbcIndirectx8,      /* X1M1 e1 */
        sep,                /* X1M1 e2 */
        sbcSp8,             /* X1M1 e3 */
        cpxZp8,             /* X1M1 e4 */
        sbcZp8,             /* X1M1 e5 */
        incZp8,             /* X1M1 e6 */
        sbcIndirectLong8,   /* X1M1 e7 */
        inx8,               /* X1M1 e8 */
        sbcImm8,            /* X1M1 e9 */
        nop,                /* X1M1 ea */
        xba,                /* X1M1 eb */
        cpxAbs8,            /* X1M1 ec */
        sbcAbs8,            /* X1M1 ed */
        incAbs8,            /* X1M1 ee */
        sbcLong8,           /* X1M1 ef */
        beq,                /* X1M1 f0 */
        sbcIndirecty8,      /* X1M1 f1 */
        sbcIndirect8,       /* X1M1 f2 */
        sbcsIndirecty8,     /* X1M1 f3 */
        pea,                /* X1M1 f4 */
        sbcZpx8,            /* X1M1 f5 */
        incZpx8,            /* X1M1 f6 */
        sbcIndirectLongy8,  /* X1M1 f7 */
        sed,                /* X1M1 f8 */
        sbcAbsy8,           /* X1M1 f9 */
        plx8,               /* X1M1 fa */
        xce,                /* X1M1 fb */
        jsrIndx,            /* X1M1 fc */
        sbcAbsx8,           /* X1M1 fd */
        incAbsx8,           /* X1M1 fe */
        sbcLongx8,          /* X1M1 ff */
    },
    {
        op_brk,             /* X1M0 00 */
        oraIndirectx16,     /* X1M0 01 */
        cop,                /* X1M0 02 */
        oraSp16,            /* X1M0 03 */
        tsbZp16,            /* X1M0 04 */
        oraZp16,            /* X1M0 05 */
        aslZp16,            /* X1M0 06 */
        oraIndirectLong16,  /* X1M0 07 */
        php,                /* X1M0 08 */
        oraImm16,           /* X1M0 09 */
        asla16,             /* X1M0 0a */
        phd,                /* X1M0 0b */
        tsbAbs16,           /* X1M0 0c */
        oraAbs16,           /* X1M0 0d */
        aslAbs16,           /* X1M0 0e */
        oraLong16,          /* X1M0 0f */
        bpl,                /* X1M0 10 */
        oraIndirecty16,     /* X1M0 11 */
        oraIndirect16,      /* X1M0 12 */
        orasIndirecty16,    /* X1M0 13 */
        trbZp16,            /* X1M0 14 */
        oraZpx16,           /* X1M0 15 */
        aslZpx16,           /* X1M0 16 */
        oraIndirectLongy16, /* X1M0 17 */
        clc,                /* X1M0 18 */
        oraAbsy16,          /* X1M0 19 */
        inca16,             /* X1M0 1a */
        tcs,                /* X1M0 1b */
        trbAbs16,           /* X1M0 1c */
        oraAbsx16,          /* X1M0 1d */
        aslAbsx16,          /* X1M0 1e */
        oraLongx16,         /* X1M0 1f */
        jsr,                /* X1M0 20 */
        andIndirectx16,     /* X1M0 21 */
        jsl,                /* X1M0 22 */
        andSp16,            /* X1M0 23 */
        bitZp16,            /* X1M0 24 */
        andZp16,            /* X1M0 25 */
        rolZp16,            /* X1M0 26 */
        andIndirectLong16,  /* X1M0 27 */
        plp,                /* X1M0 28 */
        andImm16,           /* X1M0 29 */
        rola16,             /* X1M0 2a */
        pld,                /* X1M0 2b */
        bitAbs16,           /* X1M0 2c */
        andAbs16,           /* X1M0 2d */
        rolAbs16,           /* X1M0 2e */
        andLong16,          /* X1M0 2f */
        bmi,                /* X1M0 30 */
        andIndirecty16,     /* X1M0 31 */
        andIndirect16,      /* X1M0 32 */
        andsIndirecty16,    /* X1M0 33 */
        bitZpx16,           /* X1M0 34 */
        andZpx16,           /* X1M0 35 */
        rolZpx16,           /* X1M0 36 */
        andIndirectLongy16, /* X1M0 37 */
        sec,                /* X1M0 38 */
        andAbsy16,          /* X1M0 39 */
        deca16,             /* X1M0 3a */
        tsc,                /* X1M0 3b */
        bitAbsx16,          /* X1M0 3c */
        andAbsx16,          /* X1M0 3d */
        rolAbsx16,          /* X1M0 3e */
        andLongx16,         /* X1M0 3f */
        rti,                /* X1M0 40 */
        eorIndirectx16,     /* X1M0 41 */
        wdm,                /* X1M0 42 */
        eorSp16,            /* X1M0 43 */
        mvp,                /* X1M0 44 */
        eorZp16,            /* X1M0 45 */
        lsrZp16,            /* X1M0 46 */
        eorIndirectLong16,  /* X1M0 47 */
        pha16,              /* X1M0 48 */
        eorImm16,           /* X1M0 49 */
        lsra16,             /* X1M0 4a */
        phk,                /* X1M0 4b */
        jmp,                /* X1M0 4c */
        eorAbs16,           /* X1M0 4d */
        lsrAbs16,           /* X1M0 4e */
        eorLong16,          /* X1M0 4f */
        bvc,                /* X1M0 50 */
        eorIndirecty16,     /* X1M0 51 */
        eorIndirect16,      /* X1M0 52 */
        eorsIndirecty16,    /* X1M0 53 */
        mvn,                /* X1M0 54 */
        eorZpx16,           /* X1M0 55 */
        lsrZpx16,           /* X1M0 56 */
        eorIndirectLongy16, /* X1M0 57 */
        cli,                /* X1M0 58 */
        eorAbsy16,          /* X1M0 59 */
        phy8,               /* X1M0 5a */
        tcd,                /* X1M0 5b */
        jmplong,            /* X1M0 5c */
        eorAbsx16,          /* X1M0 5d */
        lsrAbsx16,          /* X1M0 5e */
        eorLongx16,         /* X1M0 5f */
        rts,                /* X1M0 60 */
        adcIndirectx16,     /* X1M0 61 */
        per,                /* X1M0 62 */
        adcSp16,            /* X1M0 63 */
        stzZp16,            /* X1M0 64 */
        adcZp16,            /* X1M0 65 */
        rorZp16,            /* X1M0 66 */
        adcIndirectLong16,  /* X1M0 67 */
        pla16,              /* X1M0 68 */
        adcImm16,           /* X1M0 69 */
        rora16,             /* X1M0 6a */
        rtl,                /* X1M0 6b */
        jmpind,             /* X1M0 6c */
        adcAbs16,           /* X1M0 6d */
        rorAbs16,           /* X1M0 6e */
        adcLong16,          /* X1M0 6f */
        bvs,                /* X1M0 70 */
        adcIndirecty16,     /* X1M0 71 */
        adcIndirect16,      /* X1M0 72 */
        adcsIndirecty16,    /* X1M0 73 */
        stzZpx16,           /* X1M0 74 */
        adcZpx16,           /* X1M0 75 */
        rorZpx16,           /* X1M0 76 */
        adcIndirectLongy16, /* X1M0 77 */
        sei,                /* X1M0 78 */
        adcAbsy16,          /* X1M0 79 */
        ply8,               /* X1M0 7a */
        tdc,                /* X1M0 7b */
        jmpindx,            /* X1M0 7c */
        adcAbsx16,          /* X1M0 7d */
        rorAbsx16,          /* X1M0 7e */
        adcLongx16,         /* X1M0 7f */
        bra,                /* X1M0 80 */
        staIndirectx16,     /* X1M0 81 */
        brl,                /* X1M0 82 */
        staSp16,            /* X1M0 83 */
        styZp8,             /* X1M0 84 */
        staZp16,            /* X1M0 85 */
        stxZp8,             /* X1M0 86 */
        staIndirectLong16,  /* X1M0 87 */
        dey8,               /* X1M0 88 */
        bitImm16,           /* X1M0 89 */
        txa16,              /* X1M0 8a */
        phb,                /* X1M0 8b */
        styAbs8,            /* X1M0 8c */
        staAbs16,           /* X1M0 8d */
        stxAbs8,            /* X1M0 8e */
        staLong16,          /* X1M0 8f */
        bcc,                /* X1M0 90 */
        staIndirecty16,     /* X1M0 91 */
        staIndirect16,      /* X1M0 92 */
        staSIndirecty16,    /* X1M0 93 */
        styZpx8,            /* X1M0 94 */
        staZpx16,           /* X1M0 95 */
        stxZpy8,            /* X1M0 96 */
        staIndirectLongy16, /* X1M0 97 */
        tya16,              /* X1M0 98 */
        staAbsy16,          /* X1M0 99 */
        txs8,               /* X1M0 9a */
        txy8,               /* X1M0 9b */
        stzAbs16,           /* X1M0 9c */
        staAbsx16,          /* X1M0 9d */
        stzAbsx16,          /* X1M0 9e */
        staLongx16,         /* X1M0 9f */
        ldyImm8,            /* X1M0 a0 */
        ldaIndirectx16,     /* X1M0 a1 */
        ldxImm8,            /* X1M0 a2 */
        ldaSp16,            /* X1M0 a3 */
        ldyZp8,             /* X1M0 a4 */
        ldaZp16,            /* X1M0 a5 */
        ldxZp8,             /* X1M0 a6 */
        ldaIndirectLong16,  /* X1M0 a7 */
        tay8,               /* X1M0 a8 */
        ldaImm16,           /* X1M0 a9 */
        tax8,               /* X1M0 aa */
        plb,                /* X1M0 ab */
        ldyAbs8,            /* X1M0 ac */
        ldaAbs16,           /* X1M0 ad */
        ldxAbs8,            /* X1M0 ae */
        ldaLong16,          /* X1M0 af */
        bcs,                /* X1M0 b0 */
        ldaIndirecty16,     /* X1M0 b1 */
        ldaIndirect16,      /* X1M0 b2 */
        ldaSIndirecty16,    /* X1M0 b3 */
        ldyZpx8,            /* X1M0 b4 */
        ldaZpx16,           /* X1M0 b5 */
        ldxZpy8,            /* X1M0 b6 */
        ldaIndirectLongy16, /* X1M0 b7 */
        clv,                /* X1M0 b8 */
        ldaAbsy16,          /* X1M0 b9 */
        tsx8,               /* X1M0 ba */
        tyx8,               /* X1M0 bb */
        ldyAbsx8,           /* X1M0 bc */
        ldaAbsx16,          /* X1M0 bd */
        ldxAbsy8,           /* X1M0 be */
        ldaLongx16,         /* X1M0 bf */
        cpyImm8,            /* X1M0 c0 */
        cmpIndirectx16,     /* X1M0 c1 */
        rep65816,           /* X1M0 c2 */
        cmpSp16,            /* X1M0 c3 */
        cpyZp8,             /* X1M0 c4 */
        cmpZp16,            /* X1M0 c5 */
        decZp16,            /* X1M0 c6 */
        cmpIndirectLong16,  /* X1M0 c7 */
        iny8,               /* X1M0 c8 */
        cmpImm16,           /* X1M0 c9 */
        dex8,               /* X1M0 ca */
        wai,                /* X1M0 cb */
        cpyAbs8,            /* X1M0 cc */
        cmpAbs16,           /* X1M0 cd */
        decAbs16,           /* X1M0 ce */
        cmpLong16,          /* X1M0 cf */
        bne,                /* X1M0 d0 */
        cmpIndirecty16,     /* X1M0 d1 */
        cmpIndirect16,      /* X1M0 d2 */
        cmpsIndirecty16,    /* X1M0 d3 */
        pei,                /* X1M0 d4 */
        cmpZpx16,           /* X1M0 d5 */
        decZpx16,           /* X1M0 d6 */
        cmpIndirectLongy16, /* X1M0 d7 */
        cld,                /* X1M0 d8 */
        cmpAbsy16,          /* X1M0 d9 */
        phx8,               /* X1M0 da */
        stp,                /* X1M0 db */
        jmlind,             /* X1M0 dc */
        cmpAbsx16,          /* X1M0 dd */
        decAbsx16,          /* X1M0 de */
        cmpLongx16,         /* X1M0 df */
        cpxImm8,            /* X1M0 e0 */
        sbcIndirectx16,     /* X1M0 e1 */
        sep,                /* X1M0 e2 */
        sbcSp16,            /* X1M0 e3 */
        cpxZp8,             /* X1M0 e4 */
        sbcZp16,            /* X1M0 e5 */
        incZp16,            /* X1M0 e6 */
        sbcIndirectLong16,  /* X1M0 e7 */
        inx8,               /* X1M0 e8 */
        sbcImm16,           /* X1M0 e9 */
        nop,                /* X1M0 ea */
        xba,                /* X1M0 eb */
        cpxAbs8,            /* X1M0 ec */
        sbcAbs16,           /* X1M0 ed */
        incAbs16,           /* X1M0 ee */
        sbcLong16,          /* X1M0 ef */
        beq,                /* X1M0 f0 */
        sbcIndirecty16,     /* X1M0 f1 */
        sbcIndirect16,      /* X1M0 f2 */
        sbcsIndirecty16,    /* X1M0 f3 */
        pea,                /* X1M0 f4 */
        sbcZpx16,           /* X1M0 f5 */
        incZpx16,           /* X1M0 f6 */
        sbcIndirectLongy16, /* X1M0 f7 */
        sed,                /* X1M0 f8 */
        sbcAbsy16,          /* X1M0 f9 */
        plx8,               /* X1M0 fa */
        xce,                /* X1M0 fb */
        jsrIndx,            /* X1M0 fc */
        sbcAbsx16,          /* X1M0 fd */
        incAbsx16,          /* X1M0 fe */
        sbcLongx16,         /* X1M0 ff */
    },
    {
        op_brk,             /* X0M1 00 */
        oraIndirectx8,      /* X0M1 01 */
        cop,                /* X0M1 02 */
        oraSp8,             /* X0M1 03 */
        tsbZp8,             /* X0M1 04 */
        oraZp8,             /* X0M1 05 */
        aslZp8,             /* X0M1 06 */
        oraIndirectLong8,   /* X0M1 07 */
        php,                /* X0M1 08 */
        oraImm8,            /* X0M1 09 */
        asla8,              /* X0M1 0a */
        phd,                /* X0M1 0b */
        tsbAbs8,            /* X0M1 0c */
        oraAbs8,            /* X0M1 0d */
        aslAbs8,            /* X0M1 0e */
        oraLong8,           /* X0M1 0f */
        bpl,                /* X0M1 10 */
        oraIndirecty8,      /* X0M1 11 */
        oraIndirect8,       /* X0M1 12 */
        orasIndirecty8,     /* X0M1 13 */
        trbZp8,             /* X0M1 14 */
        oraZpx8,            /* X0M1 15 */
        aslZpx8,            /* X0M1 16 */
        oraIndirectLongy8,  /* X0M1 17 */
        clc,                /* X0M1 18 */
        oraAbsy8,           /* X0M1 19 */
        inca8,              /* X0M1 1a */
        tcs,                /* X0M1 1b */
        trbAbs8,            /* X0M1 1c */
        oraAbsx8,           /* X0M1 1d */
        aslAbsx8,           /* X0M1 1e */
        oraLongx8,          /* X0M1 1f */
        jsr,                /* X0M1 20 */
        andIndirectx8,      /* X0M1 21 */
        jsl,                /* X0M1 22 */
        andSp8,             /* X0M1 23 */
        bitZp8,             /* X0M1 24 */
        andZp8,             /* X0M1 25 */
        rolZp8,             /* X0M1 26 */
        andIndirectLong8,   /* X0M1 27 */
        plp,                /* X0M1 28 */
        andImm8,            /* X0M1 29 */
        rola8,              /* X0M1 2a */
        pld,                /* X0M1 2b */
        bitAbs8,            /* X0M1 2c */
        andAbs8,            /* X0M1 2d */
        rolAbs8,            /* X0M1 2e */
        andLong8,           /* X0M1 2f */
        bmi,                /* X0M1 30 */
        andIndirecty8,      /* X0M1 31 */
        andIndirect8,       /* X0M1 32 */
        andsIndirecty8,     /* X0M1 33 */
        bitZpx8,            /* X0M1 34 */
        andZpx8,            /* X0M1 35 */
        rolZpx8,            /* X0M1 36 */
        andIndirectLongy8,  /* X0M1 37 */
        sec,                /* X0M1 38 */
        andAbsy8,           /* X0M1 39 */
        deca8,              /* X0M1 3a */
        tsc,                /* X0M1 3b */
        bitAbsx8,           /* X0M1 3c */
        andAbsx8,           /* X0M1 3d */
        rolAbsx8,           /* X0M1 3e */
        andLongx8,          /* X0M1 3f */
        rti,                /* X0M1 40 */
        eorIndirectx8,      /* X0M1 41 */
        wdm,                /* X0M1 42 */
        eorSp8,             /* X0M1 43 */
        mvp,                /* X0M1 44 */
        eorZp8,             /* X0M1 45 */
        lsrZp8,             /* X0M1 46 */
        eorIndirectLong8,   /* X0M1 47 */
        pha8,               /* X0M1 48 */
        eorImm8,            /* X0M1 49 */
        lsra8,              /* X0M1 4a */
        phk,                /* X0M1 4b */
        jmp,                /* X0M1 4c */
        eorAbs8,            /* X0M1 4d */
        lsrAbs8,            /* X0M1 4e */
        eorLong8,           /* X0M1 4f */
        bvc,                /* X0M1 50 */
        eorIndirecty8,      /* X0M1 51 */
        eorIndirect8,       /* X0M1 52 */
        eorsIndirecty8,     /* X0M1 53 */
        mvn,                /* X0M1 54 */
        eorZpx8,            /* X0M1 55 */
        lsrZpx8,            /* X0M1 56 */
        eorIndirectLongy8,  /* X0M1 57 */
        cli,                /* X0M1 58 */
        eorAbsy8,           /* X0M1 59 */
        phy16,              /* X0M1 5a */
        tcd,                /* X0M1 5b */
        jmplong,            /* X0M1 5c */
        eorAbsx8,           /* X0M1 5d */
        lsrAbsx8,           /* X0M1 5e */
        eorLongx8,          /* X0M1 5f */
        rts,                /* X0M1 60 */
        adcIndirectx8,      /* X0M1 61 */
        per,                /* X0M1 62 */
        adcSp8,             /* X0M1 63 */
        stzZp8,             /* X0M1 64 */
        adcZp8,             /* X0M1 65 */
        rorZp8,             /* X0M1 66 */
        adcIndirectLong8,   /* X0M1 67 */
        pla8,               /* X0M1 68 */
        adcImm8,            /* X0M1 69 */
        rora8,              /* X0M1 6a */
        rtl,                /* X0M1 6b */
        jmpind,             /* X0M1 6c */
        adcAbs8,            /* X0M1 6d */
        rorAbs8,            /* X0M1 6e */
        adcLong8,           /* X0M1 6f */
        bvs,                /* X0M1 70 */
        adcIndirecty8,      /* X0M1 71 */
        adcIndirect8,       /* X0M1 72 */
        adcsIndirecty8,     /* X0M1 73 */
        stzZpx8,            /* X0M1 74 */
        adcZpx8,            /* X0M1 75 */
        rorZpx8,            /* X0M1 76 */
        adcIndirectLongy8,  /* X0M1 77 */
        sei,                /* X0M1 78 */
        adcAbsy8,           /* X0M1 79 */
        ply16,              /* X0M1 7a */
        tdc,                /* X0M1 7b */
        jmpindx,            /* X0M1 7c */
        adcAbsx8,           /* X0M1 7d */
        rorAbsx8,           /* X0M1 7e */
        adcLongx8,          /* X0M1 7f */
        bra,                /* X0M1 80 */
        staIndirectx8,      /* X0M1 81 */
        brl,                /* X0M1 82 */
        staSp8,             /* X0M1 83 */
        styZp16,            /* X0M1 84 */
        staZp8,             /* X0M1 85 */
        stxZp16,            /* X0M1 86 */
        staIndirectLong8,   /* X0M1 87 */
        dey16,              /* X0M1 88 */
        bitImm8,            /* X0M1 89 */
        txa8,               /* X0M1 8a */
        phb,                /* X0M1 8b */
        styAbs16,           /* X0M1 8c */
        staAbs8,            /* X0M1 8d */
        stxAbs16,           /* X0M1 8e */
        staLong8,           /* X0M1 8f */
        bcc,                /* X0M1 90 */
        staIndirecty8,      /* X0M1 91 */
        staIndirect8,       /* X0M1 92 */
        staSIndirecty8,     /* X0M1 93 */
        styZpx16,           /* X0M1 94 */
        staZpx8,            /* X0M1 95 */
        stxZpy16,           /* X0M1 96 */
        staIndirectLongy8,  /* X0M1 97 */
        tya8,               /* X0M1 98 */
        staAbsy8,           /* X0M1 99 */
        txs16,              /* X0M1 9a */
        txy16,              /* X0M1 9b */
        stzAbs8,            /* X0M1 9c */
        staAbsx8,           /* X0M1 9d */
        stzAbsx8,           /* X0M1 9e */
        staLongx8,          /* X0M1 9f */
        ldyImm16,           /* X0M1 a0 */
        ldaIndirectx8,      /* X0M1 a1 */
        ldxImm16,           /* X0M1 a2 */
        ldaSp8,             /* X0M1 a3 */
        ldyZp16,            /* X0M1 a4 */
        ldaZp8,             /* X0M1 a5 */
        ldxZp16,            /* X0M1 a6 */
        ldaIndirectLong8,   /* X0M1 a7 */
        tay16,              /* X0M1 a8 */
        ldaImm8,            /* X0M1 a9 */
        tax16,              /* X0M1 aa */
        plb,                /* X0M1 ab */
        ldyAbs16,           /* X0M1 ac */
        ldaAbs8,            /* X0M1 ad */
        ldxAbs16,           /* X0M1 ae */
        ldaLong8,           /* X0M1 af */
        bcs,                /* X0M1 b0 */
        ldaIndirecty8,      /* X0M1 b1 */
        ldaIndirect8,       /* X0M1 b2 */
        ldaSIndirecty8,     /* X0M1 b3 */
        ldyZpx16,           /* X0M1 b4 */
        ldaZpx8,            /* X0M1 b5 */
        ldxZpy16,           /* X0M1 b6 */
        ldaIndirectLongy8,  /* X0M1 b7 */
        clv,                /* X0M1 b8 */
        ldaAbsy8,           /* X0M1 b9 */
        tsx16,              /* X0M1 ba */
        tyx16,              /* X0M1 bb */
        ldyAbsx16,          /* X0M1 bc */
        ldaAbsx8,           /* X0M1 bd */
        ldxAbsy16,          /* X0M1 be */
        ldaLongx8,          /* X0M1 bf */
        cpyImm16,           /* X0M1 c0 */
        cmpIndirectx8,      /* X0M1 c1 */
        rep65816,           /* X0M1 c2 */
        cmpSp8,             /* X0M1 c3 */
        cpyZp16,            /* X0M1 c4 */
        cmpZp8,             /* X0M1 c5 */
        decZp8,             /* X0M1 c6 */
        cmpIndirectLong8,   /* X0M1 c7 */
        iny16,              /* X0M1 c8 */
        cmpImm8,            /* X0M1 c9 */
        dex16,              /* X0M1 ca */
        wai,                /* X0M1 cb */
        cpyAbs16,           /* X0M1 cc */
        cmpAbs8,            /* X0M1 cd */
        decAbs8,            /* X0M1 ce */
        cmpLong8,           /* X0M1 cf */
        bne,                /* X0M1 d0 */
        cmpIndirecty8,      /* X0M1 d1 */
        cmpIndirect8,       /* X0M1 d2 */
        cmpsIndirecty8,     /* X0M1 d3 */
        pei,                /* X0M1 d4 */
        cmpZpx8,            /* X0M1 d5 */
        decZpx8,            /* X0M1 d6 */
        cmpIndirectLongy8,  /* X0M1 d7 */
        cld,                /* X0M1 d8 */
        cmpAbsy8,           /* X0M1 d9 */
        phx16,              /* X0M1 da */
        stp,                /* X0M1 db */
        jmlind,             /* X0M1 dc */
        cmpAbsx8,           /* X0M1 dd */
        decAbsx8,           /* X0M1 de */
        cmpLongx8,          /* X0M1 df */
        cpxImm16,           /* X0M1 e0 */
        sbcIndirectx8,      /* X0M1 e1 */
        sep,                /* X0M1 e2 */
        sbcSp8,             /* X0M1 e3 */
        cpxZp16,            /* X0M1 e4 */
        sbcZp8,             /* X0M1 e5 */
        incZp8,             /* X0M1 e6 */
        sbcIndirectLong8,   /* X0M1 e7 */
        inx16,              /* X0M1 e8 */
        sbcImm8,            /* X0M1 e9 */
        nop,                /* X0M1 ea */
        xba,                /* X0M1 eb */
        cpxAbs16,           /* X0M1 ec */
        sbcAbs8,            /* X0M1 ed */
        incAbs8,            /* X0M1 ee */
        sbcLong8,           /* X0M1 ef */
        beq,                /* X0M1 f0 */
        sbcIndirecty8,      /* X0M1 f1 */
        sbcIndirect8,       /* X0M1 f2 */
        sbcsIndirecty8,     /* X0M1 f3 */
        pea,                /* X0M1 f4 */
        sbcZpx8,            /* X0M1 f5 */
        incZpx8,            /* X0M1 f6 */
        sbcIndirectLongy8,  /* X0M1 f7 */
        sed,                /* X0M1 f8 */
        sbcAbsy8,           /* X0M1 f9 */
        plx16,              /* X0M1 fa */
        xce,                /* X0M1 fb */
        jsrIndx,            /* X0M1 fc */
        sbcAbsx8,           /* X0M1 fd */
        incAbsx8,           /* X0M1 fe */
        sbcLongx8,          /* X0M1 ff */
    },
    {
        op_brk,             /* X0M0 00 */
        oraIndirectx16,     /* X0M0 01 */
        cop,                /* X0M0 02 */
        oraSp16,            /* X0M0 03 */
        tsbZp16,            /* X0M0 04 */
        oraZp16,            /* X0M0 05 */
        aslZp16,            /* X0M0 06 */
        oraIndirectLong16,  /* X0M0 07 */
        php,                /* X0M0 08 */
        oraImm16,           /* X0M0 09 */
        asla16,             /* X0M0 0a */
        phd,                /* X0M0 0b */
        tsbAbs16,           /* X0M0 0c */
        oraAbs16,           /* X0M0 0d */
        aslAbs16,           /* X0M0 0e */
        oraLong16,          /* X0M0 0f */
        bpl,                /* X0M0 10 */
        oraIndirecty16,     /* X0M0 11 */
        oraIndirect16,      /* X0M0 12 */
        orasIndirecty16,    /* X0M0 13 */
        trbZp16,            /* X0M0 14 */
        oraZpx16,           /* X0M0 15 */
        aslZpx16,           /* X0M0 16 */
        oraIndirectLongy16, /* X0M0 17 */
        clc,                /* X0M0 18 */
        oraAbsy16,          /* X0M0 19 */
        inca16,             /* X0M0 1a */
        tcs,                /* X0M0 1b */
        trbAbs16,           /* X0M0 1c */
        oraAbsx16,          /* X0M0 1d */
        aslAbsx16,          /* X0M0 1e */
        oraLongx16,         /* X0M0 1f */
        jsr,                /* X0M0 20 */
        andIndirectx16,     /* X0M0 21 */
        jsl,                /* X0M0 22 */
        andSp16,            /* X0M0 23 */
        bitZp16,            /* X0M0 24 */
        andZp16,            /* X0M0 25 */
        rolZp16,            /* X0M0 26 */
        andIndirectLong16,  /* X0M0 27 */
        plp,                /* X0M0 28 */
        andImm16,           /* X0M0 29 */
        rola16,             /* X0M0 2a */
        pld,                /* X0M0 2b */
        bitAbs16,           /* X0M0 2c */
        andAbs16,           /* X0M0 2d */
        rolAbs16,           /* X0M0 2e */
        andLong16,          /* X0M0 2f */
        bmi,                /* X0M0 30 */
        andIndirecty16,     /* X0M0 31 */
        andIndirect16,      /* X0M0 32 */
        andsIndirecty16,    /* X0M0 33 */
        bitZpx16,           /* X0M0 34 */
        andZpx16,           /* X0M0 35 */
        rolZpx16,           /* X0M0 36 */
        andIndirectLongy16, /* X0M0 37 */
        sec,                /* X0M0 38 */
        andAbsy16,          /* X0M0 39 */
        deca16,             /* X0M0 3a */
        tsc,                /* X0M0 3b */
        bitAbsx16,          /* X0M0 3c */
        andAbsx16,          /* X0M0 3d */
        rolAbsx16,          /* X0M0 3e */
        andLongx16,         /* X0M0 3f */
        rti,                /* X0M0 40 */
        eorIndirectx16,     /* X0M0 41 */
        wdm,                /* X0M0 42 */
        eorSp16,            /* X0M0 43 */
        mvp,                /* X0M0 44 */
        eorZp16,            /* X0M0 45 */
        lsrZp16,            /* X0M0 46 */
        eorIndirectLong16,  /* X0M0 47 */
        pha16,              /* X0M0 48 */
        eorImm16,           /* X0M0 49 */
        lsra16,             /* X0M0 4a */
        phk,                /* X0M0 4b */
        jmp,                /* X0M0 4c */
        eorAbs16,           /* X0M0 4d */
        lsrAbs16,           /* X0M0 4e */
        eorLong16,          /* X0M0 4f */
        bvc,                /* X0M0 50 */
        eorIndirecty16,     /* X0M0 51 */
        eorIndirect16,      /* X0M0 52 */
        eorsIndirecty16,    /* X0M0 53 */
        mvn,                /* X0M0 54 */
        eorZpx16,           /* X0M0 55 */
        lsrZpx16,           /* X0M0 56 */
        eorIndirectLongy16, /* X0M0 57 */
        cli,                /* X0M0 58 */
        eorAbsy16,          /* X0M0 59 */
        phy16,              /* X0M0 5a */
        tcd,                /* X0M0 5b */
        jmplong,            /* X0M0 5c */
        eorAbsx16,          /* X0M0 5d */
        lsrAbsx16,          /* X0M0 5e */
        eorLongx16,         /* X0M0 5f */
        rts,                /* X0M0 60 */
        adcIndirectx16,     /* X0M0 61 */
        per,                /* X0M0 62 */
        adcSp16,            /* X0M0 63 */
        stzZp16,            /* X0M0 64 */
        adcZp16,            /* X0M0 65 */
        rorZp16,            /* X0M0 66 */
        adcIndirectLong16,  /* X0M0 67 */
        pla16,              /* X0M0 68 */
        adcImm16,           /* X0M0 69 */
        rora16,             /* X0M0 6a */
        rtl,                /* X0M0 6b */
        jmpind,             /* X0M0 6c */
        adcAbs16,           /* X0M0 6d */
        rorAbs16,           /* X0M0 6e */
        adcLong16,          /* X0M0 6f */
        bvs,                /* X0M0 70 */
        adcIndirecty16,     /* X0M0 71 */
        adcIndirect16,      /* X0M0 72 */
        adcsIndirecty16,    /* X0M0 73 */
        stzZpx16,           /* X0M0 74 */
        adcZpx16,           /* X0M0 75 */
        rorZpx16,           /* X0M0 76 */
        adcIndirectLongy16, /* X0M0 77 */
        sei,                /* X0M0 78 */
        adcAbsy16,          /* X0M0 79 */
        ply16,              /* X0M0 7a */
        tdc,                /* X0M0 7b */
        jmpindx,            /* X0M0 7c */
        adcAbsx16,          /* X0M0 7d */
        rorAbsx16,          /* X0M0 7e */
        adcLongx16,         /* X0M0 7f */
        bra,                /* X0M0 80 */
        staIndirectx16,     /* X0M0 81 */
        brl,                /* X0M0 82 */
        staSp16,            /* X0M0 83 */
        styZp16,            /* X0M0 84 */
        staZp16,            /* X0M0 85 */
        stxZp16,            /* X0M0 86 */
        staIndirectLong16,  /* X0M0 87 */
        dey16,              /* X0M0 88 */
        bitImm16,           /* X0M0 89 */
        txa16,              /* X0M0 8a */
        phb,                /* X0M0 8b */
        styAbs16,           /* X0M0 8c */
        staAbs16,           /* X0M0 8d */
        stxAbs16,           /* X0M0 8e */
        staLong16,          /* X0M0 8f */
        bcc,                /* X0M0 90 */
        staIndirecty16,     /* X0M0 91 */
        staIndirect16,      /* X0M0 92 */
        staSIndirecty16,    /* X0M0 93 */
        styZpx16,           /* X0M0 94 */
        staZpx16,           /* X0M0 95 */
        stxZpy16,           /* X0M0 96 */
        staIndirectLongy16, /* X0M0 97 */
        tya16,              /* X0M0 98 */
        staAbsy16,          /* X0M0 99 */
        txs16,              /* X0M0 9a */
        txy16,              /* X0M0 9b */
        stzAbs16,           /* X0M0 9c */
        staAbsx16,          /* X0M0 9d */
        stzAbsx16,          /* X0M0 9e */
        staLongx16,         /* X0M0 9f */
        ldyImm16,           /* X0M0 a0 */
        ldaIndirectx16,     /* X0M0 a1 */
        ldxImm16,           /* X0M0 a2 */
        ldaSp16,            /* X0M0 a3 */
        ldyZp16,            /* X0M0 a4 */
        ldaZp16,            /* X0M0 a5 */
        ldxZp16,            /* X0M0 a6 */
        ldaIndirectLong16,  /* X0M0 a7 */
        tay16,              /* X0M0 a8 */
        ldaImm16,           /* X0M0 a9 */
        tax16,              /* X0M0 aa */
        plb,                /* X0M0 ab */
        ldyAbs16,           /* X0M0 ac */
        ldaAbs16,           /* X0M0 ad */
        ldxAbs16,           /* X0M0 ae */
        ldaLong16,          /* X0M0 af */
        bcs,                /* X0M0 b0 */
        ldaIndirecty16,     /* X0M0 b1 */
        ldaIndirect16,      /* X0M0 b2 */
        ldaSIndirecty16,    /* X0M0 b3 */
        ldyZpx16,           /* X0M0 b4 */
        ldaZpx16,           /* X0M0 b5 */
        ldxZpy16,           /* X0M0 b6 */
        ldaIndirectLongy16, /* X0M0 b7 */
        clv,                /* X0M0 b8 */
        ldaAbsy16,          /* X0M0 b9 */
        tsx16,              /* X0M0 ba */
        tyx16,              /* X0M0 bb */
        ldyAbsx16,          /* X0M0 bc */
        ldaAbsx16,          /* X0M0 bd */
        ldxAbsy16,          /* X0M0 be */
        ldaLongx16,         /* X0M0 bf */
        cpyImm16,           /* X0M0 c0 */
        cmpIndirectx16,     /* X0M0 c1 */
        rep65816,           /* X0M0 c2 */
        cmpSp16,            /* X0M0 c3 */
        cpyZp16,            /* X0M0 c4 */
        cmpZp16,            /* X0M0 c5 */
        decZp16,            /* X0M0 c6 */
        cmpIndirectLong16,  /* X0M0 c7 */
        iny16,              /* X0M0 c8 */
        cmpImm16,           /* X0M0 c9 */
        dex16,              /* X0M0 ca */
        wai,                /* X0M0 cb */
        cpyAbs16,           /* X0M0 cc */
        cmpAbs16,           /* X0M0 cd */
        decAbs16,           /* X0M0 ce */
        cmpLong16,          /* X0M0 cf */
        bne,                /* X0M0 d0 */
        cmpIndirecty16,     /* X0M0 d1 */
        cmpIndirect16,      /* X0M0 d2 */
        cmpsIndirecty16,    /* X0M0 d3 */
        pei,                /* X0M0 d4 */
        cmpZpx16,           /* X0M0 d5 */
        decZpx16,           /* X0M0 d6 */
        cmpIndirectLongy16, /* X0M0 d7 */
        cld,                /* X0M0 d8 */
        cmpAbsy16,          /* X0M0 d9 */
        phx16,              /* X0M0 da */
        stp,                /* X0M0 db */
        jmlind,             /* X0M0 dc */
        cmpAbsx16,          /* X0M0 dd */
        decAbsx16,          /* X0M0 de */
        cmpLongx16,         /* X0M0 df */
        cpxImm16,           /* X0M0 e0 */
        sbcIndirectx16,     /* X0M0 e1 */
        sep,                /* X0M0 e2 */
        sbcSp16,            /* X0M0 e3 */
        cpxZp16,            /* X0M0 e4 */
        sbcZp16,            /* X0M0 e5 */
        incZp16,            /* X0M0 e6 */
        sbcIndirectLong16,  /* X0M0 e7 */
        inx16,              /* X0M0 e8 */
        sbcImm16,           /* X0M0 e9 */
        nop,                /* X0M0 ea */
        xba,                /* X0M0 eb */
        cpxAbs16,           /* X0M0 ec */
        sbcAbs16,           /* X0M0 ed */
        incAbs16,           /* X0M0 ee */
        sbcLong16,          /* X0M0 ef */
        beq,                /* X0M0 f0 */
        sbcIndirecty16,     /* X0M0 f1 */
        sbcIndirect16,      /* X0M0 f2 */
        sbcsIndirecty16,    /* X0M0 f3 */
        pea,                /* X0M0 f4 */
        sbcZpx16,           /* X0M0 f5 */
        incZpx16,           /* X0M0 f6 */
        sbcIndirectLongy16, /* X0M0 f7 */
        sed,                /* X0M0 f8 */
        sbcAbsy16,          /* X0M0 f9 */
        plx16,              /* X0M0 fa */
        xce,                /* X0M0 fb */
        jsrIndx,            /* X0M0 fc */
        sbcAbsx16,          /* X0M0 fd */
        incAbsx16,          /* X0M0 fe */
        sbcLongx16,         /* X0M0 ff */
    },
    {
        brkE,               /* EMUL 00 */
        oraIndirectxE,      /* EMUL 01 */
        cope,               /* EMUL 02 */
        oraSp8,             /* EMUL 03 */
        tsbZp8,             /* EMUL 04 */
        oraZp8,             /* EMUL 05 */
        aslZp8,             /* EMUL 06 */
        oraIndirectLong8,   /* EMUL 07 */
        phpE,               /* EMUL 08 */
        oraImm8,            /* EMUL 09 */
        asla8,              /* EMUL 0a */
        phd,                /* EMUL 0b */
        tsbAbs8,            /* EMUL 0c */
        oraAbs8,            /* EMUL 0d */
        aslAbs8,            /* EMUL 0e */
        oraLong8,           /* EMUL 0f */
        bpl,                /* EMUL 10 */
        oraIndirectyE,      /* EMUL 11 */
        oraIndirect8,       /* EMUL 12 */
        orasIndirecty8,     /* EMUL 13 */
        trbZp8,             /* EMUL 14 */
        oraZpx8,            /* EMUL 15 */
        aslZpx8,            /* EMUL 16 */
        oraIndirectLongy8,  /* EMUL 17 */
        clc,                /* EMUL 18 */
        oraAbsy8,           /* EMUL 19 */
        inca8,              /* EMUL 1a */
        tcs,                /* EMUL 1b */
        trbAbs8,            /* EMUL 1c */
        oraAbsx8,           /* EMUL 1d */
        aslAbsx8,           /* EMUL 1e */
        oraLongx8,          /* EMUL 1f */
        jsrE,               /* EMUL 20 */
        andIndirectxE,      /* EMUL 21 */
        jslE,               /* EMUL 22 */
        andSp8,             /* EMUL 23 */
        bitZp8,             /* EMUL 24 */
        andZp8,             /* EMUL 25 */
        rolZp8,             /* EMUL 26 */
        andIndirectLong8,   /* EMUL 27 */
        plpE,               /* EMUL 28 */
        andImm8,            /* EMUL 29 */
        rola8,              /* EMUL 2a */
        pld,                /* EMUL 2b */
        bitAbs8,            /* EMUL 2c */
        andAbs8,            /* EMUL 2d */
        rolAbs8,            /* EMUL 2e */
        andLong8,           /* EMUL 2f */
        bmi,                /* EMUL 30 */
        andIndirectyE,      /* EMUL 31 */
        andIndirect8,       /* EMUL 32 */
        andsIndirecty8,     /* EMUL 33 */
        bitZpx8,            /* EMUL 34 */
        andZpx8,            /* EMUL 35 */
        rolZpx8,            /* EMUL 36 */
        andIndirectLongy8,  /* EMUL 37 */
        sec,                /* EMUL 38 */
        andAbsy8,           /* EMUL 39 */
        deca8,              /* EMUL 3a */
        tsc,                /* EMUL 3b */
        bitAbsx8,           /* EMUL 3c */
        andAbsx8,           /* EMUL 3d */
        rolAbsx8,           /* EMUL 3e */
        andLongx8,          /* EMUL 3f */
        rtiE,               /* EMUL 40 */
        eorIndirectxE,      /* EMUL 41 */
        wdm,                /* EMUL 42 */
        eorSp8,             /* EMUL 43 */
        mvp,                /* EMUL 44 */
        eorZp8,             /* EMUL 45 */
        lsrZp8,             /* EMUL 46 */
        eorIndirectLong8,   /* EMUL 47 */
        phaE,               /* EMUL 48 */
        eorImm8,            /* EMUL 49 */
        lsra8,              /* EMUL 4a */
        phke,               /* EMUL 4b */
        jmp,                /* EMUL 4c */
        eorAbs8,            /* EMUL 4d */
        lsrAbs8,            /* EMUL 4e */
        eorLong8,           /* EMUL 4f */
        bvc,                /* EMUL 50 */
        eorIndirectyE,      /* EMUL 51 */
        eorIndirect8,       /* EMUL 52 */
        eorsIndirecty8,     /* EMUL 53 */
        mvn,                /* EMUL 54 */
        eorZpx8,            /* EMUL 55 */
        lsrZpx8,            /* EMUL 56 */
        eorIndirectLongy8,  /* EMUL 57 */
        cli,                /* EMUL 58 */
        eorAbsy8,           /* EMUL 59 */
        phyE,               /* EMUL 5a */
        tcd,                /* EMUL 5b */
        jmplong,            /* EMUL 5c */
        eorAbsx8,           /* EMUL 5d */
        lsrAbsx8,           /* EMUL 5e */
        eorLongx8,          /* EMUL 5f */
        rtsE,               /* EMUL 60 */
        adcIndirectxE,      /* EMUL 61 */
        per,                /* EMUL 62 */
        adcSp8,             /* EMUL 63 */
        stzZp8,             /* EMUL 64 */
        adcZp8,             /* EMUL 65 */
        rorZp8,             /* EMUL 66 */
        adcIndirectLong8,   /* EMUL 67 */
        plaE,               /* EMUL 68 */
        adcImm8,            /* EMUL 69 */
        rora8,              /* EMUL 6a */
        rtlE,               /* EMUL 6b */
        jmpind,             /* EMUL 6c */
        adcAbs8,            /* EMUL 6d */
        rorAbs8,            /* EMUL 6e */
        adcLong8,           /* EMUL 6f */
        bvs,                /* EMUL 70 */
        adcIndirectyE,      /* EMUL 71 */
        adcIndirect8,       /* EMUL 72 */
        adcsIndirecty8,     /* EMUL 73 */
        stzZpx8,            /* EMUL 74 */
        adcZpx8,            /* EMUL 75 */
        rorZpx8,            /* EMUL 76 */
        adcIndirectLongy8,  /* EMUL 77 */
        sei,                /* EMUL 78 */
        adcAbsy8,           /* EMUL 79 */
        plyE,               /* EMUL 7a */
        tdc,                /* EMUL 7b */
        jmpindx,            /* EMUL 7c */
        adcAbsx8,           /* EMUL 7d */
        rorAbsx8,           /* EMUL 7e */
        adcLongx8,          /* EMUL 7f */
        bra,                /* EMUL 80 */
        staIndirectxE,      /* EMUL 81 */
        brl,                /* EMUL 82 */
        staSp8,             /* EMUL 83 */
        styZp8,             /* EMUL 84 */
        staZp8,             /* EMUL 85 */
        stxZp8,             /* EMUL 86 */
        staIndirectLong8,   /* EMUL 87 */
        dey8,               /* EMUL 88 */
        bitImm8,            /* EMUL 89 */
        txa8,               /* EMUL 8a */
        phbe,               /* EMUL 8b */
        styAbs8,            /* EMUL 8c */
        staAbs8,            /* EMUL 8d */
        stxAbs8,            /* EMUL 8e */
        staLong8,           /* EMUL 8f */
        bcc,                /* EMUL 90 */
        staIndirectyE,      /* EMUL 91 */
        staIndirect8,       /* EMUL 92 */
        staSIndirecty8,     /* EMUL 93 */
        styZpx8,            /* EMUL 94 */
        staZpx8,            /* EMUL 95 */
        stxZpy8,            /* EMUL 96 */
        staIndirectLongy8,  /* EMUL 97 */
        tya8,               /* EMUL 98 */
        staAbsy8,           /* EMUL 99 */
        txs8,               /* EMUL 9a */
        txy8,               /* EMUL 9b */
        stzAbs8,            /* EMUL 9c */
        staAbsx8,           /* EMUL 9d */
        stzAbsx8,           /* EMUL 9e */
        staLongx8,          /* EMUL 9f */
        ldyImm8,            /* EMUL a0 */
        ldaIndirectxE,      /* EMUL a1 */
        ldxImm8,            /* EMUL a2 */
        ldaSp8,             /* EMUL a3 */
        ldyZp8,             /* EMUL a4 */
        ldaZp8,             /* EMUL a5 */
        ldxZp8,             /* EMUL a6 */
        ldaIndirectLong8,   /* EMUL a7 */
        tay8,               /* EMUL a8 */
        ldaImm8,            /* EMUL a9 */
        tax8,               /* EMUL aa */
        plbe,               /* EMUL ab */
        ldyAbs8,            /* EMUL ac */
        ldaAbs8,            /* EMUL ad */
        ldxAbs8,            /* EMUL ae */
        ldaLong8,           /* EMUL af */
        bcs,                /* EMUL b0 */
        ldaIndirectyE,      /* EMUL b1 */
        ldaIndirect8,       /* EMUL b2 */
        ldaSIndirecty8,     /* EMUL b3 */
        ldyZpx8,            /* EMUL b4 */
        ldaZpx8,            /* EMUL b5 */
        ldxZpy8,            /* EMUL b6 */
        ldaIndirectLongy8,  /* EMUL b7 */
        clv,                /* EMUL b8 */
        ldaAbsy8,           /* EMUL b9 */
        tsx8,               /* EMUL ba */
        tyx8,               /* EMUL bb */
        ldyAbsx8,           /* EMUL bc */
        ldaAbsx8,           /* EMUL bd */
        ldxAbsy8,           /* EMUL be */
        ldaLongx8,          /* EMUL bf */
        cpyImm8,            /* EMUL c0 */
        cmpIndirectxE,      /* EMUL c1 */
        rep65816,           /* EMUL c2 */
        cmpSp8,             /* EMUL c3 */
        cpyZp8,             /* EMUL c4 */
        cmpZp8,             /* EMUL c5 */
        decZp8,             /* EMUL c6 */
        cmpIndirectLong8,   /* EMUL c7 */
        iny8,               /* EMUL c8 */
        cmpImm8,            /* EMUL c9 */
        dex8,               /* EMUL ca */
        wai,                /* EMUL cb */
        cpyAbs8,            /* EMUL cc */
        cmpAbs8,            /* EMUL cd */
        decAbs8,            /* EMUL ce */
        cmpLong8,           /* EMUL cf */
        bne,                /* EMUL d0 */
        cmpIndirectyE,      /* EMUL d1 */
        cmpIndirect8,       /* EMUL d2 */
        cmpsIndirecty8,     /* EMUL d3 */
        pei,                /* EMUL d4 */
        cmpZpx8,            /* EMUL d5 */
        decZpx8,            /* EMUL d6 */
        cmpIndirectLongy8,  /* EMUL d7 */
        cld,                /* EMUL d8 */
        cmpAbsy8,           /* EMUL d9 */
        phxE,               /* EMUL da */
        stp,                /* EMUL db */
        jmlind,             /* EMUL dc */
        cmpAbsx8,           /* EMUL dd */
        decAbsx8,           /* EMUL de */
        cmpLongx8,          /* EMUL df */
        cpxImm8,            /* EMUL e0 */
        sbcIndirectxE,      /* EMUL e1 */
        sep,                /* EMUL e2 */
        sbcSp8,             /* EMUL e3 */
        cpxZp8,             /* EMUL e4 */
        sbcZp8,             /* EMUL e5 */
        incZp8,             /* EMUL e6 */
        sbcIndirectLong8,   /* EMUL e7 */
        inx8,               /* EMUL e8 */
        sbcImm8,            /* EMUL e9 */
        nop,                /* EMUL ea */
        xba,                /* EMUL eb */
        cpxAbs8,            /* EMUL ec */
        sbcAbs8,            /* EMUL ed */
        incAbs8,            /* EMUL ee */
        sbcLong8,           /* EMUL ef */
        beq,                /* EMUL f0 */
        sbcIndirectyE,      /* EMUL f1 */
        sbcIndirect8,       /* EMUL f2 */
        sbcsIndirecty8,     /* EMUL f3 */
        pea,                /* EMUL f4 */
        sbcZpx8,            /* EMUL f5 */
        incZpx8,            /* EMUL f6 */
        sbcIndirectLongy8,  /* EMUL f7 */
        sed,                /* EMUL f8 */
        sbcAbsy8,           /* EMUL f9 */
        plxE,               /* EMUL fa */
        xce,                /* EMUL fb */
        jsrIndxE,           /* EMUL fc */
        sbcAbsx8,           /* EMUL fd */
        incAbsx8,           /* EMUL fe */
        sbcLongx8,          /* EMUL ff */
    }
};

/*Functions*/

static void set_cpu_mode(int mode)
{
    cpumode = mode;
    modeptr = &(opcodes[mode][0]);
}

static void updatecpumode(void)
{
    int mode;

    if (p.e) {
        mode = 4;
        x.b.h = y.b.h = 0;
    } else {
        mode = 0;
        if (!p.m)
            mode |= 1;
        if (!p.ex)
            mode |= 2;
        if (p.ex)
            x.b.h = y.b.h = 0;
    }
    set_cpu_mode(mode);
}

void w65816_reset(void)
{
    def = 1;
    // This test is rather academic as def is 1 at this point
    //if (def || !(banking & 4))
    //    w65816mask = 0xFFFF;
    //else
    //    w65816mask = 0x7FFFF;
    w65816mask = 0xFFFF;
    pbr = dbr = 0;
    s.w = 0x1FF;
    set_cpu_mode(4);
    p.e = 1;
    p.i = 1;
    pc = readmemw(0xFFFC);
    a.w = x.w = y.w = 0;
    p.ex = p.m = 1;
    cycles = 0;
}

void w65816_close(void)
{
   //    if (w65816ram)
   //        free(w65816ram);
}

#if 0
static inline unsigned char *save_reg(unsigned char *ptr, reg * rp)
{
    *ptr++ = rp->b.l;
    *ptr++ = rp->b.h;
    return ptr;
}

static void w65816_savestate(ZFILE * zfp)
{
    unsigned char bytes[38], *ptr;

    ptr = save_reg(bytes, &w65816a);
    ptr = save_reg(ptr, &w65816x);
    ptr = save_reg(ptr, &w65816y);
    ptr = save_reg(ptr, &w65816s);
    *ptr++ = pack_flags();
    ptr = save_uint32(ptr, pbr);
    ptr = save_uint32(ptr, dbr);
    ptr = save_uint16(ptr, w65816pc);
    ptr = save_uint16(ptr, dp);
    ptr = save_uint32(ptr, wins);
    *ptr++ = inwai;
    *ptr++ = cpumode;
    *ptr++ = w65816opcode;
    *ptr++ = def;
    *ptr++ = divider;
    *ptr++ = banking;
    *ptr++ = banknum;
    ptr = save_uint32(ptr, w65816mask);
    ptr = save_uint16(ptr, toldpc);
    savestate_zwrite(zfp, bytes, sizeof bytes);
    savestate_zwrite(zfp, w65816ram, W65816_RAM_SIZE);
    savestate_zwrite(zfp, w65816rom, W65816_ROM_SIZE);
}

static inline unsigned char *load_reg(unsigned char *ptr, reg * rp)
{
    rp->b.l = *ptr++;
    rp->b.h = *ptr++;
    return ptr;
}

static void w65816_loadstate(ZFILE * zfp)
{
    unsigned char bytes[38], *ptr;

    savestate_zread(zfp, bytes, sizeof bytes);
    ptr = load_reg(bytes, &w65816a);
    ptr = load_reg(ptr, &w65816x);
    ptr = load_reg(ptr, &w65816y);
    ptr = load_reg(ptr, &w65816s);
    unpack_flags(*ptr++);
    ptr = load_uint32(ptr, &pbr);
    ptr = load_uint32(ptr, &dbr);
    ptr = load_uint16(ptr, &w65816pc);
    ptr = load_uint16(ptr, &dp);
    ptr = load_uint32(ptr, &wins);
    inwai = *ptr++;
    cpumode = *ptr++;
    w65816opcode = *ptr++;
    def = *ptr++;
    divider = *ptr++;
    banking = *ptr++;
    banknum = *ptr++;
    ptr = load_uint32(ptr, &w65816mask);
    ptr = load_uint16(ptr, &toldpc);
    savestate_zread(zfp, w65816ram, W65816_RAM_SIZE);
    savestate_zread(zfp, w65816rom, W65816_ROM_SIZE);
}

#endif

void w65816_init(void *rom, uint8_t nativeVectBank)
{
    w65816ram = copro_mem_reset(W65816_RAM_SIZE);
    w65816rom = rom;
    w65816nvb = nativeVectBank;
#if 0
    tube_type = W65816;
    tube_readmem = readmem65816;
    tube_writemem = writemem65816;
    tube_exec  = w65816_exec;
    tube_proc_savestate = w65816_savestate;
    tube_proc_loadstate = w65816_loadstate;
#endif
    w65816_reset();
}

static void nmi65816(void)
{
    readmem(pbr | pc);
    cycles--;
    clockspc(6);
    if (inwai)
        pc++;
    inwai = 0;
    if (!p.e) {
        writemem(s.w--, pbr >> 16);
        writemem(s.w--, pc >> 8);
        writemem(s.w--, pc & 0xFF);
        writemem(s.w--, pack_flags());
        pc = readmemw((w65816nvb << 16) | 0xFFEA);
        pbr = 0;
        p.i = 1;
        p.d = 0;
    } else {
        writemem(s.w, pc >> 8);
        s.b.l--;
        writemem(s.w, pc & 0xFF);
        s.b.l--;
        writemem(s.w, pack_flags_em(0x30));
        s.b.l--;
        pc = readmemw(0xFFFA);
        pbr = 0;
        p.i = 1;
        p.d = 0;
    }
}

static void irq65816(void)
{
    readmem(pbr | pc);
    cycles--;
    clockspc(6);
    if (inwai && p.i) {
        pc++;
        inwai = 0;
        return;
    }
    if (inwai)
        pc++;
    inwai = 0;
    if (!p.e) {
        writemem(s.w--, pbr >> 16);
        writemem(s.w--, pc >> 8);
        writemem(s.w--, pc & 0xFF);
        writemem(s.w--, pack_flags());
        pc = readmemw((w65816nvb << 16) | 0xFFEE);
        pbr = 0;
        p.i = 1;
        p.d = 0;
    } else {
        writemem(s.w, pc >> 8);
        s.b.l--;
        writemem(s.w, pc & 0xFF);
        s.b.l--;
        writemem(s.w, pack_flags_em(0x20));
        s.b.l--;
        pc = readmemw(0xFFFE);
        pbr = 0;
        p.i = 1;
        p.d = 0;
    }
}

void w65816_exec(int tubecycles)
{
    uint32_t ia;

    cycles = tubecycles;

    while (cycles > 0) {
        ia = pbr | pc;
        toldpc = pc++;
#ifdef INCLUDE_DEBUGGER
        if (dbg_w65816)
            debug_preexec(&w65816_cpu_debug, ia);
#endif
        opcode = readmem(ia);
        modeptr[opcode]();
        wins++;
        if ((tube_irq & NMI_BIT)) {
            nmi65816();
            tube_ack_nmi();
        } else if ((tube_irq & IRQ_BIT) && !p.i) {
            irq65816();
        }
    }
}
