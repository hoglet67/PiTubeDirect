// Native ARM CoPro kernel workspace (&0000-&0FFF)
// -----------------------------------------------
// Based on Acorn ARM Evaluation System, Sprow ARM CoPro, RISC OS, JGH Module Handler

#define ZP_BASE 0x00000000;

#define ZP_RESET    ZP_BASE+0x000;  // RESET entry point
#define ZP_UNDEF    ZP_BASE+0x004;  // Undefine instruction entry point
#define ZP_SWI      ZP_BASE+0x008;  // SWI entry point
#define ZP_ABORTF   ZP_BASE+0x00C;  // Prefetch abort entry point
#define ZP_ABORTD   ZP_BASE+0x010;  // Data abort entry point
#define ZP_ABORTA   ZP_BASE+0x014;  // Address abort entry point
#define ZP_IRQ      ZP_BASE+0x018;  // Maskable Interupt entry point
#define ZP_FIRQ     ZP_BASE+0x01C;  // Fast Interrupt entry point

#define ZP_RESETV   ZP_BASE+0x0E0;  // RESET vector
#define ZP_UNDEFV   ZP_BASE+0x0E4;  // Undefine instruction vector
#define ZP_SWIV     ZP_BASE+0x0E8;  // SWI vector
#define ZP_ABORTFV  ZP_BASE+0x0EC;  // Prefetch abort vector
#define ZP_ABORTDV  ZP_BASE+0x0F0;  // Data abort vector
#define ZP_ABORTAV  ZP_BASE+0x0F4;  // Address abort vector
#define ZP_IRQV     ZP_BASE+0x0F8;  // Maskable Interupt vector
#define ZP_FIRQV    ZP_BASE+0x0FC;  // Fast Interrupt vector

#define ZP_SWIDISPATCH  ZP_BASE+0x100;  // SWI dispatch table SWI 0-127
#define ZP_DEBUGGER ZP_BASE+0x300;  // Debugger workspace
#define ZP_VECTORS  ZP_BASE+0x380;  // Software vectors 0-31
#define ZP_ENVIRONMENT  ZP_BASE+0x400;  // Program environment
#define ZP_ENVTIME  ZP_BASE+0x478;  // Environment 5-byte time, &478 fits into 8 bits
#define ZP_ESCAPE   ZP_BASE+0x47F;  // Supervisor's Escape flag
#define ZP_MODULE   ZP_BASE+0x500;  // Module handler workspace
#define ZP_ENVSTRING    ZP_BASE+0xE00;  // Environment string
#define ZP_STRING   ZP_BASE+0xF00;  // Supervisor string space


// Basic Kernel memory layout
// --------------------------
// &0000-&00FF Hardware entries and vectors   | Gets
// &0100-&02FF SWI dispatch table 0-&7F       | progressively
// &0300-&037F Debugger workspace             | higher
// &0380-&03FF Software vectors 0-&1F         v level
// &0400-&04FF Program enviromnent
// &0500-      Module handler
//
// &0E00-&0EFF Environment buffer and Error buffer
// &0F00-&0FFF Supervisor input buffer
//
//
// More detailed memory layout
// ---------------------------
// &0000 Hardware entries and vectors
//  &0000 RESET  LDR PC,[&00E0]
//  &0004 UNDEF  LDR PC,[&00E4]
//  &0008 SWI    LDR PC,[&00E8]
//  &000C ABORTF LDR PC,[&00EC]
//  &0010 ABORTD LDR PC,[&00F0]
//  &0014 ABORTA LDR PC,[&00F4]
//  &0018 IRQ    LDR PC,[&00F8]
//  &001C FIQ    LDR PC,[&00FC]
//  &0020- FIQ workspace
//  &00DF
//
//  &00E0 RESETV  EQUD RESETHAND
//  &00E4 UNDEFV  EQUD UNDEFHAND
//  &00E8 SWIV    EQUD SWIHAND
//  &00EC ABORTFV EQUD ABORTFHAND
//  &00F0 ABORTDV EQUD ABOPRTDHAND
//  &00F4 ABORTAV EQUD ABORTAHAND
//  &00F8 IRQV    EQUD IRQHAND
//  &00FC FIQV    EQUD FIQHAND
//
// &0100 SWI Dispatch table
//  &0100 EQUD swi00 ; OS_WriteC
//  &0104 EQUD swi01 ; OS_WriteS
//  &0108 EQUD swi02 ; OS_Write0
//  &010C EQUD swi03 ; OS_Newline
//  &0110 EQUD swi04 ; OS_ReadC
//  etc...
//  &02FC EQUD swi7F
//
// &0300 Debugger workspace, Brazil and RISC OS allocate 128 bytes here
//
// &0380 Software vectors - see Kernel.s.ArthurSWIs for fuller implementation
//  &0380 - &00 UserV
//  &0384 - &01 ErrorV
//  &0388 - &02 IRQV
//  &038C - &03 WriteCV
//  &0390 - &04 ReadCV    * OS_ReadC these same as SWI number
//  &0394 - &05 CLIV      * OS_CLIV
//  &0398 - &06 ByteV     * OS_ByteV
//  &039C - &07 WordV     * OS_WordV
//  &03A0 - &08 FileV     * OS_FileV
//  &03A4 - &09 ArgsV     * OS_ArgsV
//  &03A8 - &0A BGetV     * OS_BGetV
//  &03AC - &0B BPutV     * OS_BPutV
//  &03B0 - &0C GBPBV     * OS_GBPBV
//  &03B4 - &0D FindV     * OS_FindV
//  &03B8 - &0E ReadLineV * OS_ReadLineV
//  &03BC - &0F FSControlV  unused on parasite
//  &03C0 - &10 EventV
//  &03C4 - &11 UptV        unused on parasite
//  &03C8 - &12 NetV        unused on parasite
//  &03CC - &13 KeyV        unused on parasite
//  &03D0 - &14 InsV        unused on parasite
//  &03D4 - &15 RemV        unused on parasite
//  &03D8 - &16 CnpV        unused on parasite
//  &03DC - &17 UKVDU23V    unused on parasite
//  &03E0 - &18 UKSWIV
//  &03E4 - &19 UKPLOTV     unused on parasite
//  &03E8 - &1A MouseV      pass on to ADVAL(7/8),INKEY-11/-12/-13
//  &03EC - &1B VDUXV       unused on parasite
//  &03F0 - &1C TickerV     unused on parasite
//  &03F4 - &1D UpCallV     unused on parasite
//  &03F8 - &1E ChangeEnvironmentV
//  &03FC - &1F SpriteV     unused on parasite
//
// &0400 Environment, up to three words per handler
//        Environment handler addresses
//  &0400  0 MemoryLimit               MEMTOP
//  &0404  1 Undefined Instruction     UNDHAND
//  &0408  2 Prefetch Abort            PREHAND
//  &040C  3 Data Abort                ABTHAND
//  &0410  4 Address Exception         ADRHAND
//  &0414  5 Other Exception           EXPHAND      Should this change:
//  &0418  6 Error handler             ERRV      <--- ErrorV
//  &041C  7 CallBack                  BACKHAND
//  &0420  8 BreakPoint                BREAKHAND
//  &0424  9 Escape                    ESCV
//  &0428 10 Event                     EVENTV    <--- EventV
//  &042C 11 Exit                      EXITV
//  &0430 12 Ununsed SWI               UKSWIV    <--- UKSWIV
//  &0434 13 Exception register dump   REGADDR
//  &0438 14 Application space         RAMTOP
//  &043C 15 Current Active Object     PROG
//  &0440 16 UpCall                    UPHAND    <--- UpCallV
//
//       Environment handler workspace pointers
//  &0444  6 Error handler             R0
//  &0448  7 CallBack                  R12
//  &044C  8 BreakPoint                R12
//  &0450  9 Escape                    R12
//  &0454 10 Event                     R12
//  &0458 11 Exit                      R12
//  &045C 12 Ununsed SWI               R12
//  &0460 16 UpCall                    R12
//
//       Environment handler buffers
//  &0464  6 Error handler             ERRBUFF
//  &0468  7 CallBack                  regbuffer
//  &046C  8 BreakPoint                regbuffer
//  &0470
//  &0474
//  &0478  Environment 5-byte time - updated locally on parasite
//  &047D
//  &047E
//  &047F  Supervisor Escape flag
//
// &0480 Temporarily use for software vector R12 values
// &0500
//
// &0E00 ^^^^ Initial RESET stack while finding top of RAM
// &0E00 Environment string, error buffer
// &0F00 Supervisor string buffer

