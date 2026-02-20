#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "lexer.h"

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
        case 't': 
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'r': 
                        if (lexer.current - lexer.start == 3 && lexer.start[2] == 'u') return TOKEN_BOOL;
                        if (lexer.current - lexer.start == 3 && lexer.start[2] == 'y') return TOKEN_TRY;
                        break;
                    case 'y': return check_keyword(2, 2, "pe", TOKEN_TYPE);
                    case 'h': return check_keyword(2, 3, "row", TOKEN_THROW);
                }
            }
            break;
        case 'f': return check_keyword(1, 2, "ls", TOKEN_BOOL);
        case 's': 
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 't': return check_keyword(2, 4, "ruct", TOKEN_STRUCT);
                    case 'o': return check_keyword(2, 2, "me", TOKEN_SOME);
                }
            }
            break;
        case 'p': return check_keyword(1, 2, "ub", TOKEN_PUB);
        case 'i': return check_keyword(1, 2, "mp", TOKEN_IMP);
        case 'g': return check_keyword(1, 1, "o", TOKEN_GO);
        case 'c': 
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'a': return check_keyword(2, 3, "tch", TOKEN_CATCH);
                    case 'h': return check_keyword(2, 2, "an", TOKEN_CHAN);
                }
            }
            break;
        case 'e': 
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'n': return check_keyword(2, 2, "um", TOKEN_ENUM);
                    case 'r': return check_keyword(2, 1, "r", TOKEN_ERR);
                }
            }
            break;
        case 'a': return check_keyword(1, 1, "s", TOKEN_AS);
        case 'm': return check_keyword(1, 4, "atch", TOKEN_MATCH);
        case 'n': return check_keyword(1, 3, "none", TOKEN_NONE);
        case 'o': return check_keyword(1, 1, "k", TOKEN_OK);
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
        case '.': return match('.') ? make_token(TOKEN_DOT_DOT) : make_token(TOKEN_DOT);
        case '^': return make_token(TOKEN_HAT);
        case ':': return make_token(TOKEN_COLON);
        case ';': return make_token(TOKEN_SEMICOLON);
        case ',': return make_token(TOKEN_COMMA);
        case '?': return make_token(TOKEN_QUESTION);
        case '@': return make_token(TOKEN_AT);
        case '[': return make_token(TOKEN_LBRACKET);
        case ']': return make_token(TOKEN_RBRACKET);
        case '{': return make_token(TOKEN_LBRACE);
        case '}': return make_token(TOKEN_RBRACE);
        case '(': return make_token(TOKEN_LPAREN);
        case ')': return make_token(TOKEN_RPAREN);
        case '<': 
            if (match('=')) return make_token(TOKEN_LTE);
            if (match('-')) return make_token(TOKEN_L_ARROW);
            return make_token(TOKEN_LANGLE);
        case '>': return match('=') ? make_token(TOKEN_GTE) : make_token(TOKEN_RANGLE);
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
            if (match('=')) return make_token(TOKEN_BANG_EQ);
            return make_token(TOKEN_BANG);
        case '&':
            if (match('&')) return make_token(TOKEN_AND);
            break;
        case '|':
            if (match('|')) return make_token(TOKEN_OR);
            break;
        case '"': return string();
    }

    return error_token("Unexpected character.");
}
