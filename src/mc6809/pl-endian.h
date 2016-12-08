/*

Missing endian.h
Copyright 2014-2015, Ciaran Anscomb

This is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

If endian.h was not found by configure, it should have performed a test
or made an assumption, and HAVE_BIG_ENDIAN will be defined accordingly.

*/

#ifndef PL_ENDIAN_H__ubnPKXx9bODus
#define PL_ENDIAN_H__ubnPKXx9bODus

#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN    4321
#endif
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif

#ifndef __BYTE_ORDER
#ifdef HAVE_BIG_ENDIAN
#define __BYTE_ORDER __BIG_ENDIAN
#else
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif
#endif

#endif  /* PL_ENDIAN_H__ubnPKXx9bODus */
