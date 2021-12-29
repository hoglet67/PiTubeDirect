#if defined(RPI2) || defined(RPI3)
    #define PERIPHERAL_BASE     0x3F000000
#else

#if defined(RPI4)
    #define PERIPHERAL_BASE     0xFE000000
#else

    #define PERIPHERAL_BASE     0x20000000
#endif
#endif