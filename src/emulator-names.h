#ifndef EMULATOR_NAMES_H
#define EMULATOR_NAMES_H

#ifdef MINIMAL_BUILD

static const char * emulator_names[] = {
   "65C02 (65tube)"
};

#else

static const char * emulator_names[] = {
   "65C02 (fast)",           // 0
   "65C02 (3MHz)",           // 1
   "65C102 (fast)",          // 2
   "65C102 (4MHz)",          // 3
   "Z80",                    // 4
   "Null",                   // 5
   "Null",                   // 6
   "Null",                   // 7
   "80286",                  // 8
   "MC6809",                 // 9
   "Null",                   // 10
   "PDP-11",                 // 11
   "ARM2",                   // 12
   "32016",                  // 13
   "Disable",                // 14 - same as Null but we want it in the list
   "ARM Native",             // 15
   "65C02 (lib6502)",        // 16
   "65C02 Turbo (lib6502)",  // 17
   "Null",                   // 18
   "Null",                   // 19
   "OPC5LS",                 // 20
   "OPC6",                   // 21
   "OPC7",                   // 22
   "Null",                   // 23
   "Null",                   // 24
   "Null",                   // 25
   "Null",                   // 26
   "Null",                   // 27
   "Null",                   // 28
   "Null",                   // 29
   "Null",                   // 30
   "Null"                    // 31
};

#endif

#endif
