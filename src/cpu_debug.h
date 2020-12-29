// cpu_debug.h

#ifndef CPU_DEBUG_H
#define CPU_DEBUG_H

#include <stdlib.h>
#include <inttypes.h>

#define WIDTH_8BITS  0
#define WIDTH_16BITS 1
#define WIDTH_32BITS 2

typedef struct {
  const char *cpu_name;                                               // Name/model of CPU.
  int      (*debug_enable)(int newvalue);                             // enable/disable debugging on this CPU, returns previous value.
  uint32_t (*memread)(uint32_t addr);                                 // CPU's usual memory read function.
  void     (*memwrite)(uint32_t addr, uint32_t value);                // CPU's usual memory write function.
  uint32_t (*ioread)(uint32_t addr);                                  // CPU's usual I/O read function.
  void     (*iowrite)(uint32_t addr, uint32_t value);                 // CPU's usual I/O write function.
  uint32_t (*disassemble)(uint32_t addr, char *buf, size_t bufsize);  // disassemble one line, returns next address
  const char **reg_names;                                             // NULL pointer terminated list of register names.
  uint32_t (*reg_get)(int which);                                     // Get a register - which is the index into the names above
  void     (*reg_set)(int which, uint32_t value);                     // Set a register.
  size_t   (*reg_print)(int which, char *buf, size_t bufsize);        // Print register value in CPU standard form.
  void     (*reg_parse)(int which, const char *strval);               // Parse a value into a register.
  uint32_t (*get_instr_addr)();                                       // Returns the base address of the currently executing instruction
  const char **trap_names;                                            // Null terminated list of other reasons a CPU may trap to the debugger.
  const int mem_width;                                                // Width of value returned from memread(): 0=8-bit, 1=16-bit, 2=32-bit
  const int io_width;                                                 // Width of value returned from  ioread(): 0=8-bit, 1=16-bit, 2=32-bit
  const int default_base;                                             // Allows a co pro to override the default base of 16
} cpu_debug_t;

extern void debug_init    ();
extern void debug_memread (const cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size);
extern void debug_memwrite(const cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size);
extern void debug_ioread  (const cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size);
extern void debug_iowrite (const cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size);
extern void debug_preexec (const cpu_debug_t *cpu, uint32_t addr);
extern void debug_trap    (const cpu_debug_t *cpu, uint32_t addr, int reason);

#endif
