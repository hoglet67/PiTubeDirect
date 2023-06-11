#include "swi.h"

void OS_WriteC(const char c) {
  register const char r0 asm("r0") = c;
  asm volatile
    (
     "svc     %[swinum]        \r\n"
     :   // outputs
     :   // inputs
        "r" (r0),
        [swinum]  "I" (SWI_OS_WriteC)
     :   // clobber
        "r14"
     );
}

void OS_Write0(const char *cptr) {
  register const char *r0 asm("r0") = cptr;
  asm volatile
    (
     "svc     %[swinum]        \r\n"
     :   // outputs
     :   // inputs
        "r" (r0),
        [swinum]  "I" (SWI_OS_Write0)
     :   // clobber
       "r14"
     );
}

int OS_ReadC(unsigned int *flags) {
  register int r0 asm("r0");
  register unsigned int r2 asm("r2");
  asm volatile
    (
     "svc     %[swinum]        \r\n"
     "mrs     r2, cpsr         \r\n"
     :   // outputs
        "=r" (r0),
        "=r" (r2)
     :   // inputs
         [swinum]  "I" (SWI_OS_ReadC)
     :   // clobber
         "lr",
         "cc"
     );
    if (*flags)
      *flags = r2;
     return r0;
}

void OS_CLI(const char *cptr) {
  register const char *r0 asm("r0") = cptr;
  asm volatile
    (
     "svc     %[swinum]        \r\n"
     :   // outputs
     :   // inputs
        "r" (r0),
        [swinum]  "I" (SWI_OS_CLI)
     :   // clobber
        "lr"
     );
}

void OS_Word(unsigned int a, unsigned int *block) {
  register unsigned int r0 asm("r0") = a;
  register unsigned int *r1 asm("r1") = block;

  asm volatile
    (
     "svc     %[swinum]        \r\n"
     :   // outputs
     :   // inputs
          "r" (r0),
          "r" (r1),
         [swinum]  "I" (SWI_OS_Word)
     :   // clobber
         "lr",
         "memory"
     );
}

void OS_Byte(unsigned int a, unsigned int x, unsigned int y, unsigned int *retx, unsigned int *rety) {
  register unsigned int r0 asm("r0") = a;
  register unsigned int r1 asm("r1") = x;
  register unsigned int r2 asm("r2") = y;
  asm volatile
    (
     "svc     %[swinum]        \r\n" // Call osbyte with r0=a, r1=x and r2=y
     :   // outputs
        "+r" (r1),
        "+r" (r2)
     :   // inputs
        "r"  (r0),
         [swinum]  "I" (SWI_OS_Byte)
     :   // clobber
        "lr"
     );
     if (*retx)
        *retx = r1;
     if (*rety)
        *rety = r2;
}

void OS_ReadLine(const char *buffer, int buflen, int minAscii, int maxAscii, unsigned int *flags, int *length) {
  register const char *r0 asm("r0") = buffer;
  register int r1 asm("r1") = buflen;
  register int r2 asm("r2") = minAscii;
  register int r3 asm("r3") = maxAscii;

  asm volatile
    (
     "svc     %[swinum]        \r\n" // Call osreadline
     "mrs     r0, cpsr         \r\n"
     :   // outputs
        "+r" (r0),
        "+r" (r1)
     :   // inputs
        "r" (r2),
        "r" (r3),
        [swinum]  "I" (SWI_OS_ReadLine)
     :   // clobber
         "r4","lr","cc"
     );
    if (*flags)
      *flags = (unsigned int ) r0;
    if (*length)
      *length = r1;
}

void OS_Exit() {
  asm volatile
    (
     "svc     %[swinum]        \r\n"
     :   // outputs
     :   // inputs
         [swinum]  "I" (SWI_OS_Exit)
     :   // clobber
         "lr"
     );
}

#if 0
void OS_GenerateError(const ErrorBlock_type *eblk) {
    register const ErrorBlock_type *r0 asm("r0") = eblk;
  asm volatile
    (
     "svc     %[swinum]        \r\n"
     :   // outputs
     :   // inputs
        "r" (r0),
         [swinum]  "I" (SWI_OS_GenerateError)
     :   // clobber
        "lr"
     );
}
#endif

int OS_ReadModeVariable(unsigned int mode, int variable) {
  register unsigned int r0 asm("r0") = mode;
  register          int r1 asm("r1") = variable;
  register          int r2 asm("r2");
  asm volatile
    (
     "svc     %[swinum]        \r\n"
     :   // outputs
          "=r" (r2)
     :   // inputs
          "r" (r0),
          "r" (r1),
         [swinum]  "I" (SWI_OS_ReadModeVariable)
     :   // clobber
         "lr", "cc"
     );
  return r2;
}
