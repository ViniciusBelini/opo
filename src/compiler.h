#ifndef OPO_COMPILER_H
#define OPO_COMPILER_H

#include <stdint.h>
#include "lexer.h"
#include "common.h"

typedef struct {
    uint8_t* code;
    int count;
    int capacity;
    char** strings;
    int strings_count;
    int strings_capacity;
} Chunk;

Chunk* compiler_compile(const char* source, const char* base_dir);
void chunk_free(Chunk* chunk);

#endif
