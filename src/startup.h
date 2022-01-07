// startup.h

#ifndef STARTUP_H
#define STARTUP_H

/* Found in the *start.S file, implemented in assembler */

extern unsigned int _software_interrupt_vector_h;
extern unsigned int _fast_interrupt_vector_h;

extern void _enable_interrupts( void );

extern void _set_interrupts( int cpsr );

extern int _disable_interrupts( void );

extern unsigned int _get_cpsr();

extern unsigned int _get_stack_pointer();

extern void _enable_unaligned_access();

extern void _enable_l1_cache();

extern void _invalidate_icache();

extern void _invalidate_dcache();

extern void _clean_invalidate_dcache();

extern void _invalidate_dcache_mva(void *address);

extern void _clean_invalidate_dcache_mva(void *address);

extern void _invalidate_tlb_mva(void *address);

extern void _data_memory_barrier();

extern void _data_synchronization_barrier();

extern unsigned int _get_core();

extern void _init_core();

extern void _spin_core();

extern void _fast_scroll(void *dst, void *src, int num_bytes);

extern void _fast_clear(void *dst, unsigned int val, int num_bytes);

extern void _main_irq_handler();

#endif
