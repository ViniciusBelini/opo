#include <stdio.h>
#include <stdlib.h>
#include "vm.h"

void vm_init(VM* vm, uint8_t* code, char** strings, int strings_count) {
    vm->code = code;
    vm->ip = 0;
    vm->stack_ptr = 0;
    vm->locals_ptr = 0;
    vm->frame_ptr = 0;
    vm->strings = strings;
    vm->strings_count = strings_count;
    for (int i = 0; i < LOCALS_MAX; i++) {
        vm->locals[i].type = VAL_VOID;
    }
}

void vm_push(VM* vm, Value val) {
    if (vm->stack_ptr >= STACK_MAX) {
        fprintf(stderr, "Stack overflow\n");
        exit(1);
    }
    vm->stack[vm->stack_ptr++] = val;
}

Value vm_pop(VM* vm) {
    if (vm->stack_ptr <= 0) {
        fprintf(stderr, "Stack underflow\n");
        exit(1);
    }
    return vm->stack[--vm->stack_ptr];
}

static int32_t read_int32(VM* vm) {
    int32_t val = 0;
    for (int i = 0; i < 4; i++) {
        val |= (int32_t)vm->code[vm->ip++] << (i * 8);
    }
    return val;
}

static int64_t read_int64(VM* vm) {
    int64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= (int64_t)vm->code[vm->ip++] << (i * 8);
    }
    return val;
}

void vm_run(VM* vm) {
    while (true) {
        // printf("IP: %d, OP: %d, SP: %d\n", vm->ip, vm->code[vm->ip], vm->stack_ptr);
        uint8_t instruction = vm->code[vm->ip++];
        switch (instruction) {
            case OP_HALT:
                return;
            case OP_PUSH_INT: {
                int64_t val = read_int64(vm);
                vm_push(vm, (Value){VAL_INT, {.i_val = val}});
                break;
            }
            case OP_PUSH_FLT: {
                // For float let's keep it simple for now or use the same byte-by-byte
                union { double f; uint64_t u; } conv;
                conv.u = (uint64_t)read_int64(vm);
                vm_push(vm, (Value){VAL_FLT, {.f_val = conv.f}});
                break;
            }
            case OP_PUSH_STR: {
                int index = vm->code[vm->ip++];
                vm_push(vm, (Value){VAL_STR, {.s_idx = index}});
                break;
            }
            case OP_PUSH_BOOL: {
                bool val = vm->code[vm->ip++];
                vm_push(vm, (Value){VAL_BOOL, {.b_val = val}});
                break;
            }
            case OP_PRINT: {
                Value val = vm_pop(vm);
                switch (val.type) {
                    case VAL_INT: printf("%ld\n", val.as.i_val); break;
                    case VAL_FLT: printf("%g\n", val.as.f_val); break;
                    case VAL_BOOL: printf(val.as.b_val ? "tru\n" : "fls\n"); break;
                    case VAL_STR: printf("%s\n", vm->strings[val.as.s_idx]); break;
                    case VAL_VOID: printf("void\n"); break;
                }
                break;
            }
            case OP_LTE: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val <= b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.f_val <= b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in LTE\n");
                    exit(1);
                }
                break;
            }
            case OP_GTE: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val >= b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.f_val >= b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in GTE\n");
                    exit(1);
                }
                break;
            }
            case OP_NEG: {
                Value a = vm_pop(vm);
                if (a.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = -a.as.i_val}});
                } else if (a.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_FLT, {.f_val = -a.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in NEG\n");
                    exit(1);
                }
                break;
            }
            case OP_MOD: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    if (b.as.i_val == 0) {
                        fprintf(stderr, "Division by zero\n");
                        exit(1);
                    }
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val % b.as.i_val}});
                } else {
                    fprintf(stderr, "Type error in MOD\n");
                    exit(1);
                }
                break;
            }
            case OP_AND: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.b_val && b.as.b_val}});
                break;
            }
            case OP_OR: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.b_val || b.as.b_val}});
                break;
            }
            case OP_NOT: {
                Value a = vm_pop(vm);
                vm_push(vm, (Value){VAL_BOOL, {.b_val = !a.as.b_val}});
                break;
            }
            case OP_GT: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val > b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.f_val > b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in GT\n");
                    exit(1);
                }
                break;
            }
            case OP_ADD: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val + b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_FLT, {.f_val = a.as.f_val + b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in ADD\n");
                    exit(1);
                }
                break;
            }
            case OP_SUB: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val - b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_FLT, {.f_val = a.as.f_val - b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in SUB\n");
                    exit(1);
                }
                break;
            }
            case OP_MUL: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val * b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_FLT, {.f_val = a.as.f_val * b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in MUL\n");
                    exit(1);
                }
                break;
            }
            case OP_DIV: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    if (b.as.i_val == 0) { fprintf(stderr, "Division by zero\n"); exit(1); }
                    vm_push(vm, (Value){VAL_INT, {.i_val = a.as.i_val / b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    if (b.as.f_val == 0) { fprintf(stderr, "Division by zero\n"); exit(1); }
                    vm_push(vm, (Value){VAL_FLT, {.f_val = a.as.f_val / b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in DIV\n");
                    exit(1);
                }
                break;
            }
            case OP_EQ: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                bool res = false;
                if (a.type == b.type) {
                    if (a.type == VAL_INT) res = a.as.i_val == b.as.i_val;
                    else if (a.type == VAL_BOOL) res = a.as.b_val == b.as.b_val;
                }
                vm_push(vm, (Value){VAL_BOOL, {.b_val = res}});
                break;
            }
            case OP_LT: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.i_val < b.as.i_val}});
                } else if (a.type == VAL_FLT && b.type == VAL_FLT) {
                    vm_push(vm, (Value){VAL_BOOL, {.b_val = a.as.f_val < b.as.f_val}});
                } else {
                    fprintf(stderr, "Type error in LT\n");
                    exit(1);
                }
                break;
            }
            case OP_STORE: {
                int index = vm->code[vm->ip++];
                if (index >= LOCALS_PER_FRAME) {
                    fprintf(stderr, "Local variable index out of bounds\n");
                    exit(1);
                }
                int locals_offset = vm->frame_ptr > 0 ? vm->frames[vm->frame_ptr-1].locals_offset : 0;
                vm->locals[locals_offset + index] = vm_pop(vm);
                break;
            }
            case OP_LOAD: {
                int index = vm->code[vm->ip++];
                if (index >= LOCALS_PER_FRAME) {
                    fprintf(stderr, "Local variable index out of bounds\n");
                    exit(1);
                }
                int locals_offset = vm->frame_ptr > 0 ? vm->frames[vm->frame_ptr-1].locals_offset : 0;
                vm_push(vm, vm->locals[locals_offset + index]);
                break;
            }
            case OP_JUMP_IF_F: {
                int32_t addr = read_int32(vm);
                Value cond = vm_pop(vm);
                if (!cond.as.b_val) {
                    vm->ip = addr;
                }
                break;
            }
            case OP_JUMP: {
                int32_t addr = read_int32(vm);
                vm->ip = addr;
                break;
            }
            case OP_CALL: {
                int32_t addr = read_int32(vm);
                if (vm->frame_ptr >= FRAMES_MAX) {
                    fprintf(stderr, "Stack overflow (frames)\n");
                    exit(1);
                }
                CallFrame* frame = &vm->frames[vm->frame_ptr++];
                frame->return_addr = vm->ip;
                frame->locals_offset = (vm->frame_ptr - 1) * LOCALS_PER_FRAME;

                vm->ip = addr;
                break;
            }
            case OP_RET: {
                if (vm->frame_ptr <= 0) {
                    fprintf(stderr, "Stack underflow (frames)\n");
                    exit(1);
                }
                CallFrame* frame = &vm->frames[--vm->frame_ptr];
                vm->ip = frame->return_addr;
                break;
            }
            default:
                fprintf(stderr, "Unknown opcode %d\n", instruction);
                exit(1);
        }
    }
}
