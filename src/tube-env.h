// tube-env.h

#ifndef TUBE_ENV_H
#define TUBE_ENV_H

// Type definition for the Error Block
typedef struct EBKT {
   unsigned int errorNum;
   char errorMsg[252];
} ErrorBlock_type;


// Type definition for the Error Buffer
typedef struct EBFT {
   void *errorAddr;
   ErrorBlock_type errorBlock;
} ErrorBuffer_type;

// Type definition for a Generic Environment Handler, which will in practice be one of the above
typedef void (*EnvironmentHandler_type) ();

// Type definition for the handler state structure
typedef struct HST {
  EnvironmentHandler_type handler;
  unsigned int r12;
  void *address;
} HandlerState_type;

// Define handler numbers
#define MEMORY_LIMIT_HANDLER             0
#define UNDEFINED_INSTRUCTION_HANDLER    1
#define PREFETCH_ABORT_HANDLER           2
#define DATA_ABORT_HANDLER               3
#define ADDRESS_EXCEPTION_HANDLER        4
#define OTHER_EXCEPTIONS_HANDLER         5
#define ERROR_HANDLER                    6
#define CALLBACK_HANDLER                 7
#define BREAKPOINT_HANDLER               8
#define ESCAPE_HANDLER                   9
#define EVENT_HANDLER                   10
#define EXIT_HANDLER                    11
#define UNUSED_SWI_HANDLER              12
#define EXCEPTION_REGISTERS_HANDLER     13
#define APPLICATION_SPACE_HANDLER       14
#define CURRENTLY_ACTIVE_OBJECT_HANDLER 15
#define UPCALL_HANDLER                  16

#define NUM_HANDLERS                    17

// Type definition for the environment structure
typedef struct ET {
  // Buffers
  char commandBuffer[256];
  unsigned char timeBuffer[8];
  HandlerState_type handler[NUM_HANDLERS];
} Environment_type;

// Global pointer to the environment
extern Environment_type *env;

#endif
