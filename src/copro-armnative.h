// copro-armnative.h

#ifndef COPRO_ARMNATIVE_H
#define COPRO_ARMNATIVE_H

#include "tube-env.h"

#define DEBUG_ARM 0

#define tubeRead tube_parasite_read

#define tubeWrite tube_parasite_write

extern void copro_armnative_fiq_handler();
extern void copro_armnative_swi_handler();
extern void copro_armnative_enable_mailbox();
extern void copro_armnative_disable_mailbox();
extern int  _user_exec(volatile unsigned char *address, unsigned int r0, unsigned int r1, unsigned int r2);
extern void _error_handler_wrapper(ErrorBuffer_type *eb, EnvironmentHandler_type errorHandler);
extern void _escape_handler_wrapper(unsigned int escapeFlag, EnvironmentHandler_type escapeHandler);
extern void _exit_handler_wrapper(unsigned int r12, EnvironmentHandler_type exitHandler);

extern void copro_armnative_emulator();

#endif
