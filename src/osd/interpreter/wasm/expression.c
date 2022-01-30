#include "load_wasm.h"
#include <stdio.h>
#include <stdlib.h>

instruction_t* load_expression(buffer_t* bf) {
	list code = list_init(sizeof(instruction_t));

	// Process each in turn
	uint8_t opcode = INSTRUCTION_END;
	do {
		opcode = process_instruction(code, bf);
	}
	while (opcode!=INSTRUCTION_END);

	// Copy to array (as faster to access)
	instruction_t *code_array = malloc(sizeof(instruction_t)*list_size(code));
	list_copy_to_array(code_array, code);
	list_destroy(code);
	return code_array;
}

