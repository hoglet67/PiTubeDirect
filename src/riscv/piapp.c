#include <stdlib.h>

int pi_calc(int n, int *r);

int _start(char *params) {
   // Skip the filename
   while (*params && *params != ' ') {
      params++;
   }
   // Skip the space separator
   while (*params == ' ') {
      params++;
   }
   // Parse the parameter
   int ndigits = atoi(params);
   if (ndigits) {
      int *r = (int *) 0x100000;
      pi_calc(ndigits, r);
   }
   return 0;
}
#if 0
static int atoi(char *c) {
   int res = 0;
   while (*c >= '0' && *c <= '9') {
      res = res * 10 + (*c) - '0';
      c++;
   }
   return res;
}
#endif
