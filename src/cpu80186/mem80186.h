
#define DECLARE_RAM


#define SIXTEEN_K		0x04000
#define ONE_MEG		0x100000
#define ROM_SIZE		SIXTEEN_K

// Modify this to change the Range of ROM Mirroring
#define MirrorBase	0xF0000

enum cpu80186_memory
{
  m186_ExecutionBase = 0x08000,
  m186_RomBase = m186_ExecutionBase + SIXTEEN_K,
  m186_RomEnd = m186_RomBase + (ROM_SIZE - 1),
  m186_RamBase,
  m186_RamEnd = m186_RamBase + 0xFFFFF,
  m186_TotalRam
};

// Use Declare RAM when building on a machine with an REAL OS!
#ifdef DECLARE_RAM
extern uint8_t* RAM;
#else
extern uint8_t* RAM;
#endif

extern void write86(uint32_t addr32, uint8_t value);
extern void writew86(uint32_t addr32, uint16_t value);
extern uint8_t read86(uint32_t addr32);
extern uint16_t readw86(uint32_t addr32);
extern void Cleari80186Ram(void);
extern void RomCopy(void);
