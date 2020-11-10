// utils.h

#ifndef UTILS_H
#define UTILS_H

int get_elk_mode();

void check_elk_mode_and_patch(unsigned char *rom, int start, int len, int expected);

#endif
