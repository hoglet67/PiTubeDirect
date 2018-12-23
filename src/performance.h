// performance.h

#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#if defined(RPI3) ||  defined(RPI2)

// TODO - More work is needed on the RPI2 performance metrics

#define MAX_COUNTERS 6

#define PERF_TYPE_SW_INCR                    0x00
#define PERF_TYPE_L1I_CACHE_REFILL           0x01
#define PERF_TYPE_L1I_TLB_REFILL             0x02
#define PERF_TYPE_L1D_CACHE_REFILL           0x03
#define PERF_TYPE_L1D_CACHE                  0x04
#define PERF_TYPE_L1D_TLB_REFILL             0x05
#define PERF_TYPE_LD_RETIRED                 0x06
#define PERF_TYPE_ST_RETIRED                 0x07
#define PERF_TYPE_INST_RETIRED               0x08
#define PERF_TYPE_EXC_TAKEN                  0x09
#define PERF_TYPE_EXC_RETURN                 0x0A
#define PERF_TYPE_CID_WRITE_RETIRED          0x0B
#define PERF_TYPE_PC_WRITE_RETIRED           0x0C
#define PERF_TYPE_BR_IMM_RETIRED             0x0D
#define PERF_TYPE_BR_RETURN_RETIRED          0x0E
#define PERF_TYPE_UNALIGNED_LDST_RETIRED     0x0F
#define PERF_TYPE_BR_MIS_PRED                0x10
#define PERF_TYPE_CPU_CYCLES                 0x11
#define PERF_TYPE_BR_PRED                    0x12
#define PERF_TYPE_MEM_ACCESS                 0x13
#define PERF_TYPE_L1I_CACHE                  0x14
#define PERF_TYPE_L1D_CACHE_WB               0x15
#define PERF_TYPE_L2D_CACHE                  0x16
#define PERF_TYPE_L2D_CACHE_REFILL           0x17
#define PERF_TYPE_L2D_CACHE_WB               0x18
#define PERF_TYPE_BUS_ACCESS                 0x19
#define PERF_TYPE_MEMORY_ERROR               0x1A
#define PERF_TYPE_INST_SPEC                  0x1B
#define PERF_TYPE_TTRB_WRITE_RETIRED         0x1C
#define PERF_TYPE_BUS_CYCLES                 0x1D
#define PERF_TYPE_CHAIN                      0x1E
#define PERF_TYPE_L1D_CACHE_ALLOCATE         0x1F

#else

#define MAX_COUNTERS 2

#define PERF_TYPE_I_CACHE_MISS               0x00
#define PERF_TYPE_IBUF_STALL                 0x01
#define PERF_TYPE_DATA_DEP_STALL             0x02
#define PERF_TYPE_I_MICROTLB_MISS            0x03
#define PERF_TYPE_D_MICROTLB_MISS            0x04
#define PERF_TYPE_BRANCH_EXECUTED            0x05
#define PERF_TYPE_BRANCH_PRED_INCORRECT      0x06
#define PERF_TYPE_INSTRUCTION_EXECUTED       0x07
#define PERF_TYPE_D_CACHE_ACCESS_CACHEABLE   0x09
#define PERF_TYPE_D_CACHE_ACCESS             0x0A
#define PERF_TYPE_D_CACHE_MISS               0x0B
#define PERF_TYPE_D_CACHE_WRITEBACK          0x0C
#define PERF_TYPE_SOFTWARE_CHANGED_PC        0x0D
#define PERF_TYPE_MAINTLB_MISS               0x0F
#define PERF_TYPE_EXPLICIT_DATA_ACCESS       0x10
#define PERF_TYPE_FULL_LOAD_STORE_REQ_QUEUE  0x11
#define PERF_TYPE_WRITE_BUFF_DRAINED         0x12
#define PERF_TYPE_EXT0                       0x20
#define PERF_TYPE_EXT1                       0x21
#define PERF_TYPE_EXT0_AND_EXT1              0x22
#define PERF_TYPE_PROC_RETURN_PUSHED         0x23
#define PERF_TYPE_PROC_RETURN_POPPED         0x24
#define PERF_TYPE_PROC_RETURN_PRED_CORRECT   0x25
#define PERF_TYPE_PROC_RETURN_PRED_INCORRECT 0x26
#define PERF_TYPE_EVERY_CYCLE                0xFF

#endif

typedef struct {
   unsigned cycle_counter;
   int num_counters;
   int type[MAX_COUNTERS];
   unsigned counter[MAX_COUNTERS];
} perf_counters_t;

extern void reset_performance_counters(perf_counters_t *pct);

extern void read_performance_counters(perf_counters_t *pct);

extern void print_performance_counters(const perf_counters_t *pct);

extern int benchmark();

#endif
