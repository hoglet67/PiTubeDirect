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
#include "swi.h"

static unsigned int last_r12 = 0;

static const unsigned char osword_in_len[] = {
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
  5,   // 12  Palette=
  0,   // 13  =Coords
  8,   // 14  =RTC
  25,  // 15  RTC=
  16,  // 16  NetTx
  13,  // 17  NetRx
  0,   // 18  NetArgs
  8,   // 19  NetInfo
  128  // 20  NetFSOp
};

static const unsigned char osword_out_len[] = {
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

// SWI handler prototypes
static void tube_WriteC(unsigned int *reg);               // &00
static void tube_WriteS(unsigned int *reg);               // &01
static void tube_Write0(unsigned int *reg);               // &02
static void tube_NewLine(unsigned int *reg);              // &03
static void tube_ReadC(unsigned int *reg);                // &04
static void tube_CLI(unsigned int *reg);                  // &05
static void tube_Byte(unsigned int *reg);                 // &06
static void tube_Word(unsigned int *reg);                 // &07
static void tube_File(unsigned int *reg);                 // &08
static void tube_Args(unsigned int *reg);                 // &09
static void tube_BGet(unsigned int *reg);                 // &0A
static void tube_BPut(unsigned int *reg);                 // &0B
static void tube_GBPB(unsigned int *reg);                 // &0C
static void tube_Find(unsigned int *reg);                 // &0D
static void tube_ReadLine(unsigned int *reg);             // &0E
static void tube_GetEnv(unsigned int *reg);               // &10
static void tube_Exit(unsigned int *reg);                 // &11
static void tube_IntOn(unsigned int *reg);                // &13
static void tube_IntOff(unsigned int *reg);               // &14
static void tube_EnterOS(unsigned int *reg);              // &16
static void tube_Mouse(unsigned int *reg);                // &1C
static void tube_GenerateError(unsigned int *reg);        // &2B
static void tube_ReadPoint(unsigned int *reg);            // &32
static void tube_SWI_NumberToString(unsigned int *reg);   // &38
static void tube_SWI_NumberFromString(unsigned int *reg); // &39
static void tube_ChangeEnvironment(unsigned int *reg);    // &40
static void tube_Plot(unsigned int *reg);                 // &45
static void tube_WriteN(unsigned int *reg);               // &46
static void tube_SynchroniseCodeAreas(unsigned int *reg); // &6E
static void tube_CallASWI(unsigned int *reg);             // &6F
static void tube_CallASWIR12(unsigned int *reg);          // &71
static void tube_BASICTrans_HELP(unsigned int *reg);      // &42C80
static void tube_BASICTrans_Error(unsigned int *reg);     // &42C81
static void tube_BASICTrans_Message(unsigned int *reg);   // &42C82

static void handler_not_defined(unsigned int num);
static void tube_SWI_Not_Known(unsigned int *reg);

SWIDescriptor_Type SWI_Table[] = {
   {tube_WriteC,               "OS_WriteC"},                     // &00
   {tube_WriteS,               "OS_WriteS"},                     // &01
   {tube_Write0,               "OS_Write0"},                     // &02
   {tube_NewLine,              "OS_NewLine"},                    // &03
   {tube_ReadC,                "OS_ReadC"},                      // &04
   {tube_CLI,                  "OS_CLI"},                        // &05
   {tube_Byte,                 "OS_Byte"},                       // &06
   {tube_Word,                 "OS_Word"},                       // &07
   {tube_File,                 "OS_File"},                       // &08
   {tube_Args,                 "OS_Args"},                       // &09
   {tube_BGet,                 "OS_BGet"},                       // &0A
   {tube_BPut,                 "OS_BPut"},                       // &0B
   {tube_GBPB,                 "OS_GBPB"},                       // &0C
   {tube_Find,                 "OS_Find"},                       // &0D
   {tube_ReadLine,             "OS_ReadLine"},                   // &0E
   {tube_SWI_Not_Known,        "OS_Control"},                    // &0F
   {tube_GetEnv,               "OS_GetEnv"},                     // &10
   {tube_Exit,                 "OS_Exit"},                       // &11
   {tube_SWI_Not_Known,        "OS_SetEnv"},                     // &12
   {tube_IntOn,                "OS_IntOn"},                      // &13
   {tube_IntOff,               "OS_IntOff"},                     // &14
   {tube_SWI_Not_Known,        "OS_CallBack"},                   // &15
   {tube_EnterOS,              "OS_EnterOS"},                    // &16
   {tube_SWI_Not_Known,        "OS_BreakPt"},                    // &17
   {tube_SWI_Not_Known,        "OS_BreakCtrl"},                  // &18
   {tube_SWI_Not_Known,        "OS_UnusedSWI"},                  // &19
   {tube_SWI_Not_Known,        "OS_UpdateMEMC"},                 // &1A
   {tube_SWI_Not_Known,        "OS_SetCallBack"},                // &1B
   {tube_Mouse,                "OS_Mouse"},                      // &1C
   {tube_SWI_Not_Known,        "OS_Heap"},                       // &1D
   {tube_SWI_Not_Known,        "OS_Module"},                     // &1E
   {tube_SWI_Not_Known,        "OS_Claim"},                      // &1F
   {tube_SWI_Not_Known,        "OS_Release"},                    // &20
   {tube_SWI_Not_Known,        "OS_ReadUnsigned"},               // &21
   {tube_SWI_Not_Known,        "OS_GenerateEvent"},              // &22
   {tube_SWI_Not_Known,        "OS_ReadVarVal"},                 // &23
   {tube_SWI_Not_Known,        "OS_SetVarVal"},                  // &24
   {tube_SWI_Not_Known,        "OS_GSInit"},                     // &25
   {tube_SWI_Not_Known,        "OS_GSRead"},                     // &26
   {tube_SWI_Not_Known,        "OS_GSTrans"},                    // &27
   {tube_SWI_Not_Known,        "OS_BinaryToDecimal"},            // &28
   {tube_SWI_Not_Known,        "OS_FSControl"},                  // &29
   {tube_SWI_Not_Known,        "OS_ChangeDynamicArea"},          // &2A
   {tube_GenerateError,        "OS_GenerateError"},              // &2B
   {tube_SWI_Not_Known,        "OS_ReadEscapeState"},            // &2C
   {tube_SWI_Not_Known,        "OS_EvaluateExpression"},         // &2D
   {tube_SWI_Not_Known,        "OS_SpriteOp"},                   // &2E
   {tube_SWI_Not_Known,        "OS_ReadPalette"},                // &2F
   {tube_SWI_Not_Known,        "OS_ServiceCall"},                // &30
   {tube_SWI_Not_Known,        "OS_ReadVduVariables"},           // &31
   {tube_ReadPoint,            "OS_ReadPoint"},                  // &32
   {tube_SWI_Not_Known,        "OS_UpCall"},                     // &33
   {tube_SWI_Not_Known,        "OS_CallAVector"},                // &34
   {tube_SWI_Not_Known,        "OS_ReadModeVariable"},           // &35
   {tube_SWI_Not_Known,        "OS_RemoveCursors"},              // &36
   {tube_SWI_Not_Known,        "OS_RestoreCursors"},             // &37
   {tube_SWI_NumberToString,   "OS_SWINumberToString"},          // &38
   {tube_SWI_NumberFromString, "OS_SWINumberFromString"},        // &39
   {tube_SWI_Not_Known,        "OS_ValidateAddress"},            // &3A
   {tube_SWI_Not_Known,        "OS_CallAfter"},                  // &3B
   {tube_SWI_Not_Known,        "OS_CallEvery"},                  // &3C
   {tube_SWI_Not_Known,        "OS_RemoveTickerEvent"},          // &3D
   {tube_SWI_Not_Known,        "OS_InstallKeyHandler"},          // &3E
   {tube_SWI_Not_Known,        "OS_CheckModeValid"},             // &3F
   {tube_ChangeEnvironment,    "OS_ChangeEnvironment"},          // &40
   {tube_SWI_Not_Known,        "OS_ClaimScreenMemory"},          // &41
   {tube_SWI_Not_Known,        "OS_ReadMonotonicTime"},          // &42
   {tube_SWI_Not_Known,        "OS_SubstituteArgs"},             // &43
   {tube_SWI_Not_Known,        "OS_PrettyPrint"},                // &44
   {tube_Plot,                 "OS_Plot"},                       // &45
   {tube_WriteN,               "OS_WriteN"},                     // &46
   {tube_SWI_Not_Known,        "OS_AddToVector"},                // &47
   {tube_SWI_Not_Known,        "OS_WriteEnv"},                   // &48
   {tube_SWI_Not_Known,        "OS_ReadArgs"},                   // &49
   {tube_SWI_Not_Known,        "OS_ReadRAMFsLimits"},            // &4A
   {tube_SWI_Not_Known,        "OS_ClaimDeviceVector"},          // &4B
   {tube_SWI_Not_Known,        "OS_ReleaseDeviceVector"},        // &4C
   {tube_SWI_Not_Known,        "OS_DelinkApplication"},          // &4D
   {tube_SWI_Not_Known,        "OS_RelinkApplication"},          // &4E
   {tube_SWI_Not_Known,        "OS_HeapSort"},                   // &4F
   {tube_SWI_Not_Known,        "OS_ExitAndDie"},                 // &50
   {tube_SWI_Not_Known,        "OS_ReadMemMapInfo"},             // &51
   {tube_SWI_Not_Known,        "OS_ReadMemMapEntries"},          // &52
   {tube_SWI_Not_Known,        "OS_SetMemMapEntries"},           // &53
   {tube_SWI_Not_Known,        "OS_AddCallBack"},                // &54
   {tube_SWI_Not_Known,        "OS_ReadDefaultHandler"},         // &55
   {tube_SWI_Not_Known,        "OS_SetECFOrigin"},               // &56
   {tube_SWI_Not_Known,        "OS_SerialOp"},                   // &57
   {tube_SWI_Not_Known,        "OS_ReadSysInfo"},                // &58
   {tube_SWI_Not_Known,        "OS_Confirm"},                    // &59
   {tube_SWI_Not_Known,        "OS_ChangedBox"},                 // &5A
   {tube_SWI_Not_Known,        "OS_CRC"},                        // &5B
   {tube_SWI_Not_Known,        "OS_ReadDynamicArea"},            // &5C
   {tube_SWI_Not_Known,        "OS_PrintChar"},                  // &5D
   {tube_SWI_Not_Known,        "OS_ChangeRedirection"},          // &5E
   {tube_SWI_Not_Known,        "OS_RemoveCallBack"},             // &5F
   {tube_SWI_Not_Known,        "OS_FindMemMapEntries"},          // &60
   {tube_SWI_Not_Known,        "OS_SetColour"},                  // &61
   {tube_SWI_Not_Known,        "OS_ClaimSWI"},                   // &62
   {tube_SWI_Not_Known,        "OS_ReleaseSWI"},                 // &63
   {tube_SWI_Not_Known,        "OS_Pointer"},                    // &64
   {tube_SWI_Not_Known,        "OS_ScreenMode"},                 // &65
   {tube_SWI_Not_Known,        "OS_DynamicArea"},                // &66
   {tube_SWI_Not_Known,        "OS_AbortTrap"},                  // &67
   {tube_SWI_Not_Known,        "OS_Memory"},                     // &68
   {tube_SWI_Not_Known,        "OS_ClaimProcessorVector"},       // &69
   {tube_SWI_Not_Known,        "OS_Reset"},                      // &6A
   {tube_SWI_Not_Known,        "OS_MMUControl"},                 // &6B
   {tube_SWI_Not_Known,        "OS_ResyncTime"},                 // &6C
   {tube_SWI_Not_Known,        "OS_PlatformFeatures"},           // &6D
   {tube_SynchroniseCodeAreas, "OS_SynchroniseCodeAreas"},       // &6E
   {tube_CallASWI,             "OS_CallASWI"},                   // &6F
   {tube_SWI_Not_Known,        "OS_AMBControl"},                 // &70
   {tube_CallASWIR12,          "OS_CallASWIR12"},                // &71
   {tube_SWI_Not_Known,        "OS_SpecialControl"},             // &72
   {tube_SWI_Not_Known,        "OS_EnterUSR32"},                 // &73
   {tube_SWI_Not_Known,        "OS_EnterUSR26"},                 // &74
   {tube_SWI_Not_Known,        "OS_VIDCDivider"},                // &75
   {tube_SWI_Not_Known,        "OS_NVMemory"},                   // &76
   {tube_SWI_Not_Known,        "OS_ClaimOSSWI"},                 // &77
   {tube_SWI_Not_Known,        "OS_TaskControl"},                // &78
   {tube_SWI_Not_Known,        "OS_DeviceDriver"},               // &79
   {tube_SWI_Not_Known,        "OS_Hardware"},                   // &7A
   {tube_SWI_Not_Known,        "OS_IICOp"},                      // &7B
   {tube_SWI_Not_Known,        "OS_LeaveOS"},                    // &7C
   {tube_SWI_Not_Known,        "OS_ReadLine32"},                 // &7D
   {tube_SWI_Not_Known,        "OS_SubstituteArgs32"},           // &7E
   {tube_SWI_Not_Known,        "OS_HeapSort32"},                 // &7F
   {tube_SWI_Not_Known,        NULL},                            // &80
   {tube_SWI_Not_Known,        NULL},                            // &81
   {tube_SWI_Not_Known,        NULL},                            // &82
   {tube_SWI_Not_Known,        NULL},                            // &83
   {tube_SWI_Not_Known,        NULL},                            // &84
   {tube_SWI_Not_Known,        NULL},                            // &85
   {tube_SWI_Not_Known,        NULL},                            // &86
   {tube_SWI_Not_Known,        NULL},                            // &87
   {tube_SWI_Not_Known,        NULL},                            // &88
   {tube_SWI_Not_Known,        NULL},                            // &89
   {tube_SWI_Not_Known,        NULL},                            // &8A
   {tube_SWI_Not_Known,        NULL},                            // &8B
   {tube_SWI_Not_Known,        NULL},                            // &8C
   {tube_SWI_Not_Known,        NULL},                            // &8D
   {tube_SWI_Not_Known,        NULL},                            // &8E
   {tube_SWI_Not_Known,        NULL},                            // &8F
   {tube_SWI_Not_Known,        NULL},                            // &90
   {tube_SWI_Not_Known,        NULL},                            // &91
   {tube_SWI_Not_Known,        NULL},                            // &92
   {tube_SWI_Not_Known,        NULL},                            // &93
   {tube_SWI_Not_Known,        NULL},                            // &94
   {tube_SWI_Not_Known,        NULL},                            // &95
   {tube_SWI_Not_Known,        NULL},                            // &96
   {tube_SWI_Not_Known,        NULL},                            // &97
   {tube_SWI_Not_Known,        NULL},                            // &98
   {tube_SWI_Not_Known,        NULL},                            // &99
   {tube_SWI_Not_Known,        NULL},                            // &9A
   {tube_SWI_Not_Known,        NULL},                            // &9B
   {tube_SWI_Not_Known,        NULL},                            // &9C
   {tube_SWI_Not_Known,        NULL},                            // &9D
   {tube_SWI_Not_Known,        NULL},                            // &9E
   {tube_SWI_Not_Known,        NULL},                            // &9F
   {tube_SWI_Not_Known,        NULL},                            // &A0
   {tube_SWI_Not_Known,        NULL},                            // &A1
   {tube_SWI_Not_Known,        NULL},                            // &A2
   {tube_SWI_Not_Known,        NULL},                            // &A3
   {tube_SWI_Not_Known,        NULL},                            // &A4
   {tube_SWI_Not_Known,        NULL},                            // &A5
   {tube_SWI_Not_Known,        NULL},                            // &A6
   {tube_SWI_Not_Known,        NULL},                            // &A7
   {tube_SWI_Not_Known,        NULL},                            // &A8
   {tube_SWI_Not_Known,        NULL},                            // &A9
   {tube_SWI_Not_Known,        NULL},                            // &AA
   {tube_SWI_Not_Known,        NULL},                            // &AB
   {tube_SWI_Not_Known,        NULL},                            // &AC
   {tube_SWI_Not_Known,        NULL},                            // &AD
   {tube_SWI_Not_Known,        NULL},                            // &AE
   {tube_SWI_Not_Known,        NULL},                            // &AF
   {tube_SWI_Not_Known,        NULL},                            // &B0
   {tube_SWI_Not_Known,        NULL},                            // &B1
   {tube_SWI_Not_Known,        NULL},                            // &B2
   {tube_SWI_Not_Known,        NULL},                            // &B3
   {tube_SWI_Not_Known,        NULL},                            // &B4
   {tube_SWI_Not_Known,        NULL},                            // &B5
   {tube_SWI_Not_Known,        NULL},                            // &B6
   {tube_SWI_Not_Known,        NULL},                            // &B7
   {tube_SWI_Not_Known,        NULL},                            // &B8
   {tube_SWI_Not_Known,        NULL},                            // &B9
   {tube_SWI_Not_Known,        NULL},                            // &BA
   {tube_SWI_Not_Known,        NULL},                            // &BB
   {tube_SWI_Not_Known,        NULL},                            // &BC
   {tube_SWI_Not_Known,        NULL},                            // &BD
   {tube_SWI_Not_Known,        NULL},                            // &BE
   {tube_SWI_Not_Known,        NULL},                            // &BF
   {tube_SWI_Not_Known,        "OS_ConvertStandardDateAndTime"}, // &C0
   {tube_SWI_Not_Known,        "OS_ConvertDateAndTime"},         // &C1
   {tube_SWI_Not_Known,        NULL},                            // &C2
   {tube_SWI_Not_Known,        NULL},                            // &C3
   {tube_SWI_Not_Known,        NULL},                            // &C4
   {tube_SWI_Not_Known,        NULL},                            // &C5
   {tube_SWI_Not_Known,        NULL},                            // &C6
   {tube_SWI_Not_Known,        NULL},                            // &C7
   {tube_SWI_Not_Known,        NULL},                            // &C8
   {tube_SWI_Not_Known,        NULL},                            // &C9
   {tube_SWI_Not_Known,        NULL},                            // &CA
   {tube_SWI_Not_Known,        NULL},                            // &CB
   {tube_SWI_Not_Known,        NULL},                            // &CC
   {tube_SWI_Not_Known,        NULL},                            // &CD
   {tube_SWI_Not_Known,        NULL},                            // &CE
   {tube_SWI_Not_Known,        NULL},                            // &CF
   {tube_SWI_Not_Known,        "OS_ConvertHex1"},                // &D0
   {tube_SWI_Not_Known,        "OS_ConvertHex2"},                // &D1
   {tube_SWI_Not_Known,        "OS_ConvertHex3"},                // &D2
   {tube_SWI_Not_Known,        "OS_ConvertHex4"},                // &D3
   {tube_SWI_Not_Known,        "OS_ConvertHex8"},                // &D4
   {tube_SWI_Not_Known,        "OS_ConvertCardinal1"},           // &D5
   {tube_SWI_Not_Known,        "OS_ConvertCardinal2"},           // &D6
   {tube_SWI_Not_Known,        "OS_ConvertCardinal3"},           // &D7
   {tube_SWI_Not_Known,        "OS_ConvertCardinal4"},           // &D8
   {tube_SWI_Not_Known,        "OS_ConvertInteger1"},            // &D9
   {tube_SWI_Not_Known,        "OS_ConvertInteger2"},            // &DA
   {tube_SWI_Not_Known,        "OS_ConvertInteger3"},            // &DB
   {tube_SWI_Not_Known,        "OS_ConvertInteger4"},            // &DC
   {tube_SWI_Not_Known,        "OS_ConvertBinary1"},             // &DD
   {tube_SWI_Not_Known,        "OS_ConvertBinary2"},             // &DE
   {tube_SWI_Not_Known,        "OS_ConvertBinary3"},             // &DF
   {tube_SWI_Not_Known,        "OS_ConvertBinary4"},             // &E0
   {tube_SWI_Not_Known,        "OS_ConvertSpacedCardinal1"},     // &E1
   {tube_SWI_Not_Known,        "OS_ConvertSpacedCardinal2"},     // &E2
   {tube_SWI_Not_Known,        "OS_ConvertSpacedCardinal3"},     // &E3
   {tube_SWI_Not_Known,        "OS_ConvertSpacedCardinal4"},     // &E4
   {tube_SWI_Not_Known,        "OS_ConvertSpacedInteger1"},      // &E5
   {tube_SWI_Not_Known,        "OS_ConvertSpacedInteger2"},      // &E6
   {tube_SWI_Not_Known,        "OS_ConvertSpacedInteger3"},      // &E7
   {tube_SWI_Not_Known,        "OS_ConvertSpacedInteger4"},      // &E8
   {tube_SWI_Not_Known,        "OS_ConvertFixedNetStation"},     // &E9
   {tube_SWI_Not_Known,        "OS_ConvertNetStation"},          // &EA
   {tube_SWI_Not_Known,        "OS_ConvertFixedFileSize"},       // &EB
   {tube_SWI_Not_Known,        "OS_ConvertFileSize"},            // &EC
   {tube_SWI_Not_Known,        NULL},                            // &ED
   {tube_SWI_Not_Known,        NULL},                            // &EE
   {tube_SWI_Not_Known,        NULL},                            // &EF
   {tube_SWI_Not_Known,        NULL},                            // &F0
   {tube_SWI_Not_Known,        NULL},                            // &F1
   {tube_SWI_Not_Known,        NULL},                            // &F2
   {tube_SWI_Not_Known,        NULL},                            // &F3
   {tube_SWI_Not_Known,        NULL},                            // &F4
   {tube_SWI_Not_Known,        NULL},                            // &F5
   {tube_SWI_Not_Known,        NULL},                            // &F6
   {tube_SWI_Not_Known,        NULL},                            // &F7
   {tube_SWI_Not_Known,        NULL},                            // &F8
   {tube_SWI_Not_Known,        NULL},                            // &F9
   {tube_SWI_Not_Known,        NULL},                            // &FA
   {tube_SWI_Not_Known,        NULL},                            // &FB
   {tube_SWI_Not_Known,        NULL},                            // &FC
   {tube_SWI_Not_Known,        NULL},                            // &FD
   {tube_SWI_Not_Known,        NULL},                            // &FE
   {tube_SWI_Not_Known,        NULL}                             // &FF
};

static char *lookup_swi_name(unsigned int num) {
   static char name[SWI_NAME_LEN];
   char *ptr = name;

   // If bit 17 of the number is set, prefix the returned string with X
   if (num & 0x20000) {
      *ptr++ = 'X';
      num -= 0x20000;
   }

   if (num < NUM_SWI_HANDLERS) {
      // Lookup the SWI name in the handler table
      if (SWI_Table[num].name) {
         strcpy(ptr, SWI_Table[num].name);
      } else {
         strcpy(ptr, "OS_Undefined");
      }
   } else if (num >= 0x100 && num < 0x200) {
      // Special case OS_WriteI
      num &= 0xFF;
      if (num >= 0x20 && num < 0x7F) {
         sprintf(ptr, "OS_WriteI+\"%c\"", num);
      } else {
         sprintf(ptr, "OS_WriteI+%d", num);
      }
   } else {
      // Anything else causes User to be returned
      strcpy(ptr, "User");
   }

   return name;
}

// A helper method to make generating errors easier
void generate_error(void * address, unsigned int errorNum, char *errorMsg) {
  // Get the current handler's error buffer
  ErrorBuffer_type *ebuf = (ErrorBuffer_type *)env->handler[ERROR_HANDLER].address;
  // Copy the error address into the handler's error block
  ebuf->errorAddr = address;
  // Copy the error number into the handler's error block
  ebuf->errorBlock.errorNum = errorNum;
  // Copy the error string into the handler's error block
  strncpy(ebuf->errorBlock.errorMsg, errorMsg, sizeof(ebuf->errorBlock.errorMsg));
  // if we truncate the string ensure it is null terminated
  ebuf->errorBlock.errorMsg[sizeof(ebuf->errorBlock.errorMsg)-1] = '\0';
  // The wrapper drops back to user mode and calls the handler
  unsigned int r0 = env->handler[ERROR_HANDLER].r12;
  _error_handler_wrapper(r0, last_r12, env->handler[ERROR_HANDLER].handler);
}

static void updateOverflow(unsigned char ovf, unsigned int *reg) {
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

static void updateMode(unsigned char mode, unsigned int *reg) {
  // The PSW is on the stack two words before the registers
  reg-= 2;
  *reg &= ~0x1fu;
  *reg |= mode;
}

// For an unimplemented environment handler
void handler_not_implemented(char *type) {
  printf("Handler %s defined but not implemented\r\n", type);
}

// For an undefined environment handler (i.e. where num >= NUM_HANDLERS)
static void handler_not_defined(unsigned int num) {
  printf("Handler 0x%x not defined\r\n", num);
}

// For an unimplemented SWI
static void tube_SWI_Not_Known(unsigned int *reg) {
  unsigned int *lr = (unsigned int *)reg[13];
  printf("%08x %08x %08x %08x\r\n", reg[0], reg[1], reg[2], reg[3]);
  unsigned int num = *(lr - 1) & 0xFFFFFF;
  char *name = lookup_swi_name(num);
  printf("SWI %08x (%s) not implemented ************\r\n", num, name);
}

void C_SWI_Handler(unsigned int number, unsigned int *reg) {
  unsigned int num = number;
  int errorBit = 0;
  if (DEBUG_ARM) {
    printf("SWI %08x called from %08x cpsr=%08x\r\n", number, reg[13] - 4, _get_cpsr());
  }
  // Save r12 so it can be restored by the error handler wrapper
  // (ARM Basic makes the bad assumption that r12 is preserved)
  last_r12 = reg[12];
  // TODO - We need to switch to a local error handler
  // for the error bit to work as intended.
  if (num & ERROR_BIT) {
    errorBit = 1;
    num &= ~ERROR_BIT;
  }
  if (num < NUM_SWI_HANDLERS) {
    // Invoke one of the fixed handlers
    SWI_Table[num].handler(reg);
  } else if ((num & 0xFFFFFF00) == 0x0100) {      // JGH
    // SWIs 0x100 - 0x1FF are OS_WriteI
    SWI_Table[SWI_OS_WriteC].handler(&num);
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

int user_exec_fn(FunctionPtr_Type f, unsigned int param ) {
  int ret;
  if (DEBUG_ARM) {
    printf("Execution passing to %08x cpsr = %08x", (unsigned int)f, _get_cpsr());
    if (param) {
       char *p = (char *)param;
       printf(" param = ");
       while (*p != 0 && *p != 10 && *p != 13) {
          putchar(*p++);
       }
    }
    printf("\r\n");
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
  unsigned int off;
  unsigned int carry = 0, r0 = 0, r1 = 0, r12 = 0; // Entry parameters

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
      off=(unsigned int)address[16] + 256 * (unsigned int)address[17] + 65536 * (unsigned int)address[18];
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
    off=(unsigned int)address[0]+256*(unsigned int)address[1]+65536*(unsigned int)address[2];
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

static char *write_string(char *ptr) {
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

static void tube_WriteC(unsigned int *reg) {
  sendByte(R1_ID, (unsigned char)((reg[0]) & 0xff));
}

static void tube_WriteS(unsigned int *reg) {
  // Reg 13 is the stacked link register which points to the string
  reg[13] = (unsigned int) write_string((char *)reg[13]);
  // Make sure new value of link register is word aligned to the next word boundary
  reg[13] += 3;
  reg[13] &= 0xfffffffcu;
}

static void tube_Write0(unsigned int *reg) {
  // On exit, R0 points to the byte after the terminator
  reg[0] = (unsigned int)write_string((char *)reg[0]);;
}

static void tube_NewLine(unsigned int *reg) {
  sendByte(R1_ID, 0x0A);
  sendByte(R1_ID, 0x0D);
}

static void tube_ReadC(unsigned int *reg) {
  // OSRDCH   R2: &00                               Cy A
  sendByte(R2_ID, 0x00);
  // On exit, the Carry flag indicates validity
  updateCarry(receiveByte(R2_ID) & 0x80, reg);
  // On exit, R0 contains the character
  reg[0] = receiveByte(R2_ID);
}


// Legitimate terminators for OS_CLI are are 0x00, 0x0A or 0x0D

static void tube_CLI(unsigned int *reg) {

  // Keep a copy of the original command, so it's not perturbed when we fake the environmeny
  char command[256];
  char *ptr = (char *)(*reg);
  char c;
  unsigned int index = 0;
  do {
    c = (command[index++] = *ptr++);
  } while (c != 0x00 && c != 0x0a && c != 0x0d && index < sizeof(command));

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

// Skip leading spaces and stars, leaving ptr pointing
// to what needs to be passed to the filesystem, or parsed
  ptr = command;
  while (*ptr == ' ' || *ptr == '*') {
    ptr++;
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

  char *lptr = ptr;
  int run = 0;
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

  if (run) {                       // Skip any spaces proceeding filename
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
  index = 0;
  while (*lptr >= ' ') {         // Copy this command to environment string
    env->commandBuffer[index++] = *lptr++;
  }
  env->commandBuffer[index]=0;
//  env->handler[MEMORY_LIMIT_HANDLER].handler=???; // Can't remember if these are set now or later
//  env->timeBuffer=now_centiseconds();     // Will need to check real hardware


  if (run || dispatchCmd(ptr)) {       // dispatchCmd returns 0 if command was handled locally
    // OSCLI    R2: &02 string &0D                    &7F or &80
    sendByte(R2_ID, 0x02);
    // Send the command, excluding terminating control character (anything < 0x20)
    sendStringWithoutTerminator(R2_ID, ptr);
    // Send the 0x0D terminator
    sendByte(R2_ID, 0x0D);
    if (receiveByte(R2_ID) & 0x80) {
      // Execution should pass to last transfer address
      user_exec_raw(tube_address);
      // Possibly this will return...
    }
  }
}

static void tube_Byte(unsigned int *reg) {
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
          user_exec_raw(tube_address);
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
    reg[1] = x;
    reg[2] = y;
    updateCarry(cy, reg);
  }
  if (DEBUG_ARM) {
    printf("%08x %08x %08x\r\n", reg[0], reg[1], reg[2]);
  }
}

static void tube_Word(unsigned int *reg) {
  unsigned char in_len;
  unsigned char out_len;
  unsigned char a = reg[0] & 0xff;
  unsigned char *block;
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
      for (int i = 0; i < 8; i++) {
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

static void print_debug_string(char *s) {
   while (*s >= ' ') {
      putchar(*s++);
   }
   putchar(10);
   putchar(13);
}
static void tube_File(unsigned int *reg) {
  if (DEBUG_ARM) {
    printf("%08x %08x %08x %08x %08x %08x\r\n", reg[0], reg[1], reg[2], reg[3], reg[4], reg[5]);
    print_debug_string((char *)reg[1]);
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
  sendByte(R2_ID, (unsigned char )*ptr);              // r0 = action
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

static void tube_Args(unsigned int *reg) {
  // OSARGS   R2: &0C Y block A                     A block
  sendByte(R2_ID, 0x0C);
  // Y = R1 is the file namdle
  sendByte(R2_ID, (unsigned char )reg[1]);
  // R2 is the 4 byte data block
  sendWord(R2_ID, (unsigned char )reg[2]);
  // A = R0 is the operation code
  sendByte(R2_ID, (unsigned char )reg[0]);
  // get back A
  reg[0] = receiveByte(R2_ID);
  // get back 4 byte data block
  reg[2] = receiveWord(R2_ID);
}

static void tube_BGet(unsigned int *reg) {
  // OSBGET   R2: &0E Y                             Cy A
  sendByte(R2_ID, 0x0E);
  // Y = R1 is the file namdle
  sendByte(R2_ID, (unsigned char )reg[1]);
  // On exit, the Carry flag indicates validity
  updateCarry(receiveByte(R2_ID) & 0x80, reg);
  // On exit, R0 contains the character
  reg[0] = receiveByte(R2_ID);
}

static void tube_BPut(unsigned int *reg) {
  // OSBPUT   R2: &10 Y A                           &7F
  sendByte(R2_ID, 0x10);
  // Y = R1 is the file namdle
  sendByte(R2_ID, (unsigned char )reg[1]);
  // A = R0 is the character
  sendByte(R2_ID, (unsigned char )reg[0]);
  // Response is always 7F so ignored
  receiveByte(R2_ID);
}

static void tube_GBPB(unsigned int *reg) {
  // start at the last param (r4)
  unsigned int *ptr = reg + 4;
  // OSGBPB   R2: &16 block A                       block Cy A
  sendByte(R2_ID, 0x16);
  sendWord(R2_ID, *ptr--);              // r4
  sendWord(R2_ID, *ptr--);              // r3
  sendWord(R2_ID, *ptr--);              // r2
  sendByte(R2_ID, (unsigned char )*ptr--);              // r1
  sendByte(R2_ID, (unsigned char )*ptr);                // r0
  ptr = reg + 4;
  *ptr-- = receiveWord(R2_ID);          // r4
  *ptr-- = receiveWord(R2_ID);          // r3
  *ptr-- = receiveWord(R2_ID);          // r2
  *ptr-- = receiveByte(R2_ID);          // r1
  updateCarry(receiveByte(R2_ID) & 0x80, reg); // Cy
  *ptr-- = receiveByte(R2_ID);          // r0
}

static void tube_Find(unsigned int *reg) {
  // OSFIND   R2: &12 &00 Y                         &7F
  // OSFIND   R2: &12 A string &0D                  A
  sendByte(R2_ID, 0x12);
  // A = R0 is the operation type
  sendByte(R2_ID, (unsigned char )reg[0]);
  if (reg[0] == 0) {
    // Y = R1 is the file handle to close
    sendByte(R2_ID, (unsigned char )reg[1]);
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

static void tube_ReadLine(unsigned int *reg) {
   unsigned char resp;
   // OSWORD0  R2: &0A block                         &FF or &7F string &0D
   sendByte(R2_ID, 0x0A);
   sendByte(R2_ID, (unsigned char )reg[3]);      // max ascii value
   sendByte(R2_ID, (unsigned char )reg[2]);      // min ascii value
   sendByte(R2_ID, (unsigned char )reg[1]);      // max line length
   sendByte(R2_ID, 0x07);        // Buffer MSB - set as per Tube Ap Note 004
   sendByte(R2_ID, 0x00);        // Buffer LSB - set as per Tube Ap Note 004
   resp = receiveByte(R2_ID);    // 0x7F or 0xFF
   updateCarry(resp & 0x80, reg);
   // Was it valid?
   if ((resp & 0x80) == 0x00) {
      reg[1] = receiveString(R2_ID, '\r', (char *)reg[0]);
   }
}

static void tube_GetEnv(unsigned int *reg) {
  reg[0] = (unsigned int) env->commandBuffer;       // R0 => command string (0 terminated) which ran the program
  reg[1] = (unsigned int) env->handler[MEMORY_LIMIT_HANDLER].handler; // R1 = permitted RAM limit for example &10000 for 64K machine
  reg[2] = (unsigned int) env->timeBuffer;        // R2 => 5 bytes - the time the program started running
  if (DEBUG_ARM) {
    printf("%08x %08x %08x\r\n", reg[0], reg[1], reg[2]);
  }
}

static void tube_Exit(unsigned int *reg) {
  unsigned int r12 = env->handler[EXIT_HANDLER].r12;
  EnvironmentHandler_type handler = env->handler[EXIT_HANDLER].handler;
  if (DEBUG_ARM) {
    printf("Calling exit handler at %08x with r12 %08x\r\n", (unsigned int) handler, r12);
  }
  _exit_handler_wrapper(r12, handler);
}

static void tube_IntOn(unsigned int *reg) {
  _enable_interrupts();
}

static void tube_IntOff(unsigned int *reg) {
  _disable_interrupts();
}

static void tube_EnterOS(unsigned int *reg) {
  // Set the mode on return from the call to be SVR mode
  updateMode(MODE_SVR, reg);
}

static void tube_Mouse(unsigned int *reg) {
  // JGH: Read Mouse settings
  unsigned int msX, msY, msZ;

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

static void tube_GenerateError(unsigned int *reg) {
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
static void tube_ReadPoint(unsigned int *reg) {
  // OSWORD   R2: &08 A in_length block out_length  block
  sendByte(R2_ID, 0x08);
  sendByte(R2_ID, 0x09);        // OSWORD A=&09
  sendByte(R2_ID, 0x04);        // inlen = 4
  // Blocks are sent in reverse (high downto low)
  sendByte(R2_ID, (unsigned char )(reg[1] >> 8)); // MSB of Y coord
  sendByte(R2_ID, (unsigned char )(reg[1]));      // LSB of Y coord
  sendByte(R2_ID, (unsigned char )(reg[0] >> 8)); // MSB of X coord
  sendByte(R2_ID, (unsigned char )(reg[0]));      // LSB of X coord
  sendByte(R2_ID, 0x05);        // outlen = 5
  int pt = receiveByte(R2_ID);  // logical colour of point, or 0xFF is the point is off screen
  receiveByte(R2_ID);           // MSB of Y coord
  receiveByte(R2_ID);           // LSB of Y coord
  receiveByte(R2_ID);           // MSB of X coord
  receiveByte(R2_ID);           // LSB of X coord
  if (pt == 0xff) {
     reg[2] = 0xffffffffu;
     reg[3] = 0;
     reg[4] = 0xffffffffu;
  } else {
     reg[2] = ( unsigned int ) pt;
     reg[3] = 0;
     reg[4] = 0;
  }
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

static void tube_ChangeEnvironment(unsigned int *reg) {

  if (DEBUG_ARM) {
    printf("%08x %08x %08x %08x\r\n", reg[0], reg[1], reg[2], reg[3]);
  }

  unsigned int n = reg[0];

  if (n < NUM_HANDLERS) {
	unsigned int previous;
    // Grab the appropriate handler state block from the environment
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

static void tube_Plot(unsigned int *reg) {
    sendByte(R1_ID, 25);
    sendByte(R1_ID, (unsigned char )(reg[0]) );
    sendByte(R1_ID, (unsigned char )(reg[1]) );
    sendByte(R1_ID, (unsigned char )(reg[1] >> 8) );
    sendByte(R1_ID, (unsigned char )(reg[2]) );
    sendByte(R1_ID, (unsigned char )(reg[2] >> 8) );
}

static void tube_WriteN(unsigned int *reg) {
  unsigned char *ptr = (unsigned char *)reg[0];
  unsigned int len = reg[1];
  while (len-- > 0) {
    sendByte(R1_ID, *ptr++);
  }
}

static void tube_SynchroniseCodeAreas(unsigned int *reg) {
   _invalidate_icache();
}

// The purpose of this call is to lookup and output (via OS_WriteC) a help message for a given BASIC keyword.

static void tube_BASICTrans_HELP(unsigned int *reg) {
  // Return with V set to indicate BASICTrans not present
  updateOverflow(1, reg);
}

// The purpose of this call is to lookup an error message. The buffer pointer to by R1 must be at least 252 bytes long.

static void tube_BASICTrans_Error(unsigned int *reg) {
  // Return with V set to indicate BASICTrans not present
  updateOverflow(1, reg);
}

// The purpose of this call is to lookup and display (via OS_WriteC) a message.

static void tube_BASICTrans_Message(unsigned int *reg) {
  // Return with V set to indicate BASICTrans not present
  updateOverflow(1, reg);
}

static void tube_SWI_NumberFromString(unsigned int *reg) {
   char name[SWI_NAME_LEN];
   int num = 0;

   // On entry, r1 points to a control character terminated string
   char *ptr = (char *)reg[1];

   // If the name starts with X, return num plus 0x20000
   if (*ptr == 'X') {
      num += 0x20000;
      ptr++;
   }

   // Copy the string, stopping at the first control character
   unsigned int i = 0;
   while (i < SWI_NAME_LEN - 1 && *ptr >= 0x20 && *ptr < 0x7F) {
      name[i++] = *ptr++;
   }
   // Add a zero terminator
   name[i] = 0;

   // Search for the name
   for (i = 0; i < NUM_SWI_HANDLERS; i++) {
      if (!strcmp(name, SWI_Table[i].name)) {
         reg[0] = num + i;
         return;
      }
   }

   // Special case OS_WriteI as they are not in the handler table
   if (!strcmp(name, "OS_WriteI")) {
      reg[0] = 0x100 + i;
      return;
   }

   // Generate an error if name not found
   generate_error((void *)reg[13], 486, "SWI name not known");
}


static void tube_SWI_NumberToString(unsigned int *reg) {
   unsigned int num =         reg[0];
   char * buffer    = (char *)reg[1];
   int buffer_len   = (int)   reg[2];

   char *name = lookup_swi_name(num);

   // Make sure we don't overflow the buffer
   int name_len = strlen(name);
   if (name_len < buffer_len) {
      strcpy(buffer, name);
      reg[2] = name_len + 1; // Returned length includes terminator
   } else {
      generate_error((void *)reg[13], 484, "Buffer overflow");
   }
}

static void tube_CallASWI(unsigned int *reg) {
   C_SWI_Handler(reg[10], reg);
}

static void tube_CallASWIR12(unsigned int *reg) {
   C_SWI_Handler(reg[12], reg);
}
