#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "util.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "virtualmachine.h"

static Value clockNative(int argCount, Value* args) {

  return NUMBER_VALUE((double)clock() / CLOCKS_PER_SEC);
}

VirtualMachine virtualmachine;

static void cleanStack() {

  virtualmachine.stackTop = virtualmachine.stack;
  virtualmachine.frameCount = 0;
  virtualmachine.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {

  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = virtualmachine.frameCount - 1; i >= 0; i--) {

    CallFrame* frame = &virtualmachine.frames[i];
    ObjectFunction* function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ",
            function->chunk.lines[instruction]);

    if (function->name == NULL) {

      fprintf(stderr, "script\n");
    } 
    else {

      fprintf(stderr, "%s()\n", function->name->string);
    }
  }

  cleanStack();
}

void stackPush(Value value) {
  
  *virtualmachine.stackTop = value;
  virtualmachine.stackTop++;
}

Value stackPop() {

  virtualmachine.stackTop--;
  return *virtualmachine.stackTop;
}

static void defineNativeFunction(const char* name, NativeFunction function) {

  stackPush(OBJECT_VALUE(stringCopy(name, (int)strlen(name))));
  stackPush(OBJECT_VALUE(newNativeFunction(function)));
  tableSetValue(&virtualmachine.globals, AS_STRING(virtualmachine.stack[0]), virtualmachine.stack[1]);

  stackPop();
  stackPop();
}

void initVirtualMachine() {

  cleanStack();
  virtualmachine.objects = NULL;
  virtualmachine.bytesAllocated = 0;
  virtualmachine.nextGC = 1024 * 1024;

  virtualmachine.grayCount = 0;
  virtualmachine.grayCapacity = 0;
  virtualmachine.grayStack = NULL;

  initTable(&virtualmachine.globals);
  initTable(&virtualmachine.strings);

  virtualmachine.initString = NULL;
  virtualmachine.initString = stringCopy("init", 4);

  defineNativeFunction("clock", clockNative);
}

void freeVirtualMachine() {

  freeTable(&virtualmachine.globals);
  freeTable(&virtualmachine.strings);
  virtualmachine.initString = NULL;
  freeObjects();
}

static Value peek(int depth) {

  return virtualmachine.stackTop[-1-depth];
}

static bool call(ObjectClosure* closure, int argCount) {

  if (argCount != closure->function->arity) {

    runtimeError("Expected %d arguments but got %d.",
                 closure->function->arity, argCount);
    return false;
  }

  if (virtualmachine.frameCount == MAX_FRAMES) {

    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame* frame = &virtualmachine.frames[virtualmachine.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = virtualmachine.stackTop - argCount - 1;
  return true;
}

static bool callValue(Value callee, int argCount) {

  if (IS_OBJECT(callee)) {

    switch (OBJECT_TYPE(callee)) {

      case OBJECT_BOUND_FUNCTION: {

        ObjectBoundFunction* bound = AS_BOUND_FUNCTION(callee);
        virtualmachine.stackTop[-argCount - 1] = bound->receiver;
        return call(bound->function, argCount);
      }

      case OBJECT_CLASS: {

        ObjectClass* cclass = AS_CLASS(callee);
        virtualmachine.stackTop[-argCount - 1] = OBJECT_VALUE(newInstance(cclass));
        Value initializer;

        if (tableGetValue(&cclass->methods, virtualmachine.initString,
            &initializer)) {

          return call(AS_CLOSURE(initializer), argCount);
        }
        else if (argCount != 0) {

          runtimeError("Expected 0 arguments but got %d.",
                       argCount);
          return false;
        }
        return true;
      }

      case OBJECT_CLOSURE:

        return call(AS_CLOSURE(callee), argCount);

      case OBJECT_NATIVE_FUNCTION: {

        NativeFunction native = AS_NATIVE_FUNCTION(callee);
        Value result = native(argCount, virtualmachine.stackTop - argCount);
        virtualmachine.stackTop -= argCount + 1;
        stackPush(result);
        return true;
      }

      default: break;
    }
  }

  runtimeError("Can only call functions and classes.");
  return false;
}

static bool invokeFromClass(ObjectClass* cclass, ObjectString* name, int argCount) {

  Value method;
  if (!tableGetValue(&cclass->methods, name, &method)) {

    runtimeError("Undefined property '%s'.", name->string);
    return false;
  }

  return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjectString* name, int argCount) {

  Value receiver = peek(argCount);
  if (!IS_INSTANCE(receiver)) {

    runtimeError("Only instances have methods.");
    return false;
  }

  ObjectInstance* instance = AS_INSTANCE(receiver);
  Value value;
  if (tableGetValue(&instance->fields, name, &value)) {

    virtualmachine.stackTop[-argCount - 1] = value;
    return callValue(value, argCount);
  }

  return invokeFromClass(instance->cclass, name, argCount);
}

static bool bindFunction(ObjectClass* cclass, ObjectString* name) {

  Value method;
  if (!tableGetValue(&cclass->methods, name, &method)) {

    runtimeError("Undefined property '%s'.", name->string);
    return false;
  }

  ObjectBoundFunction* bound = newBoundFunction(peek(0), AS_CLOSURE(method));
  stackPop();
  stackPush(OBJECT_VALUE(bound));

  return true;
}

static ObjectUpvalue* bindUpvalue(Value* local) {

  ObjectUpvalue* prevUpvalue = NULL;
  ObjectUpvalue* upvalue = virtualmachine.openUpvalues;
  while (upvalue != NULL && upvalue->location == local) {

    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {

    return upvalue;
  }

  ObjectUpvalue* createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;
  if (prevUpvalue == NULL) {

    virtualmachine.openUpvalues = createdUpvalue;
  }
  else {

    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(Value* last) {

  while (virtualmachine.openUpvalues != NULL && 
         virtualmachine.openUpvalues->location >= last) {

    ObjectUpvalue* upvalue = virtualmachine.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    virtualmachine.openUpvalues = upvalue->next;
  }
}

static void defineBoundFunction(ObjectString* name) {

  Value method = peek(0);
  ObjectClass* cclass = AS_CLASS(peek(1));
  tableSetValue(&cclass->methods, name, method);
  stackPop();
}

static bool isFalsey(Value value) {

  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {

  ObjectString* b = AS_STRING(peek(0));
  ObjectString* a = AS_STRING(peek(1));

  int size = a->size + b->size;
  char* chars = ALLOCATE(char, size + 1);
  memcpy(chars, a->string, a->size);
  memcpy(chars + a->size, b->string, b->size);
  chars[size] = '\0';

  ObjectString* result = stringTake(chars, size);
  stackPop(); 
  stackPop();
  stackPush(OBJECT_VALUE(result));
}

static InterpretResult run() {

  CallFrame* frame = &virtualmachine.frames[virtualmachine.frameCount - 1];

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])

#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OPERATION(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_ERROR_RUNTIME; \
      } \
      \
      double b = AS_NUMBER(stackPop()); \
      double a = AS_NUMBER(stackPop()); \
      stackPush(valueType(a op b)); \
    } while (false)

  for (;;) {

#ifndef DEBUG_TRACE_EXECUTION
  printf("        ");
  for (Value* slot = virtualmachine.stack; slot < virtualmachine.stackTop; slot++) {

    printf("[ ");
    valuePrint(*slot);
    printf(" ]");
  }

  printf("\n");
  dissasembleInstruction(&frame->closure->function->chunk,
                         (int)(frame->ip - frame->closure->function->chunk.code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {

      case OPERATION_CONSTANT: {

        Value constant = READ_CONSTANT();
        stackPush(constant);
        break;
      }
      case OPERATION_NIL:   stackPush(NIL_VAL); break;
      case OPERATION_TRUE:  stackPush(BOOLEAN_VALUE(true)); break;
      case OPERATION_FALSE: stackPush(BOOLEAN_VALUE(false)); break;
      case OPERATION_POP:   stackPop(); break;
      case OPERATION_GET_LOCAL: {

        uint8_t slot = READ_BYTE();
        stackPush(frame->slots[slot]);
        break;
      }
      case OPERATION_SET_LOCAL: {

        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OPERATION_GET_GLOBAL: {

        ObjectString* name = READ_STRING();
        Value value;
        if (!tableGetValue(&virtualmachine.globals, name, &value)) {

          runtimeError("Undefined variable '%s'.", name->string);
          return INTERPRET_ERROR_RUNTIME;
        }

        stackPush(value);
        break;
      }
      case OPERATION_DEFINE_GLOBAL: {

        ObjectString* name = READ_STRING();
        tableSetValue(&virtualmachine.globals, name, peek(0));
        stackPop();
        break;
      }
      case OPERATION_SET_GLOBAL: {

        ObjectString* name = READ_STRING();
        if (tableSetValue(&virtualmachine.globals, name, peek(0))) {

          tableRemoveValue(&virtualmachine.globals, name);
          runtimeError("Undefined variable '%s'.", name->string);
          return INTERPRET_ERROR_RUNTIME;
        }

        break;
      }
      case OPERATION_GET_UPVALUE: {

        uint8_t slot = READ_BYTE();
        stackPush(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OPERATION_SET_UPVALUE: {

        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OPERATION_GET_PROPERTY: {

        if (!IS_INSTANCE(peek(0))) {

          runtimeError("Only instances have properties.");
          return INTERPRET_ERROR_RUNTIME;
        }

        ObjectInstance* instance = AS_INSTANCE(peek(0));
        ObjectString* name = READ_STRING();

        Value value;
        if (tableGetValue(&instance->fields, name, &value)) {

          stackPop();
          stackPush(value);
          break;
        }
        if (!bindFunction(instance->cclass, name)) {

          return INTERPRET_ERROR_RUNTIME;
        }
        break;
      }
      case OPERATION_SET_PROPERTY: {

        if (!IS_INSTANCE(peek(1))) {

          runtimeError("Only instances have fields.");
          return INTERPRET_ERROR_RUNTIME;
        }
        
        ObjectInstance* instance = AS_INSTANCE(peek(1));
        tableSetValue(&instance->fields, READ_STRING(), peek(0));
        Value value = stackPop();
        stackPop();
        stackPush(value);
        break;
      }
      case OPERATION_EQUALITY: {

        Value b = stackPop();
        Value a = stackPop();
        stackPush(BOOLEAN_VALUE(valuesEqual(a,b)));
        break;
      }
      case OPERATION_GET_SUPER: {

        ObjectString* name = READ_STRING();
        ObjectClass* superclass = AS_CLASS(stackPop());

        if (!bindFunction(superclass, name)) {

          return INTERPRET_ERROR_RUNTIME;
        }
        break;
      }
      case OPERATION_GREATER:  BINARY_OPERATION(BOOLEAN_VALUE, >); break;
      case OPERATION_LESS:     BINARY_OPERATION(BOOLEAN_VALUE, <); break;
      case OPERATION_ADDITION: {

        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {

          concatenate();
        } 
        else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {

          double b = AS_NUMBER(stackPop());
          double a = AS_NUMBER(stackPop());
          stackPush(NUMBER_VALUE(a + b));
        } 
        else {

          runtimeError("Operands must be two numbers or two strings.");
          return INTERPRET_ERROR_RUNTIME;
        }
        break;
      }
      case OPERATION_SUBTRACTION:    BINARY_OPERATION(NUMBER_VALUE, -); break;
      case OPERATION_MULTIPLICATION: BINARY_OPERATION(NUMBER_VALUE, *); break;
      case OPERATION_DIVISION:       BINARY_OPERATION(NUMBER_VALUE, /); break;
      case OPERATION_EXPONENTIATION: {

        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {

          runtimeError("Operands must be numbers.");
          return INTERPRET_ERROR_RUNTIME;
        }
        
        double b = AS_NUMBER(stackPop());
        double a = AS_NUMBER(stackPop());
        stackPush(NUMBER_VALUE(pow(a, b)));
        break;
      }
      case OPERATION_NOT: {

        stackPush(BOOLEAN_VALUE(isFalsey(stackPop())));
        break;
      }
      case OPERATION_NEGATION: {

        if (!IS_NUMBER(peek(0))) {

          runtimeError("Operand must be a number.");
          return INTERPRET_ERROR_RUNTIME;
        }

        stackPush(NUMBER_VALUE(-AS_NUMBER(stackPop())));
        break;
      }
      case OPERATION_PRINT: {

        valuePrint(stackPop());
        printf("\n");
        break;
      }
      case OPERATION_JUMP: {

        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OPERATION_JUMP_IF_FALSE: {

        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) frame->ip += offset;
        break;
      }
      case OPERATION_LOOP: {

        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OPERATION_CALL: {

        int argCount = READ_BYTE();
        if (!callValue(peek(argCount), argCount)) {

          return INTERPRET_ERROR_RUNTIME;
        }

        frame = &virtualmachine.frames[virtualmachine.frameCount-1];
        break;
      }
      case OPERATION_INVOKE: {

        ObjectString* method = READ_STRING();
        int argCount = READ_BYTE();
        if (!invoke(method, argCount)) {

          return INTERPRET_ERROR_RUNTIME;
        }

        frame = &virtualmachine.frames[virtualmachine.frameCount - 1];
        break;
      }
      case OPERATION_SUPER_INVOKE: {

        ObjectString* method = READ_STRING();
        int argCount = READ_BYTE();
        ObjectClass* superclass = AS_CLASS(stackPop());
        if (!invokeFromClass(superclass, method, argCount)) {

          return INTERPRET_ERROR_RUNTIME;
        }

        frame = &virtualmachine.frames[virtualmachine.frameCount - 1];
        break;
      }
      case OPERATION_CLOSURE: {

        ObjectFunction* function = AS_FUNCTION(READ_CONSTANT());
        ObjectClosure* closure = newClosure(function);
        stackPush(OBJECT_VALUE(closure));

        for (int i = 0; i < closure->upvalueCount; i++) {

          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {

            closure->upvalues[i] = bindUpvalue(frame->slots + index);
          } 
          else {

            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }

        break;
      }
      case OPERATION_CLOSE_UPVALUE: {

        closeUpvalues(virtualmachine.stackTop - 1);
        stackPop();
        break;
      }
      case OPERATION_RETURN: {

        Value result = stackPop();
        closeUpvalues(frame->slots);
        virtualmachine.frameCount--;
        if (virtualmachine.frameCount == 0) {

          stackPop();
          return INTERPRET_OK;
        }

        virtualmachine.stackTop = frame->slots;
        stackPush(result);
        frame = &virtualmachine.frames[virtualmachine.frameCount - 1];
        break;
      }
      case OPERATION_CLASS: {

        stackPush(OBJECT_VALUE(newClass(READ_STRING())));
        break;
      }
      case OPERATION_INHERIT: {

        Value superclass = peek(1);
        if (!IS_CLASS(superclass)) {

          runtimeError("Superclass must be a class.");
          return INTERPRET_ERROR_RUNTIME;
        }

        ObjectClass* subclass = AS_CLASS(peek(0));
        tableCopyTo(&AS_CLASS(superclass)->methods, &subclass->methods);
        stackPop();
        break;
      }
      case OPERATION_BOUND_FUNCTION: {

        defineBoundFunction(READ_STRING());
        break;
      }
    }
  } 

#undef READ_BYTE;
#undef READ_SHORT;
#undef READ_CONSTANT;
#undef READ_STRING;
#undef BINARY_OPERATION;
}

InterpretResult interpret(const char* input) {

  ObjectFunction* function = compile(input);
  if (function == NULL) return INTERPRET_ERROR_COMPILE;

  stackPush(OBJECT_VALUE(function));
  ObjectClosure* closure = newClosure(function);
  stackPop();
  stackPush(OBJECT_VALUE(closure));
  call(closure, 0);

  return run();
}