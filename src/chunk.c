#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "virtualmachine.h"

void initChunk(Chunk* chunk) {

  chunk->count = 0;
  chunk->size = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {

  FREE_ARRAY(uint8_t, chunk->code, chunk->size);
  FREE_ARRAY(int, chunk->lines, chunk->size);
  freeValueArray(&chunk->constants);
  initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {

  if (chunk->size < chunk->count +1) {

    int oldSize = chunk->size;
    chunk->size = INCREASE_SIZE(oldSize);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, 
                             oldSize, chunk->size);
    chunk->lines = GROW_ARRAY(int, chunk->lines,
                              oldSize, chunk->size);
  }

  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

int addConstant(Chunk* chunk, Value value) {

  stackPush(value);
  writeValueArray(&chunk->constants, value);
  stackPop();
  return chunk->constants.count -1;
}