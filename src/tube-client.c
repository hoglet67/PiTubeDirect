#include <stdio.h>
#include "copro-65tube.h"

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
   copro_65tube_main(r0, r1, atags);

   printf("Halted....\r\n");
   while (1)
   {
   }
}
