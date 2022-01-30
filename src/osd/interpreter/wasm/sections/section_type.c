#include "../load_wasm.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

void section_type(buffer_t* bf, uint32_t size, wasm_t* wat)
{
	size_t p = bf->pos;
	wat->num_types = read_leb128_32(bf);
	printf("%d type section(s) at 0x%07lX\n", wat->num_types, p);

	// Allocate space
	wat->section_types = malloc(sizeof(section_type_t)*wat->num_types);

	// Import each in turn
	for (int i = 0; i<wat->num_types; i++) {
		section_type_t* p = &wat->section_types[i];

		// Check tag
		uint8_t a = read_byte_int(bf);
		if (a!=0x60) {
			fprintf(stderr, "Tag not 0x60\n");
			exit(1);
		}

		// Parameters
		p->num_params = read_leb128_32(bf);
		if (p->num_params>0) {
			p->params = malloc(sizeof(uint32_t)*p->num_params);
			for (int j = 0; j<p->num_params; j++) {
				p->params[j] = read_leb128_32(bf);
				validate_type(p->params[j], wat);
			}
		}

		// Return type
		p->num_returns = read_leb128_32(bf);
		if (p->num_returns>0) {
			p->result = malloc(sizeof(uint32_t)*p->num_returns);
			for (int j = 0; j<p->num_returns; j++) {
				p->result[j] = read_leb128_32(bf);
				validate_type(p->result[j], wat);
			}
		}
	}
}
