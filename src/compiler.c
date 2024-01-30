#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "compiler.h"
#include "memory.h"
#include "lexer.h"

#ifdef DEBUG_PRINT_CODE
  #include "debug.h"
#endif

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {

  PRECEDENCE_NONE,
  PRECEDENCE_ASSIGNMENT,
  PRECEDENCE_OR,
  PRECEDENCE_AND,
  PRECEDENCE_EQUALITY,
  PRECEDENCE_COMPARISON,
  PRECEDENCE_TERM,
  PRECEDENCE_FACTOR,
  PRECEDENCE_UNARY,
  PRECEDENCE_CALL,
  PRECEDENCE_PRIMARY

} Precedence;

typedef void (*ParseFunction) (bool canAssign);

typedef struct {
  ParseFunction prefix;
  ParseFunction infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool isCaptured;
} Local;

typedef struct {
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef enum {

  TYPE_FUNCTION,
  TYPE_INITIALIZER,
  TYPE_BOUND_FUNCTION,
  TYPE_SCRIPT

} FunctionType;

typedef struct Compiler {
  struct Compiler* enclosing;
  ObjectFunction* function;
  FunctionType type;

  Local locals[UINT8_COUNT];
  int localCount;
  Upvalue upvalues[UINT8_COUNT];
  int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
  struct ClassCompiler* enclosing;
  bool hasSuperclass;
} ClassCompiler;

Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;

static Chunk* currentChunk() {

  return &current->function->chunk;
}

static void errorAt(Token* token, const char* message) {

  if (parser.panicMode) return;
  parser.panicMode = true;

  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {

    fprintf(stderr, " at end");
  }
  else if (token->type == TOKEN_ERROR) {}
  else {

    fprintf(stderr, " at '%.*s'", token->size, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void error(const char* message) {

  errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {

  errorAt(&parser.current, message);
}

static void advance() {

  parser.previous = parser.current;

  for (;;) {

    parser.current = lex();
    if (parser.current.type != TOKEN_ERROR) break;

    errorAtCurrent(parser.current.start);
  } 
}

static void consume(TokenType type, const char* message) {

  if (parser.current.type == type) {

    advance();
    return;
  }

  errorAtCurrent(message);
}

static bool check(TokenType type) {

  return parser.current.type == type;
}

static bool match(TokenType type) {

  if (!check(type)) return false;
  advance();
  return true;
}

static void emitByte(uint8_t byte) {

  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {

  emitByte(byte1);
  emitByte(byte2);
}

static void emitLoop(int loopStart) {

  emitByte(OPERATION_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX) error("Loop body too large.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {

  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}

static void emitReturn() {

  if (current->type == TYPE_INITIALIZER) {

    emitBytes(OPERATION_GET_LOCAL, 0);
  } 
  else {

    emitByte(OPERATION_NIL);
  }

  emitByte(OPERATION_RETURN);
}

static uint8_t makeConstant(Value value) {

  int constant = addConstant(currentChunk(), value);
  if (constant > UINT8_MAX) {

    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}

static void emitConstant(Value value) {

  emitBytes(OPERATION_CONSTANT, makeConstant(value));
}

static void patchJump(int offset) {

  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {

    error("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] == jump & 0xff;
}

static void initCompiler(Compiler* compiler, FunctionType type) {

  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->function = newFunction();

  current = compiler;

  if (type != TYPE_SCRIPT) {

    current->function->name = stringCopy(parser.previous.start,
                                         parser.previous.size);
  }

  Local* local = &current->locals[current->localCount++];
  local->depth = 0;
  local->isCaptured = false;

  if (type != TYPE_FUNCTION) {

    local->name.start = "this";
    local->name.size = 4;
  } 
  else {

    local->name.start = "";
    local->name.size = 0;
  }
}

static ObjectFunction* endCompiler() {

  emitReturn();
  ObjectFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {

    chunkDissasemble(currentChunk(), function->name != NULL
      ? function->name->string : "<script>");
  }
#endif

  current = current->enclosing;
  return function;
}

static void beginScope() {

  current->scopeDepth++;
}

static void endScope() {

  current->scopeDepth--;

  while (current->localCount > 0 && 
         current->locals[current->localCount - 1].depth >
         current->scopeDepth) {

    if (current->locals[current->localCount-1].isCaptured) {

      emitByte(OPERATION_CLOSE_UPVALUE);
    }
    else {

      emitByte(OPERATION_POP);
    }

    current->localCount--;
  }
}

static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static void binary(bool canAssign) {

  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence +1));

  switch (operatorType) {

    case TOKEN_BANG_EQUAL:    emitBytes(OPERATION_EQUALITY, OPERATION_NOT); break;
    case TOKEN_IDENTITY:      emitByte(OPERATION_EQUALITY); break;
    case TOKEN_GREATER:       emitByte(OPERATION_GREATER); break;
    case TOKEN_GREATER_EQUAL: emitBytes(OPERATION_LESS, OPERATION_NOT); break;
    case TOKEN_LESS:          emitByte(OPERATION_LESS); break;
    case TOKEN_LESS_EQUAL:    emitBytes(OPERATION_GREATER, OPERATION_NOT); break;
    case TOKEN_PLUS:          emitByte(OPERATION_ADDITION); break;
    case TOKEN_MINUS:         emitByte(OPERATION_SUBTRACTION); break;
    case TOKEN_STAR:          emitByte(OPERATION_MULTIPLICATION); break;
    case TOKEN_FWD_SLASH:     emitByte(OPERATION_DIVISION); break;
    case TOKEN_CARET:         emitByte(OPERATION_EXPONENTIATION); break;    
    default: return; 
  }
}

static uint8_t argumentList() {

    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {

        do {

            expression();
            if (argCount == 255) {

                error("Can't have more than 255 arguments");
            }

            argCount++;
        } while (match(TOKEN_COMMA));

    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static uint8_t identifierConstant(Token* name) {

    return makeConstant(OBJECT_VALUE(stringCopy(name->start, name->size)));
}

static void call(bool canAssign) {

  uint8_t argCount = argumentList();
  emitBytes(OPERATION_CALL, argCount);
}

static void dot(bool canAssign) {

  consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
  uint8_t name = identifierConstant(&parser.previous);
  
  if (canAssign && match(TOKEN_EQUAL)) {

    expression();
    emitBytes(OPERATION_SET_PROPERTY, name);
  }
  else if (match(TOKEN_LEFT_PAREN)) {

    uint8_t argCount = argumentList();
    emitBytes(OPERATION_INVOKE, name);
    emitByte(argCount);
  }
  else {

    emitBytes(OPERATION_GET_PROPERTY, name);
  }
}

static void literal(bool canAssign) {

  switch (parser.previous.type) {

    case TOKEN_FALSE: emitByte(OPERATION_FALSE); break;
    case TOKEN_NIL: emitByte(OPERATION_NIL); break;
    case TOKEN_TRUE: emitByte(OPERATION_TRUE); break;
    default: return;
  }
}

static void grouping(bool canAssign) {

  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression."); 
}

static void number(bool canAssign) {

  double value = atof(parser.previous.start);
  emitConstant(NUMBER_VALUE(value));
}

static void and_(bool canAssign) {

  int endJump = emitJump(OPERATION_JUMP_IF_FALSE);

  emitByte(OPERATION_POP);
  parsePrecedence(PRECEDENCE_AND);
  
  patchJump(endJump);
}

static void or_(bool canAssign) {

  int elseJump = emitJump(OPERATION_JUMP_IF_FALSE);
  int endJump = emitJump(OPERATION_JUMP);

  patchJump(elseJump);
  emitByte(OPERATION_POP);
  
  parsePrecedence(PRECEDENCE_OR);
  patchJump(endJump);
}

static void string(bool canAssign) {

  emitConstant(OBJECT_VALUE(stringCopy(parser.previous.start + 1, 
                                       parser.previous.size - 2)));
}

static void namedVariable(Token name, bool canAssign) {

  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);

  if (arg != -1) {

    getOp = OPERATION_GET_LOCAL;
    setOp = OPERATION_SET_LOCAL;
  }
  else if ((arg = resolveUpvalue(current, &name)) != -1) {

    getOp = OPERATION_GET_UPVALUE;
    setOp = OPERATION_SET_UPVALUE;
  }
  else {

    arg = identifierConstant(&name);
    getOp = OPERATION_GET_GLOBAL;
    setOp = OPERATION_SET_GLOBAL;
  }

  if (canAssign && match(TOKEN_EQUAL)) {

    expression();
    emitBytes(setOp, (uint8_t)arg);
  }
  else {

    emitBytes(getOp, (uint8_t)arg);
  }
}

static void variable(bool canAssign) {

  namedVariable(parser.previous, canAssign);
}

static Token syntheticToken(const char* text) {

  Token token;
  token.start = text;
  token.size = (int)strlen(text);
  return token;
}

static void super_(bool canAssign) {

  if (currentClass == NULL) {

    error("Can't use 'super' outside of a class.");
  } 
  else if (!currentClass->hasSuperclass) {

    error("Can't use 'super' in a class with no superclass.");
  }

  consume(TOKEN_DOT, "Expect '.' after 'super'.");
  consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
  uint8_t name = identifierConstant(&parser.previous);

  namedVariable(syntheticToken("this"), false);
  if (match(TOKEN_LEFT_PAREN)) {

    uint8_t argCount = argumentList();
    namedVariable(syntheticToken("super"), false);
    emitBytes(OPERATION_SUPER_INVOKE, name);
    emitByte(argCount);
  } 
  else {

    namedVariable(syntheticToken("super"), false);
    emitBytes(OPERATION_GET_SUPER, name);
  }
}

static void this_(bool canAssign) {

  if (currentClass == NULL) {

    error("Can't use 'this' outside of a class.");
    return;
  }

  variable(false);
}

static void unary(bool canAssign) {

  TokenType operatorType = parser.previous.type;

  parsePrecedence(PRECEDENCE_UNARY);

  switch (operatorType) {

    case TOKEN_BANG: emitByte(OPERATION_NOT); break;
    case TOKEN_MINUS: emitByte(OPERATION_NEGATION); break;
    default: return;
  }
}

ParseRule rules[] = {

  [TOKEN_LEFT_PAREN]     = {grouping, call, PRECEDENCE_NONE},
  [TOKEN_RIGHT_PAREN]    = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_LEFT_BRACE]     = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_RIGHT_BRACE]    = {NULL, NULL, PRECEDENCE_NONE},
 
  [TOKEN_COMMA]          = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_DOT]            = {NULL, dot, PRECEDENCE_NONE},
  [TOKEN_SEMICOLON]      = {NULL, NULL, PRECEDENCE_NONE},

  [TOKEN_PLUS]           = {NULL, binary, PRECEDENCE_TERM},
  [TOKEN_MINUS]          = {unary, binary, PRECEDENCE_TERM},  
  [TOKEN_STAR]           = {NULL, binary, PRECEDENCE_FACTOR},
  [TOKEN_FWD_SLASH]      = {NULL, binary, PRECEDENCE_FACTOR},
  [TOKEN_CARET]          = {NULL, binary, PRECEDENCE_FACTOR},

  [TOKEN_BANG]           = {unary, NULL, PRECEDENCE_NONE},
  [TOKEN_BANG_EQUAL]     = {NULL, binary, PRECEDENCE_NONE},
  [TOKEN_EQUAL]          = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_IDENTITY]       = {NULL, binary, PRECEDENCE_NONE},
  [TOKEN_GREATER]        = {NULL, binary, PRECEDENCE_NONE},
  [TOKEN_GREATER_EQUAL]  = {NULL, binary, PRECEDENCE_NONE},
  [TOKEN_LESS]           = {NULL, binary, PRECEDENCE_NONE},
  [TOKEN_LESS_EQUAL]     = {NULL, binary, PRECEDENCE_NONE},

  [TOKEN_IDENTIFIER]     = {variable, NULL, PRECEDENCE_NONE},
  [TOKEN_STRING]         = {string, NULL, PRECEDENCE_NONE},
  [TOKEN_NUMBER]         = {number, NULL, PRECEDENCE_NONE},

  [TOKEN_AND]            = {NULL, and_, PRECEDENCE_AND},
  [TOKEN_CLASS]          = {NULL, NULL, PRECEDENCE_NONE},  
  [TOKEN_ELSE]           = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_FALSE]          = {literal, NULL, PRECEDENCE_NONE},
  [TOKEN_FOR]            = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_FUNCTION]       = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_IF]             = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_NIL]            = {literal, NULL, PRECEDENCE_NONE},
  [TOKEN_OR]             = {NULL, or_, PRECEDENCE_OR},
  [TOKEN_PRINT]          = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_RETURN]         = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_SUPER]          = {super_, NULL, PRECEDENCE_NONE},
  [TOKEN_THIS]           = {this_, NULL, PRECEDENCE_NONE},
  [TOKEN_TRUE]           = {literal, NULL, PRECEDENCE_NONE},
  [TOKEN_VARIABLE]       = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_WHILE]          = {NULL, NULL, PRECEDENCE_NONE},

  [TOKEN_ERROR]          = {NULL, NULL, PRECEDENCE_NONE},
  [TOKEN_EOF]            = {NULL, NULL, PRECEDENCE_NONE},
  
};

static ParseRule* getRule(TokenType type) {

  return &rules[type];
}

static void parsePrecedence(Precedence precedence) {

  advance();
  ParseFunction prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {

    error("Expect expression.");
    return;
  }

  bool canAssign = precedence <= PRECEDENCE_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {

    advance();
    ParseFunction infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {

    error("Invalid assignment target.");
  }
}

static void expression() {

  parsePrecedence(PRECEDENCE_ASSIGNMENT);
}

static bool identifiersEqual(Token* a, Token* b) {

  if (a->size != b->size) return false;
  return memcmp(a->start, b->start, a->size) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {

  for (int i = compiler->localCount-1; i >= 0; i--) {

    Local* local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {

      if(local->depth == -1) {

        error("Can't read local variable in its own initializer.");
      }

      return i;
    }
  }

  return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {

  int upvalueCount = compiler->function->upvalueCount;
  for (int i =0; i < upvalueCount; i++) {
    
    Upvalue* upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {}
    return i;
  }

  if (upvalueCount == UINT8_COUNT) {

    error("Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {

  if (compiler->enclosing == NULL) return -1;

  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {

    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t)local, true);
  }

  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {

    return addUpvalue(compiler, (uint8_t)upvalue, false);
  }

  return -1;
}

static void addLocal(Token name) {
  
  if (current->localCount == UINT8_COUNT) {

    error("Too many local variables in function.");
    return;
  }
  Local* local = &current ->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
  local->isCaptured = false;
}

static void declareVariable() {

  if (current->scopeDepth == 0) return;

  Token* name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) {

    Local* local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {

      break;
    }

    if (identifiersEqual(name, &local->name)) {

      error("Already a variable with this name in this scope.");
    }
  }

  addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage) {

  consume(TOKEN_IDENTIFIER, errorMessage);

  declareVariable();
  if (current->scopeDepth > 0) return 0;

  return  identifierConstant(&parser.previous);
}

static void markInitialized() {

  if (current->scopeDepth == 0) return;
  current->locals[current->localCount -1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {

  if (current->scopeDepth > 0) {

    markInitialized();
    return;
  }

  emitBytes(OPERATION_DEFINE_GLOBAL, global);
}

static void block() {

  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {

    declaration();
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {

  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
  if (!check(TOKEN_RIGHT_PAREN)) {
    
    do {

      current->function->arity++;
      if (current->function->arity > 255) {
        
        errorAtCurrent("Can't have more than 255 parameters.");
      }

      uint8_t constant = parseVariable("Expect parameter name.");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));

  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
  block();

  ObjectFunction* function = endCompiler();
  emitBytes(OPERATION_CLOSURE, makeConstant(OBJECT_VALUE(function)));

  for (int i = 0; i < function->upvalueCount; i++) {

    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }

}

static void bound() {

  consume(TOKEN_IDENTIFIER, "Expect method name.");
  uint8_t constant = identifierConstant(&parser.previous);

  FunctionType type = TYPE_BOUND_FUNCTION;
  if (parser.previous.size == 4 &&
      memcmp(parser.previous.start, "init", 4) == 0) {

    type = TYPE_INITIALIZER;
  }

  function(type);
  emitBytes(OPERATION_BOUND_FUNCTION, constant);
}

static void classDeclaration() {

  consume(TOKEN_IDENTIFIER, "Expect class name.");
  Token className = parser.previous; 
  uint8_t nameConstant = identifierConstant(&parser.previous);
  declareVariable();

  emitBytes(OPERATION_CLASS, nameConstant);
  defineVariable(nameConstant);

  ClassCompiler classCompiler;
  classCompiler.hasSuperclass = false;
  classCompiler.enclosing = currentClass;
  currentClass = &classCompiler;

  if (match(TOKEN_LESS)) {

    consume(TOKEN_IDENTIFIER, "Expect superclass name.");
    variable(false);
    if (identifiersEqual(&className, &parser.previous)) {

      error("A class can't inherit from itself.");
    }

    beginScope();
    addLocal(syntheticToken("super"));
    defineVariable(0);

    namedVariable(className, false);
    emitByte(OPERATION_INHERIT);
    classCompiler.hasSuperclass = true;
  }

  namedVariable(className, false);
  consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {

    bound();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
  emitByte(OPERATION_POP);
  if (classCompiler.hasSuperclass) {

    endScope();
  }

  currentClass = currentClass->enclosing;
}

static void funDeclaration() {
  
  uint8_t global = parseVariable("Expect function name.");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}

static void varDeclaration() {

  uint8_t global = parseVariable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {

    expression();
  }
  else {

    emitByte(OPERATION_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(global);
}

static void expressionStatement() {

  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OPERATION_POP);
}

static void forStatement() {

  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (match(TOKEN_SEMICOLON)) {}

  else if (match(TOKEN_VARIABLE)) {

    varDeclaration();
  } 
  else {

    expressionStatement();
  }

  int loopStart = currentChunk()->count;
  int exitJump = -1;
  if (!match(TOKEN_SEMICOLON)) {

    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    exitJump = emitJump(OPERATION_JUMP_IF_FALSE);
    emitByte(OPERATION_POP);
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

  if (!match(TOKEN_RIGHT_PAREN)) {

    int bodyJump = emitJump(OPERATION_JUMP);
    int incrementStart = currentChunk()->count;
    expression();
    emitByte(OPERATION_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
    
    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyJump);
  }

  statement();
  emitLoop(loopStart);

  if (exitJump != -1) {

    patchJump(exitJump);
    emitByte(OPERATION_POP);
  }

  endScope();
}

static void ifStatement() {

  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  int thenJump = emitJump(OPERATION_JUMP_IF_FALSE);
  emitByte(OPERATION_POP);
  statement();

  int elseJump = emitJump(OPERATION_JUMP);

  patchJump(thenJump);
  emitByte(OPERATION_POP);

  if (match(TOKEN_ELSE)) statement();
  patchJump(elseJump);
}

static void printStatement() {

  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OPERATION_PRINT);
}

static void returnStatement() {

  if (current->type == TYPE_SCRIPT) {

    error("Can't return from top-level code.");
  }

  if (match(TOKEN_SEMICOLON)) {

    emitReturn();
  } 
  else {

    if (current->type == TYPE_INITIALIZER) {

      error("Can't return a value from an initializer.");
    }

    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitByte(OPERATION_RETURN);
  } 
}

static void whileStatement() {

  int loopStart = currentChunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  
  int exitJump = emitJump(OPERATION_JUMP_IF_FALSE);
  emitByte(OPERATION_POP);
  statement();
  emitLoop(loopStart);
  
  patchJump(exitJump);
  emitByte(OPERATION_POP);
}

static void synchronize() {

  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {

    if (parser.previous.type == TOKEN_SEMICOLON) return;
    switch (parser.current.type) {

      case TOKEN_CLASS:
      case TOKEN_FUNCTION:
      case TOKEN_VARIABLE:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;
      default:
      ;
    }

    advance();
  }
}

static void declaration() {

  if (match(TOKEN_CLASS)) {

    classDeclaration();
  }
  else if (match(TOKEN_FUNCTION)) {

    funDeclaration();
  }
  else if (match(TOKEN_VARIABLE)) {

    varDeclaration();
  }
  else {

    statement();
  }

  if (parser.panicMode) synchronize();
}

static void statement() {

  if (match(TOKEN_PRINT)) {

    printStatement();
  }
  else if (match(TOKEN_FOR)) {

    forStatement();
  }
  else if (match(TOKEN_IF)) {

    ifStatement();
  }
  else if (match(TOKEN_RETURN)) {

    returnStatement();
  }
  else if (match(TOKEN_WHILE)) {

    whileStatement();
  }
  else if (match(TOKEN_LEFT_BRACE)) {

    beginScope();
    block();
    endScope();
  }
  else {
    
    expressionStatement();
  }
}

ObjectFunction* compile(const char* input) {

  initLexer(input);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  parser.hadError = false;
  parser.panicMode = false;

  advance();

  while (!match(TOKEN_EOF)) {

    declaration();
  }

  ObjectFunction* function = endCompiler();
  return parser.hadError ? NULL : function;
}

void compilerCollectGarbage() {

  Compiler* compiler = current;
  while (compiler != NULL) {
    
    objectMarkGarbage((Object*)compiler->function);
    compiler = compiler->enclosing;
  }
}