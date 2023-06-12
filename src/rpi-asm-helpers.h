#ifndef RPI_ASM_HELPERS_H
#define RPI_ASM_HELPERS_H

void _enable_interrupts( void );
void _set_interrupts( unsigned int cpsr );
void  _disable_interrupts( void );
unsigned int _disable_interrupts_cspr(void);
unsigned int _get_cpsr();
unsigned int _get_stack_pointer();
void _invalidate_icache();
void _data_memory_barrier();
void _data_synchronization_barrier();
void _invalidate_tlb_mva(void *address);
unsigned int _get_core();
#endif
