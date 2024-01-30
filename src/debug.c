#include <stdio.h>

#include "debug.h"
#include "object.h"
#include "value.h"

void chunkDissasemble(Chunk* chunk, const char* name) {

  printf("====    %s              ==== \n\n", name);

  for (int offset = 0; offset < chunk->count;) {

    offset = instructionDissasemble(chunk, offset);
  }
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {

  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  valuePrint(chunk->constants.values[constant]);
  printf("\n");

  return offset + 2;
}

static int invokeInstruction(const char* name, Chunk* chunk,
                             int offset) {
                              
  uint8_t constant = chunk->code[offset + 1];
  uint8_t argCount = chunk->code[offset + 2];
  printf("%-16s (%d args) %4d '", name, argCount, constant);
  valuePrint(chunk->constants.values[constant]);
  printf("'\n");

  return offset + 3;
}


int instructionDissasemble(Chunk* chunk, int offset) {
     
  printf("%04d ", offset);
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset-1]) {

    printf("   | ");
  }
  else {

    printf("%4d ", chunk->lines[offset]);
  }
  
  uint8_t instruction = chunk->code[offset];
  switch (instruction) {

    case OPERATION_CONSTANT:

      return constantInstruction("OP_CONSTANT", chunk, offset);
    
        case OPERATION_NIL:

      return simpleInstruction("OP_NIL", offset);
    
        case OPERATION_TRUE:

      return simpleInstruction("OP_TRUE", offset);
    
        case OPERATION_FALSE:
      
          return simpleInstruction("OP_FALSE", offset);
    
        case OPERATION_POP:
      
          return simpleInstruction("OP_POP", offset);
    
        case OPERATION_GET_LOCAL:

      return byteInstruction("OP_GET_LOCAL", chunk, offset);
    
        case OPERATION_SET_LOCAL:

      return byteInstruction("OP_SET_LOCAL", chunk, offset);
    
        case OPERATION_GET_GLOBAL:

      return constantInstruction("OP_GET_GLOBAL", chunk, offset);
    
        case OPERATION_DEFINE_GLOBAL:

      return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
    
        case OPERATION_SET_GLOBAL:
      
          return constantInstruction("OP_SET_GLOBAL", chunk, offset);
    
        case OPERATION_GET_UPVALUE:

      return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    
        case OPERATION_SET_UPVALUE:

      return byteInstruction("OP_SET_UPVALUE", chunk, offset);
    
        case OPERATION_GET_PROPERTY:
        
      return constantInstruction("OP_GET_PROPERTY", chunk, offset);
    
        case OPERATION_SET_PROPERTY:
      
          return constantInstruction("OP_SET_PROPERTY", chunk, offset);
    
        case OPERATION_EQUALITY:

      return simpleInstruction("OP_EQUAL", offset);
    
        case OPERATION_GET_SUPER:

      return constantInstruction("OP_GET_SUPER", chunk, offset);
    
        case OPERATION_GREATER:

      return simpleInstruction("OP_GREATER", offset);
    
        case OPERATION_LESS:

      return simpleInstruction("OP_LESS", offset);
    
        case OPERATION_ADDITION:

      return simpleInstruction("OP_ADD", offset);
    
        case OPERATION_SUBTRACTION:
        
      return simpleInstruction("OP_SUBTRACT", offset);
    
        case OPERATION_MULTIPLICATION:

      return simpleInstruction("OP_MULTIPLY", offset);
    
        case OPERATION_DIVISION:

      return simpleInstruction("OP_DIVIDE", offset);

        case OPERATION_EXPONENTIATION:

      return simpleInstruction("OP_EXPONENTIATION", offset);
    
        case OPERATION_NOT:

      return simpleInstruction("OP_NOT", offset);
    
        case OPERATION_NEGATION:

      return simpleInstruction("OP_NEGATE", offset);
    
        case OPERATION_PRINT:

      return simpleInstruction("OP_PRINT", offset);

    case OPERATION_JUMP:

      return jumpInstruction("OP_JUMP", 1, chunk, offset);
    
        case OPERATION_JUMP_IF_FALSE:

      return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    
        case OPERATION_LOOP:
        
      return jumpInstruction("OP_LOOP", -1, chunk, offset);
    
        case OPERATION_CALL:

      return byteInstruction("OP_CALL", chunk, offset);
    
        case OPERATION_INVOKE:

      return invokeInstruction("OP_INVOKE", chunk, offset);
    
        case OPERATION_SUPER_INVOKE:

      return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
    
        case OPERATION_CLOSURE: {

      offset++;
      uint8_t constant = chunk->code[offset++];
      printf("%-16s %4d ", "OP_CLOSURE", constant);
      valuePrint(chunk->constants.values[constant]);
      printf("\n");

      ObjectFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
      for (int j = 0; j < function->upvalueCount; j++) {

        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d | %s %d\n",
               offset - 2, isLocal ? "local" : "upvalue", index);
      }

      return offset;
    }

    case OPERATION_CLOSE_UPVALUE: 

      return simpleInstruction("OP_CLOSE_UPVALUE", offset);

    case OPERATION_RETURN:

      return simpleInstruction("OP_RETURN", offset);
    
        case OPERATION_CLASS:

      return constantInstruction("OP_CLASS", chunk, offset);
    
        case OPERATION_BOUND_FUNCTION:

      return constantInstruction("OP_METHOD", chunk, offset);
    
        case OPERATION_INHERIT:

      return simpleInstruction("OP_INHERIT", offset);
    
        default:

      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}

static int simpleInstruction(const char* name, int offset) {

  printf("%s\n", name);
  return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    
  uint8_t slot = chunk->code[offset + 1];
  printf("%16s %4d\n", name, slot);
  return offset + 2;
}

static int jumpInstruction(const char* name, int sign, 
                                                 Chunk* chunk, int offset) {

  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}
