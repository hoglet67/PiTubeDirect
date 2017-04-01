// license:BSD-3-Clause
// copyright-holders:Bryan McPhail
#pragma once

#ifndef __ARM_H__
#define __ARM_H__

#define UINT8 unsigned char
#define UINT32 unsigned int
#define nullptr NULL

extern UINT8  copro_arm2_read8(int addr);
extern UINT32 copro_arm2_read32(int addr);
extern void   copro_arm2_write8(int addr, UINT8 data);
extern void   copro_arm2_write32(int addr, UINT32 data);

extern UINT32 m_sArmRegister[];

#define cpu_read8    copro_arm2_read8
#define cpu_read32   copro_arm2_read32
#define cpu_write8   copro_arm2_write8
#define cpu_write32  copro_arm2_write32

#define logerror printf

#define ARM_DEBUG_CORE 0
#define ARM_DEBUG_COPRO 0

/****************************************************************************************************
 *  INTERRUPT CONSTANTS
 ***************************************************************************************************/

#define ARM_IRQ_LINE    0
#define ARM_FIRQ_LINE   1

/****************************************************************************************************
 *  PUBLIC FUNCTIONS
 ***************************************************************************************************/

enum
{
	ARM_COPRO_TYPE_UNKNOWN_CP15 = 0,
	ARM_COPRO_TYPE_VL86C020
};

enum
{
	ARM32_PC=0,
	ARM32_R0, ARM32_R1, ARM32_R2, ARM32_R3, ARM32_R4, ARM32_R5, ARM32_R6, ARM32_R7,
	ARM32_R8, ARM32_R9, ARM32_R10, ARM32_R11, ARM32_R12, ARM32_R13, ARM32_R14, ARM32_R15,
	ARM32_FR8, ARM32_FR9, ARM32_FR10, ARM32_FR11, ARM32_FR12, ARM32_FR13, ARM32_FR14,
	ARM32_IR13, ARM32_IR14, ARM32_SR13, ARM32_SR14
};

void arm2_device_reset();
void arm2_execute_run(int tube_cycles);
void arm2_check_irq_state();
void arm2_execute_set_input(int irqline, int state);
UINT32 arm2_getR15();

UINT32 GetRegister( int rIndex );
void SetRegister( int rIndex, UINT32 value );
UINT32 GetModeRegister( int mode, int rIndex );
void SetModeRegister( int mode, int rIndex, UINT32 value );

void HandleBranch( UINT32 insn );
void HandleMemSingle( UINT32 insn );
void HandleALU( UINT32 insn );
void HandleMul( UINT32 insn);
int loadInc(UINT32 pat, UINT32 rbv, UINT32 s);
int loadDec(UINT32 pat, UINT32 rbv, UINT32 s, UINT32* deferredR15, int* defer);
int storeInc(UINT32 pat, UINT32 rbv);
int storeDec(UINT32 pat, UINT32 rbv);
void HandleMemBlock( UINT32 insn );
UINT32 decodeShift(UINT32 insn, UINT32 *pCarry);
UINT32 DecimalToBCD(UINT32 value);
void HandleCoProVL86C020( UINT32 insn );
void HandleCoPro( UINT32 insn );


#endif /* __ARM_H__ */
