#define TrapTRACE printf

enum TrapTypes
{
   BreakPointHit = BIT(0),
   BreakPointTrap = BIT(1),
   ReservedAddressingMode = BIT(2),
   UnknownFormat = BIT(3),
   UnknownInstruction = BIT(4),
   DivideByZero = BIT(5),
   IllegalImmediate = BIT(6),
   IllegalDoubleIndexing = BIT(7),
   IllegalSpecialReading = BIT(8),
   IllegalSpecialWriting = BIT(9),
   IllegalWritingImmediate = BIT(10),
   FlagInstruction = BIT(11),
   PrivilegedInstruction = BIT(12),

};

#define TrapCount 13
extern uint32_t TrapFlags;
#define CLEAR_TRAP() TrapFlags = 0

// Use SET_TRAP when in a function
#define SET_TRAP(in) TrapFlags |= (in)

// Use GOTO_TRAP when in the main loop
#define GOTO_TRAP(in) TrapFlags |= (in); goto DoTrap

extern void ShowTraps(void);
extern void HandleTrap(void);
extern void n32016_dumpregs();
