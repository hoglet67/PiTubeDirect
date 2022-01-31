#include "wasm/load_wasm.h"
#include "wasm_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t SP;
static uint8_t *OSD_RAM;

static inline void push_int_stack(int32_t v)
{
    SP -= 8;
    write_uint_simple(&OSD_RAM[SP], v);
    if (trace >= LogTrace)
        print("Stack push: %d\n", v);
}

static inline int32_t pop_int_stack()
{
    int32_t v = read_int_simple(&OSD_RAM[SP]);
    SP += 8;
    if (trace >= LogTrace)
        print("Stack pop: %d\n", v);
    return v;
}

void API_Call()
{
    int32_t p2 = pop_int_stack();
    int32_t p1 = pop_int_stack();
    switch (p1)
    {
    case 0:
        print(&OSD_RAM[p2]);
        break;
    default:
        if (trace >= LogTrace)
            print("Call API %d %d\n", p1, p2);
        break;
    }
    push_int_stack(0);
}

void get_local(local_t *locals, wasm_t *wat, instruction_t *t)
{
    local_t *l = &locals[t->index];
    switch (l->type)
    {
    case TYPE_I32:
        push_int_stack(l->value_i32);
        break;
        /*		case TYPE_I64:
                    break;
                case TYPE_F32:
                    break;
                case TYPE_F64:
                    break;*/
    default:
        print("Unsupported");
        exit(1);
    }
}

void set_local(local_t *locals, wasm_t *wat, instruction_t *t)
{
    local_t *l = &locals[t->index];
    switch (l->type)
    {
    case TYPE_I32:
        l->value_i32 = pop_int_stack();
        break;
        /*		case TYPE_I64:
                    break;
                case TYPE_F32:
                    break;
                case TYPE_F64:
                    break;*/
    default:
        fprintf(stderr, "Unsupported");
        exit(1);
    }
}

void get_global(wasm_t *wat, instruction_t *t)
{
    section_global_t *g = &wat->section_globals[t->index];
    switch (g->type)
    {
    case TYPE_I32:
        push_int_stack(g->value_i32);
        break;
        /*		case TYPE_I64:
                    break;
                case TYPE_F32:
                    break;
                case TYPE_F64:
                    break;*/
    default:
        print("Unsupported");
        exit(1);
    }
}

void set_global(wasm_t *wat, instruction_t *t)
{
    section_global_t *g = &wat->section_globals[t->index];
    switch (g->type)
    {
    case TYPE_I32:
        g->value_i32 = pop_int_stack();
        break;
        /*		case TYPE_I64:
                    break;
                case TYPE_F32:
                    break;
                case TYPE_F64:
                    break;*/
    default:
        print("Unsupported");
        exit(1);
    }
}

void run_function(wasm_t *wat, section_code_t *func)
{
    uint32_t PC = 0;

    // Locals?
    local_t *locals;
    uint32_t total = func->num_params + func->num_locals_total;

    if (total > 0)
    {
        size_t sz = sizeof(section_code_locals_t) * total;
        locals = malloc(sz);
        memset(locals, 0xFF, sz);

        size_t index = 0;

        // Params
        for (uint32_t i = 0; i < func->num_params; i++)
        {
            if (trace >= LogTrace)
                print("P %d/%d - %d\n", i, index, func->params[i]);
            locals[index++].type = func->params[i];
        }

        // Real locals
        for (uint32_t i = 0; i < func->num_locals; i++)
        {
            section_code_locals_t *locals_def = &func->locals[i];
            for (uint32_t j = 0; j < locals_def->count; j++)
            {
                if (trace >= LogTrace)
                    print("%d/%d - %d\n", j, index, locals_def->type);
                locals[index++].type = locals_def->type;
            }
        }
    }
    else
    {
        locals = NULL;
    }

    instruction_t *current_code_list = func->code;
    while (1)
    {
        instruction_t *t = &current_code_list[PC++];
        switch (t->opcode)
        {
        case INSTRUCTION_NOP:
            break;
        case INSTRUCTION_END:
            if (locals != NULL)
                free(locals);
            return;
        case INSTRUCTION_RETURN:
            if (trace >= LogTrace)
                print("Stack pointer on exit: %ld\n", SP);
            break;
        case INSTRUCTION_CALL: {
            uint32_t index = t->index;
            if (index < wat->num_import_functions)
            {
                // HAL
                section_import_t *it = &wat->section_imports[index];
                switch (it->type)
                {
                case API_CALL:
                    if (trace >= LogTrace)
                        print("Stack pointer on entry: %ld\n", SP);
                    API_Call();
                    if (trace >= LogTrace)
                        print("Stack pointer on exit: %ld\n", SP);
                    break;
                default:
                    print("Unknown API call index\n");
                    exit(1);
                }
            }
            else
            {
                // Real
                if (trace >= LogTrace)
                    print("Stack pointer on entry CALL: %ld\n", SP);
                index = index - wat->num_import_functions;
                run_function(wat, &wat->section_codes[index]);
                if (trace >= LogTrace)
                    print("Stack pointer on exit CALL: %ld\n", SP);
            }
            break;
        }
        case INSTRUCTION_DROP:
            SP += 8;
            if (trace >= LogTrace)
                print("Stack pointer after drop: %ld\n", SP);
            break;
        case INSTRUCTION_GLOBAL_GET:
            get_global(wat, t);
            break;
        case INSTRUCTION_GLOBAL_SET:
            set_global(wat, t);
            break;
        case INSTRUCTION_LOCAL_GET:
            get_local(locals, wat, t);
            break;
        case INSTRUCTION_LOCAL_SET:
            set_local(locals, wat, t);
            break;
        case INSTRUCTION_I32_LOAD: {
            int32_t addr = pop_int_stack() + t->offset;
            int32_t v = read_int_simple(&OSD_RAM[addr]);
            push_int_stack(v);
            break;
        }
        case INSTRUCTION_I32_STORE: {
            int32_t v = pop_int_stack();
            int32_t addr = pop_int_stack() + t->offset;
            write_int_simple(&OSD_RAM[addr], v);
            break;
        }
        case INSTRUCTION_I32_CONST:
            push_int_stack(t->value_i32);
            break;
        case INSTRUCTION_I32_ADD: {
            int32_t v2 = pop_int_stack();
            int32_t v1 = pop_int_stack();
            push_int_stack(v1 + v2);
            break;
        }
        case INSTRUCTION_I32_SUB: {
            int32_t v2 = pop_int_stack();
            int32_t v1 = pop_int_stack();
            push_int_stack(v1 - v2);
            break;
        }
            /*			case INSTRUCTION_UNREACHABLE:
                            fprintf(stderr, "Unreachable hit\n");
                            exit(1);
                        case INSTRUCTION_I32_CONST: {
                            int32_t v = read_int_simple(&RAM[PC]);
                            push_int_stack(v);
                            break;
                        }*/
        default:
            print("Unknown opcode 0x%X\n", t->opcode);
            exit(1);
        }
    }
}

void run_vm(wasm_t *wat, uint8_t *_RAM, size_t RAM_size, uint32_t heap_start)
{
    // Reset state
    OSD_RAM = _RAM;
    SP = heap_start;
    if (trace >= LogTrace)
        print("Stack pointer at start: %ld\n", SP);
    if (trace >= LogInfo)
        print("Calling global constructors...\n");
    run_function(wat, &wat->section_codes[wat->ctors_function]);
    if (trace >= LogInfo)
        print("Calling start...\n");
    run_function(wat, &wat->section_codes[wat->start_function]);
    if (trace >= LogInfo)
        print("Finished\n");
}
