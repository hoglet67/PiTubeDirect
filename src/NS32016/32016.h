// Some meaningful memory sizes
#define K128      0x0020000
#define K256      0x0040000
#define K512      0x0080000
#define K640      0x00A0000
#define MEG1      0x0100000
#define MEG2      0x0200000
#define MEG4      0x0400000
#define MEG8      0x0800000
#define MEG15     0x0F00000
#define MEG16     0x1000000

#define MEM_MASK 0xFFFFFF


#define BIT(in)   (1 <<(in))

#define TEST(in) ((in) ? 1 : 0)
#define C_FLAG PR.PSR.c_flag
#define T_FLAG PR.PSR.t_flag
#define L_FLAG PR.PSR.l_flag
#define V_FLAG PR.PSR.v_flag
#define F_FLAG PR.PSR.f_flag
#define Z_FLAG PR.PSR.z_flag
#define N_FLAG PR.PSR.n_flag

#define U_FLAG PR.PSR.u_flag
#define S_FLAG PR.PSR.s_flag
#define P_FLAG PR.PSR.p_flag
#define I_FLAG PR.PSR.i_flag

enum Formats
{
	Format0,
	Format1,
	Format2,
	Format3,
	Format4,
	Format5,
	Format6,
	Format7,
	Format8,
   Format9,
   Format10,
   Format11,
   Format12,
   Format13,
   Format14,
   FormatCount,
	FormatBad = 0xFF
};

enum RegType
{
   Integer,
   SinglePrecision,
   DoublePrecision
};

#define GET_PRECISION(in) ((in) ? SinglePrecision : DoublePrecision)
#define GET_F_SIZE(in) ((in) ? sz32 : sz64)

enum OpTypes
{
   Memory,
   Register,
   TOS,
   OpImmediate
};

enum Operands
{
   R0,
   R1,
   R2,
   R3,
   R4,
   R5,
   R6,
   R7,

   R0_Offset,
   R1_Offset,
   R2_Offset,
   R3_Offset,
   R4_Offset,
   R5_Offset,
   R6_Offset,
   R7_Offset,

   FrameRelative,
   StackRelative,
   StaticRelative,
   
   IllegalOperand,
   Immediate,
   Absolute,
   External,
   TopOfStack,
   FpRelative,
   SpRelative,
   SbRelative,
   PcRelative,
   EaPlusRn,
   EaPlus2Rn,
   EaPlus4Rn,
   EaPlus8Rn
};


enum OperandsPostitions
{
   Oper1,
   Oper2
};

enum DataSize
{
   szVaries = 0,
	sz8 = 1,
	sz16 = 2,
   Translating = 3,
	sz32 = 4,
	sz64 = 8,
};

typedef union
{
   uint8_t Op[4];
   uint32_t Whole;
} OperandSizeType;

#define WriteSize OpSize.Op[2]

typedef union
{
   struct
   {
      unsigned c_flag   : 1;          // 0x0001
      unsigned t_flag   : 1;          // 0x0002
      unsigned l_flag   : 1;          // 0x0004
      unsigned Bit3     : 1;          // 0x0008

      unsigned v_flag   : 1;          // 0x0010
      unsigned f_flag   : 1;          // 0x0020
      unsigned z_flag   : 1;          // 0x0040
      unsigned n_flag   : 1;          // 0x0080

      unsigned u_flag   : 1;          // 0x0100
      unsigned s_flag   : 1;          // 0x0200
      unsigned p_flag   : 1;          // 0x0400
      unsigned i_flag   : 1;          // 0x0800

      unsigned NU     : 20;
   };

   struct
   {
      uint8_t lsb;
      uint8_t msb;
      uint8_t byte3;
      uint8_t byte4;
    };

   uint32_t Whole;
} PsrType;

#define PrivilegedPSR(in) (BIT(in) & (BIT(0xE) | BIT(0xC) | BIT(0xC) | BIT(0xB) | BIT(0x4) | BIT(0x3) | BIT(0x2) | BIT(0x1)))

typedef union
{
   struct
   {
      unsigned i_flag      : 1;         // 0x0001
      unsigned fpu_flag    : 1;         // 0x0002
      unsigned m_flag      : 1;         // 0x0004
      unsigned c_flag      : 1;         // 0x0008

      unsigned not_used    : 4;        // 0x00X0

      unsigned de_flag     : 1;        // 0x0100
      unsigned dc_flag     : 1;        // 0x0200
      unsigned ldc_flag    : 1;        // 0x0400
      unsigned ic_flag     : 1;        // 0x0800

      unsigned lic_flag    : 1;        // 0x2000
      unsigned pf_flag     : 1;        // 0x0400
      unsigned NU          : 18;
   };

   struct
   {
      uint8_t lsb;
      uint8_t msb;
      uint8_t byte3;
      uint8_t byte4;
   };

   uint32_t Whole;
} CfgType;

typedef union
{
   struct
   {
      uint16_t Lower;
      uint16_t Upper;
   };

   uint32_t Whole;
} T16In32;

// Floating point registers
//  0 F0:L F0:F
//  1 F0:L F1:F
//  2 F1:L
//  3 F1:L
//  4 F2:L F2:F
//  5 F2:L F3:F
//  6 F3:L
//  7 F3:L
//  8 F4:L F4:F
//  9 F4:L F5:F
// 10 F5:L 
// 11 F5:L
// 12 F6:L F6:F
// 13 F6:L F7:F
// 14 F7:L
// 15 F7:L

typedef union
{
   float    fr32[16];
   double   fr64[ 8];
   uint32_t u32[16];
   uint64_t u64[ 8];
} FloatingPointRegisters;

typedef union
{
   double      f64;
   uint64_t    u64;
   int64_t     s64;
} Temp64Type;
  
typedef union
{
   float       f32;
   uint32_t    u32;
   int32_t     s32;
} Temp32Type;

typedef union
{
   struct
   {
      uint32_t UPSR;
      uint32_t DCR;
      uint32_t BPC;
      uint32_t DSR;
      uint32_t CAR;

      uint32_t NotUsed_0101;
      uint32_t NotUsed_0110;
      uint32_t NotUsed_0111;

      uint32_t FP;
      uint32_t SP;
      uint32_t SB;
      uint32_t USP;
      CfgType  CFG;
      PsrType  PSR;
      uint32_t INTBASE;
      T16In32  MOD;
   };

   uint32_t Direct[16];
} ProcessorRegisters;

#define fp           PR.FP
#define sb           PR.SB
#define psr          PR.PSR.Whole
#define psr_lsb      PR.PSR.lsb
#define intbase      PR.INTBASE
#define mod          PR.MOD.Whole
#define nscfg        PR.CFG

typedef union
{
   struct
   {
      unsigned OpType   : 5;
      unsigned RegType  : 3;
      unsigned IdxReg   : 3;
      unsigned IdxType  : 5;
   };

   struct
   {
      uint8_t LowerByte;
      uint8_t UpperByte;
   };

   uint16_t Whole;   
} RegLKU;

extern uint32_t sp[2];

#define STACK_P      sp[S_FLAG]
//#define STACK_P      PR.SP
#define SET_SP(in)   STACK_P = (in);     PrintSP("Set SP:");
#define INC_SP(in)   STACK_P += (in);    PrintSP("Inc SP:");
#define DEC_SP(in)   STACK_P -= (in);    PrintSP("Dec SP:");
#define GET_SP()     STACK_P

extern const uint32_t OpSizeLookup[6];
#define SET_OP_SIZE(in) OpSize.Whole = OpSizeLookup[(in) & 0x03]
#define SET_FOP_SIZE(in) OpSize.Whole = OpSizeLookup[0x04 | ((in) & 0x01)]

enum StringBits
{
   Translation = 15,
   Backwards   = 16,
   UntilMatch  = 17,
   WhileMatch  = 18
};

extern void n32016_init();
extern void n32016_ShowRegs(int bShowFloat);
extern void n32016_reset();
extern void n32016_reset_addr(uint32_t StartAddress);
extern void n32016_exec();
extern void n32016_close();
extern void n32016_build_matrix();
extern void BreakPoint(uint32_t pc, uint32_t opcode);
extern int32_t GetDisplacement(uint32_t* pPC);

extern ProcessorRegisters PR;
extern uint32_t r[8];
extern RegLKU Regs[2];
extern OperandSizeType FredSize;

extern void Disassemble(uint32_t Location, uint32_t End);

#ifdef INSTRUCTION_PROFILING
extern void DisassembleUsingITrace(uint32_t Location, uint32_t End);
extern uint32_t IP[MEG16];
#endif


#ifdef SHOW_INSTRUCTIONS
extern void ShowInstruction(uint32_t pc, uint32_t* pPC, uint32_t opcode, uint32_t Function, uint32_t OperandSize);
#else
#define ShowInstruction(...)
#endif

#if 1
extern void ShowRegisterWrite(RegLKU RegIn, uint32_t Value);
#else
#define ShowRegisterWrite(...)
#endif

#ifdef PC_SIMULATION
extern void CloseTrace(void);
#endif

#ifdef TRACE_TO_FILE
#define PiTRACE(...) fprintf(pTraceFile, __VA_ARGS__)
extern FILE *pTraceFile;
#elif defined(TRACE_TO_CONSOLE)
#define PiTRACE printf
#else
#define PiTRACE(...)
#endif

#define PiWARN(...)  { printf("pc=%08"PRIX32": ",pc); printf(__VA_ARGS__); }

extern int tubecycles;
extern int tube_irq;
extern uint32_t genaddr[2];
extern int gentype[2];
extern const uint8_t FormatSizes[FormatCount + 1];

//#define SHOW_STACK
#ifdef SHOW_STACK
#define PrintSP(str) PiTRACE("(%u) %s %06"PRIX32"\n", __LINE__, (str), GET_SP())
#else
#define PrintSP(str)
#endif

#define READ_PC_BYTE() read_x8(pc++) 

#define SIGN_EXTEND(size, reg) \
  if ((size == sz8) && (reg & 0x80)) { \
    reg |= 0xFFFFFF00; \
      } else if ((size == sz16) && (reg & 0x8000)) { \
    reg |= 0xFFFF0000; \
  }

#define NIBBLE_EXTEND(reg) \
   if (reg & 0x08) \
      reg |= 0xFFFFFFF0;
