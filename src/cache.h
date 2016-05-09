// cache.h

#ifndef CACHE_H
#define CACHE_H

// Mark the memory above 64MB as uncachable 
#define UNCACHED_MEM_BASE 0x04000000

#ifndef __ASSEMBLER__

void enable_MMU_and_IDCaches(void);

#endif

#endif
