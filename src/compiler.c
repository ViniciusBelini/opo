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
    Token name;
    Token fields[16];
    int field_count;
} StructDef;

typedef struct {
    Token name;
    int index;
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
    if (expected == actual) return true;
    if (expected == VAL_FUNC && actual >= VAL_FUNC) return true;
    if (actual == VAL_FUNC && expected >= VAL_FUNC) return true;
    return false;
}

static void add_native(const char* name, int index) {
    Native* n = &current_compiler->natives[current_compiler->native_count++];
    n->name.start = name;
    n->name.length = (int)strlen(name);
    n->index = index;
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
    if (t.length == 3 && memcmp(t.start, "int", 3) == 0) return VAL_INT;
    if (t.length == 3 && memcmp(t.start, "flt", 3) == 0) return VAL_FLT;
    if (t.length == 3 && memcmp(t.start, "bol", 3) == 0) return VAL_BOOL;
    if (t.length == 3 && memcmp(t.start, "str", 3) == 0) return VAL_STR;
    if (t.length == 4 && memcmp(t.start, "void", 4) == 0) return VAL_VOID;
    if (t.length == 3 && memcmp(t.start, "fun", 3) == 0) return VAL_FUNC;
    if (t.length == 4 && memcmp(t.start, "type", 4) == 0) return VAL_VOID; // simplified
    
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
    if (parser.current.type != TOKEN_RBRACKET) {
        do {
            expression();
            type_pop();
            count++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RBRACKET, "Expect ']' after array elements.");
    emit_bytes(OP_ARRAY, (uint8_t)count);
    type_push(VAL_OBJ);
}

static void dot() {
    if (match(TOKEN_INT)) {
        int64_t idx = strtoll(parser.previous.start, NULL, 10);
        emit_byte(OP_PUSH_INT);
        for (int i = 0; i < 8; i++) {
            emit_byte((idx >> (i * 8)) & 0xFF);
        }
        emit_byte(OP_INDEX);
        type_pop();
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
        if (field_idx == -1) error_at(&name, "Unknown struct field.");
        emit_bytes(OP_GET_MEMBER, (uint8_t)field_idx);
        type_pop();
        type_push(VAL_OBJ);
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

    if (name.length == 6 && memcmp(name.start, "typeOf", 6) == 0) {
        consume(TOKEN_LPAREN, "Expect '(' after 'typeOf'.");
        expression();
        consume(TOKEN_RPAREN, "Expect ')' after 'typeOf' argument.");
        emit_byte(OP_TYPEOF);
        type_pop();
        type_push(VAL_STR);
        return;
    }

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
            type_push(VAL_OBJ);
        } else {
            emit_bytes(OP_LOAD, (uint8_t)arg);
            type_push(type);
        }
        return;
    }

    int n_idx = resolve_native(current_compiler, &name);
    if (n_idx != -1) {
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
            emit_bytes(OP_LOAD_G, (uint8_t)n_idx);
            emit_bytes(OP_INVOKE, (uint8_t)arg_count);
            type_push(VAL_OBJ);
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
                    type_pop();
                    count++;
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expect ')' after struct arguments.");
            if (count != sd->field_count) error_at(&name, "Wrong number of fields for struct.");
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

    if (f_idx != -1) {
        Function* f = &current_compiler->functions[f_idx];
        if (match(TOKEN_LPAREN)) {
            int arg_count = 0;
            if (parser.current.type != TOKEN_RPAREN) {
                do {
                    expression();
                    ValueType t = type_pop();
                    arg_count++;
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expect ')' after arguments.");
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
        int arg = resolve_local(current_compiler, &name);
        if (arg == -1) {
            add_local(name, declared_type);
            arg = current_compiler->local_count - 1;
        }
        emit_bytes(OP_STORE, (uint8_t)arg);
    } else if (match(TOKEN_DOT)) {
        int arg = resolve_local(current_compiler, &name);
        if (arg == -1) error_at(&name, "Undefined object.");
        emit_bytes(OP_LOAD, (uint8_t)arg);
        if (match(TOKEN_INT)) {
            error_at(&parser.previous, "Indexing assignment not yet implemented.");
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
            if (field_idx == -1) error_at(&field_name, "Unknown struct field.");
            emit_bytes(OP_SET_MEMBER, (uint8_t)field_idx);
            type_pop();
        }
    } else {
        int arg = resolve_local(current_compiler, &name);
        if (arg == -1) error_at(&name, "Undefined identifier.");
        emit_bytes(OP_STORE, (uint8_t)arg);
        type_pop();
    }
}

static void print_op() {
    type_pop();
    emit_byte(OP_PRINT);
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
    while (parser.current.type != TOKEN_RBRACKET && parser.current.type != TOKEN_EOF) {
        statement();
    }
    consume(TOKEN_RBRACKET, "Expect ']' after block.");
    current_compiler->scope_depth--;
}

static void statement() {
    if (match(TOKEN_SEMICOLON)) return;
    int start_addr = current_chunk->count;
    expression();
    if (parser.current.type == TOKEN_QUESTION) {
        advance();
        int jump_if_f_patch = current_chunk->count;
        emit_byte(OP_JUMP_IF_F);
        emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0); 
        if (parser.current.type == TOKEN_LBRACKET) block(); else statement();
        int jump_patch = current_chunk->count;
        emit_byte(OP_JUMP);
        emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0); 
        patch_int32(jump_if_f_patch + 1, current_chunk->count);
        if (parser.current.type == TOKEN_COLON) {
            advance();
            if (parser.current.type == TOKEN_LBRACKET) block(); else statement();
        }
        patch_int32(jump_patch + 1, current_chunk->count);
        match(TOKEN_SEMICOLON);
    } else if (parser.current.type == TOKEN_AT) {
        advance();
        int jump_if_f_patch = current_chunk->count;
        emit_byte(OP_JUMP_IF_F);
        emit_byte(0); emit_byte(0); emit_byte(0); emit_byte(0);
        if (parser.current.type == TOKEN_LBRACKET) block(); else statement();
        emit_byte(OP_JUMP);
        for (int i = 0; i < 4; i++) emit_byte((start_addr >> (i * 8)) & 0xFF);
        patch_int32(jump_if_f_patch + 1, current_chunk->count);
        match(TOKEN_SEMICOLON);
    } else {
        match(TOKEN_SEMICOLON);
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
    add_string("int", 3); add_string("flt", 3); add_string("bol", 3);
    add_string("str", 3); add_string("void", 4); add_string("fun", 3);

    current_compiler = malloc(sizeof(CompilerState));
    current_compiler->local_count = 0;
    current_compiler->function_count = 0;
    current_compiler->struct_count = 0;
    current_compiler->native_count = 0;
    current_compiler->scope_depth = 0;
    add_native("len", 0); add_native("append", 1); add_native("str", 2);
    add_native("readFile", 3); add_native("writeFile", 4); add_native("args", 5);
    add_native("int", 6);

    parser.had_error = false;
    parser.panic_mode = false;
    advance();

    while (parser.current.type != TOKEN_EOF) {
        if (match(TOKEN_SEMICOLON)) continue;
        if (match(TOKEN_STRUCT)) {
            consume(TOKEN_LBRACKET, "Expect '[' after 'struct'.");
            StructDef* sd = &current_compiler->structs[current_compiler->struct_count++];
            sd->field_count = 0;
            while (parser.current.type != TOKEN_RBRACKET) {
                consume(TOKEN_ID, "Expect field name.");
                sd->fields[sd->field_count] = parser.previous;
                consume(TOKEN_COLON, "Expect ':'.");
                parse_type(); sd->field_count++;
                if (parser.current.type == TOKEN_COMMA) advance();
            }
            consume(TOKEN_RBRACKET, "Expect ']' after struct fields.");
            consume(TOKEN_ASSIGN, "Expect '=>' after struct definition.");
            consume(TOKEN_ID, "Expect struct name.");
            sd->name = parser.previous;
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
            func->name = name; func->addr = current_chunk->count;
            func->return_type = return_type; func->param_count = param_count;
            for (int i = 0; i < param_count; i++) func->param_types[i] = param_types[i];
            int old_local_count = current_compiler->local_count;
            current_compiler->local_count = 0;
            for (int i = 0; i < param_count; i++) add_local(params[i], param_types[i]);
            for (int i = param_count - 1; i >= 0; i--) emit_bytes(OP_STORE, (uint8_t)i);
            block();
            emit_byte(OP_RET);
            patch_int32(jump_over + 1, current_chunk->count);
            current_compiler->local_count = old_local_count;
        } else {
            advance();
        }
    }

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
