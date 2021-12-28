//
// tube_irq bit definitions
//
// bit 7 Selects if R7 is used to inform the copro of an interrupt event used for fast 6502
// bit 6 Selects if direct native arm irq are used
// bit 5 native arm irq lock
// bit 3 tube_enable
// bit 2 Reset event
// bit 1 NMI
// bit 0 IRQ
#define FAST6502_BIT 128
#define NATIVEARM_BIT 64
#define nativearmlock_bit 32
#define TUBE_ENABLE_BIT  8
#define RESET_BIT 4
#define NMI_BIT 2
#define IRQ_BIT 1
