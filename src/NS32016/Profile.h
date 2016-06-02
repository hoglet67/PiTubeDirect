#ifdef PROFILING

extern void ProfileInit(void);
extern void ProfileAdd(uint32_t Function, uint16_t Regs0, uint16_t Regs1);
extern void ProfileDump(void);

#else

#define ProfileInit()
#define ProfileAdd(a)
#define ProfileDump()

#endif
