// pdp11.h

#ifndef PDP11_H
#define PDP11_H

#include <inttypes.h>

void pdp11_reset(uint16_t address);

void pdp11_execute();

void pdp11_interrupt(uint8_t vec, uint8_t pri);

#endif
