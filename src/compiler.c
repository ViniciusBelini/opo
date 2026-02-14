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
    Type type;
    int depth;
    int guarded_depth;
    int guarded_variant;
} Local;

typedef struct {
    Token name;
    int addr;
    Type return_type;
    Type param_types[16];
    int param_count;
    bool is_public;
} Function;

typedef struct Loop {
    int start_addr;
    int end_jump_patches[64];
    int end_jump_count;
    struct Loop* next;
} Loop;

typedef struct {
    Token name;
    Token fields[16];
    Type field_types[16];
    int field_count;
    bool is_public;
} StructDef;

typedef struct {
    Token name;
    Token variants[16];
    Type payload_types[16];
    bool has_payload[16];
    int variant_count;
    bool is_public;
} EnumDef;

typedef struct {
    Token name;
    int index;
    Type return_type;
    Type param_types[8];
    int param_count;
} Native;

typedef struct {
    Local locals[256];
    int local_count;
    Function functions[256];
    int function_count;
    StructDef structs[64];
    int struct_count;
    EnumDef enums[64];
    int enum_count;
    Native natives[64];
    int native_count;
    int scope_depth;
    Loop* current_loop;
    Type current_return_type;
    Type type_stack[STACK_MAX];
    int local_stack[STACK_MAX];
    int type_stack_ptr;
    bool is_go;
} CompilerState;

Parser parser;
CompilerState* current_compiler = NULL;
Chunk* current_chunk = NULL;
static const char* active_prefix = NULL;

static void error_at(Token* token, const char* message) {
    if (parser.panic_mode) return;
    parser.panic_mode = true;
    fprintf(stderr, "[%s:line %d] Error", active_prefix ? active_prefix : "main", token->line);
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

static void emit_int32(int32_t value) {
    for (int i = 0; i < 4; i++) {
        emit_byte((value >> (i * 8)) & 0xFF);
    }
}

static void emit_int(int64_t val) {
    emit_byte(OP_PUSH_INT);
    for (int i = 0; i < 8; i++) {
        emit_byte((val >> (i * 8)) & 0xFF);
    }
}

static void emit_push_func(int64_t addr, Type type) {
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
static char* std_base_dir = NULL;
static char* compiled_modules[64];
static int compiled_modules_count = 0;
static char* compilation_stack[64];
static int compilation_stack_count = 0;

static char* resolve_path(const char* rel_path) {
    char* resolved = malloc(1024);
    if (strncmp(rel_path, "std/", 4) == 0) {
        if (strstr(rel_path, ".opo") == NULL) {
            sprintf(resolved, "%s/%s.opo", std_base_dir, rel_path);
        } else {
            sprintf(resolved, "%s/%s", std_base_dir, rel_path);
        }
    } else if (rel_path[0] == '/') {
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

static bool is_assignable(Type expected, Type actual) {
    if (TYPE_KIND(expected) == VAL_ANY) return true;
    if (TYPE_KIND(actual) == VAL_ANY && TYPE_KIND(expected) == VAL_ANY) return true;
    if (TYPE_KIND(actual) == VAL_ANY) return false;
    if (expected == actual) return true;
    
    // Function compatibility
    if (TYPE_KIND(expected) == VAL_FUNC && TYPE_KIND(actual) >= VAL_FUNC && TYPE_KIND(actual) <= VAL_FUNC_VOID) return true;
    if (TYPE_KIND(actual) == VAL_FUNC && TYPE_KIND(expected) >= VAL_FUNC && TYPE_KIND(expected) <= VAL_FUNC_VOID) return true;
    
    // Object compatibility
    if (TYPE_KIND(expected) == VAL_OBJ) {
        if (TYPE_KIND(actual) == VAL_STR || TYPE_KIND(actual) == VAL_MAP || TYPE_KIND(actual) == VAL_OBJ) {
            if (TYPE_SUB(expected) == 0 || TYPE_SUB(expected) == VAL_ANY || TYPE_SUB(actual) == VAL_ANY) return true;
            return TYPE_SUB(expected) == TYPE_SUB(actual);
        }
    }
    
    // Map compatibility
    if (TYPE_KIND(expected) == VAL_MAP) {
        if (TYPE_KIND(actual) == VAL_OBJ && TYPE_SUB(expected) == 0) return true; // Legacy
        if (TYPE_KIND(actual) == VAL_MAP) {
            if (TYPE_SUB(expected) == 0 || TYPE_SUB(expected) == VAL_ANY) return true;
            return TYPE_SUB(expected) == TYPE_SUB(actual) && TYPE_KEY(expected) == TYPE_KEY(actual);
        }
    }

    // Enum/Option compatibility
    if (TYPE_KIND(expected) == VAL_ENUM && TYPE_KIND(actual) == VAL_ENUM) {
        if (TYPE_SUB(expected) == OPTION_ENUM_ID && TYPE_SUB(actual) == OPTION_ENUM_ID) {
            if (TYPE_KEY(expected) == VAL_ANY || TYPE_KEY(actual) == VAL_ANY) return true;
            return TYPE_KEY(expected) == TYPE_KEY(actual);
        }
        return TYPE_SUB(expected) == TYPE_SUB(actual);
    }

    return false;
}

static void add_native(const char* name, int index, Type ret, int p_count, ...) {
    Native* n = &current_compiler->natives[current_compiler->native_count++];
    n->name.start = name;
    n->name.length = (int)strlen(name);
    n->index = index;
    n->return_type = ret;
    n->param_count = p_count;
    va_list args;
    va_start(args, p_count);
    for (int i = 0; i < p_count; i++) {
        n->param_types[i] = va_arg(args, Type);
    }
    va_end(args);
}

static void add_local(Token name, Type type) {
    if (current_compiler->local_count == 256) {
        error_at(&name, "Too many local variables.");
        return;
    }
    Local* local = &current_compiler->locals[current_compiler->local_count++];
    local->name = name;
    local->type = type;
    local->depth = current_compiler->scope_depth;
    local->guarded_depth = 0;
    local->guarded_variant = -1;
}

static int next_push_local = -1;
static Type type_push(Type type) {
    if (current_compiler->type_stack_ptr >= STACK_MAX) {
        error_at(&parser.current, "Compile-time type stack overflow.");
    }
    current_compiler->local_stack[current_compiler->type_stack_ptr] = next_push_local;
    current_compiler->type_stack[current_compiler->type_stack_ptr++] = type;
    next_push_local = -1;
    return type;
}

static int popped_local = -1;
static Type type_pop() {
    if (current_compiler->type_stack_ptr <= 0) {
        error_at(&parser.current, "Compile-time type stack underflow.");
        popped_local = -1;
        return VAL_VOID;
    }
    current_compiler->type_stack_ptr--;
    popped_local = current_compiler->local_stack[current_compiler->type_stack_ptr];
    return current_compiler->type_stack[current_compiler->type_stack_ptr];
}

static Type parse_type() {
    Type type = VAL_VOID;
    if (match(TOKEN_LBRACKET)) {
        consume(TOKEN_RBRACKET, "Expect ']' after '[' for array type.");
        Type element = parse_type();
        type = MAKE_TYPE(VAL_OBJ, TYPE_KIND(element), 0);
    } else if (match(TOKEN_LBRACE)) {
        Type key = parse_type(); // key type
        consume(TOKEN_COLON, "Expect ':' after key type in map type.");
        Type value = parse_type(); // value type
        consume(TOKEN_RBRACE, "Expect '}' after map type.");
        type = MAKE_TYPE(VAL_MAP, TYPE_KIND(value), TYPE_KIND(key));
    } else if (match(TOKEN_LANGLE)) {
        while (parser.current.type != TOKEN_RANGLE && parser.current.type != TOKEN_EOF) {
            parse_type();
            if (parser.current.type == TOKEN_COMMA) advance();
        }
        consume(TOKEN_RANGLE, "Expect '>' after function type parameters.");
        consume(TOKEN_ARROW, "Expect '->' after function type parameters.");
        Type ret = parse_type(); // Return type
        switch (ret) {
            case VAL_INT: type = VAL_FUNC_INT; break;
            case VAL_FLT: type = VAL_FUNC_FLT; break;
            case VAL_BOOL: type = VAL_FUNC_BOOL; break;
            case VAL_STR: type = VAL_FUNC_STR; break;
            case VAL_VOID: type = VAL_FUNC_VOID; break;
            default: type = VAL_FUNC; break;
        }
    } else {
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
            
            bool found = false;
            for (int i = 0; i < current_compiler->struct_count; i++) {
                if (current_compiler->structs[i].name.length == (int)strlen(full_name) &&
                    memcmp(current_compiler->structs[i].name.start, full_name, strlen(full_name)) == 0) {
                    type = VAL_OBJ;
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (int i = 0; i < current_compiler->enum_count; i++) {
                    if (current_compiler->enums[i].name.length == (int)strlen(full_name) &&
                        memcmp(current_compiler->enums[i].name.start, full_name, strlen(full_name)) == 0) {
                        type = MAKE_TYPE(VAL_ENUM, i, 0);
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                error_at(&member, "Unknown type in namespace.");
                return VAL_VOID;
            }
        } else if (t.type == TOKEN_IMP) type = VAL_IMP;
        else if (t.type == TOKEN_TYPE) type = VAL_VOID;
        else if (t.length == 3 && memcmp(t.start, "int", 3) == 0) type = VAL_INT;
        else if (t.length == 3 && memcmp(t.start, "flt", 3) == 0) type = VAL_FLT;
        else if (t.length == 3 && memcmp(t.start, "bol", 3) == 0) type = VAL_BOOL;
        else if (t.length == 3 && memcmp(t.start, "str", 3) == 0) type = VAL_STR;
        else if (t.length == 3 && memcmp(t.start, "err", 3) == 0) type = VAL_ERR;
        else if (t.length == 4 && memcmp(t.start, "void", 4) == 0) type = VAL_VOID;
        else if (t.length == 3 && memcmp(t.start, "fun", 3) == 0) type = VAL_FUNC;
        else if (t.length == 3 && memcmp(t.start, "any", 3) == 0) type = VAL_ANY;
        else if (t.length == 4 && memcmp(t.start, "chan", 4) == 0) {
            consume(TOKEN_LANGLE, "Expect '<' after 'chan' type.");
            Type element = parse_type();
            consume(TOKEN_RANGLE, "Expect '>' after chan element type.");
            type = MAKE_TYPE(VAL_CHAN, TYPE_KIND(element), 0);
        } else if (t.length == 6 && memcmp(t.start, "Option", 6) == 0) {
            if (match(TOKEN_LANGLE)) {
                Type inner = parse_type();
                consume(TOKEN_RANGLE, "Expect '>' after Option type parameter.");
                type = MAKE_TYPE(VAL_ENUM, OPTION_ENUM_ID, TYPE_KIND(inner));
            } else {
                type = MAKE_TYPE(VAL_ENUM, OPTION_ENUM_ID, VAL_ANY);
            }
        } else {
            bool found = false;
            for (int i = 0; i < current_compiler->struct_count; i++) {
                if (t.length == current_compiler->structs[i].name.length &&
                    memcmp(t.start, current_compiler->structs[i].name.start, t.length) == 0) {
                    type = VAL_OBJ;
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (int i = 0; i < current_compiler->enum_count; i++) {
                    if (t.length == current_compiler->enums[i].name.length &&
                        memcmp(t.start, current_compiler->enums[i].name.start, t.length) == 0) {
                        type = MAKE_TYPE(VAL_ENUM, i, 0);
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                error_at(&t, "Unknown type.");
                return VAL_VOID;
            }
        }
    }

    while (match(TOKEN_QUESTION)) {
        type = MAKE_TYPE(VAL_ENUM, OPTION_ENUM_ID, TYPE_KIND(type));
    }

    return type;
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

    Type b = type_pop();
    Type a = type_pop();

    switch (operator_type) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
            if (operator_type == TOKEN_PLUS && a == VAL_STR && b == VAL_STR) {
                emit_byte(OP_ADD);
                type_push(VAL_STR);
            } else {
                if (a != b) {
                    error_at(&parser.previous, "Arithmetic type error.");
                }
                if (a == VAL_ANY) {
                    error_at(&parser.previous, "Cannot use 'any' in arithmetic. Match it first.");
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
            if (a == VAL_ANY && operator_type != TOKEN_EQ_EQ && operator_type != TOKEN_BANG_EQ) {
                error_at(&parser.previous, "Cannot compare 'any' values. Match them first.");
            }
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
    Type t = type_pop();
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

static void throw_op() {
    expression();
    emit_byte(OP_THROW);
    // throw doesn't really "return" a value in the normal sense, it aborts or jumps.
    // But for the type stack, let's say it results in VAL_VOID.
    type_pop();
    type_push(VAL_VOID);
}

static void array_literal() {
    int count = 0;
    Type element_type = VAL_ANY;
    if (parser.current.type != TOKEN_RBRACKET) {
        do {
            expression();
            Type t = type_pop();
            if (count == 0) element_type = t;
            else if (!is_assignable(element_type, t) && TYPE_KIND(t) != VAL_VOID) {
                error_at(&parser.previous, "All elements in an array literal must have the same type.");
            }
            count++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RBRACKET, "Expect ']' after array elements.");
    Type full_type = MAKE_TYPE(VAL_OBJ, TYPE_KIND(element_type), 0);
    emit_byte(OP_ARRAY);
    emit_int32(full_type);
    emit_byte((uint8_t)count);
    type_push(full_type);
}

static void map_literal() {
    int count = 0;
    Type key_type = VAL_ANY;
    Type val_type = VAL_ANY;
    if (parser.current.type != TOKEN_RBRACE) {
        do {
            parse_precedence(PREC_OR);
            Type kt = type_pop();
            if (count == 0) key_type = kt;
            else if (!is_assignable(key_type, kt)) error_at(&parser.previous, "All keys in a map literal must have the same type.");
            
            consume(TOKEN_ASSIGN, "Expect '=>' between key and value in map literal.");
            
            parse_precedence(PREC_OR);
            Type vt = type_pop();
            if (count == 0) val_type = vt;
            else if (!is_assignable(val_type, vt)) error_at(&parser.previous, "All values in a map literal must have the same type.");
            
            count++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RBRACE, "Expect '}' after map elements.");
    Type full_type = MAKE_TYPE(VAL_MAP, TYPE_KIND(val_type), TYPE_KIND(key_type));
    emit_byte(OP_MAP);
    emit_int32(full_type);
    emit_byte((uint8_t)count);
    type_push(full_type);
}

static void dot() {
    Type lhs_type = type_pop();
    if (match(TOKEN_INT)) {
        if (TYPE_KIND(lhs_type) == VAL_MAP && TYPE_KEY(lhs_type) != VAL_INT && TYPE_KEY(lhs_type) != VAL_ANY) {
            error_at(&parser.previous, "Map key type mismatch.");
        }
        int64_t idx = strtoll(parser.previous.start, NULL, 10);
        emit_int(idx);
        emit_byte(OP_INDEX);
        if (TYPE_KIND(lhs_type) == VAL_OBJ || TYPE_KIND(lhs_type) == VAL_MAP) type_push(TYPE_SUB(lhs_type));
        else type_push(VAL_ANY);
    } else if (match(TOKEN_STR)) {
        if (TYPE_KIND(lhs_type) == VAL_MAP && TYPE_KEY(lhs_type) != VAL_STR && TYPE_KEY(lhs_type) != VAL_ANY) {
            error_at(&parser.previous, "Map key type mismatch.");
        }
        int idx = add_string(parser.previous.start + 1, parser.previous.length - 2);
        emit_bytes(OP_PUSH_STR, (uint8_t)idx);
        emit_byte(OP_INDEX);
        if (TYPE_KIND(lhs_type) == VAL_MAP) type_push(TYPE_SUB(lhs_type));
        else type_push(VAL_ANY);
    } else if (match(TOKEN_LPAREN)) {
        expression();
        consume(TOKEN_RPAREN, "Expect ')' after expression in dot access.");
        Type idx_type = type_pop();
        if (TYPE_KIND(lhs_type) == VAL_MAP && !is_assignable(TYPE_KEY(lhs_type), idx_type)) {
            error_at(&parser.previous, "Map key type mismatch.");
        }
        emit_byte(OP_INDEX);
        if (TYPE_KIND(lhs_type) == VAL_OBJ || TYPE_KIND(lhs_type) == VAL_MAP) type_push(TYPE_SUB(lhs_type));
        else type_push(VAL_ANY);
    } else {
        if (parser.current.type != TOKEN_ID && parser.current.type != TOKEN_SOME) {
            error_at(&parser.current, "Expect member name after '.'.");
        }
        advance();
        Token name = parser.previous;
        int field_idx = -1;
        int struct_idx = -1;
        for (int i = 0; i < current_compiler->struct_count; i++) {
            for (int j = 0; j < current_compiler->structs[i].field_count; j++) {
                if (name.length == current_compiler->structs[i].fields[j].length &&
                    memcmp(name.start, current_compiler->structs[i].fields[j].start, name.length) == 0) {
                    field_idx = j;
                    struct_idx = i;
                    break;
                }
            }
            if (field_idx != -1) break;
        }

        if (field_idx != -1) {
            emit_bytes(OP_GET_MEMBER, (uint8_t)field_idx);
            type_push(current_compiler->structs[struct_idx].field_types[field_idx]);
        } else if (TYPE_KIND(lhs_type) == VAL_ENUM) {
            int enum_id = TYPE_SUB(lhs_type);
            int variant_idx = -1;
            Type payload_type = VAL_VOID;

            if (enum_id == OPTION_ENUM_ID) {
                if (name.length == 4 && memcmp(name.start, "some", 4) == 0) {
                    variant_idx = 1;
                    payload_type = MAKE_TYPE(TYPE_KEY(lhs_type), 0, 0);
                }
            } else {
                EnumDef* ed = &current_compiler->enums[enum_id];
                for (int v = 0; v < ed->variant_count; v++) {
                    if (ed->variants[v].length == name.length &&
                        memcmp(ed->variants[v].start, name.start, name.length) == 0) {
                        variant_idx = v;
                        payload_type = ed->payload_types[v];
                        break;
                    }
                }
            }

            if (variant_idx != -1) {
                int local_idx = popped_local;
                if (local_idx == -1 || current_compiler->locals[local_idx].guarded_depth == 0 ||
                    current_compiler->locals[local_idx].guarded_variant != variant_idx) {
                    error_at(&name, "Unsafe unwrap of Enum variant. Use 'match' or existence check.");
                }
                emit_byte(OP_EXTRACT_ENUM_PAYLOAD);
                type_push(payload_type);
            } else {
                error_at(&name, "Unknown Enum variant.");
            }
        } else {
            // Try as a variable for dynamic indexing
            int arg = resolve_local(current_compiler, &name);
            if (arg != -1) {
                emit_bytes(OP_LOAD, (uint8_t)arg);
                emit_byte(OP_INDEX);
                if (TYPE_KIND(lhs_type) == VAL_OBJ || TYPE_KIND(lhs_type) == VAL_MAP) type_push(TYPE_SUB(lhs_type));
                else type_push(VAL_ANY);
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

static void some_expr() {
    consume(TOKEN_LPAREN, "Expect '(' after 'some'.");
    expression();
    Type val_type = type_pop();
    consume(TOKEN_RPAREN, "Expect ')' after 'some' argument.");
    
    Type type = MAKE_TYPE(VAL_ENUM, OPTION_ENUM_ID, TYPE_KIND(val_type));
    emit_byte(OP_ENUM_VARIANT);
    emit_int32(type);
    emit_byte(1); // some is variant 1
    emit_byte(1); // has payload
    
    type_push(type);
}

static void none_expr() {
    Type type = MAKE_TYPE(VAL_ENUM, OPTION_ENUM_ID, VAL_ANY);
    emit_byte(OP_ENUM_VARIANT);
    emit_int32(type);
    emit_byte(0); // none is variant 0
    emit_byte(0); // no payload
    
    type_push(type);
}

static void chan_expr() {
    consume(TOKEN_LANGLE, "Expect '<' after 'chan'.");
    Type element = parse_type();
    consume(TOKEN_RANGLE, "Expect '>' after chan element type.");
    consume(TOKEN_LPAREN, "Expect '(' for channel capacity.");
    expression();
    Type cap_type = type_pop();
    if (cap_type != VAL_INT && cap_type != VAL_ANY) error_at(&parser.previous, "Channel capacity must be an integer.");
    consume(TOKEN_RPAREN, "Expect ')' after channel capacity.");
    
    Type full_type = MAKE_TYPE(VAL_CHAN, TYPE_KIND(element), 0);
    emit_byte(OP_CHAN);
    emit_int32(full_type);
    type_push(full_type);
}

static void unary_larrow() {
    parse_precedence(PREC_UNARY);
    Type t = type_pop();
    if (TYPE_KIND(t) != VAL_CHAN && TYPE_KIND(t) != VAL_ANY) {
        error_at(&parser.previous, "Can only receive from a channel.");
    }
    emit_byte(OP_RECV);
    type_push(TYPE_SUB(t));
}

static void binary_larrow() {
    Type ch_type = type_pop();
    if (TYPE_KIND(ch_type) != VAL_CHAN && TYPE_KIND(ch_type) != VAL_ANY) {
        error_at(&parser.previous, "LHS of <- must be a channel.");
    }
    parse_precedence(PREC_ASSIGNMENT);
    Type val_type = type_pop();
    if (TYPE_KIND(ch_type) != VAL_ANY && !is_assignable(TYPE_SUB(ch_type), val_type)) {
        error_at(&parser.previous, "Type mismatch in channel send.");
    }
    emit_byte(OP_SEND);
    type_push(VAL_VOID);
}

static void variable() {
    Token name = parser.previous;

    // 1. Try Local
    int arg = resolve_local(current_compiler, &name);
    if (arg != -1) {
        Type type = current_compiler->locals[arg].type;
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
            if (current_compiler->is_go) {
                emit_bytes(OP_GO, (uint8_t)arg_count);
            } else {
                emit_bytes(OP_INVOKE, (uint8_t)arg_count);
            }
            Type ret_type = VAL_OBJ;
            if (type == VAL_FUNC_INT) ret_type = VAL_INT;
            else if (type == VAL_FUNC_FLT) ret_type = VAL_FLT;
            else if (type == VAL_FUNC_BOOL) ret_type = VAL_BOOL;
            else if (type == VAL_FUNC_STR) ret_type = VAL_STR;
            else if (type == VAL_FUNC_VOID) ret_type = VAL_VOID;
            type_push(ret_type);
        } else {
            emit_bytes(OP_LOAD, (uint8_t)arg);
            next_push_local = arg;
            type_push(type);
        }
        return;
    }

    // 2. Try Namespace Access
    if (match(TOKEN_DOT)) {
        consume(TOKEN_ID, "Expect member name after '.'.");
        Token member = parser.previous;

        // Check if it's an Enum variant
        for (int i = 0; i < current_compiler->enum_count; i++) {
            EnumDef* ed = &current_compiler->enums[i];
            if (ed->name.length == name.length &&
                memcmp(ed->name.start, name.start, name.length) == 0) {
                
                for (int v = 0; v < ed->variant_count; v++) {
                    if (ed->variants[v].length == member.length &&
                        memcmp(ed->variants[v].start, member.start, member.length) == 0) {
                        
                        if (ed->has_payload[v]) {
                            consume(TOKEN_LPAREN, "Expect '(' for variant payload.");
                            expression();
                            Type p_type = type_pop();
                            if (!is_assignable(ed->payload_types[v], p_type)) {
                                error_at(&parser.previous, "Variant payload type mismatch.");
                            }
                            consume(TOKEN_RPAREN, "Expect ')' after variant payload.");
                            
                            emit_byte(OP_ENUM_VARIANT);
                            emit_int32(MAKE_TYPE(VAL_ENUM, i, 0)); 
                            emit_byte((uint8_t)v); 
                            emit_byte(1); // Has payload
                        } else {
                            emit_byte(OP_ENUM_VARIANT);
                            emit_int32(MAKE_TYPE(VAL_ENUM, i, 0));
                            emit_byte((uint8_t)v);
                            emit_byte(0); // No payload
                        }
                        type_push(MAKE_TYPE(VAL_ENUM, i, 0));
                        return;
                    }
                }
            }
        }
        
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
                            Type arg_type = type_pop();
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
                    if (current_compiler->is_go) {
                        emit_push_func(f->addr, VAL_FUNC);
                        emit_bytes(OP_GO, (uint8_t)arg_count);
                    } else {
                        emit_byte(OP_CALL);
                        emit_int32(f->addr);
                    }
                    type_push(f->return_type);
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
                            Type arg_type = type_pop();
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
            Type first_arg_type = VAL_ANY;
            if (parser.current.type != TOKEN_RPAREN) {
                do {
                    expression();
                    Type arg_type = type_pop();
                    if (arg_count == 0) first_arg_type = arg_type;
                    
                    if (arg_count < n->param_count) {
                        Type expected = n->param_types[arg_count];
                        if (n_idx == 1 && arg_count == 1) { // append
                            if (TYPE_SUB(first_arg_type) != 0) expected = TYPE_SUB(first_arg_type);
                        }
                        if (!is_assignable(expected, arg_type)) {
                            error_at(&parser.previous, "Native function argument type mismatch.");
                        }
                    }
                    arg_count++;
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expect ')' after arguments.");
            if (n_idx == 29) { // ffiCall
                if (arg_count < n->param_count) error_at(&name, "ffiCall requires at least 4 arguments.");
            } else if (arg_count != n->param_count) {
                error_at(&name, "Wrong number of arguments for native function.");
            }
            emit_bytes(OP_LOAD_G, (uint8_t)n_idx);
            if (current_compiler->is_go) {
                emit_bytes(OP_GO, (uint8_t)arg_count);
            } else {
                emit_bytes(OP_INVOKE, (uint8_t)arg_count);
            }
            if (n_idx == 1) type_push(first_arg_type); // append returns its first arg type
            else type_push(n->return_type);
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
                    Type arg_type = type_pop();
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
                    Type arg_type = type_pop();
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
            if (current_compiler->is_go) {
                emit_push_func(f->addr, VAL_FUNC);
                emit_bytes(OP_GO, (uint8_t)arg_count);
            } else {
                emit_byte(OP_CALL);
                emit_int32(f->addr);
            }
            type_push(f->return_type);
        } else {
            Type f_type = VAL_FUNC;
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
        Type declared_type = parse_type();
        Type value_type = type_pop();
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
        Type lhs_type = current_compiler->locals[arg].type;
        emit_bytes(OP_LOAD, (uint8_t)arg);
        Type val_type = type_pop();
        if (match(TOKEN_INT)) {
            if (TYPE_KIND(lhs_type) == VAL_OBJ && !is_assignable(TYPE_SUB(lhs_type), val_type)) {
                error_at(&name, "Type mismatch in array assignment.");
            }
            if (TYPE_KIND(lhs_type) == VAL_MAP && TYPE_KEY(lhs_type) != VAL_INT && TYPE_KEY(lhs_type) != VAL_ANY) {
                error_at(&name, "Map key type mismatch.");
            }
            int64_t idx = strtoll(parser.previous.start, NULL, 10);
            emit_int(idx);
            emit_byte(OP_SET_INDEX);
            type_push(VAL_VOID);
        } else if (match(TOKEN_STR)) {
            if (TYPE_KIND(lhs_type) == VAL_MAP && !is_assignable(TYPE_SUB(lhs_type), val_type)) {
                error_at(&name, "Type mismatch in map assignment.");
            }
            if (TYPE_KIND(lhs_type) == VAL_MAP && TYPE_KEY(lhs_type) != VAL_STR && TYPE_KEY(lhs_type) != VAL_ANY) {
                error_at(&name, "Map key type mismatch.");
            }
            int idx = add_string(parser.previous.start + 1, parser.previous.length - 2);
            emit_bytes(OP_PUSH_STR, (uint8_t)idx);
            emit_byte(OP_SET_INDEX);
            type_push(VAL_VOID);
        } else if (match(TOKEN_LPAREN)) {
            expression();
            consume(TOKEN_RPAREN, "Expect ')' after expression.");
            Type idx_type = type_pop();
            if (TYPE_KIND(lhs_type) == VAL_MAP && !is_assignable(TYPE_KEY(lhs_type), idx_type)) {
                error_at(&name, "Map key type mismatch.");
            }
            if ((TYPE_KIND(lhs_type) == VAL_OBJ || TYPE_KIND(lhs_type) == VAL_MAP) && !is_assignable(TYPE_SUB(lhs_type), val_type)) {
                error_at(&name, "Type mismatch in assignment.");
            }
            emit_byte(OP_SET_INDEX);
            type_push(VAL_VOID);
        } else {
            consume(TOKEN_ID, "Expect member name.");
            Token field_name = parser.previous;
            int field_idx = -1;
            int struct_idx = -1;
            for (int i = 0; i < current_compiler->struct_count; i++) {
                for (int j = 0; j < current_compiler->structs[i].field_count; j++) {
                    if (field_name.length == current_compiler->structs[i].fields[j].length &&
                        memcmp(field_name.start, current_compiler->structs[i].fields[j].start, field_name.length) == 0) {
                        field_idx = j;
                        struct_idx = i;
                        break;
                    }
                }
                if (field_idx != -1) break;
            }
            
            if (field_idx != -1) {
                if (!is_assignable(current_compiler->structs[struct_idx].field_types[field_idx], val_type)) {
                    error_at(&field_name, "Type mismatch in struct field assignment.");
                }
                emit_bytes(OP_SET_MEMBER, (uint8_t)field_idx);
                type_push(VAL_VOID);
            } else {
                // Try as dynamic index
                int idx_arg = resolve_local(current_compiler, &field_name);
                if (idx_arg != -1) {
                    if ((TYPE_KIND(lhs_type) == VAL_OBJ || TYPE_KIND(lhs_type) == VAL_MAP) && !is_assignable(TYPE_SUB(lhs_type), val_type)) {
                        error_at(&name, "Type mismatch in assignment.");
                    }
                    emit_bytes(OP_LOAD, (uint8_t)idx_arg);
                    emit_byte(OP_SET_INDEX);
                    type_push(VAL_VOID);
                } else {
                    error_at(&field_name, "Unknown struct field or index variable.");
                }
            }
        }
    } else {
        int arg = resolve_local(current_compiler, &name);
        if (arg == -1) error_at(&name, "Undefined identifier.");
        Type value_type = type_pop();
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

static void break_op() {
    if (current_compiler->current_loop == NULL) {
        error_at(&parser.previous, "Cannot use '.' (break) outside of a loop.");
        return;
    }
    Loop* loop = current_compiler->current_loop;
    if (loop->end_jump_count >= 64) {
        error_at(&parser.previous, "Too many breaks in loop.");
        return;
    }
    loop->end_jump_patches[loop->end_jump_count++] = current_chunk->count;
    emit_byte(OP_JUMP);
    emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0);
    type_push(VAL_VOID);
}

static void continue_op() {
    if (current_compiler->current_loop == NULL) {
        error_at(&parser.previous, "Cannot use '..' (continue) outside of a loop.");
        return;
    }
    emit_byte(OP_JUMP);
    emit_int32(current_compiler->current_loop->start_addr);
    type_push(VAL_VOID);
}

static void return_op() {
    Type t = VAL_VOID;
    if (parser.current.type == TOKEN_RBRACKET || parser.current.type == TOKEN_SEMICOLON) {
        if (!is_assignable(current_compiler->current_return_type, VAL_VOID)) {
            error_at(&parser.previous, "Must return a value in non-void function.");
        }
        emit_int(0);
        t = VAL_VOID;
    } else {
        expression();
        t = type_pop();
        if (!is_assignable(current_compiler->current_return_type, t)) {
            error_at(&parser.previous, "Return type mismatch.");
        }
    }
    emit_byte(OP_RET);
    type_push(t);
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
    [TOKEN_L_ARROW]   = {unary_larrow,  binary_larrow, PREC_ASSIGNMENT},
    [TOKEN_DOT]       = {break_op, dot,        PREC_CALL},
    [TOKEN_DOT_DOT]   = {continue_op, NULL,    PREC_NONE},
    [TOKEN_HAT]       = {return_op, NULL,      PREC_NONE},
    [TOKEN_ASSIGN]    = {NULL,     assignment, PREC_ASSIGNMENT},
    [TOKEN_THROW]     = {throw_op, NULL,       PREC_NONE},
    [TOKEN_SOME]      = {some_expr, NULL,      PREC_NONE},
    [TOKEN_NONE]      = {none_expr, NULL,      PREC_NONE},
    [TOKEN_CHAN]      = {chan_expr, NULL,      PREC_NONE},
    [TOKEN_ENUM]      = {NULL,     NULL,       PREC_NONE},
    [TOKEN_MATCH]     = {NULL,     NULL,       PREC_NONE},
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
            Type t = type_pop();
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

static Type type_from_name(Token t) {
    if (t.length == 3 && memcmp(t.start, "int", 3) == 0) return VAL_INT;
    if (t.length == 3 && memcmp(t.start, "flt", 3) == 0) return VAL_FLT;
    if (t.length == 3 && memcmp(t.start, "bol", 3) == 0) return VAL_BOOL;
    if (t.length == 3 && memcmp(t.start, "str", 3) == 0) return VAL_STR;
    if (t.length == 4 && memcmp(t.start, "void", 4) == 0) return VAL_VOID;
    if (t.length == 3 && memcmp(t.start, "any", 3) == 0) return VAL_ANY;
    if (t.length == 3 && memcmp(t.start, "err", 3) == 0) return VAL_ERR;
    if (t.length == 3 && memcmp(t.start, "fun", 3) == 0) return VAL_FUNC;
    if (t.length == 4 && memcmp(t.start, "chan", 4) == 0) return VAL_CHAN;
    if (t.length == 4 && memcmp(t.start, "list", 4) == 0) return MAKE_TYPE(VAL_OBJ, VAL_ANY, 0);
    if (t.length == 3 && memcmp(t.start, "map", 3) == 0) return MAKE_TYPE(VAL_MAP, VAL_ANY, VAL_ANY);
    return VAL_NONE;
}

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

        if (match(TOKEN_ENUM)) {
            consume(TOKEN_LBRACKET, "Expect '[' after 'enum'.");
            EnumDef* ed = &current_compiler->enums[current_compiler->enum_count++];
            ed->variant_count = 0;
            ed->is_public = is_public;
            while (parser.current.type != TOKEN_RBRACKET) {
                consume(TOKEN_ID, "Expect variant name.");
                ed->variants[ed->variant_count] = parser.previous;
                if (match(TOKEN_LPAREN)) {
                    ed->has_payload[ed->variant_count] = true;
                    ed->payload_types[ed->variant_count] = parse_type();
                    consume(TOKEN_RPAREN, "Expect ')' after variant payload type.");
                } else {
                    ed->has_payload[ed->variant_count] = false;
                    ed->payload_types[ed->variant_count] = VAL_VOID;
                }
                ed->variant_count++;
                if (parser.current.type == TOKEN_COMMA) advance();
            }
            consume(TOKEN_RBRACKET, "Expect ']' after enum variants.");
            consume(TOKEN_ASSIGN, "Expect '=>' after enum definition.");
            consume(TOKEN_ID, "Expect enum name.");
            
            Token name = parser.previous;
            if (prefix != NULL) {
                char* new_name = malloc(strlen(prefix) + name.length + 1);
                strcpy(new_name, prefix);
                strncat(new_name, name.start, name.length);
                ed->name.start = new_name;
                ed->name.length = (int)strlen(new_name);
            } else {
                ed->name = name;
            }

            consume(TOKEN_COLON, "Expect ':'.");
            consume(TOKEN_TYPE, "Expect 'type' after enum name.");
            continue;
        }

        if (parser.current.type == TOKEN_LANGLE) {
            int jump_over = current_chunk->count;
            emit_byte(OP_JUMP);
            emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0);
            advance();
            Token params[16]; Type param_types[16]; int param_count = 0;
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
            Type return_type = parse_type();
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
            
            Type old_return_type = current_compiler->current_return_type;
            current_compiler->current_return_type = return_type;
            
            block();
            Type actual_ret = type_pop();
            if (!is_assignable(return_type, actual_ret)) {
                error_at(&name, "Function return type mismatch.");
            }
            emit_byte(OP_RET);
            patch_int32(jump_over + 1, current_chunk->count);
            current_compiler->local_count = old_local_count;
            current_compiler->current_return_type = old_return_type;
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
    if (match(TOKEN_MATCH)) {
        expression();
        Type value_type = type_pop();
        int matched_local = popped_local;

        if (TYPE_KIND(value_type) != VAL_ENUM && TYPE_KIND(value_type) != VAL_ANY) {
            error_at(&parser.previous, "Can only match on Enums, Options, or 'any'.");
        }
        
        consume(TOKEN_LBRACKET, "Expect '[' after match expression.");
        
        int enum_id = TYPE_SUB(value_type);
        EnumDef* ed = NULL;
        bool is_any = (TYPE_KIND(value_type) == VAL_ANY);
        if (!is_any && enum_id != OPTION_ENUM_ID) {
            ed = &current_compiler->enums[enum_id];
        }
        
        bool covered[16] = {0};
        int covered_count = 0;
        int end_jumps[16];
        int end_jump_count = 0;
        
        while (parser.current.type != TOKEN_RBRACKET && parser.current.type != TOKEN_EOF) {
            Token variant_token = parser.current;
            advance();
            
            int v_idx = -1;
            Type any_target_type = VAL_NONE;
            
            if (is_any) {
                any_target_type = type_from_name(variant_token);
                if (any_target_type == VAL_NONE) {
                    // Try to find struct or enum with this name
                    for (int i = 0; i < current_compiler->struct_count; i++) {
                        if (variant_token.length == current_compiler->structs[i].name.length &&
                            memcmp(variant_token.start, current_compiler->structs[i].name.start, variant_token.length) == 0) {
                            any_target_type = VAL_OBJ; break;
                        }
                    }
                }
                if (any_target_type == VAL_NONE) error_at(&variant_token, "Unknown type for 'any' match.");
            } else if (enum_id == OPTION_ENUM_ID) {
                if (variant_token.length == 4 && memcmp(variant_token.start, "some", 4) == 0) {
                    v_idx = 1;
                } else if (variant_token.length == 4 && memcmp(variant_token.start, "none", 4) == 0) {
                    v_idx = 0;
                } else {
                    error_at(&variant_token, "Expected 'some' or 'none' for Option match.");
                }
            } else {
                for (int i = 0; i < ed->variant_count; i++) {
                    if (ed->variants[i].length == variant_token.length &&
                        memcmp(ed->variants[i].start, variant_token.start, variant_token.length) == 0) {
                        v_idx = i;
                        break;
                    }
                }
                if (v_idx == -1) error_at(&variant_token, "Unknown variant for this Enum.");
            }
            
            if (!is_any && v_idx != -1) {
                if (v_idx < 16) {
                    if (covered[v_idx]) error_at(&variant_token, "Variant already covered.");
                    covered[v_idx] = true;
                    covered_count++;
                }
            }
            
            bool has_binding = false;
            Token binding_name;
            if (match(TOKEN_LPAREN)) {
                consume(TOKEN_ID, "Expect binding name.");
                binding_name = parser.previous;
                consume(TOKEN_RPAREN, "Expect ')' after binding.");
                has_binding = true;
                
                if (is_any) {
                    // all variants in 'any' have payload except 'void'
                    if (any_target_type == VAL_VOID && has_binding) error_at(&variant_token, "'void' variant cannot have a payload.");
                } else if (enum_id == OPTION_ENUM_ID) {
                    if (v_idx != 1) error_at(&variant_token, "Only 'some' variant can have a payload.");
                } else {
                    if (!ed->has_payload[v_idx]) error_at(&variant_token, "Variant does not have a payload.");
                }
            } else {
                if (!is_any) {
                    if (enum_id != OPTION_ENUM_ID && ed->has_payload[v_idx]) {
                        error_at(&variant_token, "Variant requires a payload binding.");
                    }
                    if (enum_id == OPTION_ENUM_ID && v_idx == 1) error_at(&variant_token, "'some' requires a payload binding.");
                } else {
                    if (any_target_type != VAL_VOID) error_at(&variant_token, "Type variant requires a payload binding.");
                }
            }
            
            if (is_any) {
                emit_byte(OP_CHECK_TYPE);
                emit_byte((uint8_t)TYPE_KIND(any_target_type));
            } else {
                emit_byte(OP_CHECK_VARIANT);
                emit_int32(v_idx);
            }
            
            int next_case_patch = current_chunk->count;
            emit_byte(OP_JUMP_IF_F);
            emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0);
            
            int old_local_count = current_compiler->local_count;
            current_compiler->scope_depth++;

            int old_guarded = -1;
            int old_variant = -1;
            if (matched_local != -1) {
                old_guarded = current_compiler->locals[matched_local].guarded_depth;
                old_variant = current_compiler->locals[matched_local].guarded_variant;
                current_compiler->locals[matched_local].guarded_depth = current_compiler->scope_depth;
                current_compiler->locals[matched_local].guarded_variant = v_idx;
            }

            if (has_binding) {
                Type p_type;
                if (is_any) {
                    p_type = any_target_type;
                } else if (enum_id == OPTION_ENUM_ID) {
                    p_type = MAKE_TYPE(TYPE_KEY(value_type), 0, 0);
                } else {
                    p_type = ed->payload_types[v_idx];
                }
                
                if (is_any) {
                    emit_byte(OP_GET_ENUM_PAYLOAD);
                    emit_byte(OP_AS_TYPE);
                    emit_int32(p_type);
                } else {
                    emit_byte(OP_GET_ENUM_PAYLOAD);
                }
                add_local(binding_name, p_type);
                emit_bytes(OP_STORE, (uint8_t)(current_compiler->local_count - 1));
            }
            
            block();
            type_pop(); // Ignore block result in statement-level match
            
            if (matched_local != -1) {
                current_compiler->locals[matched_local].guarded_depth = old_guarded;
                current_compiler->locals[matched_local].guarded_variant = old_variant;
            }

            end_jumps[end_jump_count++] = current_chunk->count;
            emit_byte(OP_JUMP);
            emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0);
            
            patch_int32(next_case_patch + 1, current_chunk->count);
            current_compiler->local_count = old_local_count;
            current_compiler->scope_depth--;
        }
        
        consume(TOKEN_RBRACKET, "Expect ']' after match cases.");
        
        if (!is_any) {
            if (enum_id == OPTION_ENUM_ID) {
                if (covered_count < 2) error_at(&parser.previous, "Match not exhaustive. Missing 'some' or 'none'.");
            } else {
                if (covered_count < ed->variant_count) error_at(&parser.previous, "Match not exhaustive.");
            }
        }
        
        for (int i = 0; i < end_jump_count; i++) {
            patch_int32(end_jumps[i] + 1, current_chunk->count);
        }
        
        emit_byte(OP_POP);
        type_push(VAL_VOID);
        return;
    }
    if (match(TOKEN_GO)) {
        current_compiler->is_go = true;
        expression();
        current_compiler->is_go = false;
        type_pop();
        type_push(VAL_VOID);
        return;
    }

    if (match(TOKEN_SEMICOLON)) {
        type_push(VAL_VOID);
        return;
    }
    if (match(TOKEN_TRY)) {
        emit_byte(OP_TRY);
        int try_handler_patch = current_chunk->count;
        emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0);
        
        block();
        type_pop(); // pop try block result
        emit_byte(OP_END_TRY);
        
        int jump_over_catch_patch = current_chunk->count;
        emit_byte(OP_JUMP);
        emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0);
        
        patch_int32(try_handler_patch, current_chunk->count);
        
        consume(TOKEN_CATCH, "Expect 'catch' after try block.");
        consume(TOKEN_ID, "Expect error variable name.");
        Token err_name = parser.previous;
        
        current_compiler->scope_depth++;
        add_local(err_name, VAL_ANY); // Error can be anything
        int err_local_idx = current_compiler->local_count - 1;
        emit_bytes(OP_STORE, (uint8_t)err_local_idx);
        
        block();
        type_pop(); // pop catch block result
        current_compiler->local_count--; // remove err local
        current_compiler->scope_depth--;
        
        patch_int32(jump_over_catch_patch + 1, current_chunk->count);
        type_push(VAL_VOID);
        return;
    }
    int start_addr = current_chunk->count;
    expression();
    if (parser.current.type == TOKEN_QUESTION) {
        Type cond_type = type_pop();
        int cond_local = popped_local;

        if (cond_type != VAL_BOOL) {
            emit_byte(OP_IS_TRUTHY);
        }
        advance();
        int jump_if_f_patch = current_chunk->count;
        emit_byte(OP_JUMP_IF_F);
        emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0); 
        
        int old_guarded = -1;
        int old_variant = -1;
        if (cond_local != -1) {
            old_guarded = current_compiler->locals[cond_local].guarded_depth;
            old_variant = current_compiler->locals[cond_local].guarded_variant;
            current_compiler->locals[cond_local].guarded_depth = current_compiler->scope_depth + 1;
            current_compiler->locals[cond_local].guarded_variant = 1; // 'some' variant for Options
        }

        if (parser.current.type == TOKEN_LBRACKET) block(); else statement();
        Type t1 = type_pop();

        if (cond_local != -1) {
            current_compiler->locals[cond_local].guarded_depth = old_guarded;
            current_compiler->locals[cond_local].guarded_variant = old_variant;
        }

        int jump_patch = current_chunk->count;
        emit_byte(OP_JUMP);
        emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0); 
        patch_int32(jump_if_f_patch + 1, current_chunk->count);
        if (parser.current.type == TOKEN_COLON) {
            advance();
            if (parser.current.type == TOKEN_LBRACKET) block(); else statement();
            Type t2 = type_pop();
            if (t1 == t2) type_push(t1);
            else if (is_assignable(t1, t2)) type_push(t1);
            else if (is_assignable(t2, t1)) type_push(t2);
            else type_push(VAL_VOID);
        } else {
            type_push(VAL_VOID);
        }
        patch_int32(jump_patch + 1, current_chunk->count);
        match(TOKEN_SEMICOLON);
    } else if (parser.current.type == TOKEN_AT) {
        Type cond_type = type_pop();
        int cond_local = popped_local;

        if (cond_type != VAL_BOOL) {
            emit_byte(OP_IS_TRUTHY);
        }
        advance();

        Loop loop;
        loop.start_addr = start_addr;
        loop.end_jump_count = 0;
        loop.next = current_compiler->current_loop;
        current_compiler->current_loop = &loop;

        int jump_if_f_patch = current_chunk->count;
        emit_byte(OP_JUMP_IF_F);
        emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0);

        int old_guarded = -1;
        int old_variant = -1;
        if (cond_local != -1) {
            old_guarded = current_compiler->locals[cond_local].guarded_depth;
            old_variant = current_compiler->locals[cond_local].guarded_variant;
            current_compiler->locals[cond_local].guarded_depth = current_compiler->scope_depth + 1;
            current_compiler->locals[cond_local].guarded_variant = 1; // 'some' variant
        }

        if (parser.current.type == TOKEN_LBRACKET) block(); else statement();
        type_pop();

        if (cond_local != -1) {
            current_compiler->locals[cond_local].guarded_depth = old_guarded;
            current_compiler->locals[cond_local].guarded_variant = old_variant;
        }

        emit_byte(OP_JUMP);
        for (int i = 0; i < 4; i++) emit_byte((start_addr >> (i * 8)) & 0xFF);
        patch_int32(jump_if_f_patch + 1, current_chunk->count);
        
        for (int i = 0; i < loop.end_jump_count; i++) {
            patch_int32(loop.end_jump_patches[i] + 1, current_chunk->count);
        }
        current_compiler->current_loop = loop.next;

        match(TOKEN_SEMICOLON);
        type_push(VAL_VOID);
    } else {
        match(TOKEN_SEMICOLON);
    }
}

Chunk* compiler_compile(const char* source, const char* base_dir, const char* stdlib_dir) {
    lexer_init(source);
    current_chunk = malloc(sizeof(Chunk));
    current_chunk->code = NULL;
    current_chunk->count = 0;
    current_chunk->capacity = 0;
    current_chunk->strings = NULL;
    current_chunk->strings_count = 0;
    current_chunk->strings_capacity = 0;
    add_string("none", 4);
    add_string("int", 3); add_string("flt", 3); add_string("bol", 3);
    add_string("str", 3); add_string("void", 4); add_string("fun", 3);

    compiled_modules_count = 0;
    compilation_stack_count = 0;
    active_prefix = NULL;

    current_compiler = calloc(1, sizeof(CompilerState));
    current_compiler->local_count = 0;
    current_compiler->function_count = 0;
    current_compiler->struct_count = 0;
    current_compiler->enum_count = 0;
    current_compiler->native_count = 0;
    current_compiler->scope_depth = 0;
    current_compiler->current_loop = NULL;
    current_compiler->current_return_type = VAL_VOID;
    current_compiler->type_stack_ptr = 0;
    current_compiler->is_go = false;
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
    add_native("exit", 10, VAL_VOID, 1, VAL_INT);
    add_native("clock", 11, VAL_FLT, 0);
    add_native("system", 12, VAL_INT, 1, VAL_STR);
    add_native("keys", 13, VAL_OBJ, 1, VAL_MAP);
    add_native("delete", 14, VAL_VOID, 2, VAL_MAP, VAL_ANY);
    add_native("ascii", 15, VAL_INT, 1, VAL_STR);
    add_native("char", 16, VAL_STR, 1, VAL_INT);
    add_native("has", 17, VAL_BOOL, 2, VAL_MAP, VAL_ANY);
    add_native("error", 18, VAL_ERR, 1, VAL_ANY);
    add_native("time", 19, VAL_INT, 0);
    add_native("sqrt", 20, VAL_FLT, 1, VAL_ANY);
    add_native("sin", 21, VAL_FLT, 1, VAL_ANY);
    add_native("cos", 22, VAL_FLT, 1, VAL_ANY);
    add_native("tan", 23, VAL_FLT, 1, VAL_ANY);
    add_native("log", 24, VAL_FLT, 1, VAL_ANY);
    add_native("flt", 25, VAL_FLT, 1, VAL_ANY);
    add_native("rand", 26, VAL_FLT, 2, VAL_FLT, VAL_FLT);
    add_native("seed", 27, VAL_VOID, 1, VAL_INT);
    add_native("ffiLoad", 28, VAL_INT, 1, VAL_STR);
    add_native("ffiCall", 29, VAL_ANY, 4, VAL_INT, VAL_STR, VAL_STR, VAL_STR);
    add_native("close", 30, VAL_VOID, 1, VAL_CHAN);
    add_native("json_stringify", 31, VAL_STR, 1, VAL_ANY);
    add_native("json_parse", 32, VAL_ANY, 1, VAL_STR);
    add_native("httpGet", 33, VAL_STR, 1, VAL_STR);
    add_native("regexMatch", 34, VAL_BOOL, 2, VAL_STR, VAL_STR);
    add_native("fileExists", 35, VAL_BOOL, 1, VAL_STR);
    add_native("removeFile", 36, VAL_BOOL, 1, VAL_STR);
    add_native("listDir", 37, MAKE_TYPE(VAL_OBJ, VAL_STR, 0), 1, VAL_STR);

    parser.had_error = false;
    parser.panic_mode = false;
    root_base_dir = (char*)base_dir;
    std_base_dir = (char*)stdlib_dir;
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
        emit_int32(current_compiler->functions[main_idx].addr);
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
