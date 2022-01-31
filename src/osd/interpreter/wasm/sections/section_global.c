#include "../../wasm_shared.h"
#include "../load_wasm.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

void section_global(buffer_t *bf, uint32_t size, wasm_t *wat)
{
    size_t p = bf->pos;
    wat->num_globals = read_leb128_32(bf);
    if (trace >= LogDetail)
        print("%d global section(s) at 0x%07lX\n", wat->num_globals, p);

    // Allocate space
    wat->section_globals = malloc(sizeof(section_global_t) * wat->num_globals);

    // Import each in turn
    for (uint32_t i = 0; i < wat->num_globals; i++)
    {
        if (trace >= LogTrace)
            print("  [%d] global section at 0x%07lX\n", i, bf->pos);
        section_global_t *p = &wat->section_globals[i];
        p->type = read_leb128_32(bf);
        p->mut = read_byte_int(bf);
        p->init = load_expression(bf);
    }
}
