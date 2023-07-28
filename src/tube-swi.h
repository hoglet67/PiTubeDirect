// tube-swi.h

#ifndef TUBE_SWI_H
#define TUBE_SWI_H

// The bit position of the error handling bit in the SWI number
#define ERROR_BIT (1u << 17)

// The bit position of the overflow flag in the ARM PSW
#define OVERFLOW_MASK (1u << 28)

// The bit position of the carry flag in the ARM PSW
#define CARRY_MASK (1u << 29)

// The various modes
#define MODE_USER                 0x10
#define MODE_FIQ                  0x11
#define MODE_IRQ                  0x12
#define MODE_SVR                  0x13
#define MODE_ABORT                0x17
#define MODE_UNDEFINED            0x1B
#define MODE_SYSTEM               0x1F


// Macro allowing SWI calls to be made from C
#define SWI(NUM) \
  asm volatile("swi %[swinum]" :: [swinum]  "I" (NUM):"lr");

// Type definition for a SWI handler
typedef void (*SWIHandler_Type) (unsigned int *reg);

#define NUM_SWI_HANDLERS 0x100

#define SWI_NAME_LEN 32

typedef struct {
   SWIHandler_Type handler;
   const char * const name;
} SWIDescriptor_Type;

// SWI handler table
extern SWIDescriptor_Type os_table[];

// Type definition for a generic function pointer
typedef int (*FunctionPtr_Type) ();

// Function prototypes
int  user_exec_fn(FunctionPtr_Type f, unsigned int param);
void user_exec_raw(unsigned char * address);
void handler_not_implemented(const char *type);
void generate_error(void * address, unsigned int errorNum, const char *errorMsg);


// Methods that might be useful to external SWI implementations
// (e.g. in framebuffer/swi_impl.c)

void updateCarry(unsigned char cyf, unsigned int *reg);

void swi_modules_init(int vdu);

#endif
