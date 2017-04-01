// tube.h

#ifndef TUBE_H
#define TUBE_H

#include <inttypes.h>
#include "tube-defs.h"

extern volatile unsigned int copro;

extern volatile unsigned int copro_speed;

extern volatile unsigned int copro_memory_size;

extern int arm_speed;

extern void arm_fiq_handler_flag1();

extern volatile int tube_irq;

// For Pi Direct we can just execute cycles until and event 

#define tubeContinueRunning() (!(tube_irq & (RESET_BIT | NMI_BIT | IRQ_BIT)))

#define tubeUseCycles(n)

// In B-Em use the following 
//
//#define tubeContinueRunning() (tube_cycles)
//
//#define tubeUseCycles(n)  tube_cycles -= n
//
//}


#endif
