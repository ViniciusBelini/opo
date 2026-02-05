#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "vm.h"

char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: opo [path]\n");
        exit(64);
    }

    char* source = read_file(argv[1]);
    Chunk* chunk = compiler_compile(source);
    free(source);

    if (chunk == NULL) return 65;

    VM vm;
    vm_init(&vm, chunk->code, chunk->strings, chunk->strings_count);
    vm_run(&vm);

    // Clean up
    chunk_free(chunk);

    return 0;
}
