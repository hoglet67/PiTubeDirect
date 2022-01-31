#include "wasm/load_wasm.h"
#include "wasm_shared.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

// Current state
wasm_t *wat;
buffer_t *bf;
uint8_t *OSD_RAM;
size_t data_space_needed;
uint32_t heap;
error_level_t trace = LogDetail;
wasm_stage_t stage;

void import_wat(wasm_t *wat, buffer_t *bf);
uint32_t start_of_heap(wasm_t *wat);
void build_vm(wasm_t *wat, uint8_t *RAM, size_t RAM_size);
void run_vm(wasm_t *wat, uint8_t *RAM, size_t RAM_size, uint32_t heap_start);

void WASM_init()
{
    stage = WASMInit;
    wat = malloc(sizeof(wasm_t));
    memset(wat, 0, sizeof(wasm_t));
    bf = malloc(sizeof(buffer_t));
}

void WASM_delete()
{
}

// "../../Kernel/kernel.wasm"
void WASM_loadfile(const char *filename)
{
    // Open WASM file
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        print("Can't open WASM file\n");
        exit(1);
    }

    // Work out file size and allocate a buffer, then read whole file. We are
    // only doing this for the kernel so have some control over it
    fseek(fp, 0, SEEK_END);
    bf->len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    bf->buffer = malloc(bf->len);
    bf->pos = 0;
    fread(bf->buffer, 1, bf->len, fp);
    fclose(fp);
}

extern unsigned char kernel_wasm[];
extern unsigned int kernel_wasm_len;
void WASM_loadmemory()
{
    bf->len = kernel_wasm_len;
    bf->buffer = malloc(bf->len);
    bf->pos = 0;
    memcpy(bf->buffer, kernel_wasm, bf->len);
}

void WASM_verify()
{
    // Check for header
    uint32_t magic = read_uint(bf);
    if (magic != 0x6D736100)
    {
        print("Unknown WASM magic number\n");
        exit(1);
    }

    // Check version
    uint32_t version = read_uint(bf);
    if (version != 1)
    {
        print("Unknown WASM version number\n");
        exit(1);
    }
}

void WASM_import()
{
    import_wat(wat, bf);
}

void WASM_build()
{
    // Work out allocation required for global data and the stack, this is the start of heap value
    heap = start_of_heap(wat);
    if (heap == 0)
    {
        print("Unable to figure out start of heap space");
        exit(1);
    }

    data_space_needed = wat->section_memories[0].min * K64;
    if (trace >= LogInfo)
        print("%d 64K page(s) of data space requested\n", wat->section_memories[0].min);
    OSD_RAM = (uint8_t *)malloc(data_space_needed);
    memset(OSD_RAM, 0, data_space_needed);
    build_vm(wat, OSD_RAM, data_space_needed);
}

void WASM_run()
{
    run_vm(wat, OSD_RAM, data_space_needed, heap);
}
