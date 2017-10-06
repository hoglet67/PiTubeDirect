// tube-commands.h

#ifndef TUBE_COMMANDS_H
#define TUBE_COMMANDS_H

int dispatchCmd(char *cmd);

int doCmdHelp(const char *params);
int doCmdTest(const char *params);
int doCmdGo(const char *params);
int doCmdMem(const char *params);
int doCmdDis(const char *params);
int doCmdFill(const char *params);
int doCmdCrc(const char *params);

// The Atom CRC Polynomial
#define CRC_POLY          0x002d

#endif
