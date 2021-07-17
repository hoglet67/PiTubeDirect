/*
 * 6502 Co Processor Emulation
 *
 * (c) 2016 David Banks and Ed Spittles
 *
 * based on code by
 * - Reuben Scratton.
 * - Tom Walker
 *
 */

#include <stdio.h>
#include <string.h>
#include "tube-client.h"
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "tuberom_6502.h"
#include "programs.h"
#include "copro-65tubejit.h"
#include "cache.h"
#include "copro-defs.h"

// used for the debug output
#include "lib6502.h"
#include "../darm/darm.h"
#define ADDRESS_MASK    ((unsigned int) 0xFFfffffFu)

static unsigned char *copro_65tube_poweron_reset(void) {
   // Wipe memory
   unsigned char * mpu_memory;
   mpu_memory = copro_mem_reset(0xF800); // only need to goto 0xF800 as rom will be put in later
   // Install test programs (like sphere)
   copy_test_programs(mpu_memory);
   return mpu_memory;
}

static void copro_65tube_reset(int type, unsigned char mpu_memory[]) {
   // Re-instate the Tube ROM on reset
   if (type == TYPE_65TUBE_0 || type == TYPE_65TUBE_1) {
      memcpy(mpu_memory + 0xf800, tuberom_6502_extern_1_10, 0x800);
   } else {
      memcpy(mpu_memory + 0xf800, tuberom_6502_intern_1_10, 0x800);
   }
   // Wait for rst become inactive before continuing to execute
   tube_wait_for_rst_release();
}

void copro_65tubejit_emulator(int type) {
   // Remember the current copro so we can exit if it changes
   unsigned int last_copro = copro;
   unsigned char * mpu_memory;

   mpu_memory = copro_65tube_poweron_reset();
   copro_65tube_reset(type, mpu_memory);

   // Make page 64K point to page 0 so that accesses LDA 0xFFFF, X work without needing masking
   map_4k_page(65536>>12, 0);

   // Make the JITTEDTABLE16 table wrap as well.
   map_4k_pageJIT((JITTEDTABLE16+(4*65536))>>12, JITTEDTABLE16>>12);

   while (copro == last_copro) {
      tube_reset_performance_counters();
      exec_65tubejit(mpu_memory,0 );

      tube_log_performance_counters();
      copro_65tube_reset(type, mpu_memory);
   }

   // restore memory mapping

   for ( unsigned int i= 0 ; i<=16; i++ )
     map_4k_page(i, i);

   map_4k_pageJIT((JITTEDTABLE16+(4*65536))>>12, (JITTEDTABLE16+(4*65536))>>12);
}

void dis6502( int addr)
{
   char buffer[4096];
   M6502 mpu;

   mpu.memory=0;

   M6502_disassemble(&mpu, addr,buffer);
   printf(" Jitting : %x : %s\r\n",addr,buffer);
}

static darm_t dis;
static darm_str_t dis_str;

void disarm(unsigned int addr, int endaddr)
{
   char buffer[4096];

   size_t bufsize = 4096;
   int length = (endaddr+JITLET-addr)>>2;

   for (int i=0;i<length;i++)
   {
   unsigned int instr = *(char *)addr;
   char *buf= buffer;
   int len = snprintf(buf, bufsize, "%08"PRIx32" %08"PRIx32" ",(long unsigned int) addr, (long unsigned int )instr);
   buf += len;
   bufsize -= len;
   int ok = 0;
   if (darm_armv7_disasm(&dis, instr) == 0) {
      dis.addr = addr;
      dis.addr_mask = ADDRESS_MASK;
      if (darm_str2(&dis, &dis_str, 1) == 0) {
         strncpy(buf, dis_str.total, bufsize);
         ok = 1;
      }
   }
      if (!ok) {
      strncpy(buf, "???", bufsize);
   }

   printf("%s\r\n",buffer);
      addr = addr+4;
   }
}