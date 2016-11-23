// swi.h

#ifndef SWI_H
#define SWI_H

#include "tube-env.h"

#define SWI_OS_WriteC                  0x000000
#define SWI_OS_WriteS                  0x000001
#define SWI_OS_Write0                  0x000002
#define SWI_OS_NewLine                 0x000003
#define SWI_OS_ReadC                   0x000004
#define SWI_OS_CLI                     0x000005
#define SWI_OS_Byte                    0x000006
#define SWI_OS_Word                    0x000007
#define SWI_OS_File                    0x000008
#define SWI_OS_Args                    0x000009
#define SWI_OS_BGet                    0x00000A
#define SWI_OS_BPut                    0x00000B
#define SWI_OS_GBPB                    0x00000C
#define SWI_OS_Find                    0x00000D
#define SWI_OS_ReadLine                0x00000E
#define SWI_OS_Control                 0x00000F
#define SWI_OS_GetEnv                  0x000010
#define SWI_OS_Exit                    0x000011
#define SWI_OS_SetEnv                  0x000012
#define SWI_OS_IntOn                   0x000013
#define SWI_OS_IntOff                  0x000014
#define SWI_OS_CallBack                0x000015
#define SWI_OS_EnterOS                 0x000016
#define SWI_OS_BreakPt                 0x000017
#define SWI_OS_BreakCtrl               0x000018
#define SWI_OS_UnusedSWI               0x000019
#define SWI_OS_UpdateMEMC              0x00001A
#define SWI_OS_SetCallBack             0x00001B
#define SWI_OS_Mouse                   0x00001C
#define SWI_OS_Heap                    0x00001D
#define SWI_OS_Module                  0x00001E
#define SWI_OS_Claim                   0x00001F
#define SWI_OS_Release                 0x000020
#define SWI_OS_ReadUnsigned            0x000021
#define SWI_OS_GenerateEvent           0x000022
#define SWI_OS_ReadVarVal              0x000023
#define SWI_OS_SetVarVal               0x000024
#define SWI_OS_GSInit                  0x000025
#define SWI_OS_GSRead                  0x000026
#define SWI_OS_GSTrans                 0x000027
#define SWI_OS_BinaryToDecimal         0x000028
#define SWI_OS_FSControl               0x000029
#define SWI_OS_ChangeDynamicArea       0x00002A
#define SWI_OS_GenerateError           0x00002B
#define SWI_OS_ReadEscapeState         0x00002C
#define SWI_OS_EvaluateExpression      0x00002D
#define SWI_OS_SpriteOp                0x00002E
#define SWI_OS_ReadPalette             0x00002F
#define SWI_OS_ServiceCall             0x000030
#define SWI_OS_ReadVduVariables        0x000031
#define SWI_OS_ReadPoint               0x000032
#define SWI_OS_UpCall                  0x000033
#define SWI_OS_CallAVector             0x000034
#define SWI_OS_ReadModeVariable        0x000035
#define SWI_OS_RemoveCursors           0x000036
#define SWI_OS_RestoreCursors          0x000037
#define SWI_OS_SWINumberToString       0x000038
#define SWI_OS_SWINumberFromString     0x000039
#define SWI_OS_ValidateAddress         0x00003A
#define SWI_OS_CallAfter               0x00003B
#define SWI_OS_CallEvery               0x00003C
#define SWI_OS_InstallKeyHandler       0x00003E
#define SWI_OS_CheckModeValid          0x00003F
#define SWI_OS_ChangeEnvironment       0x000040
#define SWI_OS_ClaimScreenMemory       0x000041
#define SWI_OS_ReadMonotonicTime       0x000042
#define SWI_OS_SubstituteArgs          0x000043
#define SWI_OS_PrettyPrintCode         0x000044
#define SWI_OS_Plot                    0x000045
#define SWI_OS_WriteN                  0x000046
#define SWI_OS_AddToVector             0x000047
#define SWI_OS_WriteEnv                0x000048
#define SWI_OS_ReadArgs                0x000049
#define SWI_OS_ReadRAMFsLimits         0x00004A
#define SWI_OS_ClaimDeviceVector       0x00004B
#define SWI_OS_ReleaseDeviceVector     0x00004C
#define SWI_OS_DelinkApplication       0x00004D
#define SWI_OS_RelinkApplication       0x00004E
#define SWI_OS_HeapSort                0x00004F
#define SWI_OS_ExitAndDie              0x000050
#define SWI_OS_ReadMemMapInfo          0x000051
#define SWI_OS_ReadMemMapEntries       0x000052
#define SWI_OS_SetMemMapEntries        0x000053
#define SWI_OS_AddCallBack             0x000054
#define SWI_OS_ReadDefaultHandler      0x000055
#define SWI_OS_SetECFOrigin            0x000056
#define SWI_OS_SerialOp                0x000057
#define SWI_OS_ReadSysInfo             0x000058
#define SWI_OS_Confirm                 0x000059
#define SWI_OS_ChangedBox              0x00005A
#define SWI_OS_CRC                     0x00005B
#define SWI_OS_ReadDynamicArea         0x00005C
#define SWI_OS_PrintChar               0x00005D
#define SWI_OS_ChangeRedirection       0x00005E
#define SWI_OS_RemoveCallBack          0x00005F
#define SWI_OS_FindMemMapEntries       0x000060
#define SWI_OS_SetColourCode           0x000061
#define SWI_OS_ClaimSWI                0x000062
#define SWI_OS_ReleaseSWI              0x000063
#define SWI_OS_Pointer                 0x000064
#define SWI_OS_ScreenMode              0x000065
#define SWI_OS_DynamicArea             0x000066
#define SWI_OS_AbortTrap               0x000067
#define SWI_OS_Memory                  0x000068
#define SWI_OS_ClaimProcessorVector    0x000069
#define SWI_OS_Reset                   0x00006A
#define SWI_OS_MMUControl              0x00006B
#define SWI_OS_ResyncTime              0x00006C
#define SWI_OS_PlatformFeatures        0x00006D
#define SWI_OS_SynchroniseCodeAreas    0x00006E
#define SWI_OS_CallASWI                0x00006F
#define SWI_OS_AMBControl              0x000070
#define SWI_OS_CallASWIR12             0x000071
#define SWI_OS_SpecialControl          0x000072
#define SWI_OS_EnterUSR32              0x000073
#define SWI_OS_EnterUSR26              0x000074
#define SWI_OS_VIDCDivider             0x000075
#define SWI_OS_NVMemory                0x000076
#define SWI_OS_ClaimOSSWI              0x000077
#define SWI_OS_TaskControl             0x000078
#define SWI_OS_DeviceDriver            0x000079
#define SWI_OS_Hardware                0x00007A
#define SWI_OS_IICOp                   0x00007B
#define SWI_OS_LeaveOS                 0x00007C
#define SWI_OS_ReadLine32              0x00007D
#define SWI_OS_SubstituteArgs32        0x00007E
#define SWI_OS_HeapSort32              0x00007F

#define swi(code) asm volatile ("svc %[immediate]"::[immediate] "I" (code))

void OS_WriteC(const char c);
int  OS_Write0(const char *cptr);
int  OS_ReadC(unsigned int *flags);
void OS_CLI(const char *cptr);
void OS_Byte(unsigned int a, unsigned int x, unsigned int y, unsigned int *retx, unsigned int *rety);
void OS_ReadLine(const char *buffer, int buflen, int minAscii, int maxAscii, unsigned int *flags, int *length);
void OS_Exit();
void OS_GenerateError(const ErrorBlock_type *eblk);

#endif
