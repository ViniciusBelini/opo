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
    OP_NEG,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_EQ,
    OP_LT,
    OP_GT,
    OP_LTE,
    OP_GTE,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_PRINT,
    OP_STORE,
    OP_LOAD,
    OP_JUMP,
    OP_JUMP_IF_F,
    OP_CALL,
    OP_CALL_PTR,
    OP_RET,
    OP_TYPEOF,
    OP_PUSH_FUNC
} OpCode;

typedef enum {
    VAL_INT,
    VAL_FLT,
    VAL_BOOL,
    VAL_STR,
    VAL_VOID,
    VAL_FUNC,
    VAL_FUNC_INT,
    VAL_FUNC_FLT,
    VAL_FUNC_BOOL,
    VAL_FUNC_STR,
    VAL_FUNC_VOID
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
