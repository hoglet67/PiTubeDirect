#include "swi.h"

__attribute__ ((noinline)) void OS_WriteC(const char c) {
  asm volatile
    (
     "push    {r4,lr}          \r\n"
     "svc     %[swinum]        \r\n"
     "pop     {r4,lr}          \r\n"
     :   // outputs
     :   // inputs
         [swinum]  "I" (SWI_OS_WriteC)
     :   // clobber
     );
}

#pragma GCC diagnostic ignored "-Wreturn-type"
__attribute__ ((noinline)) int OS_Write0(const char *cptr) {
  asm volatile
    (
     "push    {r4,lr}          \r\n"
     "svc     %[swinum]        \r\n"
     "pop     {r4,lr}          \r\n"
     :   // outputs
     :   // inputs
         [swinum]  "I" (SWI_OS_Write0)
     :   // clobber
     );
}

#pragma GCC diagnostic ignored "-Wreturn-type"
__attribute__ ((noinline)) int OS_ReadC(unsigned int *flags) {
  asm volatile
    (
     "push    {r4,lr}          \r\n"
     "svc     %[swinum]        \r\n"
     "pop     {r4,lr}          \r\n"
     "mrs     r2, cpsr         \r\n"
     "ldr     r1, %[flags]     \r\n"
     "teq     r1, #0           \r\n" // Skip updating flags if it's zero
     "strne   r2, [r1]         \r\n"    
     :   // outputs
     :   // inputs
         [swinum]  "I" (SWI_OS_ReadC),
         [flags]   "m" (flags)
     :   // clobber
         //"r0", "r1", "r2", "r3"
     );
}

__attribute__ ((noinline)) void OS_CLI(const char *cptr) {
  asm volatile
    (
     "push    {r4,lr}          \r\n"
     "svc     %[swinum]        \r\n"
     "pop     {r4,lr}          \r\n"
     :   // outputs
     :   // inputs
         [swinum]  "I" (SWI_OS_CLI)
     :   // clobber
     );
}

__attribute__ ((noinline)) void OS_Byte(unsigned int a, unsigned int x, unsigned int y, unsigned int *retx, unsigned int *rety) {
  asm volatile
    (
     "push    {r4,lr}          \r\n"
     "svc     %[swinum]        \r\n" // Call osbyte with r0=a, r1=x and r2=y
     "pop     {r4,lr}          \r\n"
     "ldr     r0, %[retx]      \r\n"
     "teq     r0, #0           \r\n" // Skip updating retx if it's zero
     "strne   r1, [r0]         \r\n"    
     "ldr     r0, %[rety]      \r\n"
     "teq     r0, #0           \r\n" // Skip updating rety if it's zero
     "strne   r2, [r1]         \r\n"    
     :   // outputs
     :   // inputs
         [swinum]  "I" (SWI_OS_Byte),
         [retx]    "m" (retx),
         [rety]    "m" (rety)
     :   // clobber
         "r0", "r1", "r2", "r3"
     );
}

__attribute__ ((noinline)) void OS_ReadLine(const char *buffer, int buflen, int minAscii, int maxAscii, unsigned int *flags, int *length) {
  asm volatile
    (
     "push    {r4,lr}          \r\n"
     "svc     %[swinum]        \r\n" // Call osreadline with r0=a, r1=x and r2=y
     "pop     {r4,lr}          \r\n"
     "ldr     r0, %[length]    \r\n"
     "teq     r0, #0           \r\n" // Skip updating length if it's zero
     "strne   r1, [r0]         \r\n"    
     "mrs     r1, cpsr         \r\n"
     "ldr     r0, %[flags]     \r\n"
     "teq     r0, #0           \r\n" // Skip updating flags if it's zero
     "strne   r1, [r0]         \r\n"    
     :   // outputs
     :   // inputs
         [swinum]  "I" (SWI_OS_ReadLine),
         [flags]   "m" (flags),
         [length]  "m" (length)
     :   // clobber
         "r0", "r1", "r2", "r3"
     );
}

__attribute__ ((noinline)) void OS_Exit() {
  asm volatile
    (
     "push    {r4,lr}          \r\n"
     "svc     %[swinum]        \r\n"
     "pop     {r4,lr}          \r\n"
     :   // outputs
     :   // inputs
         [swinum]  "I" (SWI_OS_Exit)
     :   // clobber
     );
}

__attribute__ ((noinline)) void OS_GenerateError(const ErrorBlock_type *eblk) {
  asm volatile
    (
     "push    {r4,lr}          \r\n"
     "svc     %[swinum]        \r\n"
     "pop     {r4,lr}          \r\n"
     :   // outputs
     :   // inputs
         [swinum]  "I" (SWI_OS_GenerateError)
     :   // clobber
     );
}
