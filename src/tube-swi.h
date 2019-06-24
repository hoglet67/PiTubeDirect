// tube-swi.h

#ifndef TUBE_SWI_H
#define TUBE_SWI_H

// The bit position of the error handling bit in the SWI number
#define ERROR_BIT (1 << 17)

// The bit position of the overflow flag in the ARM PSW
#define OVERFLOW_MASK (1 << 28)

// The bit position of the carry flag in the ARM PSW
#define CARRY_MASK (1 << 29)

// The various modes
#define MODE_USER                 0x10
#define MODE_FIQ                  0x11
#define MODE_IRQ                  0x12
#define MODE_SVR                  0x13
#define MODE_ABORT                0x17
#define MODE_UNDEFINED            0x1B
#define MODE_SYSTEM               0x1F


// Macro allowing SWI calls to be made from C
// Note: stacking lr prevents corruption of lr when invoker in supervisor mode
#define SWI(NUM) \
  asm volatile("stmfd sp!,{lr}"); \
  asm volatile("swi "#NUM);       \
  asm volatile("ldmfd sp!,{lr}")  \

// Type definition for a SWI handler
typedef void (*SWIHandler_Type) (unsigned int *reg);

// Type definition for a generic function pointer
typedef int (*FunctionPtr_Type) ();

// Function prototypes
int  user_exec_fn(FunctionPtr_Type f, int param);
void user_exec_raw(volatile unsigned char *address);
void handler_not_defined(unsigned int num);
void handler_not_implemented(char *type);
void tube_SWI_Not_Known(unsigned int *reg);

// SWI handler prototypes
void tube_WriteC(unsigned int *reg);            // &00
void tube_WriteS(unsigned int *reg);            // &01
void tube_Write0(unsigned int *reg);            // &02
void tube_NewLine(unsigned int *reg);           // &03
void tube_ReadC(unsigned int *reg);             // &04
void tube_CLI(unsigned int *reg);               // &05
void tube_Byte(unsigned int *reg);              // &06
void tube_Word(unsigned int *reg);              // &07
void tube_File(unsigned int *reg);              // &08
void tube_Args(unsigned int *reg);              // &09
void tube_BGet(unsigned int *reg);              // &0A
void tube_BPut(unsigned int *reg);              // &0B
void tube_GBPB(unsigned int *reg);              // &0C
void tube_Find(unsigned int *reg);              // &0D
void tube_ReadLine(unsigned int *reg);          // &0E
void tube_GetEnv(unsigned int *reg);            // &10
void tube_Exit(unsigned int *reg);              // &11
void tube_IntOn(unsigned int *reg);             // &13
void tube_IntOff(unsigned int *reg);            // &14
void tube_EnterOS(unsigned int *reg);           // &16
void tube_Mouse(unsigned int *reg);             // &1C
void tube_GenerateError(unsigned int *reg);     // &2B
void tube_ReadPoint(unsigned int *reg);         // &32
void tube_ChangeEnvironment(unsigned int *reg); // &40
void tube_Plot(unsigned int *reg);              // &45
void tube_WriteN(unsigned int *reg);            // &46
void tube_BASICTrans_HELP(unsigned int *reg);   // &42C80
void tube_BASICTrans_Error(unsigned int *reg);  // &42C81
void tube_BASICTrans_Message(unsigned int *reg);// &42C82

void OS_SynchroniseCodeAreas(unsigned int *reg);// (&6E) -- OS_SynchroniseCodeAreas

#endif
