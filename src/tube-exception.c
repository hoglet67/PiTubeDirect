#include "rpi-base.h"
#include "rpi-aux.h"
#include "tube-ula.h"
#include "startup.h"

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

#if (__ARM_ARCH >= 7 )
static void dump_digit(unsigned int c) {
   c &= 15;
   if (c < 10) {
      c = '0' + c;
   } else {
      c = 'A' + c - 10;
   }
   RPI_AuxMiniUartWrite((uint8_t)c);
}
#endif

static void dump_binary(unsigned int value) {
  for (int i = 0; i < 32; i++) {
    RPI_AuxMiniUartWrite((uint8_t)('0' + (value >> 31)));
    value <<= 1;
  }
}

// For some reason printf generally doesn't work here
static void dump_info(unsigned int *context, int offset, const char *type) {
  unsigned int *addr;
  unsigned int *reg;
  unsigned int flags;
  int i, j;

  // Make sure we avoid unaligned accesses
  context = (unsigned int *)(((unsigned int) context) & ~3u);
  // context point into the exception stack, at flags, followed by registers 0 .. 13
  reg = context + 1;
  dump_string(type,0);
  dump_string(" at ",0);
  // The stacked LR points one or two words after the exception address
  addr = (unsigned int *)((reg[13] & ~3u) - (uint32_t)offset);
  dump_hex((unsigned int)addr,32);
#if (__ARM_ARCH >= 7 )
  dump_string(" on core ",0);
  dump_digit(_get_core());
#endif
  dump_string("\r\n",0);
  dump_string("Registers:\r\n",0);
  for (i = 0; i <= 13; i++) {
    j = (i < 13) ? i : 14; // slot 13 actually holds the link register
    dump_string("  r[",0);
    RPI_AuxMiniUartWrite((uint8_t)('0' + (j / 10)));
    RPI_AuxMiniUartWrite((uint8_t)('0' + (j % 10)));
    dump_string("]=",0);
    dump_hex(reg[i],32);
    dump_string("\r\n",0);
  }
  dump_string("Memory:\r\n",0);
  for (i = -4; i <= 4; i++) {
    dump_string("  ",0);
    dump_hex((unsigned int) (addr + i),32);
    RPI_AuxMiniUartWrite('=');
    dump_hex(*(addr + i),32);
    if (i == 0) {
      dump_string(" <<<<<< \r\n",0);
    } else {
      dump_string("\r\n",0);
    }
  }
  // The flags are pointed to by context, before the registers
  flags = *context;
  dump_string("Flags: \r\n  NZCV--------------------IFTMMMMM\r\n  ",0);
  dump_binary(flags);
  dump_string(" (",0);
  // The last 5 bits of the flags are the mode
  switch (flags & 0x1f) {
  case 0x10:
    dump_string("User",0);
    break;
  case 0x11:
    dump_string("FIQ",0);
    break;
  case 0x12:
    dump_string("IRQ",0);
    break;
  case 0x13:
    dump_string("Supervisor",0);
    break;
  case 0x17:
    dump_string("Abort",0);
    break;
  case 0x1B:
    dump_string("Undefined",0);
    break;
  case 0x1F:
    dump_string("System",0);
    break;
  default:
    dump_string("Illegal",0);
    break;
  };
  dump_string(" Mode)\r\n",0);

  dump_string("Halted waiting for reset\r\n",0);

  // look for reset being low
  while( !tube_is_rst_active() );
  // then reset on the next rising edge
  while( tube_is_rst_active() );
  reboot_now();
}
// Called from assembler
// cppcheck-suppress unusedFunction
void undefined_instruction_handler(unsigned int *context) {
  dump_info(context, 4, "Undefined Instruction");
}
// Called from assembler
// cppcheck-suppress unusedFunction
void prefetch_abort_handler(unsigned int *context) {
  dump_info(context, 4, "Prefetch Abort");
}
// Called from assembler
// cppcheck-suppress unusedFunction
void data_abort_handler(unsigned int *context) {
  dump_info(context, 8, "Data Abort");
}
// Called from assembler
// cppcheck-suppress unusedFunction
void swi_handler(unsigned int *context) {
  dump_info(context, 4, "SWI");
}
