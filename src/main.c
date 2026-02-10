#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void run_repl(const char* stdlib_dir) {
    printf("Opo REPL v0.1\n");
    printf("Type 'exit' to quit.\n");
    char line[1024];
    while (true) {
        printf("opo> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        if (strcmp(line, "exit\n") == 0) break;
        
        if (strlen(line) <= 1) continue;

        char source[2048];
        if (strstr(line, "->") != NULL || strstr(line, "struct") != NULL || (strstr(line, "=>") != NULL && strstr(line, "imp") != NULL)) {
             // If it looks like a function/struct/import, don't wrap it in main but add a main to call it? 
             // This is tricky. Let's just wrap everything in main for now if it's an expression.
             sprintf(source, "<> -> void: main [ %s ]", line);
        } else {
             // Expression: wrap and print
             sprintf(source, "<> -> void: main [ (%s) !! ]", line);
        }
        
        Chunk* chunk = compiler_compile(source, ".", stdlib_dir);
        if (chunk != NULL) {
            VM vm;
            char* dummy_argv[] = {"opo"};
            vm_init(&vm, chunk->code, chunk->strings, chunk->strings_count, 1, dummy_argv);
            vm_run(&vm);
            // We don't free chunk immediately because VM might have references? 
            // No, Opo VM copies code and strings references are handled by retain/release?
            // Wait, vm_init takes chunk->code and chunk->strings.
            // vm_run uses them.
            // After vm_run, we can free chunk.
            chunk_free(chunk);
        }
    }
}

int main(int argc, char* argv[]) {
    // Determine stdlib directory (relative to the executable)
    char stdlib_dir[2048] = "./lib";
    char exe_path[2048];
    strcpy(exe_path, argv[0]);
    char* last_exe_slash = strrchr(exe_path, '/');
    if (last_exe_slash != NULL) {
        *last_exe_slash = '\0';
        snprintf(stdlib_dir, sizeof(stdlib_dir), "%s/lib", exe_path);
    }

    if (argc < 2) {
        run_repl(stdlib_dir);
        return 0;
    }

    char* source = read_file(argv[1]);
    
    char* last_slash = strrchr(argv[1], '/');
    char base_dir[1024] = ".";
    if (last_slash != NULL) {
        size_t len = last_slash - argv[1];
        strncpy(base_dir, argv[1], len);
        base_dir[len] = '\0';
    }

    Chunk* chunk = compiler_compile(source, base_dir, stdlib_dir);
    free(source);

    if (chunk == NULL) return 65;

    VM vm;
    vm_init(&vm, chunk->code, chunk->strings, chunk->strings_count, argc, argv);
    vm_run(&vm);

    // Clean up
    chunk_free(chunk);

    return 0;
}
