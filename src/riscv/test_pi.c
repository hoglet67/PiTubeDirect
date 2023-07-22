#include <stdio.h>
#include <stdlib.h>

#include "tuberom_pi.c"

int main(int argc, char **argv) {

   int n = atoi(argv[1]);

   int *r = (int *) calloc(n * 7 / 2 + 1, sizeof(int));

   pi_calc(n, r);

}
