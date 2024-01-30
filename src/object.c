#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "virtualmachine.h"

#define ALLOCATE_OBJECT(type, objectType) \
  (type*)objectAllocate(sizeof(type), objectType)

static Object* objectAllocate(size_t size, ObjectType type) {
 
  Object* object = (Object*)reallocate(NULL, 0, size);
  object->type = type;
  object->isGarbage = false;

  object->next = virtualmachine.objects;
  virtualmachine.objects = object;

#ifdef DEBUG_LOG_GARBAGE_COLLECTION
  printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

  return object;
}

ObjectBoundFunction* newBoundFunction(Value receiver, 
                                      ObjectClosure* function) {

  ObjectBoundFunction* bound = ALLOCATE_OBJECT(ObjectBoundFunction,
                                               OBJECT_BOUND_FUNCTION);
  bound->receiver = receiver;
  bound->function = function;
  return bound;
}

ObjectClass* newClass(ObjectString* name) {

  ObjectClass* cclass = ALLOCATE_OBJECT(ObjectClass, OBJECT_CLASS);
  cclass->name = name;
  initTable(&cclass->methods);
  return cclass;
}

ObjectClosure* newClosure(ObjectFunction* function) {

  ObjectUpvalue** upvalues = ALLOCATE(ObjectUpvalue*, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {

    upvalues[i] = NULL;
  }

  ObjectClosure* closure = ALLOCATE_OBJECT(ObjectClosure, OBJECT_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjectFunction* newFunction() {

  ObjectFunction* function = ALLOCATE_OBJECT(ObjectFunction, OBJECT_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjectInstance* newInstance(ObjectClass* cclass) {

  ObjectInstance* instance = ALLOCATE_OBJECT(ObjectInstance, OBJECT_INSTANCE);
  instance->cclass = cclass;
  initTable(&instance->fields);
  return instance;
}

ObjectNativeFunction* newNativeFunction(NativeFunction function) {

  ObjectNativeFunction* native = ALLOCATE_OBJECT(ObjectNativeFunction, 
                                                 OBJECT_NATIVE_FUNCTION);
  native->function = function;
  return native;
}

ObjectUpvalue* newUpvalue(Value* slot) {

  ObjectUpvalue* upvalue = ALLOCATE_OBJECT(ObjectUpvalue, OBJECT_UPVALUE);
  upvalue->closed = NIL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

static ObjectString* stringAllocate(char* string, int size, uint32_t hash) {
  
  ObjectString* ostring = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING);
  ostring->size = size;
  ostring->string = string;
  ostring->hash = hash;

  stackPush(OBJECT_VALUE(string));
  tableSetValue(&virtualmachine.strings, ostring, NIL_VAL);
  stackPop();
  
  return ostring;
}

ObjectString* stringTake(char* string, int size) {

  uint32_t hash = stringHash(string, size);
  ObjectString* interned = tableGetString(&virtualmachine.strings, string, size, hash);
  if (interned != NULL) {

    FREE_ARRAY(char, string, size + 1);
    return interned;
  }

  return stringAllocate(string, size, hash);
}

ObjectString* stringCopy(const char* string, int size) {

  uint32_t hash = stringHash(string, size);
  ObjectString* interned = tableGetString(&virtualmachine.strings, string, size, hash);
  if (interned != NULL) return interned;

  char* heapString = ALLOCATE(char, size + 1);
  memcpy(heapString, string, size);
  heapString[size] = '\0';
  return stringAllocate(heapString, size, hash);
}

static uint32_t stringHash(const char* string, int size) {

  uint32_t hash = 2166136261u;
  for (int i = 0; i < size; i++) {

    hash ^= (uint8_t)string[i];
    hash *= 16777619;
  }

  return hash;
}

static void functionPrint(ObjectFunction* function) {

  if (function->name == NULL) {

    printf("<script>");
    return;
  }

  printf("<fn %s>", function->name->string);
}

void objectPrint(Value value) {
  
  switch (OBJECT_TYPE(value)) {

    case OBJECT_BOUND_FUNCTION:

      functionPrint(AS_BOUND_FUNCTION(value)->function->function);
      break;

    case OBJECT_CLASS:

      printf("%s", AS_CLASS(value)->name->string);
      break;

    case OBJECT_CLOSURE:

      functionPrint(AS_CLOSURE(value)->function);
      break;
      
    case OBJECT_FUNCTION:

      functionPrint(AS_FUNCTION(value));
      break;

    case OBJECT_INSTANCE:

      printf("%s instance",
        AS_INSTANCE(value)->cclass->name->string);
      break;

    case OBJECT_NATIVE_FUNCTION:

      printf("<native fn>");
      break;
      
    case OBJECT_STRING:

      printf("%s", AS_CSTRING(value));
      break;

    case OBJECT_UPVALUE:

      printf("upvalue.");
      break;
  }
}