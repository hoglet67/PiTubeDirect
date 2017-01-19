/*

Memory allocation with checking
Copyright 2014-2015, Ciaran Anscomb

This is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xalloc.h"

void *xmalloc(size_t s) {
	void *mem = malloc(s);
	if (!mem) {
		perror(NULL);
    printf("xmalloc failed\r\n");
		//exit(EXIT_FAILURE);
	}
	return mem;
}

void *xzalloc(size_t s) {
	void *mem = xmalloc(s);
	memset(mem, 0, s);
	return mem;
}

void *xrealloc(void *p, size_t s) {
	void *mem = realloc(p, s);
	if (!mem && s != 0) {
		perror(NULL);
    printf("xmalloc failed\r\n");
		// exit(EXIT_FAILURE);
	}
	return mem;
}

void *xmemdup(const void *p, size_t s) {
	if (!p)
		return NULL;
	void *mem = xmalloc(s);
	memcpy(mem, p, s);
	return mem;
}

char *xstrdup(const char *str) {
	return xmemdup(str, strlen(str) + 1);
}
