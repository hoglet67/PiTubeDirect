// info.h

#ifndef INFO_H
#define INFO_H

#include "rpi-mailbox-interface.h"

typedef struct {
   uint32_t rate;
   uint32_t min_rate;
   uint32_t max_rate;
} clock_info_t;

/* Cached on boot, so this is safe to call at any time */
extern void init_info();

/* Cached on boot, so this is safe to call at any time */
extern uint32_t get_speed();

/* Cached on boot, so this is safe to call at any time */
extern char *get_info_string();

extern uint32_t get_clock_rate(int clk_id);

uint32_t get_revision();

#define    COMPONENT_CORE 1
#define COMPONENT_SDRAM_C 2
#define COMPONENT_SDRAM_P 3
#define COMPONENT_SDRAM_I 4

extern clock_info_t *get_clock_rates(int clk_id);

extern void dump_useful_info();

/* Cached on boot, so this is safe to call at any time */
extern char *get_cmdline_prop(char *prop);

#endif
