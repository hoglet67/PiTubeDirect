// cache.h

#ifndef CACHE_H
#define CACHE_H

// Memory below 64MB is L1 cached only (inner)

// Mark the memory above 64MB to 128MB as L2 cached only (outer)
#define L2_CACHED_MEM_BASE 0x04000000

// Mark the memory above 128MB as uncachable 
#define UNCACHED_MEM_BASE 0x08000000

#ifndef __ASSEMBLER__

void enable_MMU_and_IDCaches(void);

#endif

#endif
