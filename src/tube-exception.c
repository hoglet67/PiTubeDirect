#include "rpi-base.h"
#include "rpi-aux.h"
#include "tube-ula.h"
#include "rpi-asm-helpers.h"

// From here: https://www.raspberrypi.org/forums/viewtopic.php?f=72&t=53862
static void reboot_now(void)
{
  const int PM_PASSWORD = 0x5a000000;
  const int PM_RSTC_WRCFG_FULL_RESET = 0x00000020;
  unsigned int *PM_WDOG = (unsigned int *) (PERIPHERAL_BASE + 0x00100024);
  unsigned int *PM_RSTC = (unsigned int *) (PERIPHERAL_BASE + 0x0010001c);

  // timeout = 1/16th of a second? (whatever)
  *PM_WDOG = PM_PASSWORD | 1;
  *PM_RSTC = PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET;
  while(1);
}

// For some reason printf generally doesn't work here
static void dump_info(unsigned int *context, int offset, const char *type) {
  unsigned int *addr;
  const unsigned int *reg;
  unsigned int flags;
  int i, j;
  RPI_AuxMiniUartFlush(); // flush UART so we don't corrupt any messages
  // Make sure we avoid unaligned accesses
  context = (unsigned int *)(((unsigned int) context) & ~3u);
  // context point into the exception stack, at flags, followed by registers 0 .. 13
  reg = context + 1;
  dump_string(type,0,1);
  dump_string(" at ",0,1);
  // The stacked LR points one or two words after the exception address
  addr = (unsigned int *)((reg[13] & ~3u) - (uint32_t)offset);
  dump_hex((unsigned int)addr,32,1);
#if (__ARM_ARCH >= 7 )
  dump_string(" on core ",0,1);
  dump_hex(_get_core(),4,1);
#endif
  dump_string("\r\n",0,1);
  dump_string("Registers:\r\n",0,1);
  for (i = 0; i <= 13; i++) {
    j = (i < 13) ? i : 14; // slot 13 actually holds the link register
    dump_string("  r[",0,1);
    RPI_UnbufferedWrite((uint8_t)('0' + (j / 10)));
    RPI_UnbufferedWrite((uint8_t)('0' + (j % 10)));
    dump_string("]=",0,1);
    dump_hex(reg[i],32,1);
    dump_string("\r\n",0,1);
  }
  dump_string("Memory:\r\n",0,1);
  for (i = -4; i <= 4; i++) {
    dump_string("  ",0,1);
    dump_hex((unsigned int) (addr + i),32,1);
    RPI_UnbufferedWrite('=');
    dump_hex(*(addr + i),32,1);
    if (i == 0) {
      dump_string(" <<<<<< \r\n",0,1);
    } else {
      dump_string("\r\n",0,1);
    }
  }
  // The flags are pointed to by context, before the registers
  flags = *context;
  dump_string("Flags: \r\n  NZCV--------------------IFTMMMMM\r\n  ",0,1);
  dump_binary(flags,1);
  dump_string(" (",0,1);
  // The last 5 bits of the flags are the mode
  switch (flags & 0x1f) {
  case 0x10:
    dump_string("User",0,1);
    break;
  case 0x11:
    dump_string("FIQ",0,1);
    break;
  case 0x12:
    dump_string("IRQ",0,1);
    break;
  case 0x13:
    dump_string("Supervisor",0,1);
    break;
  case 0x17:
    dump_string("Abort",0,1);
    break;
  case 0x1B:
    dump_string("Undefined",0,1);
    break;
  case 0x1F:
    dump_string("System",0,1);
    break;
  default:
    dump_string("Illegal",0,1);
    break;
  };
  dump_string(" Mode)\r\n",0,1);

  dump_string("Halted waiting for reset\r\n",0,1);

  // look for reset being low
  while( !tube_is_rst_active() );
  // then reset on the next rising edge
  while( tube_is_rst_active() );
  reboot_now();
}

// Called from assembler
// cppcheck-suppress unusedFunction
void enable_watch0(unsigned int wp, unsigned int bytes,unsigned int storeload)
{
  //DSCR must be setup before writing to the watchpoint registers
  //NB ARM v7 mcr   p14,0,R0, c0,c1, 2
  asm volatile ("mcr     p14, 0, %0, c0, c1, 0" : : "r" (1<<15));
  unsigned int control = ((bytes& 0xF )<<5) | ((storeload &3)<<3) | 7;
// CP14 opcode1 =0, Crn =0 {opcode2 =6,crm= 0} WVR0
  asm volatile ("mcr     p14, 0, %0, c0, c0, 6" : : "r" (wp&0xFFFFFFFC));
// CP14 copdoe1 =0, Crn =0 {opcode2 =7,crm= 0} WCR0
  asm volatile ("mcr     p14, 0, %0, c0, c0, 7" : : "r" (control));
  asm volatile ("MCR p15, 0, %0, c7, c5, 4" : : "r" (0)); // flush prefetch buffer
}

// Called from assembler
// cppcheck-suppress unusedFunction
void enable_watch1(unsigned int wp,unsigned int bytes,unsigned int storeload)
{
   //DSCR
     //NB ARM v7 mcr   p14,0,R0, c0,c1, 2
  asm volatile ("mcr     p14, 0, %0, c0, c1, 0" : : "r" (1<<15));
  unsigned int control = ((bytes& 0xF )<<5) | ((storeload &3)<<3) | 7;
// CP14 opcode1 =0, Crn =0 {opcode2 =6,crm= 1} WVR1
  asm volatile ("mcr     p14, 0, %0, c0, c1, 6" : : "r" (wp));
// CP14 copdoe1 =0, Crn =0 {opcode2 =7,crm= 1}
  asm volatile ("mcr     p14, 0, %0, c0, c1, 7" : : "r" (control));
}

// Called from assembler
// cppcheck-suppress unusedFunction
void undefined_instruction_handler(unsigned int *context) {
  dump_info(context, 4, "Undefined Instruction");
}
// Called from assembler
// cppcheck-suppress unusedFunction
void prefetch_abort_handler(unsigned int *context) {
  dump_info(context, 8, "Prefetch Abort");
}
// Called from assembler
// cppcheck-suppress unusedFunction
void data_abort_handler(unsigned int *context) {
  unsigned int DFSR;
  asm volatile ("mrc p15, 0, %0, c5, c0,  0" : "=r" (DFSR));
  if ((DFSR&0xF)==2) // read DFSR
    {
    // Watchpoint triggered
    dump_string("WP:",0,0);
    asm volatile ("mcr p15, 0, %0, c5, c0,  0" : : "r" (0));

    context = (unsigned int *)(((unsigned int) context) & ~3u);
    // context point into the exception stack, at flags, followed by registers 0 .. 13
    const unsigned int *reg = context + 1;
    unsigned int *addr = (unsigned int *)((reg[13] & ~3u) - (uint32_t)12);
    dump_hex((unsigned int)addr,32,0);
    dump_string("=",0,0);
    unsigned int control;
    asm volatile ("mrc p14, 0, %0, c0, c0, 7" : "=r" (control));
    control &= 0xFFFFFFFE;
    asm volatile ("mcr p14, 0, %0, c0, c0, 7" : : "r" (control));
    const unsigned int *FAR;
    asm volatile ("mrc p15, 0, %0, c6, c0,  0" : "=r" (FAR));
    dump_hex((unsigned int)(*FAR),32,0);
    control |= 1;
    asm volatile ("mcr p14, 0, %0, c0, c0, 7" : : "r" (control));
    dump_string("\r\n",0,0);
    return;
  }
  dump_info(context, 12, "Data Abort");
}
// Called from assembler
// cppcheck-suppress unusedFunction
void swi_handler(unsigned int *context) {
  dump_info(context, 4, "SWI");
}
