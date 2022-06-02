/*
Copyright (c) 2013, Jurriaan Bremer
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
* Neither the name of the darm developer(s) nor the names of its
  contributors may be used to endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef __ARMV7_TBL__
#define __ARMV7_TBL__
#include "darm-tbl.h"
extern const darm_enctype_t armv7_instr_types[256];
extern const darm_enctype_t thumb2_instr_types[256];
extern const darm_instr_t armv7_instr_labels[256];
extern const darm_instr_t type_shift_instr_lookup[16];
extern const darm_instr_t type_brnchmisc_instr_lookup[16];
extern const darm_instr_t type_opless_instr_lookup[8];
extern const darm_instr_t type_uncond2_instr_lookup[8];
extern const darm_instr_t type_mul_instr_lookup[8];
extern const darm_instr_t type_stack0_instr_lookup[32];
extern const darm_instr_t type_stack1_instr_lookup[8];
extern const darm_instr_t type_stack2_instr_lookup[8];
extern const darm_instr_t type_bits_instr_lookup[4];
extern const darm_instr_t type_pas_instr_lookup[64];
extern const darm_instr_t type_sat_instr_lookup[4];
extern const darm_instr_t type_sync_instr_lookup[16];
extern const darm_instr_t type_pusr_instr_lookup[16];
extern const char *armv7_format_strings[479][3];
#endif
