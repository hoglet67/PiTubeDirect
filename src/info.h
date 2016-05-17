// info.h

#ifndef INFO_H
#define INFO_H

#include "rpi-mailbox-interface.h"

typedef struct {
   int rate;
   int min_rate;
   int max_rate;
} clock_info_t;

extern int get_clock_rate(int clk_id);

extern clock_info_t *get_clock_rates(int clk_id);

extern void dump_useful_info();

#endif
