#define BYTE_SWAP

#if defined(WIN32) && !defined(BEM)
#define SWAP16 _byteswap_ushort
#define SWAP32 _byteswap_ulong
#else
#define SWAP16 __builtin_bswap16
#define SWAP32 __builtin_bswap32
#endif

typedef union
{
   uint32_t u32;
   int32_t s32;

   struct
   {
      uint8_t DoNotUse_u8_3;
      uint8_t DoNotUse_u8_2;
      uint8_t DoNotUse_u8_1;
      uint8_t u8;
   };

   struct
   {
      int8_t DoNotUse_s8_3;
      int8_t DoNotUse_s8_2;
      int8_t DoNotUse_s8_1;
      int8_t s8;
   };

   struct
   {
      uint16_t DoNotUse_u16_1;
      uint16_t u16;
   };

   struct
   {
      int16_t DoNotUse_s16_1;
      int16_t s16;
   };
} MultiReg;
