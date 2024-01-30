#ifndef tango_chunk_h
#define tango_chunk_h

#include "util.h"
#include "value.h"


typedef enum {

  OPERATION_POP,
  OPERATION_CONSTANT,

  OPERATION_TRUE,
  OPERATION_FALSE,

  OPERATION_EQUALITY,
  OPERATION_GREATER,
  OPERATION_LESS,

  OPERATION_ADDITION,
  OPERATION_SUBTRACTION,
  OPERATION_MULTIPLICATION,
  OPERATION_DIVISION,
  OPERATION_EXPONENTIATION,

  OPERATION_NOT,
  OPERATION_NIL,
  OPERATION_NEGATION,

  OPERATION_GET_LOCAL,
  OPERATION_SET_LOCAL,
  OPERATION_GET_GLOBAL,
  OPERATION_DEFINE_GLOBAL,
  OPERATION_SET_GLOBAL,
  OPERATION_GET_UPVALUE,
  OPERATION_SET_UPVALUE,
  OPERATION_GET_PROPERTY,
  OPERATION_SET_PROPERTY,
  OPERATION_GET_SUPER,

  OPERATION_PRINT,
  OPERATION_JUMP,
  OPERATION_JUMP_IF_FALSE,
  OPERATION_LOOP,
  OPERATION_CALL,
  OPERATION_INVOKE,
  OPERATION_SUPER_INVOKE,
  OPERATION_CLOSURE,
  OPERATION_CLOSE_UPVALUE,

  OPERATION_CLASS,
  OPERATION_INHERIT,
  OPERATION_BOUND_FUNCTION,

  OPERATION_RETURN,

} Operation;

typedef struct {
  int count;
  int size;
  uint8_t* code;
  int* lines;
  ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);

#endif