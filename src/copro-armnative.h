// copro-armnative.h

#ifndef COPRO_ARMNATIVE_H
#define COPRO_ARMNATIVE_H

// If this is defines, a reentrant form of the FIQ handler is installed
//#define USE_REENTRANT_FIQ_HANDLER

// If this is defined, the tube interrupt handler is implemented as a state machine
// Otherwise, it is implemented as code that may block
#define TUBE_ISR_STATE_MACHINE

#define DEBUG_ARM 0

#define DEBUG_TRANSFER 0

#define DEBUG_TRANSFER_CRC 0

#if !defined(__ASSEMBLER__)

#include "tube-env.h"

extern void copro_armnative_fiq_handler();
extern void copro_armnative_swi_handler();
extern void copro_armnative_enable_mailbox();
extern void copro_armnative_disable_mailbox();
extern int  _user_exec(volatile unsigned char *address, unsigned int r0, unsigned int r1, unsigned int r2);
extern void _error_handler_wrapper(unsigned int r0, EnvironmentHandler_type errorHandler);
extern void _escape_handler_wrapper(unsigned int flag, unsigned int workspace, EnvironmentHandler_type escapeHandler);
extern void _exit_handler_wrapper(unsigned int r12, EnvironmentHandler_type exitHandler);

extern void copro_armnative_reset();

extern void copro_armnative_emulator();

#ifdef USE_REENTRANT_FIQ_HANDLER
extern volatile int in_tube_intr_flag;
#endif


#endif

#endif
