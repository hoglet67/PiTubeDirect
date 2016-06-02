#define IO_BASE         0xFFFFF0

//#define PANDORA_BASE    0xF00000
#define RAM_SIZE        MEG1

//#define TEST_SUITE
#ifdef TEST_SUITE
#undef PANDORA_BASE
#endif

//#define PANDORA_ROM_PAGE_OUT
#define NS_FAST_RAM

void init_ram(void);
uint32_t LoadBinary(const char *pFileName, uint32_t Location);

extern uint8_t		read_x8(uint32_t addr);
extern uint16_t	read_x16(uint32_t addr);
extern uint32_t	read_x32(uint32_t addr);
extern uint64_t	read_x64(uint32_t addr);
extern uint32_t   read_n(uint32_t addr, uint32_t Size);

extern void write_x8(uint32_t addr, uint8_t val);
extern void write_x16(uint32_t addr, uint16_t val);
extern void write_x32(uint32_t addr, uint32_t val);
extern void write_x64(uint32_t addr, uint64_t val);
extern void write_Arbitary(uint32_t addr, void* pData, uint32_t Size);
