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

   printf("This is the NULL copro\r\n");

   // Disable the tube, so the Beeb doesn't hang
   disable_tube();

   // Wait for copro to be changed via *FX 151,230,N
   // then exit on the next reset
   while (1) {

         // Exit on a change of copro ( changed in the FIQ handler)
         if (copro != last_copro) {
            return;
  
      }
   }
}
