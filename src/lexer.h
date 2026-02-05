#ifndef OPO_LEXER_H
#define OPO_LEXER_H

typedef enum {
    TOKEN_INT, TOKEN_FLT, TOKEN_STR, TOKEN_BOOL,
    TOKEN_ID,
    TOKEN_ARROW,    // ->
    TOKEN_ASSIGN,   // =>
    TOKEN_COLON,    // :
    TOKEN_QUESTION, // ?
    TOKEN_AT,       // @
    TOKEN_LBRACKET, // [
    TOKEN_RBRACKET, // ]
    TOKEN_LANGLE,   // <
    TOKEN_RANGLE,   // >
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
    TOKEN_BANG,     // !
    TOKEN_BANG_BANG,// !!
    TOKEN_EQ_EQ,    // ==
    TOKEN_BANG_EQ,  // !=
    TOKEN_LTE, TOKEN_GTE,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

void lexer_init(const char* source);
Token lexer_next_token();

#endif
