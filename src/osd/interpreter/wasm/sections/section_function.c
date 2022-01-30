#include "../load_wasm.h"
#include <inttypes.h>
#include <stdlib.h>

void section_function(buffer_t* bf, uint32_t size, wasm_t* wat)
{
	size_t p = bf->pos;
	wat->num_functions = read_leb128_32(bf);
	printf("%d function section(s) at 0x%07lX\n", wat->num_functions, p);

	// Allocate space
	wat->section_functions = malloc(sizeof(uint32_t)*wat->num_functions);
	wat->section_function_addresses = malloc(sizeof(uint32_t)*wat->num_functions);

	// Import each in turn
	for (int i = 0; i<wat->num_functions; i++) {
		wat->section_functions[i] = read_leb128_32(bf);
		validate_type(wat->section_functions[i], wat);
	}
}
