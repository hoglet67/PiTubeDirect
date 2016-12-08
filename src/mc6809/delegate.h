/*

Delegates in C
Copyright 2014-2015, Ciaran Anscomb

This is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

*/

#ifndef DELEGATE_H__oeW3udTBnzIVk
#define DELEGATE_H__oeW3udTBnzIVk

#include <stdint.h>

/* Underlying struct def for delegates. */

#define DELEGATE_S0(T) struct { T (*func)(void *); void *sptr; }
#define DELEGATE_S1(T,T0) struct { T (*func)(void *, T0); void *sptr; }
#define DELEGATE_S2(T,T0,T1) struct { T (*func)(void *, T0, T1); void *sptr; }
#define DELEGATE_S3(T,T0,T1,T2) struct { T (*func)(void *, T0, T1, T2); void *sptr; }

/* Type name for delegates. */

#define DELEGATE_T0(N) delegate_##N
#define DELEGATE_T1(N,N0) delegate_##N##_##N0
#define DELEGATE_T2(N,N0,N1) delegate_##N##_##N0##_##N1
#define DELEGATE_T3(N,N0,N1,N2) delegate_##N##_##N0##_##N1##_##N2

/* Define a set of delegate types. */

typedef DELEGATE_S0(void) DELEGATE_T0(void);
typedef DELEGATE_S1(void, _Bool) DELEGATE_T1(void, bool);
typedef DELEGATE_S2(void, _Bool, uint16_t) DELEGATE_T2(void, bool, uint16);
typedef DELEGATE_S1(void, int) DELEGATE_T1(void, int);
typedef DELEGATE_S2(void, int, uint8_t *) DELEGATE_T2(void, int, uint8p);
typedef DELEGATE_S2(void, int, uint16_t *) DELEGATE_T2(void, int, uint16p);
typedef DELEGATE_S1(void, unsigned) DELEGATE_T1(void, unsigned);
typedef DELEGATE_S3(void, unsigned, unsigned, unsigned) DELEGATE_T3(void, unsigned, unsigned, unsigned);
typedef DELEGATE_S1(void, uint8_t) DELEGATE_T1(void, uint8);
typedef DELEGATE_S2(void, uint16_t, uint8_t) DELEGATE_T2(void, uint16, uint8);
typedef DELEGATE_S1(void, float) DELEGATE_T1(void, float);
typedef DELEGATE_S3(void, int, _Bool, uint16_t) DELEGATE_T3(void, int, bool, uint16);
typedef DELEGATE_S0(unsigned) DELEGATE_T0(unsigned);
typedef DELEGATE_S0(uint8_t) DELEGATE_T0(uint8);
typedef DELEGATE_S1(uint8_t, uint16_t) DELEGATE_T1(uint8, uint16);
typedef DELEGATE_S0(uint8_t *) DELEGATE_T0(uint8p);
typedef DELEGATE_S2(void, uint8_t *, unsigned) DELEGATE_T2(void, uint8p, unsigned);

/* Convenience function for declaring anonymous structs. */

#define DELEGATE_INIT(f,s) {f,s}

#define DELEGATE_AS0(N,f,s) (DELEGATE_T0(N))DELEGATE_INIT(f,s)
#define DELEGATE_AS1(N,N0,f,s) (DELEGATE_T1(N,N0))DELEGATE_INIT(f,s)
#define DELEGATE_AS2(N,N0,N1,f,s) (DELEGATE_T2(N,N0,N1))DELEGATE_INIT(f,s)
#define DELEGATE_AS3(N,N0,N1,N2,f,s) (DELEGATE_T3(N,N0,N1,N2))DELEGATE_INIT(f,s)

/* Delegate default function names. */

#define DELEGATE_DEFAULT_F0(N) delegate_##N##_default
#define DELEGATE_DEFAULT_F1(N,N0) delegate_##N##_default_##N0
#define DELEGATE_DEFAULT_F2(N,N0,N1) delegate_##N##_default_##N0##_##N1
#define DELEGATE_DEFAULT_F3(N,N0,N1,N2) delegate_##N##_default_##N0##_##N1##_##N2

/* Default no-op functions for defined delegate types. */

#define DELEGATE_DEF_PROTO0(T,N) T DELEGATE_DEFAULT_F0(N)(void *)
#define DELEGATE_DEF_PROTO1(T,N,T0,N0) T DELEGATE_DEFAULT_F1(N,N0)(void *, T0)
#define DELEGATE_DEF_PROTO2(T,N,T0,N0,T1,N1) T DELEGATE_DEFAULT_F2(N,N0,N1)(void *, T0, T1)
#define DELEGATE_DEF_PROTO3(T,N,T0,N0,T1,N1,T2,N2) T DELEGATE_DEFAULT_F3(N,N0,N1,N2)(void *, T0, T1, T2)

#define DELEGATE_DEF_FUNC0(T,N,R) T DELEGATE_DEFAULT_F0(N)(void *sptr) { (void)sptr; return R; }
#define DELEGATE_DEF_FUNC1(T,N,T0,N0,R) T DELEGATE_DEFAULT_F1(N,N0)(void *sptr, T0 v0) { (void)sptr; (void)v0; return R; }
#define DELEGATE_DEF_FUNC2(T,N,T0,N0,T1,N1,R) T DELEGATE_DEFAULT_F2(N,N0,N1)(void *sptr, T0 v0, T1 v1) { (void)sptr; (void)v0; (void)v1; return R; }
#define DELEGATE_DEF_FUNC3(T,N,T0,N0,T1,N1,T2,N2,R) T DELEGATE_DEFAULT_F3(N,N0,N1,N2)(void *sptr, T0 v0, T1 v1, T2 v2) { (void)sptr; (void)v0; (void)v1; (void)v2; return R; }

DELEGATE_DEF_PROTO0(void, void);
DELEGATE_DEF_PROTO1(void, void, _Bool, bool);
DELEGATE_DEF_PROTO2(void, void, _Bool, bool, uint16_t, uint16);
DELEGATE_DEF_PROTO1(void, void, int, int);
DELEGATE_DEF_PROTO2(void, void, int, int, uint8_t *, uint8p);
DELEGATE_DEF_PROTO2(void, void, int, int, uint16_t *, uint16p);
DELEGATE_DEF_PROTO3(void, void, int, int, _Bool, bool, uint16_t, uint16);
DELEGATE_DEF_PROTO1(void, void, unsigned, unsigned);
DELEGATE_DEF_PROTO3(void, void, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
DELEGATE_DEF_PROTO1(void, void, uint8_t, uint8);
DELEGATE_DEF_PROTO2(void, void, uint16_t, uint16, uint8_t, uint8);
DELEGATE_DEF_PROTO1(void, void, float, float);
DELEGATE_DEF_PROTO0(unsigned, unsigned);
DELEGATE_DEF_PROTO0(uint8_t, uint8);
DELEGATE_DEF_PROTO1(uint8_t, uint8, uint16_t, uint16);
DELEGATE_DEF_PROTO0(uint8_t *, uint8p);
DELEGATE_DEF_PROTO2(void, void, uint8_t *, uint8p, unsigned, unsigned);

#define DELEGATE_DEFAULT0(N) DELEGATE_AS0(N, DELEGATE_DEFAULT_F0(N), NULL)
#define DELEGATE_DEFAULT1(N,N0) DELEGATE_AS1(N, N0, DELEGATE_DEFAULT_F1(N, N0), NULL)
#define DELEGATE_DEFAULT2(N,N0,N1) DELEGATE_AS2(N, N0, N1, DELEGATE_DEFAULT_F2(N, N0, N1), NULL)
#define DELEGATE_DEFAULT3(N,N0,N1,N2) DELEGATE_AS3(N, N0, N1, N2, DELEGATE_DEFAULT_F3(N, N0, N1, N2), NULL)

/* Calling interface. */

#define DELEGATE_CALL0(d) ((d).func((d).sptr))
#define DELEGATE_CALL1(d,v0) ((d).func((d).sptr,(v0)))
#define DELEGATE_CALL2(d,v0,v1) ((d).func((d).sptr,(v0),(v1)))
#define DELEGATE_CALL3(d,v0,v1,v2) ((d).func((d).sptr,(v0),(v1),(v2)))
#define DELEGATE_SAFE_CALL0(d) do { if ((d).func) { DELEGATE_CALL0((d)); } } while (0)
#define DELEGATE_SAFE_CALL1(d,v0) do { if ((d).func) { DELEGATE_CALL1((d),(v0)); } } while (0)
#define DELEGATE_SAFE_CALL2(d,v0,v1) do { if ((d).func) { DELEGATE_CALL2((d),(v0),(v1)); } } while (0)
#define DELEGATE_SAFE_CALL3(d,v0,v1,v2) do { if ((d).func) { DELEGATE_CALL3((d),(v0),(v1),(v2)); } } while (0)

#endif  /* DELEGATE_H__oeW3udTBnzIVk */
