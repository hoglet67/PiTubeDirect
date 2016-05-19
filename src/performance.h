// performance.h

#ifndef PERFORMANCE_H
#define PERFORMANCE_H

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

extern void reset_performance_counters(int type0, int type1);

#endif
