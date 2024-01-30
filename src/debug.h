#ifndef tango_debug_h
#define tango_debug_h

#include "chunk.h"

void chunkDissasemble(Chunk* chunk, const char* name);
int instructionDissasemble(Chunk* chunk, int offset);

#endif