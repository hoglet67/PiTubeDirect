#ifndef _MODULE_H
#define _MODULE_H

#include "tube-swi.h"

typedef struct module {
   const char * name;
   unsigned int swi_num_min;
   unsigned int swi_num_max;
   void (*init) (int vdu);
   SWIDescriptor_Type *swi_table;
} module_t;

#endif
