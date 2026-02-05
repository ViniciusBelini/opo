#ifndef OPO_COMMON_H
#define OPO_COMMON_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    OP_HALT,
    OP_PUSH_INT,
    OP_PUSH_FLT,
    OP_PUSH_STR,
    OP_PUSH_BOOL,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_EQ,
    OP_LT,
    OP_GT,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_PRINT,
    OP_STORE,
    OP_LOAD,
    OP_JUMP,
    OP_JUMP_IF_F,
    OP_CALL,
    OP_RET
} OpCode;

typedef enum {
    VAL_INT,
    VAL_FLT,
    VAL_BOOL,
    VAL_STR,
    VAL_VOID
} ValueType;

typedef struct {
    ValueType type;
    union {
        int64_t i_val;
        double f_val;
        bool b_val;
        int s_idx; // Index into string table
    } as;
} Value;

#endif
