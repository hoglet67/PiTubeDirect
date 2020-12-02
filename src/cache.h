// cache.h

#ifndef CACHE_H
#define CACHE_H

// All memory upto 232MB is now cached
#define L2_CACHED_MEM_BASE 0x0E800000
#define UNCACHED_MEM_BASE  0x0E800000

// The first 2MB of memory is mapped at 4K pages so the 6502 Co Pro
// can play tricks with banks selection
#define NUM_4K_PAGES 512

#ifndef __ASSEMBLER__

void map_4k_page(int logical, int physical);

void enable_MMU_and_IDCaches(void);

void _clean_cache_area(void * start, unsigned int length);

#endif

#endif
