#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "lexer.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
} Lexer;

Lexer lexer;

void lexer_init(const char* source) {
    lexer.start = source;
    lexer.current = source;
    lexer.line = 1;
}

static bool is_at_end() {
    return *lexer.current == '\0';
}

static char advance() {
    lexer.current++;
    return lexer.current[-1];
}

static char peek() {
    return *lexer.current;
}

static char peek_next() {
    if (is_at_end()) return '\0';
    return lexer.current[1];
}

static bool match(char expected) {
    if (is_at_end()) return false;
    if (*lexer.current != expected) return false;
    lexer.current++;
    return true;
}

static Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer.start;
    token.length = (int)(lexer.current - lexer.start);
    token.line = lexer.line;
    return token;
}

static Token error_token(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer.line;
    return token;
}

static void skip_whitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                lexer.line++;
                advance();
                break;
            case '#': // Comment
                while (peek() != '\n' && !is_at_end()) advance();
                break;
            default:
                return;
        }
    }
}

static Token string() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') lexer.line++;
        advance();
    }
    if (is_at_end()) return error_token("Unterminated string.");
    advance(); // The closing "
    return make_token(TOKEN_STR);
}

static Token number() {
    while (isdigit(peek())) advance();
    if (peek() == '.' && isdigit(peek_next())) {
        advance();
        while (isdigit(peek())) advance();
        return make_token(TOKEN_FLT);
    }
    return make_token(TOKEN_INT);
}

static TokenType check_keyword(int start, int length, const char* rest, TokenType type) {
    if (lexer.current - lexer.start == start + length &&
        memcmp(lexer.start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_ID;
}

static TokenType identifier_type() {
    switch (lexer.start[0]) {
        case 't': return check_keyword(1, 2, "ru", TOKEN_BOOL);
        case 'f': return check_keyword(1, 2, "ls", TOKEN_BOOL);
    }
    return TOKEN_ID;
}

static Token identifier() {
    while (isalnum(peek()) || peek() == '_') advance();
    return make_token(identifier_type());
}

Token lexer_next_token() {
    skip_whitespace();
    lexer.start = lexer.current;
    if (is_at_end()) return make_token(TOKEN_EOF);

    char c = advance();
    if (isdigit(c)) return number();
    if (isalpha(c) || c == '_') return identifier();

    switch (c) {
        case ':': return make_token(TOKEN_COLON);
        case '?': return make_token(TOKEN_QUESTION);
        case '@': return make_token(TOKEN_AT);
        case '[': return make_token(TOKEN_LBRACKET);
        case ']': return make_token(TOKEN_RBRACKET);
        case '<': return make_token(TOKEN_LANGLE);
        case '>': return make_token(TOKEN_RANGLE);
        case '+': return make_token(TOKEN_PLUS);
        case '-': return match('>') ? make_token(TOKEN_ARROW) : make_token(TOKEN_MINUS);
        case '*': return make_token(TOKEN_STAR);
        case '/': return make_token(TOKEN_SLASH);
        case '%': return make_token(TOKEN_PERCENT);
        case '=':
            if (match('=')) return make_token(TOKEN_EQ_EQ);
            if (match('>')) return make_token(TOKEN_ASSIGN);
            break;
        case '!':
            if (match('!')) return make_token(TOKEN_BANG_BANG);
            return make_token(TOKEN_BANG);
        case '"': return string();
    }

    return error_token("Unexpected character.");
}
