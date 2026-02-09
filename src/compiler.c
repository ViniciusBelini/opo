#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <libgen.h>
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
    bool is_public;
} Function;

typedef struct {
    Token name;
    Token fields[16];
    ValueType field_types[16];
    int field_count;
    bool is_public;
} StructDef;

typedef struct {
    Token name;
    int index;
    ValueType return_type;
    ValueType param_types[8];
    int param_count;
} Native;

typedef struct {
    Local locals[256];
    int local_count;
    Function functions[256];
    int function_count;
    StructDef structs[64];
    int struct_count;
    Native natives[64];
    int native_count;
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

static void emit_push_func(int64_t addr, ValueType type) {
    emit_byte(OP_PUSH_FUNC);
    for (int i = 0; i < 8; i++) {
        emit_byte((addr >> (i * 8)) & 0xFF);
    }
    emit_byte((uint8_t)type);
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

static char* root_base_dir = NULL;
static char* compiled_modules[64];
static int compiled_modules_count = 0;
static char* compilation_stack[64];
static int compilation_stack_count = 0;
static const char* active_prefix = NULL;

static char* resolve_path(const char* rel_path) {
    char* resolved = malloc(1024);
    if (rel_path[0] == '/') {
        strcpy(resolved, rel_path);
    } else {
        sprintf(resolved, "%s/%s", root_base_dir, rel_path);
    }
    return resolved;
}

static char* compiler_read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) return NULL;
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) return NULL;
    size_t bytesRead = fread(buffer, 1, fileSize, file);
    (void)bytesRead;
    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static int resolve_native(CompilerState* state, Token* name) {
    for (int i = 0; i < state->native_count; i++) {
        Native* n = &state->natives[i];
        if (name->length == n->name.length &&
            memcmp(name->start, n->name.start, name->length) == 0) {
            return n->index;
        }
    }
    return -1;
}

static bool match(TokenType type) {
    if (parser.current.type != type) return false;
    advance();
    return true;
}

static bool is_assignable(ValueType expected, ValueType actual) {
    if (expected == VAL_ANY || actual == VAL_ANY) return true;
    if (expected == actual) return true;
    if (expected == VAL_FUNC && actual >= VAL_FUNC && actual <= VAL_FUNC_VOID) return true;
    if (actual == VAL_FUNC && expected >= VAL_FUNC && expected <= VAL_FUNC_VOID) return true;
    if (expected == VAL_OBJ) {
        return actual == VAL_STR || actual == VAL_MAP || actual == VAL_OBJ;
    }
    if (expected == VAL_MAP && actual == VAL_OBJ) return true;
    return false;
}

static void add_native(const char* name, int index, ValueType ret, int p_count, ...) {
    Native* n = &current_compiler->natives[current_compiler->native_count++];
    n->name.start = name;
    n->name.length = (int)strlen(name);
    n->index = index;
    n->return_type = ret;
    n->param_count = p_count;
    va_list args;
    va_start(args, p_count);
    for (int i = 0; i < p_count; i++) {
        n->param_types[i] = va_arg(args, ValueType);
    }
    va_end(args);
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
    if (match(TOKEN_LBRACKET)) {
        consume(TOKEN_RBRACKET, "Expect ']' after '[' for array type.");
        parse_type();
        return VAL_OBJ;
    }
    if (match(TOKEN_LBRACE)) {
        parse_type(); // key type
        consume(TOKEN_COLON, "Expect ':' after key type in map type.");
        parse_type(); // value type
        consume(TOKEN_RBRACE, "Expect '}' after map type.");
        return VAL_MAP;
    }
    if (match(TOKEN_LANGLE)) {
        while (parser.current.type != TOKEN_RANGLE && parser.current.type != TOKEN_EOF) {
            parse_type();
            if (parser.current.type == TOKEN_COMMA) advance();
        }
        consume(TOKEN_RANGLE, "Expect '>' after function type parameters.");
        consume(TOKEN_ARROW, "Expect '->' after function type parameters.");
        ValueType ret = parse_type(); // Return type
        switch (ret) {
            case VAL_INT: return VAL_FUNC_INT;
            case VAL_FLT: return VAL_FUNC_FLT;
            case VAL_BOOL: return VAL_FUNC_BOOL;
            case VAL_STR: return VAL_FUNC_STR;
            case VAL_VOID: return VAL_FUNC_VOID;
            default: return VAL_FUNC;
        }
    }

    Token t = parser.current;
    advance();

    if (match(TOKEN_DOT)) {
        Token member = parser.current;
        advance();
        char full_name[256];
        memcpy(full_name, t.start, t.length);
        full_name[t.length] = '.';
        memcpy(full_name + t.length + 1, member.start, member.length);
        full_name[t.length + 1 + member.length] = '\0';
        
        for (int i = 0; i < current_compiler->struct_count; i++) {
            if (current_compiler->structs[i].name.length == (int)strlen(full_name) &&
                memcmp(current_compiler->structs[i].name.start, full_name, strlen(full_name)) == 0) {
                return VAL_OBJ;
            }
        }
        error_at(&member, "Unknown type in namespace.");
        return VAL_VOID;
    }

    if (t.type == TOKEN_IMP) return VAL_IMP;
    if (t.type == TOKEN_TYPE) return VAL_VOID;

    if (t.length == 3 && memcmp(t.start, "int", 3) == 0) return VAL_INT;
    if (t.length == 3 && memcmp(t.start, "flt", 3) == 0) return VAL_FLT;
    if (t.length == 3 && memcmp(t.start, "bol", 3) == 0) return VAL_BOOL;
    if (t.length == 3 && memcmp(t.start, "str", 3) == 0) return VAL_STR;
    if (t.length == 4 && memcmp(t.start, "void", 4) == 0) return VAL_VOID;
    if (t.length == 3 && memcmp(t.start, "fun", 3) == 0) return VAL_FUNC;
    
    // Check if it's a struct name
    for (int i = 0; i < current_compiler->struct_count; i++) {
        if (t.length == current_compiler->structs[i].name.length &&
            memcmp(t.start, current_compiler->structs[i].name.start, t.length) == 0) {
            return VAL_OBJ;
        }
    }

    error_at(&t, "Unknown type.");
    return VAL_VOID;
}

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =>
    PREC_POSTFIX,     // !!
    PREC_OR,          // ||
    PREC_AND,         // &&
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * / %
    PREC_UNARY,       // ! -
    PREC_CALL,        // ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static void parse_precedence(Precedence precedence);
static ParseRule* get_rule(TokenType type);

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static void binary() {
    TokenType operator_type = parser.previous.type;
    ParseRule* rule = get_rule(operator_type);
    parse_precedence((Precedence)(rule->precedence + 1));

    ValueType b = type_pop();
    ValueType a = type_pop();

    switch (operator_type) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
            if (operator_type == TOKEN_PLUS && a == VAL_STR && b == VAL_STR) {
                emit_byte(OP_ADD);
                type_push(VAL_STR);
            } else {
                if (a != b || (a != VAL_INT && a != VAL_FLT)) {
                    error_at(&parser.previous, "Arithmetic type error.");
                }
                if (operator_type == TOKEN_PLUS) emit_byte(OP_ADD);
                else if (operator_type == TOKEN_MINUS) emit_byte(OP_SUB);
                else if (operator_type == TOKEN_STAR) emit_byte(OP_MUL);
                else if (operator_type == TOKEN_SLASH) emit_byte(OP_DIV);
                type_push(a);
            }
            break;
        case TOKEN_PERCENT:
            if (a != VAL_INT || b != VAL_INT) {
                error_at(&parser.previous, "Modulo type error.");
            }
            emit_byte(OP_MOD);
            type_push(VAL_INT);
            break;
        case TOKEN_EQ_EQ:
        case TOKEN_BANG_EQ:
        case TOKEN_LANGLE:
        case TOKEN_RANGLE:
        case TOKEN_LTE:
        case TOKEN_GTE:
            if (a != b) error_at(&parser.previous, "Comparison type error.");
            if (operator_type == TOKEN_EQ_EQ) emit_byte(OP_EQ);
            else if (operator_type == TOKEN_BANG_EQ) {
                emit_byte(OP_EQ);
                emit_byte(OP_NOT);
            }
            else if (operator_type == TOKEN_LANGLE) emit_byte(OP_LT);
            else if (operator_type == TOKEN_RANGLE) emit_byte(OP_GT);
            else if (operator_type == TOKEN_LTE) emit_byte(OP_LTE);
            else if (operator_type == TOKEN_GTE) emit_byte(OP_GTE);
            type_push(VAL_BOOL);
            break;
        case TOKEN_AND:
        case TOKEN_OR:
            if (a != VAL_BOOL || b != VAL_BOOL) error_at(&parser.previous, "Logic type error.");
            emit_byte(operator_type == TOKEN_AND ? OP_AND : OP_OR);
            type_push(VAL_BOOL);
            break;
        default: return;
    }
}

static void unary() {
    TokenType operator_type = parser.previous.type;
    parse_precedence(PREC_UNARY);
    ValueType t = type_pop();
    if (operator_type == TOKEN_MINUS) {
        if (t != VAL_INT && t != VAL_FLT) error_at(&parser.previous, "Operand must be a number.");
        emit_byte(OP_NEG);
    } else if (operator_type == TOKEN_BANG) {
        if (t != VAL_BOOL) error_at(&parser.previous, "Operand must be a boolean.");
        emit_byte(OP_NOT);
    }
    type_push(t);
}

static void grouping() {
    expression();
    consume(TOKEN_RPAREN, "Expect ')' after expression.");
}

static void array_literal() {
    int count = 0;
    ValueType element_type = VAL_VOID;
    if (parser.current.type != TOKEN_RBRACKET) {
        do {
            expression();
            ValueType t = type_pop();
            if (count == 0) element_type = t;
            else if (element_type != t && t != VAL_VOID) {
                error_at(&parser.previous, "All elements in an array literal must have the same type.");
            }
            count++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RBRACKET, "Expect ']' after array elements.");
    emit_bytes(OP_ARRAY, (uint8_t)count);
    type_push(VAL_OBJ);
}

static void map_literal() {
    int count = 0;
    if (parser.current.type != TOKEN_RBRACE) {
        do {
            parse_precedence(PREC_OR);
            type_pop();
            consume(TOKEN_ASSIGN, "Expect '=>' between key and value in map literal.");
            parse_precedence(PREC_OR);
            type_pop();
            count++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RBRACE, "Expect '}' after map elements.");
    emit_bytes(OP_MAP, (uint8_t)count);
    type_push(VAL_MAP);
}

static void dot() {
    ValueType lhs_type = type_pop();
    (void)lhs_type;
    if (match(TOKEN_INT)) {
        int64_t idx = strtoll(parser.previous.start, NULL, 10);
        emit_int(idx);
        emit_byte(OP_INDEX);
        type_push(VAL_OBJ);
    } else if (match(TOKEN_STR)) {
        int idx = add_string(parser.previous.start + 1, parser.previous.length - 2);
        emit_bytes(OP_PUSH_STR, (uint8_t)idx);
        emit_byte(OP_INDEX);
        type_push(VAL_OBJ);
    } else if (match(TOKEN_LPAREN)) {
        expression();
        consume(TOKEN_RPAREN, "Expect ')' after expression in dot access.");
        type_pop(); // index type
        emit_byte(OP_INDEX);
        type_push(VAL_OBJ);
    } else {
        consume(TOKEN_ID, "Expect member name after '.'.");
        Token name = parser.previous;
        int field_idx = -1;
        for (int i = 0; i < current_compiler->struct_count; i++) {
            for (int j = 0; j < current_compiler->structs[i].field_count; j++) {
                if (name.length == current_compiler->structs[i].fields[j].length &&
                    memcmp(name.start, current_compiler->structs[i].fields[j].start, name.length) == 0) {
                    field_idx = j;
                    break;
                }
            }
            if (field_idx != -1) break;
        }

        if (field_idx != -1) {
            emit_bytes(OP_GET_MEMBER, (uint8_t)field_idx);
            type_push(VAL_OBJ);
        } else {
            // Try as a variable for dynamic indexing
            int arg = resolve_local(current_compiler, &name);
            if (arg != -1) {
                emit_bytes(OP_LOAD, (uint8_t)arg);
                emit_byte(OP_INDEX);
                type_push(VAL_OBJ);
            } else {
                error_at(&name, "Unknown struct field or index variable.");
            }
        }
    }
}

static void number() {
    if (parser.previous.type == TOKEN_INT) {
        int64_t val = strtoll(parser.previous.start, NULL, 10);
        emit_int(val);
        type_push(VAL_INT);
    } else {
        double val = strtod(parser.previous.start, NULL);
        emit_byte(OP_PUSH_FLT);
        union { double f; uint64_t u; } conv;
        conv.f = val;
        for (int i = 0; i < 8; i++) {
            emit_byte((conv.u >> (i * 8)) & 0xFF);
        }
        type_push(VAL_FLT);
    }
}

static void string() {
    int idx = add_string(parser.previous.start + 1, parser.previous.length - 2);
    emit_bytes(OP_PUSH_STR, (uint8_t)idx);
    type_push(VAL_STR);
}

static void literal() {
    switch (parser.previous.type) {
        case TOKEN_BOOL: {
            bool val = parser.previous.start[0] == 't';
            emit_bytes(OP_PUSH_BOOL, val ? 1 : 0);
            type_push(VAL_BOOL);
            break;
        }
        default: return;
    }
}

static void variable() {
    Token name = parser.previous;

    // 1. Try Local
    int arg = resolve_local(current_compiler, &name);
    if (arg != -1) {
        ValueType type = current_compiler->locals[arg].type;
        if (match(TOKEN_LPAREN)) {
            int arg_count = 0;
            if (parser.current.type != TOKEN_RPAREN) {
                do {
                    expression();
                    type_pop();
                    arg_count++;
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expect ')' after arguments.");
            emit_bytes(OP_LOAD, (uint8_t)arg);
            emit_bytes(OP_INVOKE, (uint8_t)arg_count);
            ValueType ret_type = VAL_OBJ;
            if (type == VAL_FUNC_INT) ret_type = VAL_INT;
            else if (type == VAL_FUNC_FLT) ret_type = VAL_FLT;
            else if (type == VAL_FUNC_BOOL) ret_type = VAL_BOOL;
            else if (type == VAL_FUNC_STR) ret_type = VAL_STR;
            else if (type == VAL_FUNC_VOID) ret_type = VAL_VOID;
            if (ret_type != VAL_VOID) type_push(ret_type);
        } else {
            emit_bytes(OP_LOAD, (uint8_t)arg);
            type_push(type);
        }
        return;
    }

    // 2. Try Namespace Access
    if (match(TOKEN_DOT)) {
        consume(TOKEN_ID, "Expect member name after '.'.");
        Token member = parser.previous;
        
        char full_name[256];
        memcpy(full_name, name.start, name.length);
        full_name[name.length] = '.';
        memcpy(full_name + name.length + 1, member.start, member.length);
        full_name[name.length + 1 + member.length] = '\0';
        
        for (int i = 0; i < current_compiler->function_count; i++) {
            Function* f = &current_compiler->functions[i];
            if (f->name.length == (int)strlen(full_name) &&
                memcmp(f->name.start, full_name, f->name.length) == 0) {
                if (!f->is_public) error_at(&member, "Cannot access private member.");
                if (match(TOKEN_LPAREN)) {
                    int arg_count = 0;
                    if (parser.current.type != TOKEN_RPAREN) {
                        do { 
                            expression(); 
                            ValueType arg_type = type_pop();
                            if (arg_count < f->param_count) {
                                if (!is_assignable(f->param_types[arg_count], arg_type)) {
                                    error_at(&parser.previous, "Namespaced function argument type mismatch.");
                                }
                            }
                            arg_count++; 
                        } while (match(TOKEN_COMMA));
                    }
                    consume(TOKEN_RPAREN, "Expect ')' after arguments.");
                    if (arg_count != f->param_count) error_at(&member, "Wrong number of arguments.");
                    emit_byte(OP_CALL);
                    patch_int32(current_chunk->count, f->addr);
                    current_chunk->count += 4;
                    if (f->return_type != VAL_VOID) type_push(f->return_type);
                } else {
                    emit_push_func(f->addr, VAL_FUNC);
                    type_push(VAL_FUNC);
                }
                return;
            }
        }
        
        for (int i = 0; i < current_compiler->struct_count; i++) {
            StructDef* sd = &current_compiler->structs[i];
            if (sd->name.length == (int)strlen(full_name) &&
                memcmp(sd->name.start, full_name, sd->name.length) == 0) {
                if (!sd->is_public) error_at(&member, "Cannot access private struct.");
                if (match(TOKEN_LPAREN)) {
                    int count = 0;
                    if (parser.current.type != TOKEN_RPAREN) {
                        do { 
                            expression(); 
                            ValueType arg_type = type_pop();
                            if (count < sd->field_count) {
                                if (!is_assignable(sd->field_types[count], arg_type)) {
                                    error_at(&parser.previous, "Namespaced struct field type mismatch.");
                                }
                            }
                            count++; 
                        } while (match(TOKEN_COMMA));
                    }
                    consume(TOKEN_RPAREN, "Expect ')'.");
                    if (count != sd->field_count) error_at(&member, "Wrong number of fields for namespaced struct instantiation.");
                    emit_bytes(OP_STRUCT, (uint8_t)count);
                    type_push(VAL_OBJ);
                }
                return;
            }
        }
        error_at(&member, "Undefined member in namespace.");
        return;
    }


    if (name.length == 6 && memcmp(name.start, "typeOf", 6) == 0) {
        consume(TOKEN_LPAREN, "Expect '(' after 'typeOf'.");
        expression();
        consume(TOKEN_RPAREN, "Expect ')' after 'typeOf' argument.");
        emit_byte(OP_TYPEOF);
        type_pop();
        type_push(VAL_STR);
        return;
    }


    int n_idx = resolve_native(current_compiler, &name);
    if (n_idx != -1) {
        Native* n = &current_compiler->natives[0];
        for (int i = 0; i < current_compiler->native_count; i++) {
            if (current_compiler->natives[i].index == n_idx) { n = &current_compiler->natives[i]; break; }
        }
        if (match(TOKEN_LPAREN)) {
            int arg_count = 0;
            if (parser.current.type != TOKEN_RPAREN) {
                do {
                    expression();
                    ValueType arg_type = type_pop();
                    if (arg_count < n->param_count) {
                        if (!is_assignable(n->param_types[arg_count], arg_type)) {
                            error_at(&parser.previous, "Native function argument type mismatch.");
                        }
                    }
                    arg_count++;
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expect ')' after arguments.");
            if (arg_count != n->param_count) error_at(&name, "Wrong number of arguments for native function.");
            emit_bytes(OP_LOAD_G, (uint8_t)n_idx);
            emit_bytes(OP_INVOKE, (uint8_t)arg_count);
            type_push(n->return_type);
        } else {
            emit_bytes(OP_LOAD_G, (uint8_t)n_idx);
            type_push(VAL_OBJ);
        }
        return;
    }

    int s_idx = -1;
    for (int i = 0; i < current_compiler->struct_count; i++) {
        if (name.length == current_compiler->structs[i].name.length &&
            memcmp(name.start, current_compiler->structs[i].name.start, name.length) == 0) {
            s_idx = i; break;
        }
    }

    if (s_idx != -1) {
        StructDef* sd = &current_compiler->structs[s_idx];
        if (match(TOKEN_LPAREN)) {
            int count = 0;
            if (parser.current.type != TOKEN_RPAREN) {
                do {
                    expression();
                    ValueType arg_type = type_pop();
                    if (count < sd->field_count) {
                        if (!is_assignable(sd->field_types[count], arg_type)) {
                            error_at(&parser.previous, "Struct field type mismatch.");
                        }
                    }
                    count++;
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expect ')' after struct arguments.");
            if (count != sd->field_count) error_at(&name, "Wrong number of fields for struct instantiation.");
            emit_bytes(OP_STRUCT, (uint8_t)count);
            type_push(VAL_OBJ);
        } else {
            error_at(&name, "Expect '(' after struct name for instantiation.");
        }
        return;
    }

    int f_idx = -1;
    for (int i = 0; i < current_compiler->function_count; i++) {
        if (name.length == current_compiler->functions[i].name.length &&
            memcmp(name.start, current_compiler->functions[i].name.start, name.length) == 0) {
            f_idx = i; break;
        }
    }
    
    // Try with active prefix if not found
    if (f_idx == -1 && active_prefix != NULL) {
        char full_name[256];
        strcpy(full_name, active_prefix);
        strncat(full_name, name.start, name.length);
        for (int i = 0; i < current_compiler->function_count; i++) {
            if (current_compiler->functions[i].name.length == (int)strlen(full_name) &&
                memcmp(current_compiler->functions[i].name.start, full_name, strlen(full_name)) == 0) {
                f_idx = i; break;
            }
        }
    }

    if (f_idx != -1) {
        Function* f = &current_compiler->functions[f_idx];
        if (match(TOKEN_LPAREN)) {
            int arg_count = 0;
            if (parser.current.type != TOKEN_RPAREN) {
                do {
                    expression();
                    ValueType arg_type = type_pop();
                    if (arg_count < f->param_count) {
                        if (!is_assignable(f->param_types[arg_count], arg_type)) {
                            error_at(&parser.previous, "Function argument type mismatch.");
                        }
                    }
                    arg_count++;
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expect ')' after arguments.");
            if (arg_count != f->param_count) error_at(&name, "Wrong number of arguments.");
            emit_byte(OP_CALL);
            patch_int32(current_chunk->count, f->addr);
            current_chunk->count += 4;
            if (f->return_type != VAL_VOID) type_push(f->return_type);
        } else {
            ValueType f_type = VAL_FUNC;
            emit_push_func(f->addr, f_type);
            type_push(f_type);
        }
        return;
    }

    error_at(&name, "Undefined identifier.");
}

static void assignment() {
    consume(TOKEN_ID, "Expect variable name after =>");
    Token name = parser.previous;
    if (match(TOKEN_COLON)) {
        ValueType declared_type = parse_type();
        ValueType value_type = type_pop();
        if (!is_assignable(declared_type, value_type)) {
            error_at(&name, "Type mismatch in variable initialization.");
        }
        int arg = resolve_local(current_compiler, &name);
        if (arg == -1) {
            add_local(name, declared_type);
            arg = current_compiler->local_count - 1;
        }
        emit_bytes(OP_STORE, (uint8_t)arg);
        type_push(VAL_VOID);
    } else if (match(TOKEN_DOT)) {
        int arg = resolve_local(current_compiler, &name);
        if (arg == -1) error_at(&name, "Undefined object.");
        emit_bytes(OP_LOAD, (uint8_t)arg);
        if (match(TOKEN_INT)) {
            int64_t idx = strtoll(parser.previous.start, NULL, 10);
            emit_int(idx);
            emit_byte(OP_SET_INDEX);
            type_pop(); // pop value
            type_push(VAL_VOID);
        } else if (match(TOKEN_STR)) {
            int idx = add_string(parser.previous.start + 1, parser.previous.length - 2);
            emit_bytes(OP_PUSH_STR, (uint8_t)idx);
            emit_byte(OP_SET_INDEX);
            type_pop(); // pop value
            type_push(VAL_VOID);
        } else if (match(TOKEN_LPAREN)) {
            expression();
            consume(TOKEN_RPAREN, "Expect ')' after expression.");
            type_pop(); // pop index type
            emit_byte(OP_SET_INDEX);
            type_pop(); // pop value
            type_push(VAL_VOID);
        } else {
            consume(TOKEN_ID, "Expect member name.");
            Token field_name = parser.previous;
            int field_idx = -1;
            for (int i = 0; i < current_compiler->struct_count; i++) {
                for (int j = 0; j < current_compiler->structs[i].field_count; j++) {
                    if (field_name.length == current_compiler->structs[i].fields[j].length &&
                        memcmp(field_name.start, current_compiler->structs[i].fields[j].start, field_name.length) == 0) {
                        field_idx = j;
                        break;
                    }
                }
                if (field_idx != -1) break;
            }
            
            if (field_idx != -1) {
                emit_bytes(OP_SET_MEMBER, (uint8_t)field_idx);
                type_pop(); // pop value
                type_push(VAL_VOID);
            } else {
                // Try as dynamic index
                int idx_arg = resolve_local(current_compiler, &field_name);
                if (idx_arg != -1) {
                    emit_bytes(OP_LOAD, (uint8_t)idx_arg);
                    emit_byte(OP_SET_INDEX);
                    type_pop(); // pop value
                    type_push(VAL_VOID);
                } else {
                    error_at(&field_name, "Unknown struct field or index variable.");
                }
            }
        }
    } else {
        int arg = resolve_local(current_compiler, &name);
        if (arg == -1) error_at(&name, "Undefined identifier.");
        ValueType value_type = type_pop();
        if (!is_assignable(current_compiler->locals[arg].type, value_type)) {
            error_at(&name, "Type mismatch in assignment.");
        }
        emit_bytes(OP_STORE, (uint8_t)arg);
        type_push(VAL_VOID);
    }
}

static void print_op() {
    type_pop();
    emit_byte(OP_PRINT);
    type_push(VAL_VOID);
}

ParseRule rules[] = {
    [TOKEN_INT]       = {number,   NULL,       PREC_NONE},
    [TOKEN_FLT]       = {number,   NULL,       PREC_NONE},
    [TOKEN_STR]       = {string,   NULL,       PREC_NONE},
    [TOKEN_BOOL]      = {literal,  NULL,       PREC_NONE},
    [TOKEN_ID]        = {variable, NULL,       PREC_NONE},
    [TOKEN_PLUS]      = {NULL,     binary,     PREC_TERM},
    [TOKEN_MINUS]     = {unary,    binary,     PREC_TERM},
    [TOKEN_STAR]      = {NULL,     binary,     PREC_FACTOR},
    [TOKEN_SLASH]     = {NULL,     binary,     PREC_FACTOR},
    [TOKEN_PERCENT]   = {NULL,     binary,     PREC_FACTOR},
    [TOKEN_BANG]      = {unary,    NULL,       PREC_NONE},
    [TOKEN_BANG_BANG] = {NULL,     print_op,   PREC_POSTFIX},
    [TOKEN_AND]       = {NULL,     binary,     PREC_AND},
    [TOKEN_OR]        = {NULL,     binary,     PREC_OR},
    [TOKEN_EQ_EQ]     = {NULL,     binary,     PREC_EQUALITY},
    [TOKEN_BANG_EQ]   = {NULL,     binary,     PREC_EQUALITY},
    [TOKEN_LANGLE]    = {NULL,     binary,     PREC_COMPARISON},
    [TOKEN_RANGLE]    = {NULL,     binary,     PREC_COMPARISON},
    [TOKEN_LTE]       = {NULL,     binary,     PREC_COMPARISON},
    [TOKEN_GTE]       = {NULL,     binary,     PREC_COMPARISON},
    [TOKEN_LPAREN]    = {grouping, NULL,       PREC_NONE},
    [TOKEN_LBRACKET]  = {array_literal, NULL,  PREC_NONE},
    [TOKEN_LBRACE]    = {map_literal,   NULL,  PREC_NONE},
    [TOKEN_DOT]       = {NULL,     dot,        PREC_CALL},
    [TOKEN_ASSIGN]    = {NULL,     assignment, PREC_ASSIGNMENT},
    [TOKEN_EOF]       = {NULL,     NULL,       PREC_NONE},
};

static ParseRule* get_rule(TokenType type) {
    return &rules[type];
}

static void parse_precedence(Precedence precedence) {
    advance();
    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error_at(&parser.previous, "Expect expression.");
        return;
    }
    prefix_rule();
    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule();
    }
}

static void statement();

static void block() {
    consume(TOKEN_LBRACKET, "Expect '[' to start block.");
    current_compiler->scope_depth++;
    bool empty = true;
    while (parser.current.type != TOKEN_RBRACKET && parser.current.type != TOKEN_EOF) {
        if (!empty) {
            ValueType t = type_pop();
            if (t != VAL_VOID) emit_byte(OP_POP);
        }
        statement();
        empty = false;
    }
    if (empty) type_push(VAL_VOID);
    consume(TOKEN_RBRACKET, "Expect ']' after block.");
    current_compiler->scope_depth--;
}

static void handle_import(Token* path_token, Token* alias_token);

static void compile_internal(const char* prefix) {
    while (parser.current.type != TOKEN_EOF) {
        if (match(TOKEN_SEMICOLON)) continue;
        
        bool is_public = match(TOKEN_PUB);

        if (match(TOKEN_STRUCT)) {
            consume(TOKEN_LBRACKET, "Expect '[' after 'struct'.");
            StructDef* sd = &current_compiler->structs[current_compiler->struct_count++];
            sd->field_count = 0;
            sd->is_public = is_public;
            while (parser.current.type != TOKEN_RBRACKET) {
                consume(TOKEN_ID, "Expect field name.");
                sd->fields[sd->field_count] = parser.previous;
                consume(TOKEN_COLON, "Expect ':'.");
                sd->field_types[sd->field_count] = parse_type();
                sd->field_count++;
                if (parser.current.type == TOKEN_COMMA) advance();
            }
            consume(TOKEN_RBRACKET, "Expect ']' after struct fields.");
            consume(TOKEN_ASSIGN, "Expect '=>' after struct definition.");
            consume(TOKEN_ID, "Expect struct name.");
            
            Token name = parser.previous;
            if (prefix != NULL) {
                char* new_name = malloc(strlen(prefix) + name.length + 1);
                strcpy(new_name, prefix);
                strncat(new_name, name.start, name.length);
                sd->name.start = new_name;
                sd->name.length = (int)strlen(new_name);
            } else {
                sd->name = name;
            }

            consume(TOKEN_COLON, "Expect ':'.");
            consume(TOKEN_TYPE, "Expect 'type' after struct name.");
            continue;
        }

        if (parser.current.type == TOKEN_LANGLE) {
            int jump_over = current_chunk->count;
            emit_byte(OP_JUMP);
            emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0);
            advance();
            Token params[16]; ValueType param_types[16]; int param_count = 0;
            while (parser.current.type != TOKEN_RANGLE) {
                consume(TOKEN_ID, "Expect param name");
                params[param_count] = parser.previous;
                consume(TOKEN_COLON, "Expect :");
                param_types[param_count] = parse_type();
                param_count++;
                if (parser.current.type == TOKEN_COMMA) advance();
            }
            advance(); // >
            consume(TOKEN_ARROW, "Expect -> after args");
            ValueType return_type = parse_type();
            consume(TOKEN_COLON, "Expect : after ret type");
            consume(TOKEN_ID, "Expect function name");
            
            Token name = parser.previous;
            Function* func = &current_compiler->functions[current_compiler->function_count++];
            
            if (prefix != NULL) {
                char* new_name = malloc(strlen(prefix) + name.length + 1);
                strcpy(new_name, prefix);
                strncat(new_name, name.start, name.length);
                func->name.start = new_name;
                func->name.length = (int)strlen(new_name);
            } else {
                func->name = name;
            }

            func->addr = current_chunk->count;
            func->return_type = return_type; func->param_count = param_count;
            func->is_public = is_public;
            for (int i = 0; i < param_count; i++) func->param_types[i] = param_types[i];
            int old_local_count = current_compiler->local_count;
            current_compiler->local_count = 0;
            for (int i = 0; i < param_count; i++) add_local(params[i], param_types[i]);
            for (int i = param_count - 1; i >= 0; i--) emit_bytes(OP_STORE, (uint8_t)i);
            block();
            ValueType actual_ret = type_pop();
            if (!is_assignable(return_type, actual_ret)) {
                error_at(&name, "Function return type mismatch.");
            }
            emit_byte(OP_RET);
            patch_int32(jump_over + 1, current_chunk->count);
            current_compiler->local_count = old_local_count;
            current_compiler->type_stack_ptr = 0;
            continue;
        }

        if (parser.current.type == TOKEN_STR) {
            Token path_token = parser.current;
            advance();
            if (match(TOKEN_ASSIGN)) {
                consume(TOKEN_ID, "Expect alias after =>");
                Token alias = parser.previous;
                consume(TOKEN_COLON, "Expect :");
                if (match(TOKEN_IMP)) {
                    handle_import(&path_token, &alias);
                    continue;
                }
            }
            error_at(&path_token, "Unexpected string at top level. Only imports are allowed.");
            continue;
        }

        error_at(&parser.current, "Expect function, struct, or import at top level.");
        advance();
    }
}

static void handle_import(Token* path_token, Token* alias_token) {
    char path[256];
    memcpy(path, path_token->start + 1, path_token->length - 2);
    path[path_token->length - 2] = '\0';

    char* full_path = resolve_path(path);
    
    for (int i = 0; i < compilation_stack_count; i++) {
        if (strcmp(compilation_stack[i], full_path) == 0) {
            error_at(path_token, "Circular import detected.");
            free(full_path);
            return;
        }
    }

    
    if (compiled_modules_count >= 64) {
        error_at(path_token, "Too many modules.");
        free(full_path);
        return;
    }
    compiled_modules[compiled_modules_count++] = full_path;
    compilation_stack[compilation_stack_count++] = full_path;

    char* source = compiler_read_file(full_path);
    if (source == NULL) {
        error_at(path_token, "Could not read imported file.");
        return;
    }

    char prefix[64];
    memcpy(prefix, alias_token->start, alias_token->length);
    prefix[alias_token->length] = '\0';
    strcat(prefix, ".");

    Lexer old_lexer = lexer;
    Parser old_parser = parser;
    const char* old_prefix = active_prefix;
    
    lexer_init(source);
    advance();
    active_prefix = prefix;
    
    compile_internal(prefix);
    
    lexer = old_lexer;
    parser = old_parser;
    active_prefix = old_prefix;
    compilation_stack_count--;
    free(source);
}

static void statement() {
    if (match(TOKEN_SEMICOLON)) {
        type_push(VAL_VOID);
        return;
    }
    int start_addr = current_chunk->count;
    expression();
    if (parser.current.type == TOKEN_QUESTION) {
        if (type_pop() != VAL_BOOL) error_at(&parser.previous, "Condition must be boolean.");
        advance();
        int jump_if_f_patch = current_chunk->count;
        emit_byte(OP_JUMP_IF_F);
        emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0); 
        if (parser.current.type == TOKEN_LBRACKET) block(); else statement();
        ValueType t1 = type_pop();
        int jump_patch = current_chunk->count;
        emit_byte(OP_JUMP);
        emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0); 
        patch_int32(jump_if_f_patch + 1, current_chunk->count);
        if (parser.current.type == TOKEN_COLON) {
            advance();
            if (parser.current.type == TOKEN_LBRACKET) block(); else statement();
            ValueType t2 = type_pop();
            if (t1 == t2) type_push(t1);
            else type_push(VAL_VOID);
        } else {
            type_push(VAL_VOID);
        }
        patch_int32(jump_patch + 1, current_chunk->count);
        match(TOKEN_SEMICOLON);
    } else if (parser.current.type == TOKEN_AT) {
        if (type_pop() != VAL_BOOL) error_at(&parser.previous, "Condition must be boolean.");
        advance();
        int jump_if_f_patch = current_chunk->count;
        emit_byte(OP_JUMP_IF_F);
        emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0);
        if (parser.current.type == TOKEN_LBRACKET) block(); else statement();
        type_pop();
        emit_byte(OP_JUMP);
        for (int i = 0; i < 4; i++) emit_byte((start_addr >> (i * 8)) & 0xFF);
        patch_int32(jump_if_f_patch + 1, current_chunk->count);
        match(TOKEN_SEMICOLON);
        type_push(VAL_VOID);
    } else {
        match(TOKEN_SEMICOLON);
    }
}

Chunk* compiler_compile(const char* source, const char* base_dir) {
    (void)base_dir; // Will use later
    lexer_init(source);
    current_chunk = malloc(sizeof(Chunk));
    current_chunk->code = NULL;
    current_chunk->count = 0;
    current_chunk->capacity = 0;
    current_chunk->strings = NULL;
    current_chunk->strings_count = 0;
    current_chunk->strings_capacity = 0;
    add_string("int", 3); add_string("flt", 3); add_string("bol", 3);
    add_string("str", 3); add_string("void", 4); add_string("fun", 3);

    compiled_modules_count = 0;
    compilation_stack_count = 0;
    active_prefix = NULL;

    current_compiler = malloc(sizeof(CompilerState));
    current_compiler->local_count = 0;
    current_compiler->function_count = 0;
    current_compiler->struct_count = 0;
    current_compiler->native_count = 0;
    current_compiler->scope_depth = 0;
    add_native("len", 0, VAL_INT, 1, VAL_OBJ);
    add_native("append", 1, VAL_OBJ, 2, VAL_OBJ, VAL_ANY);
    add_native("str", 2, VAL_STR, 1, VAL_ANY);
    add_native("readFile", 3, VAL_STR, 1, VAL_STR);
    add_native("writeFile", 4, VAL_BOOL, 2, VAL_STR, VAL_STR);
    add_native("args", 5, VAL_OBJ, 0);
    add_native("int", 6, VAL_INT, 1, VAL_ANY);
    add_native("print", 7, VAL_VOID, 1, VAL_ANY);
    add_native("println", 8, VAL_VOID, 1, VAL_ANY);
    add_native("readLine", 9, VAL_STR, 0);

    parser.had_error = false;
    parser.panic_mode = false;
    root_base_dir = (char*)base_dir;
    advance();

    compile_internal(NULL);

    int main_idx = -1;
    for (int i = 0; i < current_compiler->function_count; i++) {
        if (current_compiler->functions[i].name.length == 4 &&
            memcmp(current_compiler->functions[i].name.start, "main", 4) == 0) {
            main_idx = i; break;
        }
    }

    if (main_idx != -1) {
        emit_byte(OP_CALL);
        patch_int32(current_chunk->count, current_compiler->functions[main_idx].addr);
        current_chunk->count += 4;
    }
    emit_byte(OP_HALT);
    if (parser.had_error) { chunk_free(current_chunk); free(current_compiler); return NULL; }
    free(current_compiler);
    return current_chunk;
}

void chunk_free(Chunk* chunk) {
    if (chunk) {
        free(chunk->code);
        for (int i = 0; i < chunk->strings_count; i++) free(chunk->strings[i]);
        free(chunk->strings);
        free(chunk);
    }
}
