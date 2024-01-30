#include <stdio.h>
#include <string.h>

#include "util.h"
#include "lexer.h"

typedef struct {
  const char* start;
  const char* cursor;
  int line;
} Lexer;

Lexer lexer;

void initLexer(const char* input) {
  
  lexer.start = input;
  lexer.cursor = input;
  lexer.line = 1; // 0 or 1 index? lox uses 1? why?
}

static bool alphabetic(char c) {

  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
          c == '_';
}

static bool numeric(char c) {

  return c >= '0' && c <= '9';
}

static bool termination() {

    return *lexer.cursor == '\0';
}

static char step() {

    lexer.cursor++;
    return lexer.cursor[-1];
}

static char look() {

    return *lexer.cursor;
}

static char lookAhead() {

    if (termination()) return '\0';
    return lexer.cursor[1];
}

static char lookAheadAhead() {

    if (termination()) return '\0';
    return lexer.cursor[2];
}

static bool check(char expected) {

    if (termination()) return false;
    if (lexer.cursor != expected) return false;
    lexer.cursor++;
    return true;
}


static Token getToken(TokenType type) {

  Token token;
  token.type = type;
  token.start = lexer.start;
  token.size = (int)(lexer.cursor - lexer.start);
  token.line = lexer.line;

  return token;
}

static Token getErrorToken(const char* message) {

  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.size = (int)strlen(message);
  token.line = lexer.line;

  return token;
}

static void skipWhiteSpace() {
  
  for (;;) {

    char c = look();
    switch(c) {

      case ' ':
      case '\r':
      case '\t':
        step();
        break;
      case '\n':
        lexer.line++;
        step();
        break;
      case '/':
        if (lookAhead() == '/') {

          while (look() != '\n' && !termination()) step();
        }
        else {
          
          return;
        }
        break;
      default:
        return;
    }
  }
}

static TokenType checkKeyword(int start, int size,
                              const char* rest, TokenType type) {

  if (lexer.cursor - lexer.start == start + size && 
      memcmp(lexer.start + start, rest, size) == 0) {

    return type;
  }

  return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {

  switch (lexer.start[0]) {

    case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
    case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
    case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
    case 'f':
      if (lexer.cursor - lexer.start > 1) {

        switch (lexer.start[1]) {

          case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
          case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
          case 'u': return checkKeyword(2, 6, "nction", TOKEN_FUNCTION);
        }
      }
      break;
    case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
    case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
    case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
    case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
    case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
    case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
    case 't':
      if (lexer.cursor - lexer.start > 1) {

        switch (lexer.start[1]) {
          
          case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
          case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
        }
      }
      break;
    case 'v': return checkKeyword(1, 7, "ariable", TOKEN_VARIABLE);
    case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
  }

  return TOKEN_IDENTIFIER;
}

static Token identifier() {

  while (alphabetic(look()) || numeric(look())) step();
  return getToken(identifierType());
}

static Token number() {

  while (numeric(look())) step();
  if (look() == '.' && numeric(lookAhead())) {

    step();
    while (numeric(look())) step();
  }
  if (look() == 'e' && (lookAhead() == '+' || lookAhead() == '-') && numeric(lookAheadAhead())) {

    step(); step();
    while (numeric(look())) step();
  }

  return getToken(TOKEN_NUMBER);
}

static Token string() {

  while (look() != '"' && !termination()) {

    if (look() == '\n') lexer.line++;
    step();
  }

  if (termination()) return getErrorToken("Unterminated string.");

  step();
  return getToken(TOKEN_STRING);
}

Token lex() {

  skipWhiteSpace();
  lexer.start = lexer.cursor;
  if (termination()) return getToken(TOKEN_EOF);

  char c = step();
  if (alphabetic(c)) return identifier();
  if (numeric(c)) return number();

  switch(c) {

    case '(': return getToken(TOKEN_LEFT_PAREN);
    case ')': return getToken(TOKEN_RIGHT_PAREN);
    case '{': return getToken(TOKEN_LEFT_BRACE);
    case '}': return getToken(TOKEN_RIGHT_BRACE);

    case ',': return getToken(TOKEN_COMMA);
    case '.': return getToken(TOKEN_DOT);
    case ';': return getToken(TOKEN_SEMICOLON);

    case '+': return getToken(TOKEN_PLUS);
    case '-': return getToken(TOKEN_MINUS);
    case '*': return getToken(TOKEN_STAR);
    case '/': return getToken(TOKEN_FWD_SLASH);
    case '^': return getToken(TOKEN_CARET);

    case '!': 
      return getToken(check('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=': 
      return getToken(check('=') ? TOKEN_IDENTITY : TOKEN_EQUAL);
    case '<': 
      return getToken(check('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>': 
      return getToken(check('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);

    case '"': return string();
  }

  return getErrorToken("Unexpected character.");
}