#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H
#include "buffer.h"
#include "../containers/list.h"

typedef struct {
	uint8_t opcode;
	union {
		uint32_t index;
		uint32_t alignment;
		//		uint32_t label;
		//		uint32_t block_type;
		int32_t value_i32;
		int64_t value_i64;
		float value_f32;
		double value_f64;
	};
	uint32_t offset;
} instruction_t;

uint8_t process_instruction(list code, buffer_t* bf);

#define INSTRUCTION_UNREACHABLE                0x00
#define INSTRUCTION_NOP                        0x01
#define INSTRUCTION_BLOCK                      0x02
#define INSTRUCTION_LOOP                       0x03
#define INSTRUCTION_IF                         0x04
#define INSTRUCTION_ELSE                       0x05
#define INSTRUCTION_END                        0x0B
#define INSTRUCTION_BR                         0x0C
#define INSTRUCTION_BR_IF                      0x0D
#define INSTRUCTION_RETURN                     0x0F
#define INSTRUCTION_CALL                       0x10

#define INSTRUCTION_DROP                       0x1A

#define INSTRUCTION_LOCAL_GET                  0x20
#define INSTRUCTION_LOCAL_SET                  0x21
#define INSTRUCTION_LOCAL_TEE                  0x22
#define INSTRUCTION_GLOBAL_GET                 0x23
#define INSTRUCTION_GLOBAL_SET                 0x24

#define INSTRUCTION_I32_LOAD                   0x28
#define INSTRUCTION_I64_LOAD                   0x29
#define INSTRUCTION_F32_LOAD                   0x2A
#define INSTRUCTION_F64_LOAD                   0x2B
#define INSTRUCTION_I32_LOAD8_S                0x2C
#define INSTRUCTION_I32_LOAD8_U                0x2D
#define INSTRUCTION_I32_LOAD16_S               0x2E
#define INSTRUCTION_I32_LOAD16_U               0x2F
#define INSTRUCTION_I64_LOAD8_S                0x30
#define INSTRUCTION_I64_LOAD8_U                0x31
#define INSTRUCTION_I64_LOAD16_S               0x32
#define INSTRUCTION_I64_LOAD16_U               0x33
#define INSTRUCTION_I64_LOAD32_S               0x34
#define INSTRUCTION_I64_LOAD32_U               0x35
#define INSTRUCTION_I32_STORE                  0x36
#define INSTRUCTION_I64_STORE                  0x37
#define INSTRUCTION_F32_STORE                  0x38
#define INSTRUCTION_F64_STORE8                 0x39
#define INSTRUCTION_I32_STORE8                 0x3A
#define INSTRUCTION_I32_STORE16                0x3B
#define INSTRUCTION_I64_STORE8                 0x3C
#define INSTRUCTION_I64_STORE16                0x3D
#define INSTRUCTION_I64_STORE32                0x3E

#define INSTRUCTION_I32_CONST                  0x41

#define INSTRUCTION_I32_ADD                    0x6A
#define INSTRUCTION_I32_SUB                    0x6B
#define INSTRUCTION_I32_MUL                    0x6C
#define INSTRUCTION_I32_DIV_S                  0x6D
#define INSTRUCTION_I32_DIV_U                  0x6E
#define INSTRUCTION_I32_REM_S                  0x6F
#define INSTRUCTION_I32_REM_U                  0x70

#endif
