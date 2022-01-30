#include "load_wasm.h"
#include <stdlib.h>

/*void process_block(buffer_t* bf, buffer_t* code)
{

}*/

void debug_instruction(const char* name, uint8_t opcode, uint32_t location)
{
//	printf("    Opcode [0X%02X %s] at 0X%07X\n", opcode, name, location);
}

uint8_t process_instruction(list code, buffer_t* bf)
{
	instruction_t instruction;
	instruction.opcode = bf->buffer[bf->pos++];
	switch (instruction.opcode) {

		case INSTRUCTION_NOP:
			debug_instruction("nop", instruction.opcode, bf->pos);
			break;
		case INSTRUCTION_END:
			debug_instruction("end", instruction.opcode, bf->pos);
			break;
			// Simple, no operands
/*		case INSTRUCTION_UNREACHABLE:
		case INSTRUCTION_RETURN:
			break;*/

			// These require scanning for a INSTRUCTION_END opcode
/*		case INSTRUCTION_BLOCK:
		case INSTRUCTION_LOOP: {
			instruction.block_type = read_sleb128_32(bf);
			int a = 1;
			break;
		}*/

/*		case INSTRUCTION_IF: {
			instruction.block_type = read_sleb128_32(bf);
			break;
		}*/
/*		case INSTRUCTION_ELSE:
			instruction->block_type = read_leb128_32(bf);
			break;*/
		case INSTRUCTION_RETURN:
			debug_instruction("return", instruction.opcode, bf->pos);
			break;

		case INSTRUCTION_CALL: {
			debug_instruction("call", instruction.opcode, bf->pos);
			instruction.index = read_sleb128_32(bf);
			break;
		}
		case INSTRUCTION_DROP:
			debug_instruction("drop", instruction.opcode, bf->pos);
			break;

/*		case INSTRUCTION_BR:
		case INSTRUCTION_BR_IF:*/
		case INSTRUCTION_LOCAL_GET:
			debug_instruction("local.get", instruction.opcode, bf->pos);
			instruction.index = read_leb128_32(bf);
			break;
		case INSTRUCTION_LOCAL_SET:
			debug_instruction("local.set", instruction.opcode, bf->pos);
			instruction.index = read_leb128_32(bf);
			break;
//		case INSTRUCTION_LOCAL_TEE:*/
		case INSTRUCTION_GLOBAL_GET:
			debug_instruction("global.get", instruction.opcode, bf->pos);
			instruction.index = read_leb128_32(bf);
			break;
		case INSTRUCTION_GLOBAL_SET:
			debug_instruction("global.set", instruction.opcode, bf->pos);
			instruction.index = read_leb128_32(bf);
			break;
		case INSTRUCTION_I32_LOAD:
			debug_instruction("i32.load", instruction.opcode, bf->pos);
			instruction.alignment = read_leb128_32(bf);
			instruction.offset = read_leb128_32(bf);
			break;
		case INSTRUCTION_I32_STORE:
			debug_instruction("i32.store", instruction.opcode, bf->pos);
			instruction.alignment = read_leb128_32(bf);
			instruction.offset = read_leb128_32(bf);
			break;
/*		case INSTRUCTION_I64_LOAD:
		case INSTRUCTION_F32_LOAD:
		case INSTRUCTION_F64_LOAD:
		case INSTRUCTION_I32_LOAD8_S:
		case INSTRUCTION_I32_LOAD8_U:
		case INSTRUCTION_I32_LOAD16_S:
		case INSTRUCTION_I32_LOAD16_U:
		case INSTRUCTION_I64_LOAD8_S:
		case INSTRUCTION_I64_LOAD8_U:
		case INSTRUCTION_I64_LOAD16_S:
		case INSTRUCTION_I64_LOAD16_U:
		case INSTRUCTION_I64_LOAD32_S:
		case INSTRUCTION_I64_LOAD32_U:
		case INSTRUCTION_I64_STORE:
		case INSTRUCTION_F32_STORE:
		case INSTRUCTION_F64_STORE8:
		case INSTRUCTION_I32_STORE8:
		case INSTRUCTION_I32_STORE16:
		case INSTRUCTION_I64_STORE8:
		case INSTRUCTION_I64_STORE16:
		case INSTRUCTION_I64_STORE32:
			instruction.memory = read_leb128_32(bf);
			break;
			*/
		case INSTRUCTION_I32_CONST:
			debug_instruction("i32.const", instruction.opcode, bf->pos);
			instruction.value_i32 = read_sleb128_32(bf);
			break;
		case INSTRUCTION_I32_ADD:
			debug_instruction("i32.add", instruction.opcode, bf->pos);
			break;
		case INSTRUCTION_I32_SUB:
			debug_instruction("i32.sub", instruction.opcode, bf->pos);
			break;
			/*		case INSTRUCTION_I32_ADD:
		case INSTRUCTION_I32_MUL:
		case INSTRUCTION_I32_DIV_S:
		case INSTRUCTION_I32_DIV_U:
		case INSTRUCTION_I32_REM_S:
		case INSTRUCTION_I32_REM_U:
			break;*/
		default:
			fprintf(stderr, "Unknown opcode %X\n", instruction.opcode);
			exit(1);
	}
	list_add_last(code, &instruction);
	return instruction.opcode;
}
