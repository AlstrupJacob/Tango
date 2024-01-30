#ifndef tango_virtualmachine_h
#define tango_virtualmachine_h

#include "object.h"
#include "table.h"
#include "value.h"

#define MAX_FRAMES 64
#define STACK_MAX_LOAD (MAX_FRAMES * UINT8_COUNT)

typedef struct {
  ObjectClosure* closure;
  uint8_t* ip;
  Value* slots;
} CallFrame;

typedef struct {
  CallFrame frames[MAX_FRAMES];
  int frameCount;
  Value stack[STACK_MAX_LOAD];
  Value* stackTop;
  Table globals;
  Table strings;
  ObjectString* initString;
  ObjectUpvalue* openUpvalues;

  size_t bytesAllocated;
  size_t nextGC;
  Object* objects;
  int grayCount;
  int grayCapacity;
  Object** grayStack;
} VirtualMachine;

typedef enum {
  INTERPRET_OK,
  INTERPRET_ERROR_COMPILE,
  INTERPRET_ERROR_RUNTIME,
} InterpretResult;

extern VirtualMachine virtualmachine;

void initVirtualMachine();
void freeVirtualMachine();
InterpretResult interpret(const char* input);
void stackPush(Value value);
Value stackPop();

#endif