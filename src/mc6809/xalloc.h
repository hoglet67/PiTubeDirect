/*

Memory allocation with checking
Copyright 2014-2015, Ciaran Anscomb

This is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

A small set of convenience functions that wrap standard system calls and
provide out of memory checking.  See Gnulib for a far more complete set.

*/

#ifndef XALLOC_H__yqbm4hifLvT2k
#define XALLOC_H__yqbm4hifLvT2k

#include <stddef.h>

void *xmalloc(size_t s);
void *xzalloc(size_t s);
void *xrealloc(void *p, size_t s);

void *xmemdup(const void *p, size_t s);
char *xstrdup(const char *str);

#endif  /* XALLOC_H__yqbm4hifLvT2k */
