/*
 * Null Co Processor Emulation
 *
 * (c) 2016 David Banks
 * 
 */

#include <stdio.h>
#include <string.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "copro-null.h"
#include "startup.h"

void copro_null_emulator() {
   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   printf("This Co Pro has not been implemented yet\r\n");

   // Disable the tube, so the Beeb doesn't hang
   disable_tube();

   // Wait for copro to be changed via *FX 151,230,N
   // then exit on the next reset
   while (1) {
#ifdef USE_GPU
      // temp hack to get around coherency issues
      _invalidate_dcache_mva((void*) &tube_mailbox);
#endif
      if (tube_mailbox & ATTN_MASK) {
         unsigned int tube_mailbox_copy = tube_mailbox;
         tube_mailbox &= ~(ATTN_MASK | OVERRUN_MASK);
#ifdef USE_GPU
         // temp hack to get around coherency issues
         _clean_invalidate_dcache_mva((void *) &tube_mailbox);
#endif
         // With tube disabled, only writes to reg 6 are implements
         // which update the copro global variable
         tube_io_handler(tube_mailbox_copy);
         // Exit on a change of copro
         if (copro != last_copro) {
            return;
         }
      }
   }
}
