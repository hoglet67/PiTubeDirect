// tube.h

#ifndef TUBE_H
#define TUBE_H

#include <inttypes.h>

extern volatile unsigned int copro;

extern volatile unsigned int copro_speed;

extern int arm_speed;

extern void arm_fiq_handler_flag1();

#endif
