/*
 * Arm2 Co Pro Emulation
 *
 * (c) 2015 David Banks
 * 
 * based on code by from MAME
 */


#include <stdio.h>
#include <string.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "mame/arm.h"
#include "tuberom_arm.h"
#include "copro-arm2.h"
#include "startup.h"

#define RAM_MASK8    ((UINT32) 0x003fffff)
#define ROM_MASK8    ((UINT32) 0x00003fff)
#define RAM_MASK32   ((UINT32) 0x003ffffc)
#define ROM_MASK32   ((UINT32) 0x00003ffc)

// 4MB of RAM starting at 0x00000000
UINT8 arm2_ram[1024 * 1024 * 4] __attribute__((aligned(0x10000)));

// 16KB of ROM starting at 0x03000000
UINT8 arm2_rom[0x4000] __attribute__((aligned(0x10000)));

#define R15 arm2_getR15()

UINT8 copro_arm2_read8(int addr)
{
  if (addr <= RAM_MASK8)
  {
    return arm2_ram[addr];
  }

  int type = (addr >> 24) & 3;
  switch (type)
  {
    case 0:
      return *(UINT8*) (arm2_ram + (addr & RAM_MASK8));
    case 1:
      return tube_parasite_read((addr >> 2) & 7);
    case 3:
      return *(UINT8*) (arm2_rom + (addr & ROM_MASK8));
  }
  return 0;

}

UINT32 copro_arm2_read32(int addr)
{
  UINT32 result;

  if ((addr & ~RAM_MASK32) == 0)
  {
    return *(UINT32*) (arm2_ram + addr);
  }

  int type = (addr >> 24) & 3;
  switch (type)
  {
    case 0:
      result = *(UINT32*) (arm2_ram + (addr & RAM_MASK32));
    break;
    case 1:
      result = tube_parasite_read((addr >> 2) & 7);
    break;
    case 3:
      result = *(UINT32*) (arm2_rom + (addr & ROM_MASK32));
    break;
    default:
      result = 0;
  }
  /* Unaligned reads rotate the word, they never combine words */
  if (addr & 3)
  {
    if (ARM_DEBUG_CORE && (addr & 1))
      logerror("%08x: Unaligned byte read %08x\n", R15, addr);
    if ((addr & 3) == 1)
      return ((result & 0x000000ff) << 24) | ((result & 0xffffff00) >> 8);
    if ((addr & 3) == 2)
      return ((result & 0x0000ffff) << 16) | ((result & 0xffff0000) >> 16);
    if ((addr & 3) == 3)
      return ((result & 0x00ffffff) << 8) | ((result & 0xff000000) >> 24);
  }
  return result;
}

void copro_arm2_write8(int addr, UINT8 data)
{
  int type = (addr >> 24) & 3;
  switch (type)
  {
    case 0:
      *(UINT8*) (arm2_ram + (addr & RAM_MASK8)) = data;
    break;
    case 1:
      tube_parasite_write((addr >> 2) & 7, data);
    break;
  }
}

void copro_arm2_write32(int addr, UINT32 data)
{
  int type = (addr >> 24) & 3;
  switch (type)
  {
    case 0:
      *(UINT32*) (arm2_ram + (addr & RAM_MASK32)) = data;
    break;
    case 1:
      tube_parasite_write((addr >> 2) & 7, data);
    break;
  }
  /* Unaligned writes are treated as normal writes */
  if (addr & 3)
    printf("%08x: Unaligned write %08x\n", R15, addr);
}

static void copro_arm2_poweron_reset()
{
  // Wipe memory
  memset(arm2_ram, 0, sizeof(arm2_ram));
}

static void copro_arm2_reset()
{
  // Log ARM performance counters
  tube_log_performance_counters();
  // Re-instate the Tube ROM on reset
  memcpy(arm2_ram, tuberom_arm_v100, 0x4000);
  memcpy(arm2_rom, tuberom_arm_v100, 0x4000);
  // Reset the ARM device
  arm2_device_reset();
  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();
  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_arm2_emulator()
{
   static unsigned int last_rst = 0;

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_arm2_poweron_reset();
   copro_arm2_reset();
  
   while (1)
   {
      arm2_execute_run(1);
#ifdef USE_GPU
      // temp hack to get around coherency issues
      // _invalidate_dcache_mva((void*) tube_mailbox);
#endif
      if (*tube_mailbox & ATTN_MASK) {
         unsigned int tube_mailbox_copy = *tube_mailbox;
         *tube_mailbox &= ~(ATTN_MASK | OVERRUN_MASK);
#ifdef USE_GPU
         // temp hack to get around coherency issues
         // _clean_invalidate_dcache_mva((void *) tube_mailbox);
#endif
         unsigned int intr = tube_io_handler(tube_mailbox_copy);
         unsigned int nmi = intr & 2;
         unsigned int rst = intr & 4;
         // Reset the processor on active edge of rst
         if (rst && !last_rst) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_arm2_reset();
         }
         // NMI is edge sensitive, so only check after mailbox activity
         if (nmi) {
            arm2_execute_set_input(ARM_FIRQ_LINE, 1);
         }
         last_rst = rst;
      }
      // IRQ is level sensitive, so check between every instruction
      arm2_execute_set_input(ARM_IRQ_LINE, tube_irq & 1);
   }
}
