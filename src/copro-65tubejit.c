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
#include <inttypes.h>
#include "tube-client.h"
#include "tube.h"
#include "tube-ula.h"
#include "tuberom_6502.h"
#include "programs.h"
#include "copro-65tubejit.h"
#include "cache.h"
#include "copro-defs.h"
#include "rpi-aux.h"

// used for the debug output
#include "lib6502.h"
#include "darm/darm.h"
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
      copro_memcpy(mpu_memory + 0xf800, tuberom_6502_extern_1_20, 0x800);
   } else {
      copro_memcpy(mpu_memory + 0xf800, tuberom_6502_intern_1_20, 0x800);
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
#ifdef DEBUG

static void dis6502(unsigned int addr, int padding)
{
   char buffer[64];
   M6502 mpu;
   mpu.memory=0;
   dump_hex(addr,16,UART_BUFFERED);dump_char(' ',UART_BUFFERED);
   char * ptr =buffer;
   M6502_disassemble(&mpu, (unsigned short)addr,ptr);
   dump_string(buffer,padding,UART_BUFFERED);
}
// Called from assembler
// cppcheck-suppress unusedFunction
void dissall(unsigned int addr, unsigned length)
{
   // don't print debug for rom as it prevents language xfer
   if ((addr< 0xF800 ) || (addr >0xFF00))
   {
      unsigned int jitletaddr = JITLET+ ( addr<<3);

      for ( unsigned int i = 0; i < length ; i++)
      {
         darm_t dis;
         darm_str_t dis_str;
         if (i ==0)
            dis6502( addr, 12);
         else
            padding(17, UART_BUFFERED );

         dump_hex(jitletaddr,32, UART_BUFFERED );
         dump_char(' ',UART_BUFFERED);

         unsigned int instr = * (unsigned int *) (jitletaddr);
         if (darm_armv7_disasm(&dis, instr) == 0) {
            dis.addr = (jitletaddr);
            darm_str2(&dis, &dis_str, 0);
            dump_string(dis_str.total,0, UART_BUFFERED );
         }

         //dump_string(dis_str.total,30,0);
         /*
         unsigned int tableaddr = (JITTEDTABLE16+(addr<<2));
         if (darm_armv7_disasm(&dis, * (unsigned int *) (tableaddr)) == 0) {
            dis.addr = tableaddr;
            darm_str2(&dis, &dis_str, 0);
            tableaddr+=4;
         }
         RPI_AuxMiniUartString(dis_str.total,0);*/
         dump_string("\r\n", 2, UART_BUFFERED);
         jitletaddr+=4;
      }
      RPI_UARTTriggerTx();
   }

}
#endif
