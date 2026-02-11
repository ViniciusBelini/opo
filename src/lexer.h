#ifndef OPO_LEXER_H
#define OPO_LEXER_H

typedef enum {
    TOKEN_INT, TOKEN_FLT, TOKEN_STR, TOKEN_BOOL,
    TOKEN_ID,
    TOKEN_ARROW,    // ->
    TOKEN_ASSIGN,   // =>
    TOKEN_COLON,    // :
    TOKEN_SEMICOLON,// ;
    TOKEN_COMMA,    // ,
    TOKEN_QUESTION, // ?
    TOKEN_AT,       // @
    TOKEN_LBRACKET, // [
    TOKEN_RBRACKET, // ]
    TOKEN_LBRACE,   // {
    TOKEN_RBRACE,   // }
    TOKEN_LPAREN,   // (
    TOKEN_RPAREN,   // )
    TOKEN_LANGLE,   // <
    TOKEN_RANGLE,   // >
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
    TOKEN_BANG,     // !
    TOKEN_BANG_BANG,// !!
    TOKEN_AND,      // &&
    TOKEN_OR,       // ||
    TOKEN_EQ_EQ,    // ==
    TOKEN_BANG_EQ,  // !=
    TOKEN_LTE, TOKEN_GTE,
    TOKEN_DOT,
    TOKEN_STRUCT, TOKEN_ENUM, TOKEN_MATCH,
    TOKEN_SOME, TOKEN_NONE,
    TOKEN_TYPE,
    TOKEN_PUB, TOKEN_IMP,
    TOKEN_TRY, TOKEN_CATCH, TOKEN_THROW,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

typedef struct {
    const char* start;
    const char* current;
    int line;
} Lexer;

extern Lexer lexer;

void lexer_init(const char* source);
Token lexer_next_token();

#endif
