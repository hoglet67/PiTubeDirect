/*
   i386 Disassembler

   Written by Ville Linde
   NEC V-Series support by Bryan McPhail (currently incomplete)
*/

//#include "windows.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <inttypes.h>

#define LSB_FIRST

/* disassembler constants */
#define DASMFLAG_SUPPORTED		0x80000000	/* are disassembly flags supported? */
#define DASMFLAG_STEP_OUT		0x40000000	/* this instruction should be the end of a step out sequence */
#define DASMFLAG_STEP_OVER		0x20000000	/* this instruction should be stepped over by setting a breakpoint afterwards */
#define DASMFLAG_OVERINSTMASK	0x18000000	/* number of extra instructions to skip when stepping over */
#define DASMFLAG_OVERINSTSHIFT	27			/* bits to shift after masking to get the value */
#define DASMFLAG_LENGTHMASK 	0x0000ffff	/* the low 16-bits contain the actual length */
#define DASMFLAG_STEP_OVER_EXTRA(x)			((x) << DASMFLAG_OVERINSTSHIFT)

//#include "osd_cpu.h"

typedef unsigned int                        offs_t;
#define INLINE

// When opcodes 63h,64h,65h,66h,67h,F1h,FEh xx111xxxb and FFh xx111xxxb
// are executed, the 80186,188 will execute an illegal instruction
// exception (interrupt type 6)
#define PATCH_FOR_80186

extern uint8_t *RAM;

/* ----- opcode and opcode argument reading ----- */
INLINE uint8_t  cpu_readop(offs_t A)			{ return (RAM[(A) & 0xFFFFF]); }
INLINE uint8_t  cpu_readop_arg(offs_t A)			{ return (RAM[(A) & 0xFFFFF]); }

enum {
	PARAM_REG = 1,		/* 16 or 32-bit register */
	PARAM_REG8,			/* 8-bit register */
	PARAM_REG16,		/* 16-bit register */
	PARAM_RM,			/* 16 or 32-bit memory or register */
	PARAM_RM8,			/* 8-bit memory or register */
	PARAM_RM16,			/* 16-bit memory or register */
	PARAM_I8,			/* 8-bit immediate */
	PARAM_I16,			/* 16-bit immediate */
	PARAM_IMM,			/* 16 or 32-bit immediate */
	PARAM_ADDR,			/* 16:16 or 16:32 address */
	PARAM_REL,			/* 16 or 32-bit PC-relative displacement */
	PARAM_REL8,			/* 8-bit PC-relative displacement */
	PARAM_MEM_OFFS_B,	/* 8-bit mem offset */
	PARAM_MEM_OFFS_V,	/* 16 or 32-bit mem offset */
	PARAM_SREG,			/* segment register */
	PARAM_CREG,			/* control register */
	PARAM_DREG,			/* debug register */
	PARAM_TREG,			/* test register */
	PARAM_1,			/* used by shift/rotate instructions */
	PARAM_AL,
	PARAM_CL,
	PARAM_DL,
	PARAM_BL,
	PARAM_AH,
	PARAM_CH,
	PARAM_DH,
	PARAM_BH,
	PARAM_DX,
	PARAM_EAX,			/* EAX or AX */
	PARAM_ECX,			/* ECX or CX */
	PARAM_EDX,			/* EDX or DX */
	PARAM_EBX,			/* EBX or BX */
	PARAM_ESP,			/* ESP or SP */
	PARAM_EBP,			/* EBP or BP */
	PARAM_ESI,			/* ESI or SI */
	PARAM_EDI,			/* EDI or DI */
};

enum {
	MODRM = 1,
	GROUP,
	VAR_NAME,
	OP_SIZE,
	ADDR_SIZE,
	TWO_BYTE,
	PREFIX,
	SEG_CS,
	SEG_DS,
	SEG_ES,
	SEG_FS,
	SEG_GS,
	SEG_SS
};

typedef struct {
	char mnemonic[32];
	uint32_t flags;
	uint32_t param1;
	uint32_t param2;
	uint32_t param3;
	offs_t dasm_flags;
} I386_OPCODE;

typedef struct {
	char mnemonic[32];
	I386_OPCODE *opcode;
} GROUP_OP;

static I386_OPCODE *opcode_table1 = 0;
static I386_OPCODE *opcode_table2 = 0;

static I386_OPCODE i386_opcode_table1[256] =
{
	// 0x00
	{"add",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"add",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"add",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"add",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"add",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"add",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"push es",			0,				0,					0,					0				},
	{"pop es",			0,				0,					0,					0				},
	{"or",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"or",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"or",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"or",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"or",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"or",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"push cs",			0,				0,					0,					0				},
	{"two_byte",		TWO_BYTE,		0,					0,					0				},
	// 0x10
	{"adc",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"adc",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"adc",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"adc",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"adc",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"adc",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"push ss",			0,				0,					0,					0				},
	{"pop ss",			0,				0,					0,					0				},
	{"sbb",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"sbb",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"sbb",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"sbb",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"sbb",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"sbb",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"push ds",			0,				0,					0,					0				},
	{"pop ds",			0,				0,					0,					0				},
	// 0x20
	{"and",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"and",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"and",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"and",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"and",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"and",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"seg_es",			SEG_ES,			0,					0,					0				},
	{"daa",				0,				0,					0,					0				},
	{"sub",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"sub",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"sub",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"sub",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"sub",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"sub",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"seg_cs",			SEG_CS,			0,					0,					0				},
	{"das",				0,				0,					0,					0				},
	// 0x30
	{"xor",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"xor",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"xor",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"xor",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"xor",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"xor",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"seg_ss",			SEG_SS,			0,					0,					0				},
	{"aaa",				0,				0,					0,					0				},
	{"cmp",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"cmp",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"cmp",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"cmp",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmp",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"cmp",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"seg_ds",			SEG_DS,			0,					0,					0				},
	{"aas",				0,				0,					0,					0				},
	// 0x40
	{"inc",				0,				PARAM_EAX,			0,					0				},
	{"inc",				0,				PARAM_ECX,			0,					0				},
	{"inc",				0,				PARAM_EDX,			0,					0				},
	{"inc",				0,				PARAM_EBX,			0,					0				},
	{"inc",				0,				PARAM_ESP,			0,					0				},
	{"inc",				0,				PARAM_EBP,			0,					0				},
	{"inc",				0,				PARAM_ESI,			0,					0				},
	{"inc",				0,				PARAM_EDI,			0,					0				},
	{"dec",				0,				PARAM_EAX,			0,					0				},
	{"dec",				0,				PARAM_ECX,			0,					0				},
	{"dec",				0,				PARAM_EDX,			0,					0				},
	{"dec",				0,				PARAM_EBX,			0,					0				},
	{"dec",				0,				PARAM_ESP,			0,					0				},
	{"dec",				0,				PARAM_EBP,			0,					0				},
	{"dec",				0,				PARAM_ESI,			0,					0				},
	{"dec",				0,				PARAM_EDI,			0,					0				},
	// 0x50
	{"push",			0,				PARAM_EAX,			0,					0				},
	{"push",			0,				PARAM_ECX,			0,					0				},
	{"push",			0,				PARAM_EDX,			0,					0				},
	{"push",			0,				PARAM_EBX,			0,					0				},
	{"push",			0,				PARAM_ESP,			0,					0				},
	{"push",			0,				PARAM_EBP,			0,					0				},
	{"push",			0,				PARAM_ESI,			0,					0				},
	{"push",			0,				PARAM_EDI,			0,					0				},
	{"pop",				0,				PARAM_EAX,			0,					0				},
	{"pop",				0,				PARAM_ECX,			0,					0				},
	{"pop",				0,				PARAM_EDX,			0,					0				},
	{"pop",				0,				PARAM_EBX,			0,					0				},
	{"pop",				0,				PARAM_ESP,			0,					0				},
	{"pop",				0,				PARAM_EBP,			0,					0				},
	{"pop",				0,				PARAM_ESI,			0,					0				},
	{"pop",				0,				PARAM_EDI,			0,					0				},
	// 0x60
	{"pusha\0pushad",	VAR_NAME,		0,					0,					0				},
	{"popa\0popad",		VAR_NAME,		0,					0,					0				},
	{"bound",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
#ifdef PATCH_FOR_80186
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
#else
	{"arpl",			MODRM,			PARAM_RM,			PARAM_REG16,		0				},
	{"seg_fs",			SEG_FS,			0,					0,					0				},
	{"seg_gs",			SEG_GS,			0,					0,					0				},
	{"op_size",			OP_SIZE,		0,					0,					0				},
	{"addr_size",		ADDR_SIZE,		0,					0,					0				},
#endif
	{"push",			0,				PARAM_IMM,			0,					0				},
	{"imul",			MODRM,			PARAM_REG,			PARAM_RM,			PARAM_IMM		},
	{"push",			0,				PARAM_I8,			0,					0				},
	{"imul",			MODRM,			PARAM_REG,			PARAM_RM,			PARAM_I8		},
	{"insb",			0,				0,					0,					0				},
	{"insw\0insd",		VAR_NAME,		0,					0,					0				},
	{"outsb",			0,				0,					0,					0				},
	{"outsw\0outsd",	VAR_NAME,		0,					0,					0				},
	// 0x70
	{"jo",				0,				PARAM_REL8,			0,					0				},
	{"jno",				0,				PARAM_REL8,			0,					0				},
	{"jb",				0,				PARAM_REL8,			0,					0				},
	{"jae",				0,				PARAM_REL8,			0,					0				},
	{"je",				0,				PARAM_REL8,			0,					0				},
	{"jne",				0,				PARAM_REL8,			0,					0				},
	{"jbe",				0,				PARAM_REL8,			0,					0				},
	{"ja",				0,				PARAM_REL8,			0,					0				},
	{"js",				0,				PARAM_REL8,			0,					0				},
	{"jns",				0,				PARAM_REL8,			0,					0				},
	{"jp",				0,				PARAM_REL8,			0,					0				},
	{"jnp",				0,				PARAM_REL8,			0,					0				},
	{"jl",				0,				PARAM_REL8,			0,					0				},
	{"jge",				0,				PARAM_REL8,			0,					0				},
	{"jle",				0,				PARAM_REL8,			0,					0				},
	{"jg",				0,				PARAM_REL8,			0,					0				},
	// 0x80
	{"group80",			GROUP,			0,					0,					0				},
	{"group81",			GROUP,			0,					0,					0				},
	{"group80",			GROUP,			0,					0,					0				},
	{"group83",			GROUP,			0,					0,					0				},
	{"test",			MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"test",			MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"xchg",			MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"xchg",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"mov",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"mov",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"mov",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"mov",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"mov",				MODRM,			PARAM_RM,			PARAM_SREG,			0				},
	{"lea",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"mov",				MODRM,			PARAM_SREG,			PARAM_RM,			0				},
	{"pop",				MODRM,			PARAM_RM,			0,					0				},
	// 0x90
	{"nop",				0,				0,					0,					0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_ECX,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_EDX,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_EBX,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_ESP,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_EBP,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_ESI,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_EDI,			0				},
	{"cbw\0cwde",		VAR_NAME,		0,					0,					0				},
	{"cwd\0cdq",		VAR_NAME,		0,					0,					0				},
	{"call",			0,				PARAM_ADDR,			0,					0,				DASMFLAG_STEP_OVER},
	{"wait",			0,				0,					0,					0				},
	{"pushf\0pushfd",	VAR_NAME,		0,					0,					0				},
	{"popf\0popfd",		VAR_NAME,		0,					0,					0				},
	{"sahf",			0,				0,					0,					0				},
	{"lahf",			0,				0,					0,					0				},
	// 0xa0
	{"mov",				0,				PARAM_AL,			PARAM_MEM_OFFS_V,	0				},
	{"mov",				0,				PARAM_EAX,			PARAM_MEM_OFFS_V,	0				},
	{"mov",				0,				PARAM_MEM_OFFS_V,	PARAM_AL,			0				},
	{"mov",				0,				PARAM_MEM_OFFS_V,	PARAM_EAX,			0				},
	{"movsb",			0,				0,					0,					0				},
	{"movsw\0movsd",	VAR_NAME,		0,					0,					0				},
	{"cmpsb",			0,				0,					0,					0				},
	{"cmpsw\0cmpsd",	VAR_NAME,		0,					0,					0				},
	{"test",			0,				PARAM_AL,			PARAM_I8,			0				},
	{"test",			0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"stosb",			0,				0,					0,					0				},
	{"stosw\0stosd",	VAR_NAME,		0,					0,					0				},
	{"lodsb",			0,				0,					0,					0				},
	{"lodsw\0lodsd",	VAR_NAME,		0,					0,					0				},
	{"scasb",			0,				0,					0,					0				},
	{"scasw\0scasd",	VAR_NAME,		0,					0,					0				},
	// 0xb0
	{"mov",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"mov",				0,				PARAM_CL,			PARAM_I8,			0				},
	{"mov",				0,				PARAM_DL,			PARAM_I8,			0				},
	{"mov",				0,				PARAM_BL,			PARAM_I8,			0				},
	{"mov",				0,				PARAM_AH,			PARAM_I8,			0				},
	{"mov",				0,				PARAM_CH,			PARAM_I8,			0				},
	{"mov",				0,				PARAM_DH,			PARAM_I8,			0				},
	{"mov",				0,				PARAM_BH,			PARAM_I8,			0				},
	{"mov",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"mov",				0,				PARAM_ECX,			PARAM_IMM,			0				},
	{"mov",				0,				PARAM_EDX,			PARAM_IMM,			0				},
	{"mov",				0,				PARAM_EBX,			PARAM_IMM,			0				},
	{"mov",				0,				PARAM_ESP,			PARAM_IMM,			0				},
	{"mov",				0,				PARAM_EBP,			PARAM_IMM,			0				},
	{"mov",				0,				PARAM_ESI,			PARAM_IMM,			0				},
	{"mov",				0,				PARAM_EDI,			PARAM_IMM,			0				},
	// 0xc0
	{"groupC0",			GROUP,			0,					0,					0				},
	{"groupC1",			GROUP,			0,					0,					0				},
	{"ret",				0,				PARAM_I16,			0,					0,				DASMFLAG_STEP_OUT},
	{"ret",				0,				0,					0,					0,				DASMFLAG_STEP_OUT},
	{"les",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"lds",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"mov",				MODRM,			PARAM_RM8,			PARAM_I8,			0				},
	{"mov",				MODRM,			PARAM_RM,			PARAM_IMM,			0				},
	{"enter",			0,				PARAM_I16,			PARAM_I8,			0				},
	{"leave",			0,				0,					0,					0				},
	{"retf",			0,				PARAM_I16,			0,					0,				DASMFLAG_STEP_OUT},
	{"retf",			0,				0,					0,					0,				DASMFLAG_STEP_OUT},
	{"int 3",			0,				0,					0,					0,				DASMFLAG_STEP_OVER},
	{"int",				0,				PARAM_I8,			0,					0,				DASMFLAG_STEP_OVER},
	{"into",			0,				0,					0,					0				},
	{"iret",			0,				0,					0,					0,				DASMFLAG_STEP_OUT},
	// 0xd0
	{"groupD0",			GROUP,			0,					0,					0				},
	{"groupD1",			GROUP,			0,					0,					0				},
	{"groupD2",			GROUP,			0,					0,					0				},
	{"groupD3",			GROUP,			0,					0,					0				},
	{"aam",				0,				PARAM_I8,			0,					0				},
	{"aad",				0,				PARAM_I8,			0,					0				},
	{"???",				0,				0,					0,					0				},
	{"xlat",			0,				0,					0,					0				},
	{"escape",			GROUP,			0,					0,					0				},
	{"escape",			GROUP,			0,					0,					0				},
	{"escape",			GROUP,			0,					0,					0				},
	{"escape",			GROUP,			0,					0,					0				},
	{"escape",			GROUP,			0,					0,					0				},
	{"escape",			GROUP,			0,					0,					0				},
	{"escape",			GROUP,			0,					0,					0				},
	{"escape",			GROUP,			0,					0,					0				},
	// 0xe0
	{"loopne",			0,				PARAM_REL8,			0,					0				},
	{"loopz",			0,				PARAM_REL8,			0,					0				},
	{"loop",			0,				PARAM_REL8,			0,					0				},
	{"jcxz\0jecxz",		VAR_NAME,		PARAM_REL8,			0,					0				},
	{"in",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"in",				0,				PARAM_EAX,			PARAM_I8,			0				},
	{"out",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"out",				0,				PARAM_EAX,			PARAM_I8,			0				},
	{"call",			0,				PARAM_REL,			0,					0,				DASMFLAG_STEP_OVER},
	{"jmp",				0,				PARAM_REL,			0,					0				},
	{"jmp",				0,				PARAM_ADDR,			0,					0				},
	{"jmp",				0,				PARAM_REL8,			0,					0				},
	{"in",				0,				PARAM_AL,			PARAM_DX,			0				},
	{"in",				0,				PARAM_EAX,			PARAM_DX,			0				},
	{"out",				0,				PARAM_AL,			PARAM_DX,			0				},
	{"out",				0,				PARAM_EAX,			PARAM_DX,			0				},
	// 0xf0
	{"lock",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"repne",			PREFIX,			0,					0,					0				},
	{"rep",				PREFIX,			0,					0,					0				},
	{"hlt",				0,				0,					0,					0				},
	{"cmc",				0,				0,					0,					0				},
	{"groupF6",			GROUP,			0,					0,					0				},
	{"groupF7",			GROUP,			0,					0,					0				},
	{"clc",				0,				0,					0,					0				},
	{"stc",				0,				0,					0,					0				},
	{"cli",				0,				0,					0,					0				},
	{"sti",				0,				0,					0,					0				},
	{"cld",				0,				0,					0,					0				},
	{"std",				0,				0,					0,					0				},
	{"groupFE",			GROUP,			0,					0,					0				},
	{"groupFF",			GROUP,			0,					0,					0				}
};

static I386_OPCODE i386_opcode_table2[256] =
{
	// 0x00
	{"group0F00",		GROUP,			0,					0,					0				},
	{"group0F01",		GROUP,			0,					0,					0				},
	{"lar",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"lsl",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"clts",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"ud2",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x10
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x20
	{"mov",				MODRM,			PARAM_REG,			PARAM_CREG,			0				},
	{"mov",				MODRM,			PARAM_REG,			PARAM_DREG,			0				},
	{"mov",				MODRM,			PARAM_CREG,			PARAM_REG,			0				},
	{"mov",				MODRM,			PARAM_DREG,			PARAM_REG,			0				},
	{"mov",				MODRM,			PARAM_REG,			PARAM_TREG,			0				},
	{"???",				0,				0,					0,					0				},
	{"mov",				MODRM,			PARAM_TREG,			PARAM_REG,			0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x30
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x40
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x50
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x60
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x70
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x80
	{"jo",				0,				PARAM_REL,			0,					0				},
	{"jno",				0,				PARAM_REL,			0,					0				},
	{"jb",				0,				PARAM_REL,			0,					0				},
	{"jae",				0,				PARAM_REL,			0,					0				},
	{"je",				0,				PARAM_REL,			0,					0				},
	{"jne",				0,				PARAM_REL,			0,					0				},
	{"jbe",				0,				PARAM_REL,			0,					0				},
	{"ja",				0,				PARAM_REL,			0,					0				},
	{"js",				0,				PARAM_REL,			0,					0				},
	{"jns",				0,				PARAM_REL,			0,					0				},
	{"jp",				0,				PARAM_REL,			0,					0				},
	{"jnp",				0,				PARAM_REL,			0,					0				},
	{"jl",				0,				PARAM_REL,			0,					0				},
	{"jge",				0,				PARAM_REL,			0,					0				},
	{"jle",				0,				PARAM_REL,			0,					0				},
	{"jg",				0,				PARAM_REL,			0,					0				},
	// 0x90
	{"seto",			MODRM,			PARAM_RM8,			0,					0				},
	{"setno",			MODRM,			PARAM_RM8,			0,					0				},
	{"setb",			MODRM,			PARAM_RM8,			0,					0				},
	{"setae",			MODRM,			PARAM_RM8,			0,					0				},
	{"sete",			MODRM,			PARAM_RM8,			0,					0				},
	{"setne",			MODRM,			PARAM_RM8,			0,					0				},
	{"setbe",			MODRM,			PARAM_RM8,			0,					0				},
	{"seta",			MODRM,			PARAM_RM8,			0,					0				},
	{"sets",			MODRM,			PARAM_RM8,			0,					0				},
	{"setns",			MODRM,			PARAM_RM8,			0,					0				},
	{"setp",			MODRM,			PARAM_RM8,			0,					0				},
	{"setnp",			MODRM,			PARAM_RM8,			0,					0				},
	{"setl",			MODRM,			PARAM_RM8,			0,					0				},
	{"setge",			MODRM,			PARAM_RM8,			0,					0				},
	{"setle",			MODRM,			PARAM_RM8,			0,					0				},
	{"setg",			MODRM,			PARAM_RM8,			0,					0				},
	// 0xa0
	{"push fs",			0,				0,					0,					0				},
	{"pop fs",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"bt",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"shld",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_I8		},
	{"shld",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_CL		},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"push gs",			0,				0,					0,					0				},
	{"pop gs",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"bts",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"shrd",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_I8		},
	{"shrd",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_CL		},
	{"???",				0,				0,					0,					0				},
	{"imul",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	// 0xb0
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"lss",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"btr",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"lfs",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"lgs",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"movzx",			MODRM,			PARAM_REG,			PARAM_RM8,			0				},
	{"movzx",			MODRM,			PARAM_REG,			PARAM_RM16,			0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"group0FBA",		GROUP,			0,					0,					0				},
	{"btc",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"bsf",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"bsr",				MODRM,			PARAM_REG,			PARAM_RM,			0,				DASMFLAG_STEP_OVER},
	{"movsx",			MODRM,			PARAM_REG,			PARAM_RM8,			0				},
	{"movsx",			MODRM,			PARAM_REG,			PARAM_RM16,			0				},
	// 0xc0
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0xd0
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0xe0
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0xf0
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				}
};

//## Not used, so commented out.
/*
static I386_OPCODE necv_opcode_table2[256] =
{
	// 0x00
	{"group0F00",		GROUP,			0,					0,					0				},
	{"group0F01",		GROUP,			0,					0,					0				},
	{"lar",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"lsl",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"clts",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"ud2",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x10 - NEC V series only
	{"test1",			0,				PARAM_RM8,			PARAM_1,			0				},
	{"test1",			0,				PARAM_RM16,			PARAM_1,			0				},
	{"clr1",			0,				PARAM_RM8,			PARAM_1,			0				},
	{"clr1",			0,				PARAM_RM16,			PARAM_1,			0				},
	{"set1",			0,				PARAM_RM8,			PARAM_1,			0				},
	{"set1",			0,				PARAM_RM16,			PARAM_1,			0				},
	{"not1",			0,				PARAM_RM8,			PARAM_1,			0				},
	{"not1",			0,				PARAM_RM16,			PARAM_1,			0				},
	{"test1",			0,				PARAM_RM8,			PARAM_I8,			0				},
	{"test1",			0,				PARAM_RM16,			PARAM_I8,			0				},
	{"clr1",			0,				PARAM_RM8,			PARAM_I8,			0				},
	{"clr1",			0,				PARAM_RM16,			PARAM_I8,			0				},
	{"set1",			0,				PARAM_RM8,			PARAM_I8,			0				},
	{"set1",			0,				PARAM_RM16,			PARAM_I8,			0				},
	{"not1",			0,				PARAM_RM8,			PARAM_I8,			0				},
	{"not1",			0,				PARAM_RM16,			PARAM_I8,			0				},
	// 0x20
	{"mov",				MODRM,			PARAM_REG,			PARAM_CREG,			0				},
	{"mov",				MODRM,			PARAM_REG,			PARAM_DREG,			0				},
	{"mov",				MODRM,			PARAM_CREG,			PARAM_REG,			0				},
	{"mov",				MODRM,			PARAM_DREG,			PARAM_REG,			0				},
	{"mov",				MODRM,			PARAM_REG,			PARAM_TREG,			0				},
	{"???",				0,				0,					0,					0				},
	{"mov",				MODRM,			PARAM_TREG,			PARAM_REG,			0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x30
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x40
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x50
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x60
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x70
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x80
	{"jo",				0,				PARAM_REL,			0,					0				},
	{"jno",				0,				PARAM_REL,			0,					0				},
	{"jb",				0,				PARAM_REL,			0,					0				},
	{"jae",				0,				PARAM_REL,			0,					0				},
	{"je",				0,				PARAM_REL,			0,					0				},
	{"jne",				0,				PARAM_REL,			0,					0				},
	{"jbe",				0,				PARAM_REL,			0,					0				},
	{"ja",				0,				PARAM_REL,			0,					0				},
	{"js",				0,				PARAM_REL,			0,					0				},
	{"jns",				0,				PARAM_REL,			0,					0				},
	{"jp",				0,				PARAM_REL,			0,					0				},
	{"jnp",				0,				PARAM_REL,			0,					0				},
	{"jl",				0,				PARAM_REL,			0,					0				},
	{"jge",				0,				PARAM_REL,			0,					0				},
	{"jle",				0,				PARAM_REL,			0,					0				},
	{"jg",				0,				PARAM_REL,			0,					0				},
	// 0x90
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"fint",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0xa0
	{"push fs",			0,				0,					0,					0				},
	{"pop fs",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"bt",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"shld",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_I8		},
	{"shld",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_CL		},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"push gs",			0,				0,					0,					0				},
	{"pop gs",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"bts",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"shrd",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_I8		},
	{"shrd",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_CL		},
	{"???",				0,				0,					0,					0				},
	{"imul",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	// 0xb0
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"lss",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"btr",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"lfs",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"lgs",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"movzx",			MODRM,			PARAM_REG,			PARAM_RM8,			0				},
	{"movzx",			MODRM,			PARAM_REG,			PARAM_RM16,			0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"group0FBA",		GROUP,			0,					0,					0				},
	{"btc",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"bsf",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"bsr",				MODRM,			PARAM_REG,			PARAM_RM,			0,				DASMFLAG_STEP_OVER},
	{"movsx",			MODRM,			PARAM_REG,			PARAM_RM8,			0				},
	{"movsx",			MODRM,			PARAM_REG,			PARAM_RM16,			0				},
	// 0xc0
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0xd0
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0xe0
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0xf0
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				}
};
*/

static I386_OPCODE group80_table[8] =
{
	{"add",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"or",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"adc",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"sbb",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"and",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"sub",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"xor",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"cmp",				0,				PARAM_RM8,			PARAM_I8,			0				}
};

static I386_OPCODE group81_table[8] =
{
	{"add",				0,				PARAM_RM,			PARAM_IMM,			0				},
	{"or",				0,				PARAM_RM,			PARAM_IMM,			0				},
	{"adc",				0,				PARAM_RM,			PARAM_IMM,			0				},
	{"sbb",				0,				PARAM_RM,			PARAM_IMM,			0				},
	{"and",				0,				PARAM_RM,			PARAM_IMM,			0				},
	{"sub",				0,				PARAM_RM,			PARAM_IMM,			0				},
	{"xor",				0,				PARAM_RM,			PARAM_IMM,			0				},
	{"cmp",				0,				PARAM_RM,			PARAM_IMM,			0				}
};

static I386_OPCODE group83_table[8] =
{
	{"add",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"or",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"adc",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"sbb",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"and",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"sub",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"xor",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"cmp",				0,				PARAM_RM,			PARAM_I8,			0				}
};

static I386_OPCODE groupC0_table[8] =
{
	{"rol",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"ror",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"rcl",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"rcr",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"shl",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"shr",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"sal",				0,				PARAM_RM8,			PARAM_I8,			0				},
	{"sar",				0,				PARAM_RM8,			PARAM_I8,			0				}
};

static I386_OPCODE groupC1_table[8] =
{
	{"rol",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"ror",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"rcl",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"rcr",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"shl",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"shr",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"sal",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"sar",				0,				PARAM_RM,			PARAM_I8,			0				}
};

static I386_OPCODE groupD0_table[8] =
{
	{"rol",				0,				PARAM_RM8,			PARAM_1,			0				},
	{"ror",				0,				PARAM_RM8,			PARAM_1,			0				},
	{"rcl",				0,				PARAM_RM8,			PARAM_1,			0				},
	{"rcr",				0,				PARAM_RM8,			PARAM_1,			0				},
	{"shl",				0,				PARAM_RM8,			PARAM_1,			0				},
	{"shr",				0,				PARAM_RM8,			PARAM_1,			0				},
	{"sal",				0,				PARAM_RM8,			PARAM_1,			0				},
	{"sar",				0,				PARAM_RM8,			PARAM_1,			0				}
};

static I386_OPCODE groupD1_table[8] =
{
	{"rol",				0,				PARAM_RM,			PARAM_1,			0				},
	{"ror",				0,				PARAM_RM,			PARAM_1,			0				},
	{"rcl",				0,				PARAM_RM,			PARAM_1,			0				},
	{"rcr",				0,				PARAM_RM,			PARAM_1,			0				},
	{"shl",				0,				PARAM_RM,			PARAM_1,			0				},
	{"shr",				0,				PARAM_RM,			PARAM_1,			0				},
	{"sal",				0,				PARAM_RM,			PARAM_1,			0				},
	{"sar",				0,				PARAM_RM,			PARAM_1,			0				}
};

static I386_OPCODE groupD2_table[8] =
{
	{"rol",				0,				PARAM_RM8,			PARAM_CL,			0				},
	{"ror",				0,				PARAM_RM8,			PARAM_CL,			0				},
	{"rcl",				0,				PARAM_RM8,			PARAM_CL,			0				},
	{"rcr",				0,				PARAM_RM8,			PARAM_CL,			0				},
	{"shl",				0,				PARAM_RM8,			PARAM_CL,			0				},
	{"shr",				0,				PARAM_RM8,			PARAM_CL,			0				},
	{"sal",				0,				PARAM_RM8,			PARAM_CL,			0				},
	{"sar",				0,				PARAM_RM8,			PARAM_CL,			0				}
};

static I386_OPCODE groupD3_table[8] =
{
	{"rol",				0,				PARAM_RM,			PARAM_CL,			0				},
	{"ror",				0,				PARAM_RM,			PARAM_CL,			0				},
	{"rcl",				0,				PARAM_RM,			PARAM_CL,			0				},
	{"rcr",				0,				PARAM_RM,			PARAM_CL,			0				},
	{"shl",				0,				PARAM_RM,			PARAM_CL,			0				},
	{"shr",				0,				PARAM_RM,			PARAM_CL,			0				},
	{"sal",				0,				PARAM_RM,			PARAM_CL,			0				},
	{"sar",				0,				PARAM_RM,			PARAM_CL,			0				}
};

static I386_OPCODE groupF6_table[8] =
{
	{"test",			0,				PARAM_RM8,			PARAM_I8,			0				},
	{"???",				0,				0,					0,					0				},
	{"not",				0,				PARAM_RM8,			0,					0				},
	{"neg",				0,				PARAM_RM8,			0,					0				},
	{"mul",				0,				PARAM_RM8,			0,					0				},
	{"imul",			0,				PARAM_RM8,			0,					0				},
	{"div",				0,				PARAM_RM8,			0,					0				},
	{"idiv",			0,				PARAM_RM8,			0,					0				}
};

static I386_OPCODE groupF7_table[8] =
{
	{"test",			0,				PARAM_RM,			PARAM_IMM,			0				},
	{"???",				0,				0,					0,					0				},
	{"not",				0,				PARAM_RM,			0,					0				},
	{"neg",				0,				PARAM_RM,			0,					0				},
	{"mul",				0,				PARAM_RM,			0,					0				},
	{"imul",			0,				PARAM_RM,			0,					0				},
	{"div",				0,				PARAM_RM,			0,					0				},
	{"idiv",			0,				PARAM_RM,			0,					0				}
};

static I386_OPCODE groupFE_table[8] =
{
	{"inc",				0,				PARAM_RM8,			0,					0				},
	{"dec",				0,				PARAM_RM8,			0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				}
};

static I386_OPCODE groupFF_table[8] =
{
	{"inc",				0,				PARAM_RM,			0,					0				},
	{"dec",				0,				PARAM_RM,			0,					0				},
	{"call",			0,				PARAM_RM,			0,					0,				DASMFLAG_STEP_OVER},
	{"call",			0,				PARAM_RM,			0,					0,				DASMFLAG_STEP_OVER},
	{"jmp",				0,				PARAM_RM,			0,					0				},
	{"jmp",				0,				PARAM_RM,			0,					0				},
	{"push",			0,				PARAM_RM,			0,					0				},
	{"???",				0,				0,					0,					0				}
};

static I386_OPCODE group0F00_table[8] =
{
	{"sldt",			0,				PARAM_RM,			0,					0				},
	{"str",				0,				PARAM_RM,			0,					0				},
	{"lldt",			0,				PARAM_RM,			0,					0				},
	{"ltr",				0,				PARAM_RM,			0,					0				},
	{"verr",			0,				PARAM_RM,			0,					0				},
	{"verw",			0,				PARAM_RM,			0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				}
};

static I386_OPCODE group0F01_table[8] =
{
	{"sgdt",			0,				PARAM_RM,			0,					0				},
	{"sidt",			0,				PARAM_RM,			0,					0				},
	{"lgdt",			0,				PARAM_RM,			0,					0				},
	{"lidt",			0,				PARAM_RM,			0,					0				},
	{"smsw",			0,				PARAM_RM,			0,					0				},
	{"???",				0,				0,					0,					0				},
	{"lmsw",			0,				PARAM_RM,			0,					0				},
	{"???",				0,				0,					0,					0				}
};

static I386_OPCODE group0FBA_table[8] =
{
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"bt",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"bts",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"btr",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"btc",				0,				PARAM_RM,			PARAM_I8,			0				}
};

static GROUP_OP group_op_table[] =
{
	{ "group80",			group80_table			},
	{ "group81",			group81_table			},
	{ "group83",			group83_table			},
	{ "groupC0",			groupC0_table			},
	{ "groupC1",			groupC1_table			},
	{ "groupD0",			groupD0_table			},
	{ "groupD1",			groupD1_table			},
	{ "groupD2",			groupD2_table			},
	{ "groupD3",			groupD3_table			},
	{ "groupF6",			groupF6_table			},
	{ "groupF7",			groupF7_table			},
	{ "groupFE",			groupFE_table			},
	{ "groupFF",			groupFF_table			},
	{ "group0F00",			group0F00_table			},
	{ "group0F01",			group0F01_table			},
	{ "group0FBA",			group0FBA_table			}
};



static char i386_reg[2][8][4] =
{
	{"ax",	"cx",	"dx",	"bx",	"sp",	"bp",	"si",	"di"},
	{"eax",	"ecx",	"edx",	"ebx",	"esp",	"ebp",	"esi",	"edi"}
};

static char i386_reg8[8][4] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
static char i386_sreg[8][4] = {"es", "cs", "ss", "ds", "fs", "gs", "???", "???"};

static int address_size;
static int operand_size;
static uint32_t pc;
static uint8_t modrm;
static uint32_t segment;
static offs_t dasm_flags;
static char modrm_string[256];

#define MODRM_REG1	((modrm >> 3) & 0x7)
#define MODRM_REG2	(modrm & 0x7)

INLINE uint8_t FETCH(void)
{
	pc++;
	return cpu_readop(pc-1);
}

INLINE uint16_t FETCH16(void)
{
	uint16_t d;
	d = cpu_readop(pc) | (cpu_readop(pc+1) << 8);
	pc += 2;
	return d;
}

INLINE uint32_t FETCH32(void)
{
	uint32_t d;
	d = cpu_readop(pc) | (cpu_readop(pc+1) << 8) | (cpu_readop(pc+2) << 16) | (cpu_readop(pc+3) << 24);
	pc += 4;
	return d;
}

INLINE uint8_t FETCHD(void)
{
	pc++;
	return cpu_readop_arg(pc-1);
}

INLINE uint16_t FETCHD16(void)
{
	uint16_t d;
	d = cpu_readop_arg(pc) | (cpu_readop_arg(pc+1) << 8);
	pc += 2;
	return d;
}

INLINE uint32_t FETCHD32(void)
{
	uint32_t d;
	d = cpu_readop_arg(pc) | (cpu_readop_arg(pc+1) << 8) | (cpu_readop_arg(pc+2) << 16) | (cpu_readop_arg(pc+3) << 24);
	pc += 4;
	return d;
}

static char* handle_sib_byte( char* s, uint8_t mod )
{
	uint32_t i32;
	uint8_t scale, i, base;
	uint8_t sib = FETCHD();
	scale = (sib >> 6) & 0x3;
	i = (sib >> 3) & 0x7;
	base = sib & 0x7;

	switch( base )
	{
		case 0: s += sprintf( s, "eax"); break;
		case 1: s += sprintf( s, "ecx"); break;
		case 2: s += sprintf( s, "edx"); break;
		case 3: s += sprintf( s, "ebx"); break;
		case 4: s += sprintf( s, "esp"); break;
		case 5:
			if( mod == 0 ) {
				i32 = FETCH32();
				s += sprintf( s, "$%08" PRIX32, i32 );
			} else if( mod == 1 ) {
				s += sprintf( s, "ebp" );
			} else if( mod == 2 ) {
				s += sprintf( s, "ebp" );
			}
			break;
		case 6: s += sprintf( s, "esi"); break;
		case 7: s += sprintf( s, "edi"); break;
	}
	switch( i )
	{
		case 0: s += sprintf( s, "+eax*%d", (1 << scale)); break;
		case 1: s += sprintf( s, "+ecx*%d", (1 << scale)); break;
		case 2: s += sprintf( s, "+edx*%d", (1 << scale)); break;
		case 3: s += sprintf( s, "+ebx*%d", (1 << scale)); break;
		case 4: break;
		case 5: s += sprintf( s, "+ebp*%d", (1 << scale)); break;
		case 6: s += sprintf( s, "+esi*%d", (1 << scale)); break;
		case 7: s += sprintf( s, "+edi*%d", (1 << scale)); break;
	}
	return s;
}

static void handle_modrm(char* s)
{
	int8_t disp8;
	int16_t disp16;
	int32_t disp32;
	uint8_t mod, rm;

	modrm = FETCHD();
	mod = (modrm >> 6) & 0x3;
	rm = modrm & 0x7;

	if( modrm >= 0xc0 )
		return;

	switch(segment)
	{
		case SEG_CS: s += sprintf( s, "cs:" ); break;
		case SEG_DS: s += sprintf( s, "ds:" ); break;
		case SEG_ES: s += sprintf( s, "es:" ); break;
		case SEG_FS: s += sprintf( s, "fs:" ); break;
		case SEG_GS: s += sprintf( s, "gs:" ); break;
		case SEG_SS: s += sprintf( s, "ss:" ); break;
	}

	s += sprintf( s, "[" );
	if( address_size ) {
		switch( rm )
		{
			case 0: s += sprintf( s, "eax" ); break;
			case 1: s += sprintf( s, "ecx" ); break;
			case 2: s += sprintf( s, "edx" ); break;
			case 3: s += sprintf( s, "ebx" ); break;
			case 4: s = handle_sib_byte( s, mod ); break;
			case 5:
				if( mod == 0 ) {
					disp32 = FETCHD32();
					s += sprintf( s, "$%08" PRIX32, disp32 );
				} else {
					s += sprintf( s, "ebp" );
				}
				break;
			case 6: s += sprintf( s, "esi" ); break;
			case 7: s += sprintf( s, "edi" ); break;
		}
		if( mod == 1 ) {
			disp8 = FETCHD();
			s += sprintf( s, "+$%08" PRIX32, (int32_t)disp8 );
		} else if( mod == 2 ) {
			disp32 = FETCHD32();
			s += sprintf( s, "+$%08" PRIX32, disp32 );
		}
	} else {
		switch( rm )
		{
			case 0: s += sprintf( s, "bx+si" ); break;
			case 1: s += sprintf( s, "bx+di" ); break;
			case 2: s += sprintf( s, "bx+si" ); break;
			case 3: s += sprintf( s, "bx+di" ); break;
			case 4: s += sprintf( s, "si" ); break;
			case 5: s += sprintf( s, "di" ); break;
			case 6:
				if( mod == 0 ) {
					disp16 = FETCHD16();
					s += sprintf( s, "$%04" PRIX16,  (uint16_t) disp16 );
				} else {
					s += sprintf( s, "bp" );
				}
				break;
			case 7: s += sprintf( s, "bx" ); break;
		}
		if( mod == 1 ) {
			disp8 = FETCHD();
			s += sprintf( s, "+$%08" PRIX32, (int32_t)disp8 );
		} else if( mod == 2 ) {
			disp16 = FETCHD16();
			s += sprintf( s, "+$%08" PRIX32, (int32_t)disp16 );
		}
	}
	sprintf( s, "]" );
}

static char* handle_param(char* s, uint32_t param)
{
	uint8_t i8;
	uint16_t i16;
	uint32_t i32;
	uint16_t ptr;
	uint32_t addr;
	int8_t d8;
	int16_t d16;
	int32_t d32;

	switch(param)
	{
		case PARAM_REG:
			s += sprintf( s, "%s", i386_reg[operand_size][MODRM_REG1] );
			break;

		case PARAM_REG8:
			s += sprintf( s, "%s", i386_reg8[MODRM_REG1] );
			break;

		case PARAM_REG16:
			s += sprintf( s, "%s", i386_reg[0][MODRM_REG1] );
			break;

		case PARAM_RM:
			if( modrm >= 0xc0 ) {
				s += sprintf( s, "%s", i386_reg[operand_size][MODRM_REG2] );
			} else {
				if( operand_size )
					s += sprintf( s, "dword " );
				else
					s += sprintf( s, "word " );
				s += sprintf( s, "%s", modrm_string );
			}
			break;

		case PARAM_RM8:
			if( modrm >= 0xc0 ) {
				s += sprintf( s, "%s", i386_reg8[MODRM_REG2] );
			} else {
				s += sprintf( s, "byte " );
				s += sprintf( s, "%s", modrm_string );
			}
			break;

		case PARAM_RM16:
			if( modrm >= 0xc0 ) {
				s += sprintf( s, "%s", i386_reg[0][MODRM_REG2] );
			} else {
				s += sprintf( s, "word " );
				s += sprintf( s, "%s", modrm_string );
			}
			break;

		case PARAM_I8:
			i8 = FETCHD();
			s += sprintf( s, "$%02" PRIX8, i8 );
			break;

		case PARAM_I16:
			i16 = FETCHD16();
			s += sprintf( s, "$%04" PRIX16, i16 );
			break;

		case PARAM_IMM:
			if( operand_size ) {
				i32 = FETCHD32();
				s += sprintf( s, "$%08" PRIX32, i32 );
			} else {
				i16 = FETCHD16();
				s += sprintf( s, "$%04" PRIX16, i16 );
			}
			break;

		case PARAM_ADDR:
			if( operand_size ) {
				addr = FETCHD32();
				ptr = FETCHD16();
				s += sprintf( s, "[$%04" PRIX16 ":%08" PRIX32 "]",ptr,addr );
			} else {
				addr = FETCHD16();
				ptr = FETCHD16();
				s += sprintf( s, "[$%04" PRIX16 ":%04" PRIX32 "]",ptr,addr );
			}
			break;

		case PARAM_REL:
			if( operand_size ) {
				d32 = FETCHD32();
				s += sprintf( s, "[$%08" PRIX32 "]", pc + d32 );
			} else {
				d16 = FETCHD16();
				s += sprintf( s, "[$%08" PRIX32 "]", pc + d16 );
			}
			break;

		case PARAM_REL8:
			d8 = FETCHD();
			s += sprintf( s, "[$%08" PRIX32 "]", pc + d8 );
			break;

		case PARAM_MEM_OFFS_B:
			d8 = FETCHD();
			s += sprintf( s, "[$%08" PRIX8 "]", d8 );
			break;

		case PARAM_MEM_OFFS_V:
			if( address_size ) {
				d32 = FETCHD32();
				s += sprintf( s, "[$%08" PRIX32 "]", d32 );
			} else {
				d32 = FETCHD16();
				s += sprintf( s, "[$%08" PRIX32 "]", d32 );
			}
			break;

		case PARAM_SREG:
			s += sprintf( s, "%s", i386_sreg[MODRM_REG1] );
			break;

		case PARAM_CREG:
			s += sprintf( s, "cr%d", MODRM_REG1 );
			break;

		case PARAM_1:
			s += sprintf( s, "1" );
			break;

		case PARAM_DX:
			s += sprintf( s, "dx" );
			break;

		case PARAM_AL: s += sprintf( s, "al" ); break;
		case PARAM_CL: s += sprintf( s, "cl" ); break;
		case PARAM_DL: s += sprintf( s, "dl" ); break;
		case PARAM_BL: s += sprintf( s, "bl" ); break;
		case PARAM_AH: s += sprintf( s, "ah" ); break;
		case PARAM_CH: s += sprintf( s, "ch" ); break;
		case PARAM_DH: s += sprintf( s, "dh" ); break;
		case PARAM_BH: s += sprintf( s, "bh" ); break;

		case PARAM_EAX: s += sprintf( s, "%s", i386_reg[operand_size][0] ); break;
		case PARAM_ECX: s += sprintf( s, "%s", i386_reg[operand_size][1] ); break;
		case PARAM_EDX: s += sprintf( s, "%s", i386_reg[operand_size][2] ); break;
		case PARAM_EBX: s += sprintf( s, "%s", i386_reg[operand_size][3] ); break;
		case PARAM_ESP: s += sprintf( s, "%s", i386_reg[operand_size][4] ); break;
		case PARAM_EBP: s += sprintf( s, "%s", i386_reg[operand_size][5] ); break;
		case PARAM_ESI: s += sprintf( s, "%s", i386_reg[operand_size][6] ); break;
		case PARAM_EDI: s += sprintf( s, "%s", i386_reg[operand_size][7] ); break;
	}
	return s;
}

static void decode_opcode(char *s, I386_OPCODE *op)
{
	int i;
	uint8_t op2;

	switch( op->flags )
	{
		case OP_SIZE:
			operand_size ^= 1;
			op2 = FETCH();
			decode_opcode( s, &opcode_table1[op2] );
			return;

		case ADDR_SIZE:
			address_size ^= 1;
			op2 = FETCH();
			decode_opcode( s, &opcode_table1[op2] );
			return;

		case TWO_BYTE:
			op2 = FETCHD();
			decode_opcode( s, &opcode_table2[op2] );
			return;

		case SEG_CS:
		case SEG_DS:
		case SEG_ES:
		case SEG_FS:
		case SEG_GS:
		case SEG_SS:
			segment = op->flags;
			op2 = FETCH();
			decode_opcode( s, &opcode_table1[op2] );
			return;

		case PREFIX:
			s += sprintf( s, "%s ", op->mnemonic );
			op2 = FETCH();
			decode_opcode( s, &opcode_table1[op2] );
			return;

		case VAR_NAME:
			if( operand_size ) {
				int p=0;
				while(op->mnemonic[p] != 0)
				{
					p++;
				};
				s += sprintf( s, "%s", &op->mnemonic[p+1] );
			} else {
				s += sprintf( s, "%s", op->mnemonic );
			}
			goto handle_params;

		case GROUP:
			handle_modrm( modrm_string );
			for( i=0; i < (int) (sizeof(group_op_table) / sizeof(GROUP_OP)); i++ ) {
				if(strcmp(op->mnemonic, group_op_table[i].mnemonic) == 0 ) {
					decode_opcode( s, &group_op_table[i].opcode[MODRM_REG1] );
					return;
				}
			}
			goto handle_unknown;

		case MODRM:
			handle_modrm( modrm_string );
			break;
	}

	s += sprintf( s, "%s ", op->mnemonic );
	dasm_flags = op->dasm_flags;

handle_params:
	if( op->param1 != 0 ) {
		s = handle_param( s, op->param1 );
	}

	if( op->param2 != 0 ) {
		s += sprintf( s, ", " );
		s = handle_param( s, op->param2 );
	}

	if( op->param3 != 0 ) {
		s += sprintf( s, ", " );
		handle_param( s, op->param3 );
	}
	return;

handle_unknown:
	sprintf(s, "???");
}

int i386_dasm_one(char *buffer, uint32_t eip, int addr_size, int op_size)
{
	uint8_t op;

	opcode_table1 = i386_opcode_table1;
	opcode_table2 = i386_opcode_table2;
	address_size = addr_size;
	operand_size = op_size;
	pc = eip;
	dasm_flags = 0;
	segment = 0;

	op = FETCH();

	decode_opcode( buffer, &opcode_table1[op] );
	return (pc-eip) | dasm_flags | DASMFLAG_SUPPORTED;
}

