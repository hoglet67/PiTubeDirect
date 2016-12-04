// tube-commands.h

#ifndef TUBE_COMMANDS_H
#define TUBE_COMMANDS_H

int dispatchCmd(char *cmd);

int doCmdHelp(char *params);
int doCmdTest(char *params);
int doCmdGo(char *params);
int doCmdMem(char *params);
int doCmdDis(char *params);
int doCmdFill(char *params);
int doCmdCrc(char *params);

// The Atom CRC Polynomial
#define CRC_POLY          0x002d

#endif
