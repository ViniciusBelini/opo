#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "compiler.h"
#include "lexer.h"
#include "vm.h"

typedef struct {
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

typedef struct {
    Token name;
    ValueType type;
    int depth;
} Local;

typedef struct {
    Token name;
    int addr;
    ValueType return_type;
    ValueType param_types[16];
    int param_count;
} Function;

typedef struct {
    Local locals[256];
    int local_count;
    Function functions[256];
    int function_count;
    int scope_depth;
    ValueType type_stack[STACK_MAX];
    int type_stack_ptr;
} CompilerState;

Parser parser;
CompilerState* current_compiler = NULL;
Chunk* current_chunk = NULL;

static void error_at(Token* token, const char* message) {
    if (parser.panic_mode) return;
    parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void advance() {
    parser.previous = parser.current;
    for (;;) {
        parser.current = lexer_next_token();
        if (parser.current.type != TOKEN_ERROR) break;
        error_at(&parser.current, parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    error_at(&parser.current, message);
}

static void emit_byte(uint8_t byte) {
    if (current_chunk->count >= current_chunk->capacity) {
        current_chunk->capacity = current_chunk->capacity < 8 ? 8 : current_chunk->capacity * 2;
        current_chunk->code = realloc(current_chunk->code, current_chunk->capacity);
    }
    current_chunk->code[current_chunk->count++] = byte;
}

static void emit_bytes(uint8_t b1, uint8_t b2) {
    emit_byte(b1);
    emit_byte(b2);
}

static void patch_int32(int offset, int32_t value) {
    for (int i = 0; i < 4; i++) {
        current_chunk->code[offset + i] = (value >> (i * 8)) & 0xFF;
    }
}

static void emit_int(int64_t val) {
    emit_byte(OP_PUSH_INT);
    for (int i = 0; i < 8; i++) {
        emit_byte((val >> (i * 8)) & 0xFF);
    }
}

static int add_string(const char* start, int length) {
    if (current_chunk->strings_count >= current_chunk->strings_capacity) {
        current_chunk->strings_capacity = current_chunk->strings_capacity < 8 ? 8 : current_chunk->strings_capacity * 2;
        current_chunk->strings = realloc(current_chunk->strings, sizeof(char*) * current_chunk->strings_capacity);
    }
    char* s = malloc(length + 1);
    memcpy(s, start, length);
    s[length] = '\0';
    current_chunk->strings[current_chunk->strings_count] = s;
    return current_chunk->strings_count++;
}

static void expression();

static int resolve_local(CompilerState* state, Token* name) {
    for (int i = state->local_count - 1; i >= 0; i--) {
        Local* local = &state->locals[i];
        if (name->length == local->name.length &&
            memcmp(name->start, local->name.start, name->length) == 0) {
            return i;
        }
    }
    return -1;
}


static void add_local(Token name, ValueType type) {
    if (current_compiler->local_count == 256) {
        error_at(&name, "Too many local variables.");
        return;
    }
    Local* local = &current_compiler->locals[current_compiler->local_count++];
    local->name = name;
    local->type = type;
    local->depth = current_compiler->scope_depth;
}

static ValueType type_push(ValueType type) {
    if (current_compiler->type_stack_ptr >= STACK_MAX) {
        error_at(&parser.current, "Compile-time type stack overflow.");
    }
    current_compiler->type_stack[current_compiler->type_stack_ptr++] = type;
    return type;
}

static ValueType type_pop() {
    if (current_compiler->type_stack_ptr <= 0) {
        error_at(&parser.current, "Compile-time type stack underflow.");
        return VAL_VOID;
    }
    return current_compiler->type_stack[--current_compiler->type_stack_ptr];
}

static ValueType parse_type() {
    Token t = parser.current;
    advance();
    if (t.length == 3 && memcmp(t.start, "int", 3) == 0) return VAL_INT;
    if (t.length == 3 && memcmp(t.start, "flt", 3) == 0) return VAL_FLT;
    if (t.length == 3 && memcmp(t.start, "bol", 3) == 0) return VAL_BOOL;
    if (t.length == 3 && memcmp(t.start, "str", 3) == 0) return VAL_STR;
    if (t.length == 4 && memcmp(t.start, "void", 4) == 0) return VAL_VOID;
    error_at(&t, "Unknown type.");
    return VAL_VOID;
}

static void expression() {
    for (;;) {
        if (parser.current.type == TOKEN_INT) {
            int64_t val = strtoll(parser.current.start, NULL, 10);
            emit_int(val);
            type_push(VAL_INT);
            advance();
        } else if (parser.current.type == TOKEN_STR) {
            int idx = add_string(parser.current.start + 1, parser.current.length - 2);
            emit_bytes(OP_PUSH_STR, (uint8_t)idx);
            type_push(VAL_STR);
            advance();
        } else if (parser.current.type == TOKEN_BOOL) {
            bool val = parser.current.start[0] == 't';
            emit_bytes(OP_PUSH_BOOL, val ? 1 : 0);
            type_push(VAL_BOOL);
            advance();
        } else if (parser.current.type == TOKEN_ID) {
            Token name = parser.current;
            advance();
            int arg = resolve_local(current_compiler, &name);
            if (arg != -1) {
                emit_bytes(OP_LOAD, (uint8_t)arg);
                type_push(current_compiler->locals[arg].type);
            } else {
                int f_idx = -1;
                for (int i = 0; i < current_compiler->function_count; i++) {
                    if (name.length == current_compiler->functions[i].name.length &&
                        memcmp(name.start, current_compiler->functions[i].name.start, name.length) == 0) {
                        f_idx = i; break;
                    }
                }
                if (f_idx != -1) {
                    Function* f = &current_compiler->functions[f_idx];
                    // Check params
                    for (int i = f->param_count - 1; i >= 0; i--) {
                        ValueType t = type_pop();
                        if (t != f->param_types[i]) error_at(&name, "Type mismatch in function call.");
                    }
                    emit_byte(OP_CALL);
                    patch_int32(current_chunk->count, f->addr);
                    current_chunk->count += 4;
                    if (f->return_type != VAL_VOID) type_push(f->return_type);
                    return;
                } else {
                    error_at(&name, "Undefined identifier.");
                }
            }
        } else if (parser.current.type == TOKEN_PLUS || parser.current.type == TOKEN_MINUS ||
                   parser.current.type == TOKEN_STAR || parser.current.type == TOKEN_SLASH) {
            ValueType b = type_pop();
            ValueType a = type_pop();
            if (a != b || (a != VAL_INT && a != VAL_FLT)) error_at(&parser.current, "Arithmetic type error.");
            OpCode op = OP_ADD;
            if (parser.current.type == TOKEN_MINUS) op = OP_SUB;
            else if (parser.current.type == TOKEN_STAR) op = OP_MUL;
            else if (parser.current.type == TOKEN_SLASH) op = OP_DIV;
            advance(); emit_byte(op); type_push(a); return;
        } else if (parser.current.type == TOKEN_BANG_BANG) {
            type_pop(); // anything can be printed
            advance(); emit_byte(OP_PRINT); return;
        } else if (parser.current.type == TOKEN_EQ_EQ || parser.current.type == TOKEN_LANGLE || parser.current.type == TOKEN_RANGLE) {
            ValueType b = type_pop();
            ValueType a = type_pop();
            if (a != b) error_at(&parser.current, "Comparison type error.");
            OpCode op = OP_EQ;
            if (parser.current.type == TOKEN_LANGLE) op = OP_LT;
            else if (parser.current.type == TOKEN_RANGLE) op = OP_GT;
            advance(); emit_byte(op); type_push(VAL_BOOL); return;
        } else if (parser.current.type == TOKEN_ASSIGN) {
            advance();
            consume(TOKEN_ID, "Expect variable name after =>");
            Token var_name = parser.previous;
            consume(TOKEN_COLON, "Expect : after variable name");
            ValueType declared_type = parse_type();
            ValueType value_type = type_pop();
            if (declared_type != value_type) error_at(&var_name, "Assignment type mismatch.");

            int arg = resolve_local(current_compiler, &var_name);
            if (arg == -1) {
                add_local(var_name, declared_type);
                arg = current_compiler->local_count - 1;
            }
            emit_bytes(OP_STORE, (uint8_t)arg);
            return;
        } else {
            break;
        }
    }
}

static void statement() {
    if (parser.current.type == TOKEN_LBRACKET) {
        advance();
        current_compiler->scope_depth++;
        while (parser.current.type != TOKEN_RBRACKET && parser.current.type != TOKEN_EOF) {
            statement();
        }
        consume(TOKEN_RBRACKET, "Expect ']' after block.");
        current_compiler->scope_depth--;
    } else {
        int start_addr = current_chunk->count;
        expression();

        if (parser.current.type == TOKEN_QUESTION) {
            advance();

            int jump_if_f_patch = current_chunk->count;
            emit_byte(OP_JUMP_IF_F);
            emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0); // placeholder

            statement(); // true block

            int jump_patch = current_chunk->count;
            emit_byte(OP_JUMP);
            emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0); // placeholder

            int false_addr = current_chunk->count;
        patch_int32(jump_if_f_patch + 1, false_addr);

            if (parser.current.type == TOKEN_COLON) {
                advance();
                statement(); // false block
            }

            int end_addr = current_chunk->count;
        patch_int32(jump_patch + 1, end_addr);
        } else if (parser.current.type == TOKEN_AT) {
            advance();

            int jump_if_f_patch = current_chunk->count;
            emit_byte(OP_JUMP_IF_F);
            emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0); // placeholder

            statement(); // block

            emit_byte(OP_JUMP);
            int jump_back_addr = start_addr;
            for (int i = 0; i < 4; i++) {
                emit_byte((jump_back_addr >> (i * 8)) & 0xFF);
            }

            int end_addr = current_chunk->count;
        patch_int32(jump_if_f_patch + 1, end_addr);
        }
    }
}

Chunk* compiler_compile(const char* source) {
    lexer_init(source);
    current_chunk = malloc(sizeof(Chunk));
    current_chunk->code = NULL;
    current_chunk->count = 0;
    current_chunk->capacity = 0;
    current_chunk->strings = NULL;
    current_chunk->strings_count = 0;
    current_chunk->strings_capacity = 0;

    current_compiler = malloc(sizeof(CompilerState));
    current_compiler->local_count = 0;
    current_compiler->function_count = 0;
    current_compiler->scope_depth = 0;

    parser.had_error = false;
    parser.panic_mode = false;

    advance();

    // For now, let's just parse a list of expressions/statements
    // Specifically look for main function if we want to be correct,
    // but for the first test let's just parse everything.

    while (parser.current.type != TOKEN_EOF) {
        if (parser.current.type == TOKEN_LANGLE) {
            int jump_over = current_chunk->count;
            emit_byte(OP_JUMP);
            emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0);

            advance();
            Token params[16];
            ValueType param_types[16];
            int param_count = 0;
            while (parser.current.type != TOKEN_RANGLE) {
                consume(TOKEN_ID, "Expect param name");
                params[param_count] = parser.previous;
                consume(TOKEN_COLON, "Expect :");
                param_types[param_count] = parse_type();
                param_count++;
            }
            advance(); // >
            consume(TOKEN_ARROW, "Expect -> after args");
            ValueType return_type = parse_type();
            consume(TOKEN_COLON, "Expect : after ret type");
            consume(TOKEN_ID, "Expect function name");
            Token name = parser.previous;

            Function* func = &current_compiler->functions[current_compiler->function_count++];
            func->name = name;
            func->addr = current_chunk->count;
            func->return_type = return_type;
            func->param_count = param_count;
            for (int i = 0; i < param_count; i++) func->param_types[i] = param_types[i];

            int old_local_count = current_compiler->local_count;
            int old_type_stack_ptr = current_compiler->type_stack_ptr;
            current_compiler->local_count = 0; // Reset locals for new function
            current_compiler->type_stack_ptr = 0; // Reset type stack for new function

            for (int i = 0; i < param_count; i++) {
                add_local(params[i], param_types[i]);
            }
            // Emit stores for params in reverse order
            for (int i = param_count - 1; i >= 0; i--) {
                emit_bytes(OP_STORE, (uint8_t)i);
            }

            statement(); // body

            // Final check of return type (simplified)
            if (return_type != VAL_VOID && current_compiler->type_stack_ptr > 0) {
                ValueType actual = type_pop();
                if (actual != return_type) error_at(&name, "Return type mismatch.");
            }

            emit_byte(OP_RET);

            patch_int32(jump_over + 1, current_chunk->count);
            current_compiler->local_count = old_local_count;
            current_compiler->type_stack_ptr = old_type_stack_ptr;
        } else {
            statement();
        }
    }

    emit_byte(OP_HALT);

    if (parser.had_error) {
        chunk_free(current_chunk);
        free(current_compiler);
        return NULL;
    }

    free(current_compiler);
    return current_chunk;
}

void chunk_free(Chunk* chunk) {
    if (chunk) {
        free(chunk->code);
        for (int i = 0; i < chunk->strings_count; i++) {
            free(chunk->strings[i]);
        }
        free(chunk->strings);
        free(chunk);
    }
}
