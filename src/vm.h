#ifndef OPO_VM_H
#define OPO_VM_H

#include "common.h"

#define STACK_MAX 256
#define LOCALS_PER_FRAME 32
#define FRAMES_MAX 64
#define LOCALS_MAX (FRAMES_MAX * LOCALS_PER_FRAME)

typedef struct {
    int return_addr;
    int locals_offset;
} CallFrame;

typedef struct {
    int handler_addr;
    int stack_ptr;
    int frame_ptr;
} TryFrame;

#define TRY_STACK_MAX 16

struct VM {
    uint8_t* code;
    int ip;
    Value stack[STACK_MAX];
    int stack_ptr;
    Value locals[LOCALS_MAX];
    int locals_ptr;
    CallFrame frames[FRAMES_MAX];
    int frame_ptr;
    TryFrame try_stack[TRY_STACK_MAX];
    int try_ptr;
    char** strings;
    int strings_count;
    int argc;
    char** argv;
    bool panic;
};

void vm_init(VM* vm, uint8_t* code, char** strings, int strings_count, int argc, char** argv);
void vm_run(VM* vm);
void vm_push(VM* vm, Value val);
Value vm_pop(VM* vm);

void retain(Value val);
void release(Value val);

ObjString* allocate_string(VM* vm, const char* chars, int length);
ObjArray* allocate_array(VM* vm);

#endif
