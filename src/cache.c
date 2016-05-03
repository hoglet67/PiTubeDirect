#include <stdio.h>
#include "startup.h"
#include "rpi-base.h"

void enable_MMU_and_IDCaches(void)
{
  static volatile __attribute__ ((aligned (0x4000))) unsigned PageTable[4096];

  printf("enable_MMU_and_IDCaches\r\n");
  printf("cpsr    = %08x\r\n", _get_cpsr());

  unsigned base;
  unsigned threshold = PERIPHERAL_BASE >> 20;
  for (base = 0; base < threshold; base++)
  {
    // Value from my original RPI code = 11C0E (outer and inner write back, write allocate, shareable)
    // bits 11..10 are the AP bits, and setting them to 11 enables user mode access as well
    // Values from RPI2 = 11C0E (outer and inner write back, write allocate, shareable (fast but unsafe)); works on RPI
    // Values from RPI2 = 10C0A (outer and inner write through, no write allocate, shareable)
    // Values from RPI2 = 15C0A (outer write back, write allocate, inner write through, no write allocate, shareable)
    PageTable[base] = base << 20 | 0x01C0E;
  }
  for (; base < 4096; base++)
  {
    // shared device, never execute
    PageTable[base] = base << 20 | 0x10C16;
  }

  // RPI:  bit 6 is restrict cache size to 16K (no page coloring)
  // RPI2: bit 6 is set SMP bit, otherwise all caching disabled
  unsigned auxctrl;
  asm volatile ("mrc p15, 0, %0, c1, c0,  1" : "=r" (auxctrl));
  auxctrl |= 1 << 6;
  asm volatile ("mcr p15, 0, %0, c1, c0,  1" :: "r" (auxctrl));
  asm volatile ("mrc p15, 0, %0, c1, c0,  1" : "=r" (auxctrl));
  printf("auxctrl = %08x\r\n", auxctrl);

  // set domain 0 to client
  asm volatile ("mcr p15, 0, %0, c3, c0, 0" :: "r" (1));

  // always use TTBR0
  asm volatile ("mcr p15, 0, %0, c2, c0, 2" :: "r" (0));

  unsigned ttbcr;
  asm volatile ("mrc p15, 0, %0, c2, c0, 2" : "=r" (ttbcr));
  printf("ttbcr   = %08x\r\n", ttbcr);

#ifdef RPI2
  // set TTBR0 - page table walk memory cacheability/shareable
  // [Bit 0, Bit 6] indicates inner cachability: 01 = normal memory, inner write-back write-allocate cacheable
  // [Bit 4, Bit 3] indicates outer cachability: 01 = normal memory, outer write-back write-allocate cacheable
  // Bit 1 indicates sharable 
  asm volatile ("mcr p15, 0, %0, c2, c0, 0" :: "r" (0x4a | (unsigned) &PageTable));
#else
  // set TTBR0 (page table walk inner cacheable, outer non-cacheable, shareable memory)
  asm volatile ("mcr p15, 0, %0, c2, c0, 0" :: "r" (0x03 | (unsigned) &PageTable));
#endif
  unsigned ttbr0;
  asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r" (ttbr0));
  printf("ttbr0   = %08x\r\n", ttbr0);

#ifdef RPI2
  asm volatile ("isb" ::: "memory");
#else
  // invalidate data cache and flush prefetch buffer
  // NOTE: The below code seems to cause a Pi 2 to crash
  asm volatile ("mcr p15, 0, %0, c7, c5,  4" :: "r" (0) : "memory");
  asm volatile ("mcr p15, 0, %0, c7, c6,  0" :: "r" (0) : "memory");
#endif

  // enable MMU, L1 cache and instruction cache, L2 cache, write buffer,
  //   branch prediction and extended page table on
  unsigned sctrl;
  asm volatile ("mrc p15,0,%0,c1,c0,0" : "=r" (sctrl));
  // Bit 12 enables the L1 instruction cache
  // Bit 11 enables branch pre-fetching
  // Bit  2 enables the L1 data cache
  // Bit  0 enabled the MMU  
  // The L1 instruction cache can be used independently of the MMU
  // The L1 data cache will one be enabled if the MMU is enabled
  sctrl |= 0x00001805;
  asm volatile ("mcr p15,0,%0,c1,c0,0" :: "r" (sctrl) : "memory");
  asm volatile ("mrc p15,0,%0,c1,c0,0" : "=r" (sctrl));
  printf("sctrl   = %08x\r\n", sctrl);
}
