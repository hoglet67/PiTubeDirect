#include <stdio.h>
#include "performance.h"

void reset_performance_counters(int type0, int type1) {

   unsigned val;
   unsigned cycle_counter;
   unsigned counter0;
   unsigned counter1;

   // TODO: Update for compatibility with RPI2

  // Read counters
#ifndef RPI2
  asm volatile ("mrc p15,0,%0,c15,c12,1" : "=r" (cycle_counter));
  asm volatile ("mrc p15,0,%0,c15,c12,2" : "=r" (counter0));
  asm volatile ("mrc p15,0,%0,c15,c12,3" : "=r" (counter1));

  // Print counters
  printf("cycle_counter = %u\r\n", cycle_counter);
  printf("counter0: type = %02x; count = %u\r\n", type0, counter0);
  printf("counter1: type = %02x; count = %u\r\n", type1, counter1);

  // Set control register and zero counters

  // bit 3 = 1 means count every 64th processor cycle
  // bit 2 = 1 means reset cycle counter to zero
  // bit 1 = 1 means reset counters to zero
  // bit 0 = 1 enable counters

  val =  (type0 << 20) | (type1 << 12) | 0xF;
  asm volatile ("mcr p15,0,%0,c15,c12,0" :: "r" (val) : "memory");
#endif
}
