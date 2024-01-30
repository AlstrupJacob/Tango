#ifndef tango_scanner_h
#define tango_scanner_h

typedef enum {
  
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,

  TOKEN_COMMA, TOKEN_DOT, TOKEN_SEMICOLON,

  TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, 
  TOKEN_FWD_SLASH, TOKEN_CARET,

  TOKEN_BANG, TOKEN_BANG_EQUAL,
  TOKEN_EQUAL, TOKEN_IDENTITY,
  TOKEN_GREATER, TOKEN_GREATER_EQUAL,
  TOKEN_LESS, TOKEN_LESS_EQUAL,

  TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

  TOKEN_TRUE, TOKEN_FALSE,

  TOKEN_AND, TOKEN_OR, TOKEN_IF, TOKEN_ELSE,
  TOKEN_FOR, TOKEN_WHILE, 

  TOKEN_CLASS, TOKEN_FUNCTION, TOKEN_VARIABLE,
  TOKEN_SUPER, TOKEN_THIS, TOKEN_PRINT,

  TOKEN_NIL, TOKEN_RETURN,
  TOKEN_ERROR, TOKEN_EOF,

} TokenType;

typedef struct {
  TokenType type;
  const char* start;
  int size;
  int line;
} Token;

void initLexer(const char* input);
Token lex();

#endif