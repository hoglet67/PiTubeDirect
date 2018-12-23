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
#include <stdio.h>
#include <stdint.h>
#include "armv7-tbl.h"
darm_enctype_t armv7_instr_types[] = {
    T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT,  //    0-3 
    T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT,  //    4-7
    T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT,  //    8-0xB
    T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT,  //  0xC-0xF
    T_ARM_SM, T_ARM_CMP_OP, T_ARM_BRNCHMISC, T_ARM_CMP_OP,                       // 0x10-0x13
    T_ARM_SM, T_ARM_CMP_OP, T_ARM_MISC, T_ARM_CMP_OP,                            // 0x14-0x17 
    T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT, T_ARM_DST_SRC, T_ARM_DST_SRC,          // 0x18-0x1B
    T_ARM_ARITH_SHIFT, T_ARM_ARITH_SHIFT, T_ARM_MISC, T_ARM_MISC,                // 0x1C-0x1F
    T_ARM_ARITH_IMM, T_ARM_ARITH_IMM, T_ARM_ARITH_IMM, T_ARM_ARITH_IMM,          // 0x20-0x23
    T_ARM_ARITH_IMM, T_ARM_ARITH_IMM, T_ARM_ARITH_IMM, T_ARM_ARITH_IMM,          // 0x24-0x27
    T_ARM_ARITH_IMM, T_ARM_ARITH_IMM, T_ARM_ARITH_IMM, T_ARM_ARITH_IMM,          // 0x28-0x2B
    T_ARM_ARITH_IMM, T_ARM_ARITH_IMM, T_ARM_ARITH_IMM, T_ARM_ARITH_IMM,          // 0x2C-0x2F
    T_ARM_MOV_IMM, T_ARM_CMP_IMM, T_ARM_OPLESS, T_ARM_CMP_IMM,                   // 0x30-0x33
    T_ARM_MOV_IMM, T_ARM_CMP_IMM, T_INVLD, T_ARM_CMP_IMM,                        // 0x34-0x37
    T_ARM_ARITH_IMM, T_ARM_ARITH_IMM, T_ARM_MOV_IMM, T_ARM_MOV_IMM,              // 0x38-0x3B
    T_ARM_ARITH_IMM, T_ARM_ARITH_IMM, T_ARM_MOV_IMM, T_ARM_MOV_IMM,              // 0x3C-0x3F   
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x40-0x43   
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x44-0x47
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x48-0x4B
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x4C-0x4F
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x50-0x53
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x54-0x57
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x58-0x5b
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x5C-0x5f
    T_INVLD, T_ARM_PAS, T_ARM_PAS, T_ARM_PAS,                                    // 0x60-0x63
    T_INVLD, T_ARM_PAS, T_ARM_PAS, T_ARM_PAS,                                    // 0x64-0x67
    T_ARM_MISC, T_INVLD, T_INVLD, T_ARM_BITREV,                                  // 0x68-0x6B   
    T_INVLD, T_INVLD, T_INVLD, T_ARM_BITREV,                                     // 0x6c-0x6f   
    T_ARM_SM, T_INVLD, T_INVLD, T_INVLD,                                         // 0x70-0x73
    T_ARM_SM, T_ARM_SM, T_INVLD, T_INVLD,                                        // 0x74-0x77
    T_INVLD, T_INVLD, T_ARM_BITS, T_ARM_BITS,                                    // 0x78-0x7b
    T_ARM_BITS, T_ARM_BITS, T_ARM_BITS, T_ARM_UDF,                               // 0x7C-0x7f
    T_ARM_LDSTREGS, T_ARM_LDSTREGS, T_ARM_LDSTREGS, T_ARM_LDSTREGS,              // 0x80-0x83
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x84-0x87
    T_ARM_LDSTREGS, T_ARM_LDSTREGS, T_ARM_LDSTREGS, T_ARM_LDSTREGS,              // 0x88-0x8B
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x8c-0x8f
    T_ARM_LDSTREGS, T_ARM_LDSTREGS, T_ARM_LDSTREGS, T_ARM_LDSTREGS,              // 0x90-0x93
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x94-0x97
    T_ARM_LDSTREGS, T_ARM_LDSTREGS, T_ARM_LDSTREGS, T_ARM_LDSTREGS,              // 0x98-0x9b
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0x9C-0x9f
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC,                  // 0xA0-0xA3
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC,                  // 0xA4-0xA7
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC,                  // 0xA8-0xAB
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC,                  // 0xAc-0xAf
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC,                  // 0xB0-0xB3
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC,                  // 0xB4-0xB7
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC,                  // 0xB8-0xBb
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC,                  // 0xBC-0xBf
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0xC0-0xC3
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0xC4-0xC7
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0xC8-0xCB
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0xCc-0xCf
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0xD0-0xD3
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0xD4-0xD7
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0xD8-0xDb
    T_INVLD, T_INVLD, T_INVLD, T_INVLD,                                          // 0xDC-0xDf
    T_ARM_MVCR, T_ARM_MVCR, T_ARM_MVCR, T_ARM_MVCR,                              // 0xE0-0xE3
    T_ARM_MVCR, T_ARM_MVCR, T_ARM_MVCR, T_ARM_MVCR,                              // 0xE4-0xE7
    T_ARM_MVCR, T_ARM_MVCR, T_ARM_MVCR, T_ARM_MVCR,                              // 0xE8-0xEB
    T_ARM_MVCR, T_ARM_MVCR, T_ARM_MVCR, T_ARM_MVCR,                              // 0xEc-0xEf
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC,                  // 0xF0-0xF3
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC,                  // 0xF4-0xF7
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC,                  // 0xF8-0xFb
    T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC, T_ARM_BRNCHSC                   // 0xFC-0xFf
};

darm_instr_t armv7_instr_labels[] = {                                            
    I_AND, I_AND, I_EOR, I_EOR,           //    0-3 
    I_SUB, I_SUB, I_RSB, I_RSB,           //    4-7
    I_ADD, I_ADD, I_ADC, I_ADC,           //    8-0xB
    I_SBC, I_SBC, I_RSC, I_RSC,           //  0xC-0xF
    I_SMLA, I_TST, I_SMULW, I_TEQ,        // 0x10-0x13
    I_SMLAL, I_CMP, I_SMC, I_CMN,         // 0x14-0x17 
    I_ORR, I_ORR, I_STREXD, I_RRX,        // 0x18-0x1B
    I_BIC, I_BIC, I_MVN, I_MVN,           // 0x1C-0x1F
    I_AND, I_AND, I_EOR, I_EOR,           // 0x20-0x23
    I_SUB, I_SUB, I_RSB, I_RSB,           // 0x24-0x27
    I_ADD, I_ADD, I_ADC, I_ADC,           // 0x28-0x2B
    I_SBC, I_SBC, I_RSC, I_RSC,           // 0x2C-0x2F
    I_MOVW, I_TST, I_YIELD, I_TEQ,        // 0x30-0x33
    I_MOVT, I_CMP, I_INVLD, I_CMN,        // 0x34-0x37
    I_ORR, I_ORR, I_MOV, I_MOV,           // 0x38-0x3B
    I_BIC, I_BIC, I_MVN, I_MVN,           // 0x3C-0x3F 
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x40-0x43 
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x44-0x47
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x48-0x4B
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x4C-0x4F
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x50-0x53
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x54-0x57
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x58-0x5b
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x5C-0x5f
    I_INVLD, I_SSUB8, I_QSUB8, I_SHSUB8,  // 0x60-0x63
    I_INVLD, I_USUB8, I_UQSUB8, I_UHSUB8, // 0x64-0x67
    I_SEL, I_INVLD, I_INVLD, I_REV16,     // 0x68-0x6B 
    I_INVLD, I_INVLD, I_INVLD, I_REVSH,   // 0x6c-0x6f 
    I_SMUSD, I_INVLD, I_INVLD, I_INVLD,   // 0x70-0x73
    I_SMLSLD, I_SMMUL, I_INVLD, I_INVLD,  // 0x74-0x77
    I_INVLD, I_INVLD, I_SBFX, I_SBFX,     // 0x78-0x7b
    I_BFI, I_BFI, I_UBFX, I_UDF,          // 0x7C-0x7f
    I_STMDA, I_LDMDA, I_STMDA, I_LDMDA,   // 0x80-0x83
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x84-0x87
    I_STM, I_LDM, I_STM, I_LDM,           // 0x88-0x8B
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x8c-0x8f
    I_STMDB, I_LDMDB, I_STMDB, I_LDMDB,   // 0x90-0x93
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x94-0x97
    I_STMIB, I_LDMIB, I_STMIB, I_LDMIB,   // 0x98-0x9b
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0x9C-0x9f
    I_B, I_B, I_B, I_B,                   // 0xA0-0xA3
    I_B, I_B, I_B, I_B,                   // 0xA4-0xA7
    I_B, I_B, I_B, I_B,                   // 0xA8-0xAB
    I_B, I_B, I_B, I_B,                   // 0xAc-0xAf
    I_BL, I_BL, I_BL, I_BL,               // 0xB0-0xB3
    I_BL, I_BL, I_BL, I_BL,               // 0xB4-0xB7
    I_BL, I_BL, I_BL, I_BL,               // 0xB8-0xBb
    I_BL, I_BL, I_BL, I_BL,               // 0xBC-0xBf
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0xC0-0xC3
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0xC4-0xC7
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0xC8-0xCB
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0xCc-0xCf
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0xD0-0xD3
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0xD4-0xD7
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0xD8-0xDb
    I_INVLD, I_INVLD, I_INVLD, I_INVLD,   // 0xDC-0xDf
    I_MCR, I_MRC, I_MCR, I_MRC,           // 0xE0-0xE3
    I_MCR, I_MRC, I_MCR, I_MRC,           // 0xE4-0xE7
    I_MCR, I_MRC, I_MCR, I_MRC,           // 0xE8-0xEB
    I_MCR, I_MRC, I_MCR, I_MRC,           // 0xEc-0xEf
    I_SVC, I_SVC, I_SVC, I_SVC,           // 0xF0-0xF3
    I_SVC, I_SVC, I_SVC, I_SVC,           // 0xF4-0xF7
    I_SVC, I_SVC, I_SVC, I_SVC,           // 0xF8-0xFb
    I_SVC, I_SVC, I_SVC, I_SVC            // 0xFC-0xFf
};

darm_instr_t type_shift_instr_lookup[] = {
    I_LSL, I_LSL, I_LSR, I_LSR, I_ASR, I_ASR, I_ROR, I_ROR, I_LSL, I_INVLD,
    I_LSR, I_INVLD, I_ASR, I_INVLD, I_ROR, I_INVLD
};

darm_instr_t type_brnchmisc_instr_lookup[] = {
    I_MSR, I_BX, I_BXJ, I_BLX, I_INVLD, I_QSUB, I_INVLD, I_BKPT, I_SMLAW,
    I_INVLD, I_SMULW, I_INVLD, I_SMLAW, I_INVLD, I_SMULW, I_INVLD
};

darm_instr_t type_opless_instr_lookup[] = {
    I_NOP, I_YIELD, I_WFE, I_WFI, I_SEV, I_INVLD, I_INVLD, I_INVLD
};

darm_instr_t type_uncond2_instr_lookup[] = {
    I_INVLD, I_CLREX, I_INVLD, I_INVLD, I_DSB, I_DMB, I_ISB, I_INVLD
};

darm_instr_t type_mul_instr_lookup[] = {
    I_MUL, I_MLA, I_UMAAL, I_MLS, I_UMULL, I_UMLAL, I_SMULL, I_SMLAL
};

darm_instr_t type_stack0_instr_lookup[] = {
    I_STR, I_LDR, I_STRT, I_LDRT, I_STRB, I_LDRB, I_STRBT, I_LDRBT, I_STR,
    I_LDR, I_STRT, I_LDRT, I_STRB, I_LDRB, I_STRBT, I_LDRBT, I_STR, I_LDR,
    I_STR, I_LDR, I_STRB, I_LDRB, I_STRB, I_LDRB, I_STR, I_LDR, I_STR, I_LDR,
    I_STRB, I_LDRB, I_STRB, I_LDRB
};

darm_instr_t type_stack1_instr_lookup[] = {
    I_INVLD, I_INVLD, I_STRHT, I_LDRHT, I_INVLD, I_LDRSBT, I_INVLD, I_LDRSHT
};

darm_instr_t type_stack2_instr_lookup[] = {
    I_INVLD, I_INVLD, I_STRH, I_LDRH, I_LDRD, I_LDRSB, I_STRD, I_LDRSH
};

darm_instr_t type_bits_instr_lookup[] = {
    I_INVLD, I_SBFX, I_BFI, I_UBFX
};

darm_instr_t type_pas_instr_lookup[] = {
    I_INVLD, I_INVLD, I_INVLD, I_INVLD, I_INVLD, I_INVLD, I_INVLD, I_INVLD,
    I_SADD16, I_SASX, I_SSAX, I_SSUB16, I_SADD8, I_INVLD, I_INVLD, I_SSUB8,
    I_QADD16, I_QASX, I_QSAX, I_QSUB16, I_QADD8, I_INVLD, I_INVLD, I_QSUB8,
    I_SHADD16, I_SHASX, I_SHSAX, I_SHSUB16, I_SHADD8, I_INVLD, I_INVLD,
    I_SHSUB8, I_INVLD, I_INVLD, I_INVLD, I_INVLD, I_INVLD, I_INVLD, I_INVLD,
    I_INVLD, I_UADD16, I_UASX, I_USAX, I_USUB16, I_UADD8, I_INVLD, I_INVLD,
    I_USUB8, I_UQADD16, I_UQASX, I_UQSAX, I_UQSUB16, I_UQADD8, I_INVLD,
    I_INVLD, I_UQSUB8, I_UHADD16, I_UHASX, I_UHSAX, I_UHSUB16, I_UHADD8,
    I_INVLD, I_INVLD, I_UHSUB8
};

darm_instr_t type_sat_instr_lookup[] = {
    I_QADD, I_QSUB, I_QDADD, I_QDSUB
};

darm_instr_t type_sync_instr_lookup[] = {
    I_SWP, I_INVLD, I_INVLD, I_INVLD, I_SWPB, I_INVLD, I_INVLD, I_INVLD,
    I_STREX, I_LDREX, I_STREXD, I_LDREXD, I_STREXB, I_LDREXB, I_STREXH,
    I_LDREXH
};

darm_instr_t type_pusr_instr_lookup[] = {
    I_SXTAB16, I_SXTB16, I_INVLD, I_INVLD, I_SXTAB, I_SXTB, I_SXTAH, I_SXTH,
    I_UXTAB16, I_UXTB16, I_INVLD, I_INVLD, I_UXTAB, I_UXTB, I_UXTAH, I_UXTH
};

const char *armv7_format_strings[479][3] = {
    [I_ADC] = {"scdnmS", "scdni"},
    [I_ADD] = {"scdnmS", "scdni"},
    [I_ADR] = {"cdb"},
    [I_AND] = {"scdnmS", "scdni"},
    [I_ASR] = {"scdnm", "scdmS"},
    [I_BFC] = {"cdLw"},
    [I_BFI] = {"cdnLw"},
    [I_BIC] = {"scdnmS", "scdni"},
    [I_BKPT] = {"i"},
    [I_BLX] = {"b", "cm"},
    [I_BL] = {"cb"},
    [I_BXJ] = {"cm"},
    [I_BX] = {"cm"},
    [I_B] = {"cb"},
    [I_CDP2] = {"cCpINJ<opc2>"},
    [I_CDP] = {"cCpINJ<opc2>"},
    [I_CLREX] = {""},
    [I_CLZ] = {"cdm"},
    [I_CMN] = {"cnmS", "cni"},
    [I_CMP] = {"cnmS", "cni"},
    [I_DBG] = {"co"},
    [I_DMB] = {"o"},
    [I_DSB] = {"o"},
    [I_EOR] = {"scdnmS", "scdni"},
    [I_ISB] = {"o"},
    [I_LDC2] = {"{L}cCIB#+/-<imm>"},
    [I_LDC] = {"{L}cCIB#+/-<imm>"},
    [I_LDMDA] = {"cn!r"},
    [I_LDMDB] = {"cn!r"},
    [I_LDMIB] = {"cn!r"},
    [I_LDM] = {"cn!r"},
    [I_LDRBT] = {"ctBOS"},
    [I_LDRB] = {"ctBOS"},
    [I_LDRD] = {"ct2BO"},
    [I_LDREXB] = {"ctB"},
    [I_LDREXD] = {"ct2B"},
    [I_LDREXH] = {"ctB"},
    [I_LDREX] = {"ctB"},
    [I_LDRHT] = {"ctBO"},
    [I_LDRH] = {"ctBO"},
    [I_LDRSBT] = {"ctBO"},
    [I_LDRSB] = {"ctBO"},
    [I_LDRSHT] = {"ctBO"},
    [I_LDRSH] = {"ctBO"},
    [I_LDRT] = {"ctBOS"},
    [I_LDR] = {"ctBOS"},
    [I_LSL] = {"scdnm", "scdmS"},
    [I_LSR] = {"scdnm", "scdmS"},
    [I_MCR2] = {"cCptNJP"},
    [I_MCRR2] = {"cCpt2J"},
    [I_MCRR] = {"cCpt2J"},
    [I_MCR] = {"cCptNJP"},
    [I_MLA] = {"scdnma"},
    [I_MLS] = {"cdnma"},
    [I_MOVT] = {"cdi"},
    [I_MOVW] = {"cdi"},
    [I_MOV] = {"scdi", "scdm"},
    [I_MRC2] = {"cCptNJP"},
    [I_MRC] = {"cCptNJP"},
    [I_MRRC2] = {"cC<opc>t2J"},
    [I_MRRC] = {"cC<opc>t2J"},
    [I_MRS] = {"cdz"},
    [I_MSR] = {"czn", "czi"},
    [I_MUL] = {"scdnm"},
    [I_MVN] = {"scdi", "scdmS"},
    [I_NOP] = {"c"},
    [I_ORR] = {"scdnmS", "scdni"},
    [I_PKH] = {"TcdnmS"},
    [I_PLD] = {"cM"},
    [I_PLI] = {"M"},
    [I_POP] = {"cr"},
    [I_PUSH] = {"cr"},
    [I_QADD16] = {"cdnm"},
    [I_QADD8] = {"cdnm"},
    [I_QADD] = {"cdmn"},
    [I_QASX] = {"cdnm"},
    [I_QDADD] = {"cdmn"},
    [I_QDSUB] = {"cdmn"},
    [I_QSAX] = {"cdnm"},
    [I_QSUB16] = {"cdnm"},
    [I_QSUB8] = {"cdnm"},
    [I_QSUB] = {"cdmn"},
    [I_RBIT] = {"cdm"},
    [I_REV16] = {"cdm"},
    [I_REVSH] = {"cdm"},
    [I_REV] = {"cdm"},
    [I_ROR] = {"scdnm", "scdmS"},
    [I_RRX] = {"scdm"},
    [I_RSB] = {"scdnmS", "scdni"},
    [I_RSC] = {"scdnmS", "scdni"},
    [I_SADD16] = {"cdnm"},
    [I_SADD8] = {"cdnm"},
    [I_SASX] = {"cdnm"},
    [I_SBC] = {"scdnmS", "scdni"},
    [I_SBFX] = {"cdnLw"},
    [I_SEL] = {"cdnm"},
    [I_SETEND] = {"e"},
    [I_SEV] = {"c"},
    [I_SHADD16] = {"cdnm"},
    [I_SHADD8] = {"cdnm"},
    [I_SHASX] = {"cdnm"},
    [I_SHSAX] = {"cdnm"},
    [I_SHSUB16] = {"cdnm"},
    [I_SHSUB8] = {"cdnm"},
    [I_SMC] = {"ci"},
    [I_SMLAD] = {"xcdnma"},
    [I_SMLALD] = {"xclhnm"},
    [I_SMLAL] = {"Xclhnm", "sclhnm"},
    [I_SMLAW] = {"<y>cdnma"},
    [I_SMLA] = {"Xcdnma"},
    [I_SMLSD] = {"xcdnma"},
    [I_SMLSLD] = {"xclhnm"},
    [I_SMMLA] = {"Rcdnma"},
    [I_SMMLS] = {"Rcdnma"},
    [I_SMMUL] = {"Rcdnm"},
    [I_SMUAD] = {"xcdnm"},
    [I_SMULL] = {"sclhnm"},
    [I_SMULW] = {"<y>cdnm"},
    [I_SMUL] = {"Xcdnm"},
    [I_SMUSD] = {"xcdnm"},
    [I_SSAT16] = {"cd#<imm>n"},
    [I_SSAT] = {"cd#<imm>nS"},
    [I_SSAX] = {"cdnm"},
    [I_SSUB16] = {"cdnm"},
    [I_SSUB8] = {"cdnm"},
    [I_STC2] = {"{L}cCIB#+/-<imm>"},
    [I_STC] = {"{L}cCIB#+/-<imm>"},
    [I_STMDA] = {"cn!r"},
    [I_STMDB] = {"cn!r"},
    [I_STMIB] = {"cn!r"},
    [I_STM] = {"cn!r"},
    [I_STRBT] = {"ctBOS"},
    [I_STRB] = {"ctBOS"},
    [I_STRD] = {"ct2BO"},
    [I_STREXB] = {"cdtB"},
    [I_STREXD] = {"cdt2B"},
    [I_STREXH] = {"cdtB"},
    [I_STREX] = {"cdtB"},
    [I_STRHT] = {"ctBO"},
    [I_STRH] = {"ctBO"},
    [I_STRT] = {"ctBOS"},
    [I_STR] = {"ctBOS"},
    [I_SUB] = {"scdnmS", "scdni"},
    [I_SVC] = {"ci"},
    [I_SWPB] = {"ct2B"},
    [I_SWP] = {"ct2B"},
    [I_SXTAB16] = {"cdnmA"},
    [I_SXTAB] = {"cdnmA"},
    [I_SXTAH] = {"cdnmA"},
    [I_SXTB16] = {"cdmA"},
    [I_SXTB] = {"cdmA"},
    [I_SXTH] = {"cdmA"},
    [I_TEQ] = {"cnmS", "cni"},
    [I_TST] = {"cnmS", "cni"},
    [I_UADD16] = {"cdnm"},
    [I_UADD8] = {"cdnm"},
    [I_UASX] = {"cdnm"},
    [I_UBFX] = {"cdnLw"},
    [I_UDF] = {"ci"},
    [I_UHADD16] = {"cdnm"},
    [I_UHADD8] = {"cdnm"},
    [I_UHASX] = {"cdnm"},
    [I_UHSAX] = {"cdnm"},
    [I_UHSUB16] = {"cdnm"},
    [I_UHSUB8] = {"cdnm"},
    [I_UMAAL] = {"clhnm"},
    [I_UMLAL] = {"sclhnm"},
    [I_UMULL] = {"sclhnm"},
    [I_UQADD16] = {"cdnm"},
    [I_UQADD8] = {"cdnm"},
    [I_UQASX] = {"cdnm"},
    [I_UQSAX] = {"cdnm"},
    [I_UQSUB16] = {"cdnm"},
    [I_UQSUB8] = {"cdnm"},
    [I_USAD8] = {"cdnm"},
    [I_USADA8] = {"cdnma"},
    [I_USAT16] = {"cdin"},
    [I_USAT] = {"cdinS"},
    [I_USAX] = {"cdnm"},
    [I_USUB16] = {"cdnm"},
    [I_USUB8] = {"cdnm"},
    [I_UXTAB16] = {"cdnmA"},
    [I_UXTAB] = {"cdnmA"},
    [I_UXTAH] = {"cdnmA"},
    [I_UXTB16] = {"cdmA"},
    [I_UXTB] = {"cdmA"},
    [I_UXTH] = {"cdmA"},
    [I_WFE] = {"c"},
    [I_WFI] = {"c"},
    [I_YIELD] = {"c"},
};
