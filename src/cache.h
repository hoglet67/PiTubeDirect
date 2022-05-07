// cache.h

#ifndef CACHE_H
#define CACHE_H

// All memory up to 232MB is now cached
#define L2_CACHED_MEM_BASE 0x0E800000
#define UNCACHED_MEM_BASE  0x0E800000

// The first 2MB of memory is mapped at 4K pages so the 6502 Co Pro
// can play tricks with banks selection
#define NUM_4K_PAGES 512

void map_4k_page(unsigned int logical, unsigned int physical);
void map_4k_pageJIT(unsigned int logical, unsigned int physical);

void enable_MMU_and_IDCaches(void);

void _clean_invalidate_dcache_area(void * start, unsigned int length);
void _invalidate_cache_area(void * start, unsigned int length);
void CleanDataCache (void);

#endif
