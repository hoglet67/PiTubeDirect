// tube.h

#ifndef TUBE_H
#define TUBE_H

#include <inttypes.h>
#include "tube-defs.h"

extern volatile unsigned int copro;

extern volatile unsigned int copro_speed;

extern int arm_speed;

extern void arm_fiq_handler_flag1();

extern volatile int tube_irq;

#define tubeContinueRunning() (!(tube_irq & (RESET_BIT | NMI_BIT | IRQ_BIT)))

#define tubeUseCycles(n)

// In B-Em use the following 
//
//#define tubeContinueRunning() (tube_cycles)
//
//#define tubeUseCycles(n)  tubecycles -= n
//
//}


#endif
