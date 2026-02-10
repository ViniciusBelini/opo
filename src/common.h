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
    OP_PUSH_FUNC,
    OP_GET_MEMBER,
    OP_SET_MEMBER,
    OP_INDEX,
    OP_CALL_NATIVE,
    OP_ARRAY,
    OP_STRUCT,
    OP_INVOKE,
    OP_LOAD_G,
    OP_POP,
    OP_MAP,
    OP_SET_INDEX,
    OP_TRY,
    OP_END_TRY,
    OP_THROW
} OpCode;

typedef enum {
    VAL_NONE,
    VAL_INT,
    VAL_FLT,
    VAL_BOOL,
    VAL_STR, // Legacy/Static strings
    VAL_VOID,
    VAL_FUNC,
    VAL_FUNC_INT,
    VAL_FUNC_FLT,
    VAL_FUNC_BOOL,
    VAL_FUNC_STR,
    VAL_FUNC_VOID,
    VAL_OBJ,
    VAL_IMP,
    VAL_MAP,
    VAL_ERR,
    VAL_ANY
} ValueType;

typedef uint32_t Type;
#define TYPE_KIND(t) ((t) & 0xFF)
#define TYPE_SUB(t) (((t) >> 8) & 0xFF)
#define TYPE_KEY(t) (((t) >> 16) & 0xFF)
#define MAKE_TYPE(kind, sub, key) ((Type)((kind) | ((sub) << 8) | ((key) << 16)))

typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_STRUCT,
    OBJ_NATIVE,
    OBJ_MAP
} ObjType;

struct HeapObject {
    ObjType type;
    int ref_count;
};

typedef struct HeapObject HeapObject;

typedef struct {
    Type type;
    union {
        int64_t i_val;
        double f_val;
        bool b_val;
        int s_idx; // Index into static string table
        HeapObject* obj;
    } as;
} Value;

typedef struct {
    HeapObject obj;
    char* chars;
    int length;
} ObjString;

typedef struct {
    HeapObject obj;
    Value* items;
    int count;
    int capacity;
} ObjArray;

typedef struct {
    HeapObject obj;
    char** fields;
    Value* values;
    int field_count;
} ObjStruct;

typedef struct {
    Value key;
    Value value;
    bool is_used;
} MapEntry;

typedef struct {
    HeapObject obj;
    MapEntry* entries;
    int count;
    int capacity;
} ObjMap;

typedef struct VM VM;
typedef Value (*NativeFn)(VM* vm, int arg_count, Value* args);

typedef struct {
    HeapObject obj;
    NativeFn function;
    const char* name;
} ObjNative;

#endif
