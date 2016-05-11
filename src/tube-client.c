#include <stdio.h>
#include "copro-lib6502.h"
#include "copro-65tube.h"

#define COPRO_LIB6502 3
#define COPRO_65TUBE  4

#define COPRO COPRO_LIB6502

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
   if (COPRO == COPRO_65TUBE)
   {
      copro_65tube_main(r0, r1, atags);
   }
   else if (COPRO == COPRO_LIB6502)
   {
      copro_lib6502_main(r0, r1, atags);
   }
   else
   {
      printf("Co Pro %d has not been defined yet\r\n", COPRO);
   }

   printf("Halted....\r\n");
   while (1)
   {
   }
}
