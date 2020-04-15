// tube-swi.c
//
// 23-Nov-2016   DMB:
//           Code imported from PiTubeClient project
// 25-Nov-2016   DMB:
//           Implemented OS_Plot (SWI &45)
// 02-Feb-2017   JGH:
//           Updated comments
//           Corrected in-length of OSWORD A=&05 (=IOMEM)
//           Corrected OSWORD &80+ block transfer
//           Corrected printf formatting of an error message
//           Corrected mask to detect OS_WriteI
//           Added SWI_Mouse
//           OSBYTE &8E/&9D don't return parameters
//           Corrected R1 return from OSBYTE &80+
// 14-Feb-2017   JGH and DMB:
//           Updated code entry to check code header
// 14-Feb-2017   DMB:
//           Implemented OS_ReadPoint (SWI &32)
// 19-Feb-2017   JGH:
//           tube_CLI prepares environment for later collection by OS_GetEnv.
//           Fixed typo fetching address of module title.
//           exec_raw temporarily doesn't swap in module title to commandBuffer
//           as we need a static buffer for that to work.

#include <stdio.h>
#include <string.h>

#include "copro-armnative.h"

#include "startup.h"
#include "tube-lib.h"
#include "tube-commands.h"
#include "tube-env.h"
#include "tube-swi.h"
#include "tube-isr.h"

#define NUM_SWI_HANDLERS 0x80

const int osword_in_len[] = {
  0,   // OSWORD 0x00
  0,   //  1  =TIME
  5,   //  2  TIME=
  0,   //  3  =IntTimer
  5,   //  4  IntTimer=
  4,   //  5  =IOMEM   JGH: must send full 4-byte address
  5,   //  6  IOMEM=
  8,   //  7  SOUND
  14,  //  8  ENVELOPE
  4,   //  9  =POINT()
  1,   // 10  =CHR$()
  1,   // 11  =Palette
  5,   // 12  Pallette=
  0,   // 13  =Coords
  8,   // 14  =RTC
  25,  // 15  RTC=
  16,  // 16  NetTx
  13,  // 17  NetRx
  0,   // 18  NetArgs
  8,   // 19  NetInfo
  128  // 20  NetFSOp
};

const int osword_out_len[] = {
  0,   // OSWORD 0x00
  5,   //  1  =TIME
  0,   //  2  TIME=
  5,   //  3  =IntTimer
  0,   //  4  IntTimer=
  5,   //  5  =IOMEM
  0,   //  6  IOMEM=
  0,   //  7  SOUND
  0,   //  8  ENVELOPE
  5,   //  9  =POINT()
  9,   // 10  =CHR$()
  5,   // 11  =Palette
  0,   // 12  Palette=
  8,   // 13  =Coords
  25,  // 14  =RTC
  1,   // 15  RTC=
  13,  // 16  NetTx
  13,  // 17  NetRx
  128, // 18  NetArgs
  8,   // 19  NetInfo
  128  // 20  NetFSOp
};

// Basic 135 uses the following on startup
//   SWI 00000010 // OS_GetEnv
//   SWI 0002006e // OS_SynchroniseCodeAreas *** Not Implemented ***
//   SWI 00020040 // OS_ChangeEnvironment
//   SWI 00020040 // OS_ChangeEnvironment
//   SWI 00020040 // OS_ChangeEnvironment
//   SWI 00020040 // OS_ChangeEnvironment
//   SWI 00000010 // OS_GetEnv
//   SWI 00000010 // OS_GetEnv
//   SWI 00000001 // OS_WriteS
//
//   SWI 00062c82 // BASICTrans_Message *** Not Implemented ***
//   SWI 0000013e // OS_WriteI
//   SWI 0000000e // OS_ReadLine
//   SWI 00062c81 // BASICTrans_Error *** Not Implemented ***
//   SWI 00000006 // OS_Byte
//
//   SWI 00062c82 // BASICTrans_Message *** Not Implemented ***
//   SWI 0000013e // OS_WriteI
//   SWI 0000000e // OS_ReadLine


SWIHandler_Type SWIHandler_Table[NUM_SWI_HANDLERS] = {
  tube_WriteC,                // (&00) -- OS_WriteC
  tube_WriteS,                // (&01) -- OS_WriteS
  tube_Write0,                // (&02) -- OS_Write0
  tube_NewLine,               // (&03) -- OS_NewLine
  tube_ReadC,                 // (&04) -- OS_ReadC
  tube_CLI,                   // (&05) -- OS_CLI
  tube_Byte,                  // (&06) -- OS_Byte
  tube_Word,                  // (&07) -- OS_Word
  tube_File,                  // (&08) -- OS_File
  tube_Args,                  // (&09) -- OS_Args
  tube_BGet,                  // (&0A) -- OS_BGet
  tube_BPut,                  // (&0B) -- OS_BPut
  tube_GBPB,                  // (&0C) -- OS_GBPB
  tube_Find,                  // (&0D) -- OS_Find
  tube_ReadLine,              // (&0E) -- OS_ReadLine
  tube_SWI_Not_Known,         // (&0F) -- OS_Control
  tube_GetEnv,                // (&10) -- OS_GetEnv
  tube_Exit,                  // (&11) -- OS_Exit
  tube_SWI_Not_Known,         // (&12) -- OS_SetEnv
  tube_IntOn,                 // (&13) -- OS_IntOn
  tube_IntOff,                // (&14) -- OS_IntOff
  tube_SWI_Not_Known,         // (&15) -- OS_CallBack
  tube_EnterOS,               // (&16) -- OS_EnterOS
  tube_SWI_Not_Known,         // (&17) -- OS_BreakPt
  tube_SWI_Not_Known,         // (&18) -- OS_BreakCtrl
  tube_SWI_Not_Known,         // (&19) -- OS_UnusedSWI
  tube_SWI_Not_Known,         // (&1A) -- OS_UpdateMEMC
  tube_SWI_Not_Known,         // (&1B) -- OS_SetCallBack
  tube_Mouse,                 // (&1C) -- OS_Mouse
  tube_SWI_Not_Known,         // (&1D) -- OS_Heap
  tube_SWI_Not_Known,         // (&1E) -- OS_Module
  tube_SWI_Not_Known,         // (&1F) -- OS_Claim
  tube_SWI_Not_Known,         // (&20) -- OS_Release
  tube_SWI_Not_Known,         // (&21) -- OS_ReadUnsigned
  tube_SWI_Not_Known,         // (&22) -- OS_GenerateEvent
  tube_SWI_Not_Known,         // (&23) -- OS_ReadVarVal
  tube_SWI_Not_Known,         // (&24) -- OS_SetVarVal
  tube_SWI_Not_Known,         // (&25) -- OS_GSInit
  tube_SWI_Not_Known,         // (&26) -- OS_GSRead
  tube_SWI_Not_Known,         // (&27) -- OS_GSTrans
  tube_SWI_Not_Known,         // (&28) -- OS_BinaryToDecimal
  tube_SWI_Not_Known,         // (&29) -- OS_FSControl
  tube_SWI_Not_Known,         // (&2A) -- OS_ChangeDynamicArea
  tube_GenerateError,         // (&2B) -- OS_GenerateError
  tube_SWI_Not_Known,         // (&2C) -- OS_ReadEscapeState
  tube_SWI_Not_Known,         // (&2D) -- OS_EvaluateExpression
  tube_SWI_Not_Known,         // (&2E) -- OS_SpriteOp
  tube_SWI_Not_Known,         // (&2F) -- OS_ReadPalette
  tube_SWI_Not_Known,         // (&30) -- OS_ServiceCall
  tube_SWI_Not_Known,         // (&31) -- OS_ReadVduVariables
  tube_ReadPoint,             // (&32) -- OS_ReadPoint
  tube_SWI_Not_Known,         // (&33) -- OS_UpCall
  tube_SWI_Not_Known,         // (&34) -- OS_CallAVector
  tube_SWI_Not_Known,         // (&35) -- OS_ReadModeVariable
  tube_SWI_Not_Known,         // (&36) -- OS_RemoveCursors
  tube_SWI_Not_Known,         // (&37) -- OS_RestoreCursors
  tube_SWI_Not_Known,         // (&38) -- OS_SWINumberToString
  tube_SWI_Not_Known,         // (&39) -- OS_SWINumberFromString
  tube_SWI_Not_Known,         // (&3A) -- OS_ValidateAddress
  tube_SWI_Not_Known,         // (&3B) -- OS_CallAfter
  tube_SWI_Not_Known,         // (&3C) -- OS_CallEvery
  tube_SWI_Not_Known,         // (&3D) -- OS_RemoveTickerEvent
  tube_SWI_Not_Known,         // (&3E) -- OS_InstallKeyHandler
  tube_SWI_Not_Known,         // (&3F) -- OS_CheckModeValid
  tube_ChangeEnvironment,     // (&40) -- OS_ChangeEnvironment
  tube_SWI_Not_Known,         // (&41) -- OS_ClaimScreenMemory
  tube_SWI_Not_Known,         // (&42) -- OS_ReadMonotonicTime
  tube_SWI_Not_Known,         // (&43) -- OS_SubstituteArgs
  tube_SWI_Not_Known,         // (&44) -- OS_PrettyPrintCode
  tube_Plot,                  // (&45) -- OS_Plot
  tube_WriteN,                // (&46) -- OS_WriteN
  tube_SWI_Not_Known,         // (&47) -- OS_AddToVector
  tube_SWI_Not_Known,         // (&48) -- OS_WriteEnv
  tube_SWI_Not_Known,         // (&49) -- OS_ReadArgs
  tube_SWI_Not_Known,         // (&4A) -- OS_ReadRAMFsLimits
  tube_SWI_Not_Known,         // (&4B) -- OS_ClaimDeviceVector
  tube_SWI_Not_Known,         // (&4C) -- OS_ReleaseDeviceVector
  tube_SWI_Not_Known,         // (&4D) -- OS_DelinkApplication
  tube_SWI_Not_Known,         // (&4E) -- OS_RelinkApplication
  tube_SWI_Not_Known,         // (&4F) -- OS_HeapSort
  tube_SWI_Not_Known,         // (&50) -- OS_ExitAndDie
  tube_SWI_Not_Known,         // (&51) -- OS_ReadMemMapInfo
  tube_SWI_Not_Known,         // (&52) -- OS_ReadMemMapEntries
  tube_SWI_Not_Known,         // (&53) -- OS_SetMemMapEntries
  tube_SWI_Not_Known,         // (&54) -- OS_AddCallBack
  tube_SWI_Not_Known,         // (&55) -- OS_ReadDefaultHandler
  tube_SWI_Not_Known,         // (&56) -- OS_SetECFOrigin
  tube_SWI_Not_Known,         // (&57) -- OS_SerialOp
  tube_SWI_Not_Known,         // (&58) -- OS_ReadSysInfo
  tube_SWI_Not_Known,         // (&59) -- OS_Confirm
  tube_SWI_Not_Known,         // (&5A) -- OS_ChangedBox
  tube_SWI_Not_Known,         // (&5B) -- OS_CRC
  tube_SWI_Not_Known,         // (&5C) -- OS_ReadDynamicArea
  tube_SWI_Not_Known,         // (&5D) -- OS_PrintChar
  tube_SWI_Not_Known,         // (&5E) -- OS_ChangeRedirection
  tube_SWI_Not_Known,         // (&5F) -- OS_RemoveCallBack
  tube_SWI_Not_Known,         // (&60) -- OS_FindMemMapEntries
  tube_SWI_Not_Known,         // (&61) -- OS_SetColourCode
  tube_SWI_Not_Known,         // (&62) -- OS_ClaimSWI
  tube_SWI_Not_Known,         // (&63) -- OS_ReleaseSWI
  tube_SWI_Not_Known,         // (&64) -- OS_Pointer
  tube_SWI_Not_Known,         // (&65) -- OS_ScreenMode
  tube_SWI_Not_Known,         // (&66) -- OS_DynamicArea
  tube_SWI_Not_Known,         // (&67) -- OS_AbortTrap
  tube_SWI_Not_Known,         // (&68) -- OS_Memory
  tube_SWI_Not_Known,         // (&69) -- OS_ClaimProcessorVector
  tube_SWI_Not_Known,         // (&6A) -- OS_Reset
  tube_SWI_Not_Known,         // (&6B) -- OS_MMUControl
  tube_SWI_Not_Known,         // (&6C) -- OS_ResyncTime
  tube_SWI_Not_Known,         // (&6D) -- OS_PlatformFeatures
  OS_SynchroniseCodeAreas,    // (&6E) -- OS_SynchroniseCodeAreas
  tube_SWI_Not_Known,         // (&6F) -- OS_CallASWI
  tube_SWI_Not_Known,         // (&70) -- OS_AMBControl
  tube_SWI_Not_Known,         // (&71) -- OS_CallASWIR12
  tube_SWI_Not_Known,         // (&72) -- OS_SpecialControl
  tube_SWI_Not_Known,         // (&73) -- OS_EnterUSR32
  tube_SWI_Not_Known,         // (&74) -- OS_EnterUSR26
  tube_SWI_Not_Known,         // (&75) -- OS_VIDCDivider
  tube_SWI_Not_Known,         // (&76) -- OS_NVMemory
  tube_SWI_Not_Known,         // (&77) -- OS_ClaimOSSWI
  tube_SWI_Not_Known,         // (&78) -- OS_TaskControl
  tube_SWI_Not_Known,         // (&79) -- OS_DeviceDriver
  tube_SWI_Not_Known,         // (&7A) -- OS_Hardware
  tube_SWI_Not_Known,         // (&7B) -- OS_IICOp
  tube_SWI_Not_Known,         // (&7C) -- OS_LeaveOS
  tube_SWI_Not_Known,         // (&7D) -- OS_ReadLine32
  tube_SWI_Not_Known,         // (&7E) -- OS_SubstituteArgs32
  tube_SWI_Not_Known          // (&7F) -- OS_HeapSort32
};

// A helper method to make generating errors easier
void generate_error(void * address, unsigned int errorNum, char *errorMsg) {
  // Get the current handler's error buffer
  ErrorBuffer_type *ebuf = (ErrorBuffer_type *)env->handler[ERROR_HANDLER].address;
  // Copy the error address into the handler's error block
  ebuf->errorAddr = address;
  // Copy the error number into the handler's error block
  ebuf->errorBlock.errorNum = errorNum;
  // Copy the error string into the handler's error block
  strcpy(ebuf->errorBlock.errorMsg, errorMsg);
  // The wrapper drops back to user mode and calls the handler
  _error_handler_wrapper(ebuf, env->handler[ERROR_HANDLER].handler);
}

void updateOverflow(unsigned char ovf, unsigned int *reg) {
  // The PSW is on the stack two words before the registers
  reg-= 2;
  if (ovf) {
    *reg |= OVERFLOW_MASK;
  } else {
    *reg &= ~OVERFLOW_MASK;
  }
}

void updateCarry(unsigned char cyf, unsigned int *reg) {
  // The PSW is on the stack two words before the registers
  reg-= 2;
  if (cyf) {
    *reg |= CARRY_MASK;
  } else {
    *reg &= ~CARRY_MASK;
  }
}

void updateMode(unsigned char mode, unsigned int *reg) {
  // The PSW is on the stack two words before the registers
  reg-= 2;
  *reg &= ~0x1f;
  *reg |= mode;
}

// For an unimplemented environment handler
void handler_not_implemented(char *type) {
  printf("Handler %s defined but not implemented\r\n", type);
}

// For an undefined environment handler (i.e. where num >= NUM_HANDLERS)
void handler_not_defined(unsigned int num) {
  printf("Handler 0x%x not defined\r\n", num);
}

// For an unimplemented SWI
void tube_SWI_Not_Known(unsigned int *reg) {
  unsigned int *lr = (unsigned int *)reg[13];
  printf("%08x %08x %08x %08x\r\n", reg[0], reg[1], reg[2], reg[3]);
  printf("SWI %08x not implemented ************\r\n", *(lr - 1) & 0xFFFFFF);
}

void C_SWI_Handler(unsigned int number, unsigned int *reg) {
  unsigned int num = number;
  int errorBit = 0;
  if (DEBUG_ARM) {
    printf("SWI %08x called from %08x cpsr=%08x\r\n", number, reg[13] - 4, _get_cpsr());
  }
  // TODO - We need to switch to a local error handler
  // for the error bit to work as intended.
  if (num & ERROR_BIT) {
    errorBit = 1;
    num &= ~ERROR_BIT;
  }
  if (num < NUM_SWI_HANDLERS) {
    // Invoke one of the fixed handlers
    SWIHandler_Table[num](reg);
  } else if ((num & 0xFFFFFF00) == 0x0100) {      // JGH
    // SWIs 0x100 - 0x1FF are OS_WriteI
    tube_WriteC(&num);
  } else if (num == 0x42c80) {
    tube_BASICTrans_HELP(reg);
  } else if (num == 0x42c81) {
    tube_BASICTrans_Error(reg);
  } else if (num == 0x42c82) {
    tube_BASICTrans_Message(reg);
  } else {
    tube_SWI_Not_Known(reg);
    if (errorBit) {
      updateOverflow(1, reg);
    }
  }
  if (DEBUG_ARM) {
    printf("SWI %08x complete cpsr=%08x\r\n", number, _get_cpsr());
  }
}

// Helper functions

int user_exec_fn(FunctionPtr_Type f, int param ) {
  int ret;
  if (DEBUG_ARM) {
    printf("Execution passing to %08x cpsr = %08x param = %s\r\n", (unsigned int)f, _get_cpsr(), (unsigned char *)param);
  }
  // setTubeLibDebug(1);
  // The machine code version in copro-armnativeasm.S does the real work
  // of dropping down to user mode
  ret = _user_exec((unsigned char *)f, param, 0, 0);
  if (DEBUG_ARM) {
    printf("Execution returned from %08x ret = %08x cpsr = %08x\r\n", (unsigned int)f, ret, _get_cpsr());
  }
  return ret;
}

void user_exec_raw(volatile unsigned char *address) {
  int off;
  int carry = 0, r0 = 0; int r1 = 0; int r12 = 0; // Entry parameters

  if (DEBUG_ARM) {
    printf("Execution passing to %08x cpsr = %08x\r\n", (unsigned int)address, _get_cpsr());
  }

// JGH: set up parameters and find correct entry address
// tube_CLI has already set commandBuffer="filename parameters"
// If we enter a module this needs to be changed to "modulename parameters"
//
// On entry to code, registers need to be:
// r0=>command line if >255, 0=raw code, 1=BBC header
// r1=>command tail
// r12=workspace - pass as zero to say 'none allocated, you must claim some'
// r13=stack
// r14=return address

  off=address[7];
  if (address[off+0]==0 && address[off+1]=='(' && address[off+2]=='C' && address[off+3]==')') {
    // BBC header
    if ((address[6] & 0x4F) != 0x4D) {
      generate_error((void *) address, 249, "This is not ARM code");
      return;
    } else {
      r0 = 1;       // Entering code with a BBC header
      carry = 1;    // Set Carry = not entering from RESET
      // ToDo: Should use ROM title as commandBuffer startup command
    }
  } else {
    if (address[19] == 0 && address[23] == 0 && address[27] == 0) {
      // RISC OS module header
      off=address[16] + 256 * address[17] + 65536 * address[18];
      r0 = (unsigned int) address + off;    // R0=>module title

// We need to do commandBuffer=moduleTitle+" "+MID$(commandBuffer,offset_to_space)
// which means we need some string space to construct a new string.
// Real hardware has a static command line buffer for this use, similar to &DC00 on the Master.
// For the moment, just use the the existing *command string until we sort out a static
// commandBuffer string space
// This is also needed for OS_SetEnv which copies a new environment string to commandBuffer

    }
    else
      // No header, r0 should point to *command used to run code
      r0 = (unsigned int) env->commandBuffer;
  }

  r1 = (unsigned int) env->commandBuffer;
  while (*(char*)r1 > ' ') r1++;      // Step past command
  while (*(char*)r1 == ' ') r1++;     // Step past spaces, r1=>command tail

  if (address[3]==0) {
    off=address[0]+256*address[1]+65536*address[2];
    address=address+off;              // Entry word is offset, not branch
  }

  // Bit zero of the address param is used by _user_exec as the carry
  address = (unsigned char *) (((unsigned int) address) | carry);

  // setTubeLibDebug(1);
  // The machine code version in copro-armnativeasm.S does the real work
  // of dropping down to user mode

  _user_exec(address, r0, r1, r12);
  // r0=>startup command string if r0>255, 0=raw, 1=bbc header)
  // r1=>startup command parameters. Code should not rely on R1 but should call OS_GetEnv,
  //     but it is provided to assist Utility code which does not have an environment
  // r12=0 - no workspace allocated

  if (DEBUG_ARM) {
    printf("Execution returned from %08x cpsr = %08x\r\n", (unsigned int)address, _get_cpsr());
  }
}

char *write_string(char *ptr) {
  unsigned char c;
  // Output characters pointed to by R0, until a terminating zero
  while ((c = *ptr++) != 0) {
    sendByte(R1_ID, c);
  }
  return ptr;
}
// Client to Host transfers
// Reference: http://mdfs.net/Software/Tube/Protocol
// OSWRCH   R1: A
// OSRDCH   R2: &00                               Cy A
// OSCLI    R2: &02 string &0D                    &7F or &80
// OSBYTELO R2: &04 X A                           X
// OSBYTEHI R2: &06 X Y A                         Cy Y X
// OSWORD   R2: &08 A in_length block out_length  block
// OSWORD0  R2: &0A block                         &FF or &7F string &0D
// OSARGS   R2: &0C Y block A                     A block
// OSBGET   R2: &0E Y                             Cy A
// OSBPUT   R2: &10 Y A                           &7F
// OSFIND   R2: &12 &00 Y                         &7F
// OSFIND   R2: &12 A string &0D                  A
// OSFILE   R2: &14 block string &0D A            A block
// OSGBPB   R2: &16 block A                       block Cy A

void tube_WriteC(unsigned int *reg) {
  sendByte(R1_ID, (unsigned char)((reg[0]) & 0xff));
}

void tube_WriteS(unsigned int *reg) {
  // Reg 13 is the stacked link register which points to the string
  reg[13] = (unsigned int) write_string((char *)reg[13]);
  // Make sure new value of link register is word aligned to the next word boundary
  reg[13] += 3;
  reg[13] &= ~3;
}

void tube_Write0(unsigned int *reg) {
  // On exit, R0 points to the byte after the terminator
  reg[0] = (unsigned int)write_string((char *)reg[0]);;
}

void tube_NewLine(unsigned int *reg) {
  sendByte(R1_ID, 0x0A);
  sendByte(R1_ID, 0x0D);
}

void tube_ReadC(unsigned int *reg) {
  // OSRDCH   R2: &00                               Cy A
  sendByte(R2_ID, 0x00);
  // On exit, the Carry flag indicates validity
  updateCarry(receiveByte(R2_ID) & 0x80, reg);
  // On exit, R0 contains the character
  reg[0] = receiveByte(R2_ID);
}

void tube_CLI(unsigned int *reg) {
  char *ptr = (char *)(*reg);
  char *lptr = ptr;
  int run=0;

// We need to prepare the environment in case code is entered
// Command buffer is:
// * ** ***foobar   hazel sheila
//         ^
//   *  **  *  /  foobar   hazel   sheila
//                ^
// *  *** * ** RUN   foobar   hazel  sheila
// *  *** * ** RU.   foobar   hazel  sheila
// *  *** * ** R.    foobar   hazel  sheila
//                   ^

  while (*lptr == ' ' || *lptr == '*') {
    lptr++;    // Skip leading spaces and stars
  }
// Now at:
// *foobar hazel
//  ^
// */foobar hazel
//  ^
// */ foobar hazel
//  ^
// *RUN foobar hazel
//  ^

  if (lptr[0] == '/') {
     run   = 1;                     // */file or */ file
     lptr += 1;                     // skip past /
  } else {
    if ((lptr[0] & 0xDF) == 'R') {  // Might be *RUN
      if (lptr[1] == '.') {
        run   = 1;                  // *R.file or *R. file
        lptr += 2;                  // skip past *R.
      } else if ((lptr[1] & 0xDF) == 'U') {
        if (lptr[2] == '.') {
          run   = 1;                // *RU. file or *RU. file
          lptr += 3;                // skip past *RU.
        } else if ((lptr[2] & 0xDF)=='N' && lptr[3] < 'A') {
          run   = 1;                // *RUN file
          lptr += 3;                // skip past *RUN
        }
      }
    }
  }

  if (run) {                       // Skip any spaces preceeding filename
    while (*lptr == ' ') {
      lptr++;
    }
  }

// Now at:
// *foobar hazel
//  ^
// */foobar hazel
//   ^
// */ foobar hazel
//    ^
// *RUN foobar hazel
//      ^

// Fake an OS_SetEnv call
  run = 0;
  while (*lptr >= ' ') {         // Copy this command to environment string
    env->commandBuffer[run] = *lptr;
    run++;
    lptr++;
  }
  env->commandBuffer[run]=0x0D;
//  env->handler[MEMORY_LIMIT_HANDLER].handler=???; // Can't remember if these are set now or later
//  env->timeBuffer=now_centiseconds();     // Will need to check real hardware

  if (dispatchCmd(ptr)) {       // dispatchCmd returns 0 if command was handled locally
    // OSCLI    R2: &02 string &0D                    &7F or &80
    sendByte(R2_ID, 0x02);
    // Send the command, excluding terminating control character (anything < 0x20)
    sendStringWithoutTerminator(R2_ID, ptr);
    // Send the 0x0D terminator
    sendByte(R2_ID, 0x0D);
    if (receiveByte(R2_ID) & 0x80) {
      // Execution should pass to last transfer address
      user_exec_raw(address);
      // Possibly this will return...
    }
  }
}

void tube_Byte(unsigned int *reg) {
  if (DEBUG_ARM) {
    printf("%08x %08x %08x\r\n", reg[0], reg[1], reg[2]);
  }
  unsigned char cy;
  unsigned char a = reg[0] & 0xff;
  unsigned char x = reg[1] & 0xff;
  unsigned char y = reg[2] & 0xff;
  if (a < 128) {
    // OSBYTELO R2: &04 X A                           X
    sendByte(R2_ID, 0x04);
    sendByte(R2_ID, x);
    sendByte(R2_ID, a);
    x = receiveByte(R2_ID);
    reg[1] = x;

  } else {
    // OSBYTEHI R2: &06 X Y A                         Cy Y X
    sendByte(R2_ID, 0x06);
    sendByte(R2_ID, x);
    sendByte(R2_ID, y);
    sendByte(R2_ID, a);

    // JGH: OSBYTE &8E and &9D do not return any data.
    if (a == 0x8E) {
       // OSBYTE &8E returns a 1-byte OSCLI acknowledgement
       if (receiveByte(R2_ID) & 0x80) {
          env->commandBuffer[0] = 0x0D;   // Null command line
          user_exec_raw(address);
       }
       return;
    }
    if (a == 0x9D) {
       // OSBYTE &9D immediately returns.
       return;
    }

    cy = receiveByte(R2_ID) & 0x80;
    y = receiveByte(R2_ID);
    x = receiveByte(R2_ID);
    reg[1] = x | (y << 8);      // JGH
    reg[2] = y;
    updateCarry(cy, reg);
  }
  if (DEBUG_ARM) {
    printf("%08x %08x %08x\r\n", reg[0], reg[1], reg[2]);
  }
}

void tube_Word(unsigned int *reg) {
  int in_len;
  int out_len;
  unsigned char a = reg[0] & 0xff;
  unsigned char *block;
  int i;
  // Note that call with R0b=0 (Acorn MOS RDLN) does nothing, the ReadLine call should be used instead.
  // Ref: http://chrisacorns.computinghistory.org.uk/docs/Acorn/OEM/AcornOEM_ARMUtilitiesRM.pdf
  if (a == 0) {
    return;
  }
  // Work out block lengths
  // Ref: http://mdfs.net/Info/Comp/Acorn/AppNotes/004.pdf
  block = (unsigned char *)reg[1];
  if (a < 0x15) {
    in_len = osword_in_len[a];
    out_len = osword_out_len[a];
  } else if (a < 128) {
    in_len = 16;
    out_len = 16;
  } else {
    // TODO: Check with JGH whether it is correct to update block to exclude the lengths
    // JGH: No, the lengths are the entire block, including the lengths
    //      For example, &02,&02 is the shortest possible control block
    //                   &03,&03,nn sends and receives three bytes, two lengths plus one byte of data
//    in_len = *block++;
//    out_len = *block++;

    in_len = block[0];
    out_len = block[1];
  }

  // Implement sub-reason codes of OSWORD A=&0E (Read CMOS Clock)
  if (a == 0x0e) {
    if (block[0] != 0x00) {
      printf("OSWORD A=&0E sub-reason %d not implemented\r\n", block[0]);
      // Return something, as this gets used in a files load/exec address on SAVE in Basic
      for (i = 0; i < 8; i++) {
        block[i] = 0xFF;
      }
      return;
    }
  }

  // OSWORD   R2: &08 A in_length block out_length  block
  sendByte(R2_ID, 0x08);
  sendByte(R2_ID, a);
  sendByte(R2_ID, in_len);
  sendBlock(R2_ID, in_len, block);
  sendByte(R2_ID, out_len);
  receiveBlock(R2_ID, out_len, block);
}

void tube_File(unsigned int *reg) {
  if (DEBUG_ARM) {
    printf("%08x %08x %08x %08x %08x %08x\r\n", reg[0], reg[1], reg[2], reg[3], reg[4], reg[5]);
    printf("%s\r\n", (char *)reg[1]);
  }
  // start at the last param (r5)
  unsigned int *ptr = reg + 5;
  // OSFILE   R2: &14 block string &0D A            A block
  sendByte(R2_ID, 0x14);
  sendWord(R2_ID, *ptr--);            // r5 = attr
  sendWord(R2_ID, *ptr--);            // r4 = leng
  sendWord(R2_ID, *ptr--);            // r3 = exec
  sendWord(R2_ID, *ptr--);            // r2 = load
  // Send the filename, excluding terminating control character (anything < 0x20)
  sendStringWithoutTerminator(R2_ID, (char *)*ptr--);  // r1 = filename ptr
  // Send the 0x0D terminator
  sendByte(R2_ID, 0x0D);
  sendByte(R2_ID, *ptr);              // r0 = action
  *ptr = receiveByte(R2_ID);          // r0 = action
  ptr = reg + 5;                   // ptr = r5
  *ptr-- = receiveWord(R2_ID);        // r5 = attr
  *ptr-- = receiveWord(R2_ID);        // r4 = lang
  *ptr-- = receiveWord(R2_ID);        // r3 = exec
  *ptr-- = receiveWord(R2_ID);        // r2 = load
  if (DEBUG_ARM) {
    printf("%08x %08x %08x %08x %08x %08x\r\n", reg[0], reg[1], reg[2], reg[3], reg[4], reg[5]);
  }
}

void tube_Args(unsigned int *reg) {
  // OSARGS   R2: &0C Y block A                     A block
  sendByte(R2_ID, 0x0C);
  // Y = R1 is the file namdle
  sendByte(R2_ID, reg[1]);
  // R2 is the 4 byte data block
  sendWord(R2_ID, reg[2]);
  // A = R0 is the operation code
  sendByte(R2_ID, reg[0]);
  // get back A
  reg[0] = receiveByte(R2_ID);
  // get back 4 byte data block
  reg[2] = receiveWord(R2_ID);
}

void tube_BGet(unsigned int *reg) {
  // OSBGET   R2: &0E Y                             Cy A
  sendByte(R2_ID, 0x0E);
  // Y = R1 is the file namdle
  sendByte(R2_ID, reg[1]);
  // On exit, the Carry flag indicates validity
  updateCarry(receiveByte(R2_ID) & 0x80, reg);
  // On exit, R0 contains the character
  reg[0] = receiveByte(R2_ID);
}

void tube_BPut(unsigned int *reg) {
  // OSBPUT   R2: &10 Y A                           &7F
  sendByte(R2_ID, 0x10);
  // Y = R1 is the file namdle
  sendByte(R2_ID, reg[1]);
  // A = R0 is the character
  sendByte(R2_ID, reg[0]);
  // Response is always 7F so ignored
  receiveByte(R2_ID);
}

void tube_GBPB(unsigned int *reg) {
  // start at the last param (r4)
  unsigned int *ptr = reg + 4;
  // OSGBPB   R2: &16 block A                       block Cy A
  sendByte(R2_ID, 0x16);
  sendWord(R2_ID, *ptr--);              // r4
  sendWord(R2_ID, *ptr--);              // r3
  sendWord(R2_ID, *ptr--);              // r2
  sendByte(R2_ID, *ptr--);              // r1
  sendByte(R2_ID, *ptr);                // r0
  ptr = reg + 4;
  *ptr-- = receiveWord(R2_ID);          // r4
  *ptr-- = receiveWord(R2_ID);          // r3
  *ptr-- = receiveWord(R2_ID);          // r2
  *ptr-- = receiveByte(R2_ID);          // r1
  updateCarry(receiveByte(R2_ID) & 0x80, reg); // Cy
  *ptr-- = receiveByte(R2_ID);          // r0
}

void tube_Find(unsigned int *reg) {
  // OSFIND   R2: &12 &00 Y                         &7F
  // OSFIND   R2: &12 A string &0D                  A
  sendByte(R2_ID, 0x12);
  // A = R0 is the operation type
  sendByte(R2_ID, reg[0]);
  if (reg[0] == 0) {
    // Y = R1 is the file handle to close
    sendByte(R2_ID, reg[1]);
    // Response is always 7F so ignored
    receiveByte(R2_ID);
  } else {
    // R1 points to the string, terminated by any control character
    sendStringWithoutTerminator(R2_ID, (char *)reg[1]);
    // Send the 0x0D terminator
    sendByte(R2_ID, 0x0D);
    // Response is the file handle of file just opened
    reg[0] = receiveByte(R2_ID);
  }
}

void tube_ReadLine(unsigned int *reg) {
  unsigned char resp;
  // OSWORD0  R2: &0A block                         &FF or &7F string &0D
  sendByte(R2_ID, 0x0A);
  sendByte(R2_ID, reg[3]);      // max ascii value
  sendByte(R2_ID, reg[2]);      // min ascii value
  sendByte(R2_ID, reg[1]);      // max line length
  sendByte(R2_ID, 0x07);        // Buffer MSB - set as per Tube Ap Note 004
  sendByte(R2_ID, 0x00);        // Buffer LSB - set as per Tube Ap Note 004
  resp = receiveByte(R2_ID);    // 0x7F or 0xFF
  updateCarry(resp & 0x80, reg);
  // Was it valid?
  if ((resp & 0x80) == 0x00) {
    reg[1] = receiveString(R2_ID, '\r', (char *)reg[0]);
  }
}

void tube_GetEnv(unsigned int *reg) {
  reg[0] = (unsigned int) env->commandBuffer;       // R0 => command string (0 terminated) which ran the program
  reg[1] = (unsigned int) env->handler[MEMORY_LIMIT_HANDLER].handler; // R1 = permitted RAM limit for example &10000 for 64K machine
  reg[2] = (unsigned int) env->timeBuffer;        // R2 => 5 bytes - the time the program started running
  if (DEBUG_ARM) {
    printf("%08x %08x %08x\r\n", reg[0], reg[1], reg[2]);
  }
}

void tube_Exit(unsigned int *reg) {
  unsigned int r12 = env->handler[EXIT_HANDLER].r12;
  EnvironmentHandler_type handler = env->handler[EXIT_HANDLER].handler;
  if (DEBUG_ARM) {
    printf("Calling exit handler at %08x with r12 %08x\r\n", (unsigned int) handler, r12);
  }
  _exit_handler_wrapper(r12, handler);
}

void tube_IntOn(unsigned int *reg) {
  _enable_interrupts();
}

void tube_IntOff(unsigned int *reg) {
  _disable_interrupts();
}

void tube_EnterOS(unsigned int *reg) {
  // Set the mode on return from the call to be SVR mode
  updateMode(MODE_SVR, reg);
}

void tube_Mouse(unsigned int *reg) {
  // JGH: Read Mouse settings
  int msX, msY, msZ;

  reg[0]=128; reg[1]=7; reg[2]=0;
  tube_Byte(reg);      // ADVAL(7)
  msX=reg[1];         // Mouse X

  reg[0]=128; reg[1]=8; reg[2]=0;
  tube_Byte(reg);      // ADVAL(8)
  msY=reg[1];         // Mouse Y

  reg[0]=128; reg[1]=9; reg[2]=0;
  tube_Byte(reg);      // ADVAL(9)
  msZ=reg[1];         // Mouse Z (buttons)

  reg[0]=msX;
  reg[1]=msY;
  reg[2]=msZ;
}

void tube_GenerateError(unsigned int *reg) {
  // The error block is passed to the SWI in reg 0
  ErrorBlock_type *eblk = (ErrorBlock_type *)reg[0];
  // Error address from the stacked link register
  generate_error((void *)reg[13], eblk->errorNum, eblk->errorMsg);
}

// Entry:
// R0   X coordinate
// R1   Y coordinate
//
// Exit:
// R0   preserved
// R1   preserved
// R2   colour
// Map to OSWORD A=&9;
void tube_ReadPoint(unsigned int *reg) {
  // OSWORD   R2: &08 A in_length block out_length  block
  sendByte(R2_ID, 0x08);
  sendByte(R2_ID, 0x09);        // OSWORD A=&09
  sendByte(R2_ID, 0x04);        // inlen = 4
  sendByte(R2_ID, reg[0]);      // LSB of X coord
  sendByte(R2_ID, reg[0] >> 8); // MSB of X coord
  sendByte(R2_ID, reg[1]);      // LSB of Y coord
  sendByte(R2_ID, reg[1] >> 8); // MSB of Y coord
  sendByte(R2_ID, 0x05);        // outlen = 5
  receiveByte(R2_ID);           // LSB of X coord
  receiveByte(R2_ID);           // MSB of X coord
  receiveByte(R2_ID);           // LSB of Y coord
  receiveByte(R2_ID);           // MSB of Y coord
  reg[2] = receiveByte(R2_ID);  // logical colour of point, or 0xFF is the point is off screen
}

// Entry:
// R0   Handler number
// R1   New address, or 0 to read
// R2   Value of R12 when code is called
// R3   Pointer to buffer if appropriate or 0 to read
// Exit:
// R0   Preserved
// R1   Previous address
// R2   Previous R12
// R3   Previous buffer pointer

void tube_ChangeEnvironment(unsigned int *reg) {
  unsigned int previous;

  if (DEBUG_ARM) {
    printf("%08x %08x %08x %08x\r\n", reg[0], reg[1], reg[2], reg[3]);
  }

  unsigned int n = reg[0];

  if (n < NUM_HANDLERS) {

    // Grab the approriate handler state block from the environment
    HandlerState_type *hs = &env->handler[n];

    // Update the handler function from reg1
    previous = (unsigned int) hs->handler;
    if (reg[1]) {
      hs->handler = (EnvironmentHandler_type) reg[1];
    }
    reg[1] = previous;

    // Update the r12 value from reg2
    previous = hs->r12;
    if (reg[2]) {
      hs->r12 = reg[2];
    }
    reg[2] = previous;

    // Update the buffer address from reg3
    previous = (unsigned int) hs->address;
    if (reg[3]) {
      hs->address = (void *) reg[3];
    }
    reg[3] = previous;


  } else {

    handler_not_defined(n);

  }

  if (DEBUG_ARM) {
    printf("%08x %08x %08x %08x\r\n", reg[0], reg[1], reg[2], reg[3]);
  }

}

void tube_Plot(unsigned int *reg) {
    sendByte(R1_ID, 25);
    sendByte(R1_ID, reg[0]);
    sendByte(R1_ID, reg[1]);
    sendByte(R1_ID, reg[1] >> 8);
    sendByte(R1_ID, reg[2]);
    sendByte(R1_ID, reg[2] >> 8);
}

void tube_WriteN(unsigned int *reg) {
  unsigned char *ptr = (unsigned char *)reg[0];
  int len = reg[1];
  while (len-- > 0) {
    sendByte(R1_ID, *ptr++);
  }
}

void OS_SynchroniseCodeAreas(unsigned int *reg)
{
 _invalidate_icache();
}

// The purpose of this call is to lookup and output (via OS_WriteC) a help message for a given BASIC keyword.

void tube_BASICTrans_HELP(unsigned int *reg) {
  // Return with V set to indicate BASICTrans not present
  updateOverflow(1, reg);
}

// The purpose of this call is to lookup an error message. The buffer pointer to by R1 must be at least 252 bytes long.

void tube_BASICTrans_Error(unsigned int *reg) {
  // Return with V set to indicate BASICTrans not present
  updateOverflow(1, reg);
}

// The purpose of this call is to lookup and display (via OS_WriteC) a message.

void tube_BASICTrans_Message(unsigned int *reg) {
  // Return with V set to indicate BASICTrans not present
  updateOverflow(1, reg);
}
