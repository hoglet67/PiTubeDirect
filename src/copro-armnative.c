/*

    Part of the Raspberry-Pi Bare Metal Tutorials
    Copyright (c) 2015, Brian Sidebotham
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
        this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
        this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "copro-armnative.h"

#include "rpi-aux.h"
#include "rpi-armtimer.h"
#include "rpi-gpio.h"
#include "rpi-interrupts.h"
#include "rpi-systimer.h"

#include "info.h"
#include "startup.h"
#include "swi.h"
#include "tube.h"
#include "tube-lib.h"
#include "tube-env.h"
#include "tube-swi.h"
#include "tube-isr.h"
#include "tube-ula.h"

static int last_copro;

static jmp_buf reboot;

static jmp_buf enterOS;

static unsigned int defaultEscapeFlag;

static Environment_type defaultEnvironment;

static ErrorBuffer_type defaultErrorBuffer;

static char banner[80];

static const char *prompt = "arm>*";

Environment_type *env = &defaultEnvironment;

/***********************************************************
 * Default Handlers
 ***********************************************************/

// Note: this will be executed in user mode
static void defaultErrorHandler(const ErrorBuffer_type *eb) {
  // TODO: Consider resetting the user stack?
  if (DEBUG_ARM) {
    printf("Error = %p %02x %s\r\n", eb->errorAddr, eb->errorBlock.errorNum, eb->errorBlock.errorMsg);
  }
  sendString(R1_ID, 0x00, eb->errorBlock.errorMsg);
  sendString(R1_ID, 0x00, "\n\r");
  OS_Exit();
}

// Entered with R11 bit 6 as escape status
// R12 contains 0/-1 if not in/in the kernal presently
// R11 and R12 may be altered. Return with MOV PC,R14
// If R12 contains 1 on return then the Callback will be used

// Note, the way we invoke this via the _escape_handler_wrapper will
// work, because the flag will also still be in r0.
static void defaultEscapeHandler(unsigned int flag) {
  if (DEBUG_ARM) {
    printf("Escape flag = %02x\r\n", flag);
  }
  *((unsigned int *)(env->handler[ESCAPE_HANDLER].address)) = flag;
}

// Entered with R0, R1 and R2 containing the A, X and Y parameters. R0,
// R1, R2, R11 and R12 may be altered. Return with MOV PC, R14
// R12 contains 0/-1 if not in/in the kernal presently
// R13 contains the IRQ handling routine stack. When you return to the
// system LDMFD R13!, (R0,R1,R2,R11,R12,PC}^ will be executed. If R12
// contains 1 on return then the Callback will be used.
static void defaultEventHandler(unsigned int a, unsigned int x, unsigned int y) {
  if (DEBUG_ARM) {
    printf("Event: A=%02x X=%02x Y=%02x\r\n", a, x, y);
  }
}

// This should be called in USR mode whenever OS_Exit or OS_ExitAndDie is called.
static void defaultExitHandler() {
  if (DEBUG_ARM) {
    printf("Invoking default exit handler\r\n");
  }
  // Move back to supervisor mode
  swi(SWI_OS_EnterOS);
  // Jump back to the command prompt
  longjmp(enterOS, 1);  
}

static void defaultUndefinedInstructionHandler() {
  handler_not_implemented("UNDEFINED_INSTRUCTION_HANDLER");
}

static void defaultPrefetchAbortHandler() {
  handler_not_implemented("PREFETCH_ABORT_HANDLER");
}

static void defaultDataAbortHandler() {
  handler_not_implemented("DATA_ABORT_HANDLER");
}

static void defaultAddressExceptionHandler() {
  handler_not_implemented("ADDRESS_EXCEPTION_HANDLER");
}

static void defaultOtherExceptionHandler() {
  handler_not_implemented("OTHER_EXCEPTIONS_HANDLER");
}

static void defaultCallbackHandler() {
  handler_not_implemented("CALLBACK_HANDLER");
}

static void defaultUnusedSWIHandler() {
  handler_not_implemented("UNUSED_SWI_HANDLER");
}

static void defaultExceptionRegistersHandler() {
  handler_not_implemented("EXCEPTION_REGISTERS_HANDLER");
}

static void defaultUpcallHandler() {
  handler_not_implemented("UPCALL_HANDLER");
}

/***********************************************************
 * Initialize the envorinment
 ***********************************************************/

static void initEnv() {
  defaultEscapeFlag = 0;
  int i;
  for (i = 0; i < sizeof(env->commandBuffer); i++) {
    env->commandBuffer[i] = 0;
  }
  for (i = 0; i < sizeof(env->timeBuffer); i++) {
    env->timeBuffer[i] = 0;
  }
  for (i = 0; i < NUM_HANDLERS; i++) {
    env->handler[i].address = (void *)0;
    env->handler[i].r12 = 0xAAAAAAAA;
  }
  // Handlers that are code points
  env->handler[  UNDEFINED_INSTRUCTION_HANDLER].handler = defaultUndefinedInstructionHandler;
  env->handler[         PREFETCH_ABORT_HANDLER].handler = defaultPrefetchAbortHandler;
  env->handler[             DATA_ABORT_HANDLER].handler = defaultDataAbortHandler;
  env->handler[      ADDRESS_EXCEPTION_HANDLER].handler = defaultAddressExceptionHandler;
  env->handler[       OTHER_EXCEPTIONS_HANDLER].handler = defaultOtherExceptionHandler;
  env->handler[                  ERROR_HANDLER].handler = defaultErrorHandler;
  env->handler[                  ERROR_HANDLER].address = &defaultErrorBuffer;
  env->handler[               CALLBACK_HANDLER].handler = defaultCallbackHandler;
  env->handler[                 ESCAPE_HANDLER].handler = defaultEscapeHandler;
  env->handler[                 ESCAPE_HANDLER].address = &defaultEscapeFlag;
  env->handler[                  EVENT_HANDLER].handler = defaultEventHandler;
  env->handler[                   EXIT_HANDLER].handler = defaultExitHandler;
  env->handler[             UNUSED_SWI_HANDLER].handler = defaultUnusedSWIHandler;
  env->handler[    EXCEPTION_REGISTERS_HANDLER].handler = defaultExceptionRegistersHandler;
  env->handler[                 UPCALL_HANDLER].handler = defaultUpcallHandler;

  // Handlers where the handler is just data
  env->handler[           MEMORY_LIMIT_HANDLER].handler = (EnvironmentHandler_type) (16 * 1024 * 1024);
  env->handler[      APPLICATION_SPACE_HANDLER].handler = (EnvironmentHandler_type) (16 * 1024 * 1024);
  env->handler[CURRENTLY_ACTIVE_OBJECT_HANDLER].handler = (EnvironmentHandler_type) (0);
}

/***********************************************************
 * Do the tube reset protocol (in Supervisor Mode)
 ***********************************************************/

// Tube Reset protocol
static void tube_Reset() {
  // Send the reset message
  if (DEBUG_ARM) {
    printf( "Sending banner\r\n" );
  }
  sendString(R1_ID, 0x00, banner);
  sendByte(R1_ID, 0x00);
  if (DEBUG_ARM) {
    printf( "Banner sent, awaiting response\r\n" );
  }
  // Wait for the reponse in R2
  receiveByte(R2_ID);
  if (DEBUG_ARM) {
    printf( "Received response\r\n" );
  }
}


/***********************************************************
 * Initialize the hardware (in User Mode)
 ***********************************************************/

static int cli_loop() {
  unsigned int flags;
  int length;

  while( 1 ) {

    // In debug mode, print the mode (which should be user mode...)
    if (DEBUG_ARM) {
      printf("tube_cli: cpsr=%08x stack=%08x\r\n", _get_cpsr(), _get_stack_pointer());
    }

    // Print the supervisor prompt
    OS_Write0(prompt);

    // Ask for user input (OSWORD 0)
    OS_ReadLine(env->commandBuffer, sizeof(env->commandBuffer) - 1, 0x20, 0x7F, &flags, &length);

    // Was it escape
    if (flags & CARRY_MASK) {

      // Yes, print Escape
      OS_Write0("\n\rEscape\n\r");

      // Acknowledge escape condition
      if (DEBUG_ARM) {
        printf("Acknowledging Escape\r\n");
      }
      OS_Byte(0x7e, 0x00, 0x00, NULL, NULL);

    } else {
      // No, so execute the command using OSCLI
      OS_CLI(env->commandBuffer);
    }
  }
  return 0;
}

void copro_armnative_reset() {
  if (DEBUG_ARM) {
    printf("Invoking reset handler\r\n");
  }
  // Move back to supervisor mode
  swi(SWI_OS_EnterOS);
  // Jump back to the boot message
  longjmp(reboot, 1);  
}

/***********************************************************
 * copro_armnative_emulator
 ***********************************************************/

void copro_armnative_emulator() {

  // Disable interrupts!
  _disable_interrupts();

  // Record our copro number
  last_copro = copro;

  // Active the mailbox
  copro_armnative_enable_mailbox();

  // When a reset occurs, we want to return to here
  setjmp(reboot);

  if (copro != last_copro) {
    // Deactive the mailbox
    copro_armnative_disable_mailbox();
    // Allow another copro to be selected
    return;
  }

  // Create the startup banner
  sprintf(banner, "Native ARM Co Processor %dMHz\r\n\n", get_speed());

  // Initialize the environment structure
  initEnv();

  // If the default exit handler is called during tube_Reset(), we return here
  // This should not be necessary, but I've seen a couple of cases
  // where R4 errors happened during the startup message
  setjmp(enterOS);

  // Log ARM performance counters
  tube_log_performance_counters();

  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();

  // Enable interrupts!
  _enable_interrupts();

  // Reset ARM performance counters
  tube_reset_performance_counters();

  // Send reset message
  tube_Reset();

  // When the default exit handler is called, we return here
  setjmp(enterOS);

  // Make sure the reentrant interrupt flag is clear
#ifdef USE_REENTRANT_FIQ_HANDLER
  in_tube_intr_flag = 0;
#endif

  // Execute cli_loop as a normal user program
  user_exec_fn(cli_loop, 0);
}
