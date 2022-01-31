#ifndef SHARED_H
#define SHARED_H

void print(const char* format, ...);

// Need to implement a multi-step process so it fits in with how PiTubeDirect works
typedef enum {
	WASMInit = 0,
	WASMLoad,
	WASMVerify,
	WASMBuild,
	WASMRun,
} wasm_stage_t;
extern wasm_stage_t stage;

#define K64 0x10000

typedef enum {
	LogErrors = 0,
	LogInfo,
	LogDetail,
	LogTrace,
} error_level_t;
extern error_level_t trace;

#endif
