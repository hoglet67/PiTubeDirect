#define CASE2(in) case (in): case ((in) | 0x80)
#define CASE4(in) case (in): case ((in) | 0x40): case ((in) | 0x80): case ((in) | 0xC0)

uint8_t FunctionLookup[256];

#define F_BASE(in) ((in) << 4)
enum Functions
{
   BEQ = F_BASE(Format0),
   BNE,
   BCS,
   BCC,
   BH,
   BLS,
   BGT,
   BLE,
   BFS,
   BFC,
   BLO,
   BHS,
   BLT,
   BGE,
   BR,
   BN,

   BSR = F_BASE(Format1),
   RET,
   CXP,
   RXP,
   RETT,
   RETI,
   SAVE,
   RESTORE,
   ENTER,
   EXIT,
   NOP,
   WAIT,
   DIA,
   FLAG,
   SVC,
   BPT,

   ADDQ = F_BASE(Format2),
   CMPQ,
   SPR,
   Scond,
   ACB,
   MOVQ,
   LPR,

   CXPD = F_BASE(Format3),
   TRAP_F3_0001,
   BICPSR,
   TRAP_F3_0011,
   JUMP,
   TRAP_F3_0101,
   BISPSR,
   TRAP_F3_0111,
   TRAP_F3_1000,
   TRAP_F3_1001,
   ADJSP,
   TRAP_F3_1011,
   JSR,
   TRAP_F3_1101,
   CASE,
   TRAP_F3_1111,

   ADD = F_BASE(Format4),
   CMP,
   BIC,
   TRAP_F4_0011,
   ADDC,
   MOV,
   OR,
   TRAP_F4_0111,
   SUB,
   ADDR,
   AND,
   TRAP_F4_1011,
   SUBC,
   TBIT,
   XOR,

   MOVS = F_BASE(Format5),
   CMPS,
   SETCFG,
   SKPS,

   ROT = F_BASE(Format6),
   ASH,
   CBIT,
   CBITI,
   TRAP_F5_0100,
   LSH,
   SBIT,
   SBITI,
   NEG,
   NOT,
   TRAP_F5_1010,
   SUBP,
   ABS,
   COM,
   IBIT,
   ADDP,

   MOVM = F_BASE(Format7),
   CMPM,
   INSS,
   EXTS,
   MOVXiW,
   MOVZiW,
   MOVZiD,
   MOVXiD,
   MUL,
   MEI,
   Trap,
   DEI,
   QUO,
   REM,
   MOD,
   DIV,

   EXT = F_BASE(Format8),
   CVTP,
   INS,
   CHECK,
   INDEX,
   FFS,
   MOVUS,
   MOVSU,

   MOVif = F_BASE(Format9),
   LFSR,
   MOVLF,
   MOVFL,
   ROUND,
   TRUNC,
   SFSR,
   FLOOR,

   ADDf = F_BASE(Format11),
   MOVf,
   CMPf,
   TRAP_F11_0011,
   SUBf,
   NEGf,
   TRAP_F11_0110,
   TRAP_F11_0111,
   DIVf,
   TRAP_F11_1001,
   TRAP_F11_1010,
   TRAP_F11_1011,
   MULf,
   ABSf,

   RDVAL = F_BASE(Format14),
   WRVAL,
   LMR,
   SMR,
   TRAP_F14_0100,
   TRAP_F14_0101,
   TRAP_F14_0110,
   TRAP_F14_0111,
   TRAP_F14_1000,
   CINV,

   TRAP = F_BASE(FormatCount),
   InstructionCount,

   BAD = 0xFF
};

// See Table 4-1 page 4-5 in the manual
enum OperandFlags
{
   Force8Bit         = 0,
   Force16Bit        = 1,
   Force32Bit        = 2,
   Force64Bit        = 3,
   SizeVaries        = 4,

   read              = BIT(3),
   write             = BIT(4),
   rmw               = (read | write),
   addr              = BIT(5),
   Regaddr           = BIT(6),
   FloatingPoint     = BIT(7)
};

typedef struct
{
   uint8_t Flags;
   uint8_t Size;
} SingleOperand;

typedef union
{
   SingleOperand Op[2];
} OperandInformation;