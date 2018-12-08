// license:BSD-3-Clause
// copyright-holders:Bryan McPhail
/* arm.c

 ARM 2/3/6 Emulation (26 bit address bus)

 Todo:
 Timing - Currently very approximated, nothing relies on proper timing so far.
 IRQ timing not yet correct (again, nothing is affected by this so far).

 Recent changes (2005):
 Fixed software interrupts
 Fixed various mode change bugs
 Added preliminary co-processor support.

 By Bryan McPhail (bmcphail@tendril.co.uk) and Phil Stroffolino

 */

#include <stdio.h>
#include <string.h>
#include "arm.h"
#include "../tube.h"

// #define TRACE 1

#ifdef TRACE
#include "../darm/darm.h"
darm_t d;
darm_str_t str;
int m_trace;
#endif

#ifdef INCLUDE_DEBUGGER
#include "arm_debug.h"
#include "../cpu_debug.h"
#endif

//int    m_icount;
#define CYCLE_COUNT(in)

UINT32 m_sArmRegister[27];
UINT32 m_coproRegister[16];

UINT8 m_pendingIrq;
UINT8 m_pendingFiq;
UINT8 m_copro_type;

enum
{
  eARM_MODE_USER = 0x0, eARM_MODE_FIQ = 0x1, eARM_MODE_IRQ = 0x2, eARM_MODE_SVC = 0x3,

  kNumModes
};

/* There are 27 32 bit processor registers */
enum
{
  eR0 = 0, eR1, eR2, eR3, eR4, eR5, eR6, eR7, eR8, eR9, eR10, eR11, eR12, eR13, /* Stack Pointer */
  eR14, /* Link Register (holds return address) */
  eR15, /* Program Counter */

  /* Fast Interrupt */
  eR8_FIQ, eR9_FIQ, eR10_FIQ, eR11_FIQ, eR12_FIQ, eR13_FIQ, eR14_FIQ,

  /* IRQ */
  eR13_IRQ, eR14_IRQ,

  /* Software Interrupt */
  eR13_SVC, eR14_SVC,

  kNumRegisters
};

/* 16 processor registers are visible at any given time,
 * banked depending on processor mode.
 */
static const int sRegisterTable[kNumModes][16] =
{
{ /* USR */
eR0, eR1, eR2, eR3, eR4, eR5, eR6, eR7, eR8, eR9, eR10, eR11, eR12, eR13, eR14, eR15 },
{ /* FIQ */
eR0, eR1, eR2, eR3, eR4, eR5, eR6, eR7, eR8_FIQ, eR9_FIQ, eR10_FIQ, eR11_FIQ, eR12_FIQ, eR13_FIQ, eR14_FIQ, eR15 },
{ /* IRQ */
eR0, eR1, eR2, eR3, eR4, eR5, eR6, eR7, eR8, eR9, eR10, eR11, eR12, eR13_IRQ, eR14_IRQ, eR15 },
{ /* SVC */
eR0, eR1, eR2, eR3, eR4, eR5, eR6, eR7, eR8, eR9, eR10, eR11, eR12, eR13_SVC, eR14_SVC, eR15 } };

#define N_BIT   31
#define Z_BIT   30
#define C_BIT   29
#define V_BIT   28
#define I_BIT   27
#define F_BIT   26

#define N_MASK  ((UINT32)(1U<<N_BIT)) /* Negative flag */
#define Z_MASK  ((UINT32)(1U<<Z_BIT)) /* Zero flag */
#define C_MASK  ((UINT32)(1U<<C_BIT)) /* Carry flag */
#define V_MASK  ((UINT32)(1U<<V_BIT)) /* oVerflow flag */
#define I_MASK  ((UINT32)(1U<<I_BIT)) /* Interrupt request disable */
#define F_MASK  ((UINT32)(1U<<F_BIT)) /* Fast interrupt request disable */

#define N_IS_SET(pc)    ((pc) & N_MASK)
#define Z_IS_SET(pc)    ((pc) & Z_MASK)
#define C_IS_SET(pc)    ((pc) & C_MASK)
#define V_IS_SET(pc)    ((pc) & V_MASK)
#define I_IS_SET(pc)    ((pc) & I_MASK)
#define F_IS_SET(pc)    ((pc) & F_MASK)

#define N_IS_CLEAR(pc)  (!N_IS_SET(pc))
#define Z_IS_CLEAR(pc)  (!Z_IS_SET(pc))
#define C_IS_CLEAR(pc)  (!C_IS_SET(pc))
#define V_IS_CLEAR(pc)  (!V_IS_SET(pc))
#define I_IS_CLEAR(pc)  (!I_IS_SET(pc))
#define F_IS_CLEAR(pc)  (!F_IS_SET(pc))

#define PSR_MASK        ((UINT32) 0xf0000000u)
#define IRQ_MASK        ((UINT32) 0x0c000000u)
#define ADDRESS_MASK    ((UINT32) 0x03fffffcu)
#define MODE_MASK       ((UINT32) 0x00000003u)

#define R15                     m_sArmRegister[eR15]
#define MODE                    (R15&0x03)
#define SIGN_BIT                ((UINT32)(1U<<31))
#define SIGN_BITS_DIFFER(a,b)   (((a)^(b)) >> 31)

/* Deconstructing an instruction */

#define INSN_COND           ((UINT32) 0xf0000000u)
#define INSN_SDT_L          ((UINT32) 0x00100000u)
#define INSN_SDT_W          ((UINT32) 0x00200000u)
#define INSN_SDT_B          ((UINT32) 0x00400000u)
#define INSN_SDT_U          ((UINT32) 0x00800000u)
#define INSN_SDT_P          ((UINT32) 0x01000000u)
#define INSN_BDT_L          ((UINT32) 0x00100000u)
#define INSN_BDT_W          ((UINT32) 0x00200000u)
#define INSN_BDT_S          ((UINT32) 0x00400000u)
#define INSN_BDT_U          ((UINT32) 0x00800000u)
#define INSN_BDT_P          ((UINT32) 0x01000000u)
#define INSN_BDT_REGS       ((UINT32) 0x0000ffffu)
#define INSN_SDT_IMM        ((UINT32) 0x00000fffu)
#define INSN_MUL_A          ((UINT32) 0x00200000u)
#define INSN_MUL_RM         ((UINT32) 0x0000000fu)
#define INSN_MUL_RS         ((UINT32) 0x00000f00u)
#define INSN_MUL_RN         ((UINT32) 0x0000f000u)
#define INSN_MUL_RD         ((UINT32) 0x000f0000u)
#define INSN_I              ((UINT32) 0x02000000u)
#define INSN_OPCODE         ((UINT32) 0x01e00000u)
#define INSN_S              ((UINT32) 0x00100000u)
#define INSN_BL             ((UINT32) 0x01000000u)
#define INSN_BRANCH         ((UINT32) 0x00ffffffu)
#define INSN_SWI            ((UINT32) 0x00ffffffu)
#define INSN_RN             ((UINT32) 0x000f0000u)
#define INSN_RD             ((UINT32) 0x0000f000u)
#define INSN_OP2            ((UINT32) 0x00000fffu)
#define INSN_OP2_SHIFT      ((UINT32) 0x00000f80u)
#define INSN_OP2_SHIFT_TYPE ((UINT32) 0x00000070u)
#define INSN_OP2_RM         ((UINT32) 0x0000000fu)
#define INSN_OP2_ROTATE     ((UINT32) 0x00000f00u)
#define INSN_OP2_IMM        ((UINT32) 0x000000ffu)
#define INSN_OP2_SHIFT_TYPE_SHIFT   4
#define INSN_OP2_SHIFT_SHIFT        7
#define INSN_OP2_ROTATE_SHIFT       8
#define INSN_MUL_RS_SHIFT           8
#define INSN_MUL_RN_SHIFT           12
#define INSN_MUL_RD_SHIFT           16
#define INSN_OPCODE_SHIFT           21
#define INSN_RN_SHIFT               16
#define INSN_RD_SHIFT               12
#define INSN_COND_SHIFT             28

#define S_CYCLE 1
#define N_CYCLE 1
#define I_CYCLE 1

enum
{
  OPCODE_AND, /* 0000 */
  OPCODE_EOR, /* 0001 */
  OPCODE_SUB, /* 0010 */
  OPCODE_RSB, /* 0011 */
  OPCODE_ADD, /* 0100 */
  OPCODE_ADC, /* 0101 */
  OPCODE_SBC, /* 0110 */
  OPCODE_RSC, /* 0111 */
  OPCODE_TST, /* 1000 */
  OPCODE_TEQ, /* 1001 */
  OPCODE_CMP, /* 1010 */
  OPCODE_CMN, /* 1011 */
  OPCODE_ORR, /* 1100 */
  OPCODE_MOV, /* 1101 */
  OPCODE_BIC, /* 1110 */
  OPCODE_MVN /* 1111 */
};

enum
{
  COND_EQ = 0, /* Z: equal */
  COND_NE, /* ~Z: not equal */
  COND_CS, COND_HS = 2, /* C: unsigned higher or same */
  COND_CC, COND_LO = 3, /* ~C: unsigned lower */
  COND_MI, /* N: negative */
  COND_PL, /* ~N: positive or zero */
  COND_VS, /* V: overflow */
  COND_VC, /* ~V: no overflow */
  COND_HI, /* C && ~Z: unsigned higher */
  COND_LS, /* ~C || Z: unsigned lower or same */
  COND_GE, /* N == V: greater or equal */
  COND_LT, /* N != V: less than */
  COND_GT, /* ~Z && (N == V): greater than */
  COND_LE, /* Z || (N != V): less than or equal */
  COND_AL, /* always */
  COND_NV /* never */
};

#define LSL(v,s) ((v) << (s))
#define LSR(v,s) ((v) >> (s))
#define ROL(v,s) (LSL((v),(s)) | (LSR((v),32u - (s))))
#define ROR(v,s) (LSR((v),(s)) | (LSL((v),32u - (s))))

/***************************************************************************/

UINT32 GetRegister(int rIndex)
{
  if (MODE == 0)
  {
    return m_sArmRegister[rIndex];
  }

  return m_sArmRegister[sRegisterTable[MODE][rIndex]];
}

void SetRegister(int rIndex, UINT32 value)
{
  m_sArmRegister[sRegisterTable[MODE][rIndex]] = value;
}

UINT32 GetModeRegister(int mode, int rIndex)
{
  return m_sArmRegister[sRegisterTable[mode][rIndex]];
}

void SetModeRegister(int mode, int rIndex, UINT32 value)
{
  m_sArmRegister[sRegisterTable[mode][rIndex]] = value;
}

/***************************************************************************/

void arm2_device_reset()
{
  int i;
  m_copro_type = ARM_COPRO_TYPE_VL86C020;
  for (i = 0; i < sizeof(m_sArmRegister) / sizeof(UINT32); i++)
  {
    m_sArmRegister[i] = 0;
  }
  for (i = 0; i < sizeof(m_coproRegister) / sizeof(UINT32); i++)
  {
    m_coproRegister[i] = 0;
  }
  m_pendingIrq = 0;
  m_pendingFiq = 0;

  /* start up in SVC mode with interrupts disabled. */
  R15= eARM_MODE_SVC|I_MASK|F_MASK;
#ifdef TRACE
  m_trace = 0;
#endif
}

void arm2_execute_run(int tube_cycles)
{
#ifdef TRACE
  int i;
#endif
  UINT32 pc;
  UINT32 insn;
  //int m_icount = number;

  do
  {
    //debugger_instruction_hook(this, R15 & ADDRESS_MASK);

    /* load instruction */
    pc = R15;
#ifdef INCLUDE_DEBUGGER
      if (arm2_debug_enabled)
      {
         debug_preexec(&arm2_cpu_debug, pc & ADDRESS_MASK);
      }
#endif
    insn = cpu_read32( pc & ADDRESS_MASK );

#ifdef TRACE
    if ((pc & ADDRESS_MASK) == 0xa060)
    {
      m_trace = 1;
    }
    if (m_trace)
    {
      printf("%08X %08X ", pc, insn);
      for (i = eR0; i <= eR7; i++)
      {
        printf("%08X ", m_sArmRegister[i]);
      }
      if(darm_armv7_disasm(&d, insn) == 0 && darm_str2(&d, &str, 0) == 0)
      {
        printf("%s\r\n", str.total);
      }
      else
      {
        printf("***\r\n");
      }
    }
    if ((pc & ADDRESS_MASK) == 0xa0c0)
    {
      m_trace = 0;
    }
    if ((pc & ADDRESS_MASK) == 0xa2ac)
    {
      m_trace = 0;
    }
    if ((insn & 0x0D900000) == 0x01000000)
    {
      printf("S bit not set in %08x at %08x\r\n", insn, pc & ADDRESS_MASK);
      insn |= INSN_S;
    }
#endif
    switch (insn >> INSN_COND_SHIFT)
    {
      case COND_EQ:
      if (Z_IS_CLEAR(pc)) goto L_Next;
      break;
      case COND_NE:
      if (Z_IS_SET(pc)) goto L_Next;
      break;
      case COND_CS:
      if (C_IS_CLEAR(pc)) goto L_Next;
      break;
      case COND_CC:
      if (C_IS_SET(pc)) goto L_Next;
      break;
      case COND_MI:
      if (N_IS_CLEAR(pc)) goto L_Next;
      break;
      case COND_PL:
      if (N_IS_SET(pc)) goto L_Next;
      break;
      case COND_VS:
      if (V_IS_CLEAR(pc)) goto L_Next;
      break;
      case COND_VC:
      if (V_IS_SET(pc)) goto L_Next;
      break;
      case COND_HI:
      if (C_IS_CLEAR(pc) || Z_IS_SET(pc)) goto L_Next;
      break;
      case COND_LS:
      if (C_IS_SET(pc) && Z_IS_CLEAR(pc)) goto L_Next;
      break;
      case COND_GE:
      if (!(pc & N_MASK) != !(pc & V_MASK)) goto L_Next; /* Use x ^ (x >> ...) method */
      break;
      case COND_LT:
      if (!(pc & N_MASK) == !(pc & V_MASK)) goto L_Next;
      break;
      case COND_GT:
      if (Z_IS_SET(pc) || (!(pc & N_MASK) != !(pc & V_MASK))) goto L_Next;
      break;
      case COND_LE:
      if (Z_IS_CLEAR(pc) && (!(pc & N_MASK) == !(pc & V_MASK))) goto L_Next;
      break;
      case COND_NV:
      goto L_Next;
    }
    /* Condition satisfied, so decode the instruction */
    if ((insn & 0x0fc000f0u) == 0x00000090u) /* Multiplication */
    {
      HandleMul(insn);
      R15 += 4;
    }
    else if (!(insn & 0x0c000000u)) /* Data processing */
    {
      HandleALU(insn);
    }
    else if ((insn & 0x0c000000u) == 0x04000000u) /* Single data access */
    {
      HandleMemSingle(insn);
      R15 += 4;
    }
    else if ((insn & 0x0e000000u) == 0x08000000u ) /* Block data access */
    {
      HandleMemBlock(insn);
      R15 += 4;
    }
    else if ((insn & 0x0e000000u) == 0x0a000000u) /* Branch */
    {
      HandleBranch(insn);
    }
    else if ((insn & 0x0f000000u) == 0x0e000000u) /* Coprocessor */
    {
      if (m_copro_type == ARM_COPRO_TYPE_VL86C020)
      HandleCoProVL86C020(insn);
      else
      HandleCoPro(insn);

      R15 += 4;
    }
    else if ((insn & 0x0f000000u) == 0x0f000000u) /* Software interrupt */
    {
      pc=R15+4;
      R15 = eARM_MODE_SVC; /* Set SVC mode so PC is saved to correct R14 bank */
      SetRegister( 14, pc ); /* save PC */
      R15 = (pc&PSR_MASK)|(pc&IRQ_MASK)|0x8|eARM_MODE_SVC|I_MASK|(pc&MODE_MASK);
      CYCLE_COUNT(2 * S_CYCLE + N_CYCLE);
    }
    else /* Undefined */
    {
      logerror("%08x:  Undefined instruction\n",R15);
      L_Next:
      CYCLE_COUNT(S_CYCLE);
      R15 += 4;
    }

    //arm2_check_irq_state();

    tubeUseCycles(1); 
    } while (tubeContinueRunning());
  //while( m_icount > 0 );
  //while (number--);
} /* arm_execute */

void arm2_check_irq_state()
{
  UINT32 pc = R15+4; /* save old pc (already incremented in pipeline) */;

  /* Exception priorities (from ARM6, not specifically ARM2/3):
   
   Reset
   Data abort
   FIRQ
   IRQ
   Prefetch abort
   Undefined instruction
   */

  if (m_pendingFiq && (pc&F_MASK)==0)
  {
    R15 = eARM_MODE_FIQ; /* Set FIQ mode so PC is saved to correct R14 bank */
    SetRegister( 14, pc ); /* save PC */
    R15 = (pc&PSR_MASK)|(pc&IRQ_MASK)|0x1c|eARM_MODE_FIQ|I_MASK|F_MASK; /* Mask both IRQ & FIRQ, set PC=0x1c */
    m_pendingFiq=0;
    return;
  }

  if (m_pendingIrq && (pc&I_MASK)==0)
  {
    R15 = eARM_MODE_IRQ; /* Set IRQ mode so PC is saved to correct R14 bank */
    SetRegister( 14, pc ); /* save PC */
    R15 = (pc&PSR_MASK)|(pc&IRQ_MASK)|0x18|eARM_MODE_IRQ|I_MASK|(pc&F_MASK); /* Mask only IRQ, set PC=0x18 */
    m_pendingIrq=0;
    return;
  }
}

void arm2_execute_set_input(int irqline, int state)
{
  switch (irqline)
  {
    case ARM_IRQ_LINE: /* IRQ */
      if (state && (R15&0x3)!=eARM_MODE_IRQ) /* Don't allow nested IRQs */
      m_pendingIrq=1;
      else
      m_pendingIrq=0;
      break;

      case ARM_FIRQ_LINE: /* FIRQ */
      if (state && (R15&0x3)!=eARM_MODE_FIQ) /* Don't allow nested FIRQs */
      m_pendingFiq=1;
      else
      m_pendingFiq=0;
      break;
    }

    arm2_check_irq_state();
  }

UINT32 arm2_getR15()
{
  return R15;
}

/***************************************************************************/

void HandleBranch(UINT32 insn)
{
  UINT32 off = (insn & INSN_BRANCH) << 2;

  /* Save PC into LR if this is a branch with link */
  if (insn & INSN_BL)
  {
    SetRegister(14, R15+ 4);
  }

  /* Sign-extend the 24-bit offset in our calculations */
  if (off & 0x2000000u)
  {
    R15 = ((R15 - (((~(off | 0xfc000000u)) + 1) - 8)) & ADDRESS_MASK) | (R15 & ~ADDRESS_MASK);
  }
  else
  {
    R15 = ((R15 + (off + 8)) & ADDRESS_MASK) | (R15 & ~ADDRESS_MASK);
  }
  CYCLE_COUNT(2 * S_CYCLE + N_CYCLE);
}

void HandleMemSingle(UINT32 insn)
{
  UINT32 rn, rnv, off, rd;

  /* Fetch the offset */
  if (insn & INSN_I)
  {
    off = decodeShift(insn, nullptr);
  }
  else
  {
    off = insn & INSN_SDT_IMM;
  }

  /* Calculate Rn, accounting for PC */
  rn = (insn & INSN_RN) >> INSN_RN_SHIFT;

  //  if (rn==0xf) logerror("%08x:  Source R15\n",R15);

  if (insn & INSN_SDT_P)
  {
    /* Pre-indexed addressing */
    if (insn & INSN_SDT_U)
    {
      if (rn != eR15)
        rnv = (GetRegister(rn) + off);
      else
        rnv = (R15& ADDRESS_MASK) + off;
      }
      else
      {
        if (rn != eR15)
        rnv = (GetRegister(rn) - off);
        else
        rnv = (R15 & ADDRESS_MASK) - off;
      }

      if (rn == eR15)
      {
        rnv = rnv + 8;
      }
    }
    else
    {
      /* Post-indexed addressing */
      if (rn == eR15)
      {
        rnv = (R15 & ADDRESS_MASK) + 8;
      }
      else
      {
        rnv = GetRegister(rn);
      }
    }

    /* Do the transfer */
  rd = (insn & INSN_RD) >> INSN_RD_SHIFT;
  if (insn & INSN_SDT_L)
  {
    /* Load */
    CYCLE_COUNT(S_CYCLE + I_CYCLE + N_CYCLE);
    if (insn & INSN_SDT_B)
    {
      if (ARM_DEBUG_CORE && rd == eR15)
        logerror("read byte R15 %08x\n", R15);
        SetRegister(rd,(UINT32) cpu_read8(rnv) );
      }
      else
      {
        if (rd == eR15)
        {
          R15 = (cpu_read32(rnv) & ADDRESS_MASK) | (R15 & PSR_MASK) | (R15 & IRQ_MASK) | (R15 & MODE_MASK);

          /*
           The docs are explicit in that the bottom bits should be masked off
           when writing to R15 in this way, however World Cup Volleyball 95 has
           an example of an unaligned jump (bottom bits = 2) where execution
           should definitely continue from the rounded up address.
           
           In other cases, 4 is subracted from R15 here to account for pipelining.
           */
          if (m_copro_type == ARM_COPRO_TYPE_VL86C020 || (cpu_read32(rnv)&3)==0)
          R15 -= 4;

          CYCLE_COUNT(S_CYCLE + N_CYCLE);
        }
        else
        {
          SetRegister(rd, cpu_read32(rnv));
        }
      }
    }
    else
    {
      /* Store */
      CYCLE_COUNT(2 * N_CYCLE);
      if (insn & INSN_SDT_B)
      {
        if (ARM_DEBUG_CORE && rd==eR15)
        logerror("Wrote R15 in byte mode\n");

        cpu_write8(rnv, (UINT8) GetRegister(rd) & 0xffu);
      }
      else
      {
        if (ARM_DEBUG_CORE && rd==eR15)
        logerror("Wrote R15 in 32bit mode\n");

        cpu_write32(rnv, rd == eR15 ? R15 + 8 : GetRegister(rd));
      }
    }
  /* Do pre-indexing writeback */
  if ((insn & INSN_SDT_P) && (insn & INSN_SDT_W))
  {
    if ((insn & INSN_SDT_L) && rd == rn)
      SetRegister(rn, GetRegister(rd));
    else
      SetRegister(rn, rnv);

    if (ARM_DEBUG_CORE && rn == eR15)
      logerror("writeback R15 %08x\n", R15);
  }

    /* Do post-indexing writeback */
  if (!(insn & INSN_SDT_P)/* && (insn&INSN_SDT_W)*/)
  {
    if (insn & INSN_SDT_U)
    {
      /* Writeback is applied in pipeline, before value is read from mem,
       so writeback is effectively ignored */
      if (rd == rn)
      {
        SetRegister(rn, GetRegister(rd));
      }
      else
      {
        if ((insn & INSN_SDT_W) != 0)
          logerror("%08x:  RegisterWritebackIncrement %d %d %d\n", R15,(insn & INSN_SDT_P)!=0,(insn&INSN_SDT_W)!=0,(insn & INSN_SDT_U)!=0);

          SetRegister(rn,(rnv + off));
        }
      }
      else
      {
        /* Writeback is applied in pipeline, before value is read from mem,
         so writeback is effectively ignored */
        if (rd==rn)
        {
          SetRegister(rn,GetRegister(rd));
        }
        else
        {
          SetRegister(rn,(rnv - off));

          if ((insn&INSN_SDT_W)!=0)
          logerror("%08x:  RegisterWritebackDecrement %d %d %d\n",R15,(insn & INSN_SDT_P)!=0,(insn&INSN_SDT_W)!=0,(insn & INSN_SDT_U)!=0);
        }
      }
    }
  } /* HandleMemSingle */

#define IsNeg(i) ((i) >> 31)
#define IsPos(i) ((~(i)) >> 31)

  /* Set NZCV flags for ADDS / SUBS */

#define HandleALUAddFlags(rd, rn, op2)          \
  if (insn & INSN_S)                            \
    R15 =                                             \
      ((R15 &~ (N_MASK | Z_MASK | V_MASK | C_MASK))                   \
       | (((!SIGN_BITS_DIFFER(rn, op2)) && SIGN_BITS_DIFFER(rn, rd))  \
          << V_BIT)                                                   \
       | (((IsNeg(rn) & IsNeg(op2)) | (IsNeg(rn) & IsPos(rd)) | (IsNeg(op2) & IsPos(rd))) ? C_MASK : 0) \
       | HandleALUNZFlags(rd))                                        \
      + 4;                                                            \
  else R15 += 4;

#define HandleALUSubFlags(rd, rn, op2)          \
  if (insn & INSN_S)                            \
    R15 =                                             \
      ((R15 &~ (N_MASK | Z_MASK | V_MASK | C_MASK))                \
       | ((SIGN_BITS_DIFFER(rn, op2) && SIGN_BITS_DIFFER(rn, rd))  \
          << V_BIT)                                                     \
       | (((IsNeg(rn) & IsPos(op2)) | (IsNeg(rn) & IsPos(rd)) | (IsPos(op2) & IsPos(rd))) ? C_MASK : 0) \
       | HandleALUNZFlags(rd))                                          \
      + 4;                                                              \
  else R15 += 4;

  /* Set NZC flags for logical operations. */

#define HandleALUNZFlags(rd)                    \
  (((rd) & SIGN_BIT) | ((!(rd)) << Z_BIT))

#define HandleALULogicalFlags(rd, sc)           \
  if (insn & INSN_S)                            \
    R15 = ((R15 &~ (N_MASK | Z_MASK | C_MASK))  \
           | HandleALUNZFlags(rd)                      \
           | (((sc) != 0) << C_BIT)) + 4;              \
  else R15 += 4;

void HandleALU(UINT32 insn)
{
  UINT32 op2, sc = 0, rd, rn, opcode;
  UINT32 by, rdn;

  opcode = (insn & INSN_OPCODE) >> INSN_OPCODE_SHIFT;
  CYCLE_COUNT(S_CYCLE);

  rd = 0;
  rn = 0;

  /* Construct Op2 */
  if (insn & INSN_I)
  {
    /* Immediate constant */
    by = (insn & INSN_OP2_ROTATE) >> INSN_OP2_ROTATE_SHIFT;
    if (by)
    {
      op2 = ROR(insn & INSN_OP2_IMM, by << 1);
      sc = op2 & SIGN_BIT;
    }
    else
    {
      op2 = insn & INSN_OP2;
      sc = R15& C_MASK;
    }
  }
  else
  {
    op2 = decodeShift(insn, (insn & INSN_S) ? &sc : nullptr);

    if (!(insn & INSN_S))
    sc=0;
  }

  /* Calculate Rn to account for pipelining */
  if ((opcode & 0xd) != 0xd) /* No Rn in MOV */
  {
    if ((rn = (insn & INSN_RN) >> INSN_RN_SHIFT) == eR15)
    {
      if (ARM_DEBUG_CORE)
        logerror("%08x:  Pipelined R15 (Shift %d)\n", R15,(insn&INSN_I?8:12));

        /* Docs strongly suggest the mode bits should be included here, but it breaks Captain
         America, as it starts doing unaligned reads */
        rn=(R15+8)&ADDRESS_MASK;
      }
      else
      {
        rn = GetRegister(rn);
      }
    }

    /* Perform the operation */
  switch ((insn & INSN_OPCODE) >> INSN_OPCODE_SHIFT)
  {
    /* Arithmetic operations */
    case OPCODE_SBC:
      rd = (rn - op2 - ((R15& C_MASK) ? 0 : 1));
      HandleALUSubFlags(rd, rn, op2);
      break;
      case OPCODE_CMP:
      case OPCODE_SUB:
      rd = (rn - op2);
      HandleALUSubFlags(rd, rn, op2);
      break;
      case OPCODE_RSC:
      rd = (op2 - rn - ((R15 & C_MASK) ? 0 : 1));
      HandleALUSubFlags(rd, op2, rn);
      break;
      case OPCODE_RSB:
      rd = (op2 - rn);
      HandleALUSubFlags(rd, op2, rn);
      break;
      case OPCODE_ADC:
      rd = (rn + op2 + ((R15 & C_MASK) >> C_BIT));
      HandleALUAddFlags(rd, rn, op2);
      break;
      case OPCODE_CMN:
      case OPCODE_ADD:
      rd = (rn + op2);
      HandleALUAddFlags(rd, rn, op2);
      break;
      /* Logical operations */
      case OPCODE_AND:
      case OPCODE_TST:
      rd = rn & op2;
      HandleALULogicalFlags(rd, sc);
      break;
      case OPCODE_BIC:
      rd = rn &~ op2;
      HandleALULogicalFlags(rd, sc);
      break;
      case OPCODE_TEQ:
      case OPCODE_EOR:
      rd = rn ^ op2;
      HandleALULogicalFlags(rd, sc);
      break;
      case OPCODE_ORR:
      rd = rn | op2;
      HandleALULogicalFlags(rd, sc);
      break;
      case OPCODE_MOV:
      rd = op2;
      HandleALULogicalFlags(rd, sc);
      break;
      case OPCODE_MVN:
      rd = (~op2);
      HandleALULogicalFlags(rd, sc);
      break;
    }

    /* Put the result in its register if not a test */
  rdn = (insn & INSN_RD) >> INSN_RD_SHIFT;
  if ((opcode & 0xc) != 0x8)
  {
    if (rdn == eR15 && !(insn & INSN_S))
    {
      /* Merge the old NZCV flags into the new PC value */
      R15= (rd & ADDRESS_MASK) | (R15 & PSR_MASK) | (R15 & IRQ_MASK) | (R15&MODE_MASK);
      CYCLE_COUNT(S_CYCLE + N_CYCLE);
    }
    else
    {
      if (rdn==eR15)
      {
        /* S Flag is set - update PSR & mode if in non-user mode only */
        if ((R15&MODE_MASK)!=0)
        {
          SetRegister(rdn,rd);
        }
        else
        {
          SetRegister(rdn,(rd&ADDRESS_MASK) | (rd&PSR_MASK) | (R15&IRQ_MASK) | (R15&MODE_MASK));
        }
        CYCLE_COUNT(S_CYCLE + N_CYCLE);
      }
      else
      {
        SetRegister(rdn,rd);
      }
    }
    /* TST & TEQ can affect R15 (the condition code register) with the S bit set */
  }
  else if ((rdn==eR15) && (insn & INSN_S))
  {
    // update only the flags
    if ((R15&MODE_MASK)!=0)
    {
      // combine the flags from rd with the address from R15
      rd &= ~ADDRESS_MASK;
      rd |= (R15 & ADDRESS_MASK);
      SetRegister(rdn,rd);
    }
    else
    {
      // combine the flags from rd with the address from R15
      rd &= ~ADDRESS_MASK;// clear address part of RD
      rd |= (R15 & ADDRESS_MASK);// RD = address part of R15
      SetRegister(rdn,(rd&ADDRESS_MASK) | (rd&PSR_MASK) | (R15&IRQ_MASK) | (R15&MODE_MASK));
    }
    CYCLE_COUNT(S_CYCLE + N_CYCLE);
  }
}

void HandleMul(UINT32 insn)
{
  UINT32 r;

  CYCLE_COUNT(S_CYCLE + I_CYCLE);
  /* should be:
   Range of Rs            Number of cycles

   &0 -- &1            1S + 1I
   &2 -- &7            1S + 2I
   &8 -- &1F           1S + 3I
   &20 -- &7F           1S + 4I
   &80 -- &1FF          1S + 5I
   &200 -- &7FF          1S + 6I
   &800 -- &1FFF         1S + 7I
   &2000 -- &7FFF         1S + 8I
   &8000 -- &1FFFF        1S + 9I
   &20000 -- &7FFFF        1S + 10I
   &80000 -- &1FFFFF       1S + 11I
   &200000 -- &7FFFFF       1S + 12I
   &800000 -- &1FFFFFF      1S + 13I
   &2000000 -- &7FFFFFF      1S + 14I
   &8000000 -- &1FFFFFFF     1S + 15I
   &20000000 -- &FFFFFFFF     1S + 16I
   */

  /* Do the basic multiply of Rm and Rs */
  r = GetRegister(insn & INSN_MUL_RM) * GetRegister((insn & INSN_MUL_RS) >> INSN_MUL_RS_SHIFT);

  if (ARM_DEBUG_CORE
      && ((insn & INSN_MUL_RM) == 0xf || ((insn & INSN_MUL_RS) >> INSN_MUL_RS_SHIFT) == 0xf || ((insn & INSN_MUL_RN) >> INSN_MUL_RN_SHIFT) == 0xf))
    logerror("%08x:  R15 used in mult\n", R15);

    /* Add on Rn if this is a MLA */
  if (insn & INSN_MUL_A)
  {
    r += GetRegister((insn & INSN_MUL_RN) >> INSN_MUL_RN_SHIFT);
  }

  /* Write the result */
  SetRegister((insn & INSN_MUL_RD) >> INSN_MUL_RD_SHIFT, r);

  /* Set N and Z if asked */
  if (insn & INSN_S)
  {
    R15= (R15 &~ (N_MASK | Z_MASK)) | HandleALUNZFlags(r);
  }
}

int loadInc(UINT32 pat, UINT32 rbv, UINT32 s)
{
  int i, result;

  result = 0;
  for (i = 0; i < 16; i++)
  {
    if ((pat >> i) & 1)
    {
      if (i == 15)
      {
        if (s) /* Pull full contents from stack */
          SetRegister(15, cpu_read32(rbv += 4));
        else
          /* Pull only address, preserve mode & status flags */
          SetRegister(15, (R15&PSR_MASK) | (R15&IRQ_MASK) | (R15&MODE_MASK) | ((cpu_read32(rbv+=4))&ADDRESS_MASK) );
        }
        else
          SetRegister( i, cpu_read32(rbv+=4) );

        result++;
      }
    }
  return result;
}

int loadDec(UINT32 pat, UINT32 rbv, UINT32 s, UINT32* deferredR15, int* defer)
{
  int i, result;

  result = 0;
  for (i = 15; i >= 0; i--)
  {
    if ((pat >> i) & 1)
    {
      if (i == 15)
      {
        *defer = 1;
        if (s) /* Pull full contents from stack */
          *deferredR15 = cpu_read32(rbv -= 4);
        else
          /* Pull only address, preserve mode & status flags */
          *deferredR15 = (R15&PSR_MASK) | (R15&IRQ_MASK) | (R15&MODE_MASK) | ((cpu_read32(rbv-=4))&ADDRESS_MASK);
      }
      else
        SetRegister( i, cpu_read32(rbv -=4) );
      result++;
    } 
  }
  return result;
}

int storeInc(UINT32 pat, UINT32 rbv)
{
  int i, result;

  result = 0;
  for (i = 0; i < 16; i++)
  {
    if ((pat >> i) & 1)
    {
      if (ARM_DEBUG_CORE && i == 15) /* R15 is plus 12 from address of STM */
        logerror("%08x: StoreInc on R15\n", R15);

        cpu_write32( rbv += 4, GetRegister(i) );
        result++;
      }
    }
  return result;
} /* storeInc */

int storeDec(UINT32 pat, UINT32 rbv)
{
  int i, result;

  result = 0;
  for (i = 15; i >= 0; i--)
  {
    if ((pat >> i) & 1)
    {
      if (ARM_DEBUG_CORE && i == 15) /* R15 is plus 12 from address of STM */
        logerror("%08x: StoreDec on R15\n", R15);

        cpu_write32( rbv -= 4, GetRegister(i) );
        result++;
      }
    }
  return result;
} /* storeDec */

void HandleMemBlock(UINT32 insn)
{
  UINT32 rb = (insn & INSN_RN) >> INSN_RN_SHIFT;
  UINT32 rbp = GetRegister(rb);
  int result;

  if (ARM_DEBUG_CORE && (insn & INSN_BDT_S))
    logerror("%08x:  S Bit set in MEMBLOCK\n", R15);

  if (insn & INSN_BDT_L)
  {
    /* Loading */
    if (insn & INSN_BDT_U)
    {
      int mode = MODE;

      /* Incrementing */
      if (!(insn & INSN_BDT_P))
        rbp = rbp + (-4);

      // S Flag Set, but R15 not in list = Transfers to User Bank
      if ((insn & INSN_BDT_S) && !(insn & 0x8000))
      {
        int curmode = MODE;
        R15 = R15 & ~MODE_MASK;
        result = loadInc( insn & 0xffff, rbp, insn&INSN_BDT_S );
        R15 = R15 | curmode;
      }
      else
        result = loadInc(insn & 0xffff, rbp, insn & INSN_BDT_S);

      if (insn & 0x8000)
      {
        R15-=4;
        CYCLE_COUNT(S_CYCLE + N_CYCLE);
      }

      if (insn & INSN_BDT_W)
      {
        /* Arm docs notes: The base register can always be loaded without any problems.
         However, don't specify writeback if the base register is being loaded -
         you can't end up with both a written-back value and a loaded value in the base register!

         However - Fighter's History does exactly that at 0x121e4 (LDMUW [R13], { R13-R15 })!

         This emulator implementation skips applying writeback in this case, which is confirmed
         correct for this situation, but that is not necessarily true for all ARM hardware
         implementations (the results are officially undefined).
         */

        if (ARM_DEBUG_CORE && rb == 15)
          logerror("%08x:  Illegal LDRM writeback to r15\n", R15);

          if ((insn&(1<<rb))==0)
          SetModeRegister(mode, rb, GetModeRegister(mode, rb) + result * 4);
          else if (ARM_DEBUG_CORE)
          logerror("%08x:  Illegal LDRM writeback to base register (%u)\n",R15, rb);
        }
      }
      else
      {
        UINT32 deferredR15=0;
        int defer=0;

        /* Decrementing */
        if (!(insn & INSN_BDT_P))
        {
          rbp = rbp - (- 4);
        }

              // S Flag Set, but R15 not in list = Transfers to User Bank
        if ((insn & INSN_BDT_S) && !(insn & 0x8000))
        {
          int curmode = MODE;
          R15 = R15 & ~MODE_MASK;
          result = loadDec( insn&0xffff, rbp, insn&INSN_BDT_S, &deferredR15, &defer );
          R15 = R15 | curmode;
        }
        else
          result = loadDec( insn&0xffff, rbp, insn&INSN_BDT_S, &deferredR15, &defer );

        if (insn & INSN_BDT_W)
        {
          if (rb==0xf)
          logerror("%08x:  Illegal LDRM writeback to r15\n",R15);
          SetRegister(rb,GetRegister(rb)-result*4);
        }

        // If R15 is pulled from memory we defer setting it until after writeback
        // is performed, else we may writeback to the wrong context (ie, the new
        // context if the mode has changed as a result of the R15 read)
        if (defer)
        SetRegister(15, deferredR15);

        if (insn & 0x8000)
        {
          CYCLE_COUNT(S_CYCLE + N_CYCLE);
          R15-=4;
        }
      }
      CYCLE_COUNT(result * S_CYCLE + N_CYCLE + I_CYCLE);
    } /* Loading */
    else
    {
      /* Storing

       ARM docs notes: Storing a list of registers including the base register using writeback
       will write the value of the base register before writeback to memory only if the base
       register is the first in the list. Otherwise, the value which is used is not defined.

       */
      if (insn & (1<<eR15))
      {
        if (ARM_DEBUG_CORE)
        logerror("%08x: Writing R15 in strm\n",R15);

        /* special case handling if writing to PC */
        R15 += 12;
      }
      if (insn & INSN_BDT_U)
      {
        /* Incrementing */
        if (!(insn & INSN_BDT_P))
        {
          rbp = rbp + (- 4);
        }
        // S bit set = Transfers to User Bank
        if (insn & INSN_BDT_S)
        {
          int curmode = MODE;
          R15 = R15 & ~MODE_MASK;
          result = storeInc( insn&0xffff, rbp );
          R15 = R15 | curmode;
        }
        else
          result = storeInc( insn&0xffff, rbp );
        if( insn & INSN_BDT_W )
        {
          SetRegister(rb,GetRegister(rb)+result*4);
        }
      }
      else
      {
        /* Decrementing */
        if (!(insn & INSN_BDT_P))
        {
          rbp = rbp - (- 4);
        }
        // S bit set = Transfers to User Bank
        if (insn & INSN_BDT_S)
        {
          int curmode = MODE;
          R15 = R15 & ~MODE_MASK;
          result = storeDec( insn&0xffff, rbp );
          R15 = R15 | curmode;
        }
        else
          result = storeDec( insn&0xffff, rbp );
        if( insn & INSN_BDT_W )
        {
          SetRegister(rb,GetRegister(rb)-result*4);
        }
      }
      if( insn & (1<<eR15) )
      R15 -= 12;

      CYCLE_COUNT((result - 1) * S_CYCLE + 2 * N_CYCLE);
    }
  } /* HandleMemBlock */

  /* Decodes an Op2-style shifted-register form.  If @carry@ is non-zero the
   * shifter carry output will manifest itself as @*carry == 0@ for carry clear
   * and @*carry != 0@ for carry set.
   */
UINT32 decodeShift(UINT32 insn, UINT32 *pCarry)
{
  UINT32 k = (insn & INSN_OP2_SHIFT) >> INSN_OP2_SHIFT_SHIFT;
  UINT32 rm = GetRegister(insn & INSN_OP2_RM);
  UINT32 t = (insn & INSN_OP2_SHIFT_TYPE) >> INSN_OP2_SHIFT_TYPE_SHIFT;

  if ((insn & INSN_OP2_RM) == 0xf)
  {
    /* If hardwired shift, then PC is 8 bytes ahead, else if register shift
     is used, then 12 bytes - TODO?? */
    rm += 8;
  }

  /* All shift types ending in 1 are Rk, not #k */
  if (t & 1)
  {
//      logerror("%08x:  RegShift %02x %02x\n",R15, k>>1,GetRegister(k >> 1));
    if (ARM_DEBUG_CORE && (insn & 0x80) == 0x80)
      logerror("%08x:  RegShift ERROR (p36)\n", R15);

      //see p35 for check on this
      k = GetRegister(k >> 1)&0xff;
      CYCLE_COUNT(S_CYCLE);
      if( k == 0 ) /* Register shift by 0 is a no-op */
      {
//          logerror("%08x:  NO-OP Regshift\n",R15);
        if (pCarry) *pCarry = R15 & C_MASK;
        return rm;
      }
    }
    /* Decode the shift type and perform the shift */
  switch (t >> 1)
  {
    case 0: /* LSL */
      if (k >= 32)
      {
        if (pCarry)
          *pCarry = (k == 32) ? rm & 1 : 0;
        return 0;
      }
      else if (pCarry)
      {
        *pCarry = k ? (rm & (1 << (32 - k))) : (R15& C_MASK);
      }
      return k ? LSL(rm, k) : rm;

      case 1: /* LSR */
      if (k == 0 || k == 32)
      {
        if (pCarry) *pCarry = rm & SIGN_BIT;
        return 0;
      }
      else if (k > 32)
      {
        if (pCarry) *pCarry = 0;
        return 0;
      }
      else
      {
        if (pCarry) *pCarry = (rm & (1U << (k - 1)));
        return LSR(rm, k);
      }

      case 2: /* ASR */
      if (k == 0 || k > 32)
        k = 32;
      if (pCarry) *pCarry = (rm & (1U << (k - 1)));
      if (k >= 32)
        return (rm & SIGN_BIT) ? 0xffffffffu : 0;
      else
      {
        if (rm & SIGN_BIT)
        return LSR(rm, k) | (0xffffffffu << (32 - k));
        else
        return LSR(rm, k);
      }

      case 3: /* ROR and RRX */
      if (k)
      {
        while (k > 32) k -= 32;
        if (pCarry) *pCarry = rm & (1 << (k - 1));
        return ROR(rm, k);
      }
      else
      {
        if (pCarry) *pCarry = (rm & 1);
        return LSR(rm, 1) | ((R15 & C_MASK) << 2);
      }
    }

  logerror("%08x: Decodeshift error\n", R15);
  return 0;
} /* decodeShift */

UINT32 BCDToDecimal(UINT32 value)
{
  UINT32 accumulator = 0;
  UINT32 multiplier = 1;
  int i;

  for (i = 0; i < 8; i++)
  {
    accumulator += (value & 0xF) * multiplier;

    multiplier *= 10;
    value >>= 4;
  }

  return accumulator;
}

UINT32 DecimalToBCD(UINT32 value)
{
  UINT32 accumulator = 0;
  UINT32 divisor = 10;
  int i;

  for (i = 0; i < 8; i++)
  {
    UINT32 temp;

    temp = value % divisor;
    value -= temp;
    temp /= divisor / 10;

    accumulator += temp << (i * 4);

    divisor *= 10;
  }

  return accumulator;
}

void HandleCoProVL86C020(UINT32 insn)
{
  UINT32 rn = (insn >> 12) & 0xf;
  UINT32 crn = (insn >> 16) & 0xf;

  CYCLE_COUNT(S_CYCLE);

  /* MRC - transfer copro register to main register */
  if ((insn & 0x0f100010) == 0x0e100010)
  {
    if (crn == 0) // ID, read only
    {
      /*
       0x41<<24 <- Designer code, Acorn Computer Ltd.
       0x56<<16 <- Manufacturer code, VLSI Technology Inc.
       0x03<<8 <- Part type, VLC86C020
       0x00<<0 <- Revision number, 0
       */
      SetRegister(rn, 0x41560300);
      //debugger_break(machine());
    }
    else
      SetRegister(rn, m_coproRegister[crn]);

  }
  /* MCR - transfer main register to copro register */
  else if ((insn & 0x0f100010) == 0x0e000010)
  {
    if (crn != 0)
      m_coproRegister[crn] = GetRegister(rn);

    //printf("%08x:  VL86C020 copro instruction write %08x %d %d\n", R15 & 0x3ffffff, insn,rn,crn);
  }
  else
  {
    printf("%08x:  Unimplemented VL86C020 copro instruction %08x %u %u\n", R15& 0x3ffffff, insn,rn,crn);
    //debugger_break(machine());
  }
}

void HandleCoPro(UINT32 insn)
{
  UINT32 rn = (insn >> 12) & 0xf;
  UINT32 crn = (insn >> 16) & 0xf;

  CYCLE_COUNT(S_CYCLE);

  /* MRC - transfer copro register to main register */
  if ((insn & 0x0f100010) == 0x0e100010)
  {
    SetRegister(rn, m_coproRegister[crn]);

    if (ARM_DEBUG_COPRO)
      logerror("%08x:  Copro read CR%u (%08x) to R%u\n", R15, crn, m_coproRegister[crn], rn);
    }
    /* MCR - transfer main register to copro register */
    else if( (insn&0x0f100010)==0x0e000010 )
    {
      m_coproRegister[crn]=GetRegister(rn);

      /* Data East 156 copro specific - trigger BCD operation */
      if (crn==2)
      {
        if (m_coproRegister[crn]==0)
        {
          /* Unpack BCD */
          int v0=BCDToDecimal(m_coproRegister[0]);
          int v1=BCDToDecimal(m_coproRegister[1]);

          /* Repack vcd */
          m_coproRegister[5]=DecimalToBCD(v0+v1);

          if (ARM_DEBUG_COPRO)
          logerror("Cmd:  Add 0 + 1, result in 5 (%08x + %08x == %08x)\n", v0, v1, m_coproRegister[5]);
        }
        else if (m_coproRegister[crn]==1)
        {
          /* Unpack BCD */
          int v0=BCDToDecimal(m_coproRegister[0]);
          int v1=BCDToDecimal(m_coproRegister[1]);

          /* Repack vcd */
          m_coproRegister[5]=DecimalToBCD(v0*v1);

          if (ARM_DEBUG_COPRO)
          logerror("Cmd:  Multiply 0 * 1, result in 5 (%08x * %08x == %08x)\n", v0, v1, m_coproRegister[5]);
        }
        else if (m_coproRegister[crn]==3)
        {
          /* Unpack BCD */
          int v0=BCDToDecimal(m_coproRegister[0]);
          int v1=BCDToDecimal(m_coproRegister[1]);

          /* Repack vcd */
          m_coproRegister[5]=DecimalToBCD(v0-v1);

          if (ARM_DEBUG_COPRO)
          logerror("Cmd:  Sub 0 - 1, result in 5 (%08x - %08x == %08x)\n", v0, v1, m_coproRegister[5]);
        }
        else
        {
          logerror("Unknown bcd copro command %08x\n", m_coproRegister[crn]);
        }
      }

      if (ARM_DEBUG_COPRO)
      logerror("%08x:  Copro write R%u (%08x) to CR%u\n", R15, rn, GetRegister(rn), crn);
    }
    /* CDP - perform copro operation */
    else if( (insn&0x0f000010)==0x0e000000 )
    {
      /* Data East 156 copro specific divider - result in reg 3/4 */
      if (m_coproRegister[1])
      {
        m_coproRegister[3]=m_coproRegister[0] / m_coproRegister[1];
        m_coproRegister[4]=m_coproRegister[0] % m_coproRegister[1];
      }
      else
      {
        /* Unverified */
        m_coproRegister[3]=0xffffffff;
        m_coproRegister[4]=0xffffffff;
      }

      if (ARM_DEBUG_COPRO)
      logerror("%08x:  Copro cdp (%08x) (3==> %08x, 4==> %08x)\n", R15, insn, m_coproRegister[3], m_coproRegister[4]);
    }
    else
    {
      logerror("%08x:  Unimplemented copro instruction %08x\n", R15, insn);
    }
  }
