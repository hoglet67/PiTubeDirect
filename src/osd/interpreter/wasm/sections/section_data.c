#include "../load_wasm.h"
#include <inttypes.h>
#include <stdlib.h>

void section_data(buffer_t* bf, uint32_t size, wasm_t* wat)
{
	size_t p = bf->pos;
	wat->num_datas = read_leb128_32(bf);
	printf("%d data section(s) at 0x%07lX\n", wat->num_datas, p);

	// Allocate space
	wat->section_datas = malloc(sizeof(section_data_t)*wat->num_datas);

	// Import each in turn
	for (int i = 0; i<wat->num_datas; i++) {
		section_data_t* p = &wat->section_datas[i];
		p->memory_index = read_leb128_32(bf);
		p->offset = load_expression(bf);
		p->data_size = read_leb128_32(bf);
		p->data = read_data(bf, p->data_size);
	}
}
