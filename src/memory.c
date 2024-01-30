#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "virtualmachine.h"

#ifdef DEBUG_LOG_GARBAGE_COLLECTION
  #include <stdio.h>
  #include "debug.h"
#endif

#define GARBAGE_COLLECTOR_HEAP_SIZE_MULTIPLIER 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize)  {

  virtualmachine.bytesAllocated += newSize - oldSize;
  if (newSize > oldSize) {

#ifdef DEBUG_STRESS_GARBAGE_COLLECTION
  collectGarbage();
#endif
    
    if (virtualmachine.bytesAllocated > virtualmachine.nextGC) {

      collectGarbage();
    }
  }
  if (newSize == 0) {

    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}

void objectMarkGarbage(Object* object) {
    
  if (object == NULL) return;
  if (object->isGarbage) return;

#ifdef DEBUG_LOG_GARBAGE_COLLECTION
  printf("%p mark ", (void*)object);
  valuePrint(OBJECT_VALUE(object));
  printf("\n");
#endif

  object->isGarbage = true;

 if (virtualmachine.grayCapacity < virtualmachine.grayCount + 1) {
    
    virtualmachine.grayCapacity = INCREASE_SIZE(virtualmachine.grayCapacity);
    virtualmachine.grayStack = (Object**)realloc(virtualmachine.grayStack,
                               sizeof(Object*) * virtualmachine.grayCapacity);
    
    if (virtualmachine.grayStack == NULL) exit(1);
  }

  virtualmachine.grayStack[virtualmachine.grayCount++] = object;
}

void valueMarkGarbage(Value value) {

  if (IS_OBJECT(value)) objectMarkGarbage(AS_OBJECT(value));
}

static void arrayMarkGarbage(ValueArray* array) {

  for (int i = 0; i < array->count; i++) {

    valueMarkGarbage(array->values[i]);
  }
}

objectBlacken(Object* object) {

#ifdef DEBUG_LOG_GARBAGE_COLLECTION
  printf("%p blacken ", (void*)object);
  valuePrint(OBJECT_VALUE(object));
  printf("\n");
#endif

  switch(object->type) {

    case OBJECT_BOUND_FUNCTION: {

      ObjectBoundFunction* bound = (ObjectBoundFunction*)object;
      valueMarkGarbage(bound->receiver);
      objectMarkGarbage((Object*)bound->function);
      break;  
    }
    case OBJECT_CLASS: {

      ObjectClass* cclass = (ObjectClass*)object;
      objectMarkGarbage((Object*)cclass->name);
      tableCollectGarbage(&cclass->methods);
      break;
    }
    case OBJECT_CLOSURE: {
      
      ObjectClosure* closure = (ObjectClosure*)object;
      objectMarkGarbage((Object*)closure->function);
      for (int i = 0; i < closure->upvalueCount; i++) {

        objectMarkGarbage((Object*)closure->upvalues[i]);
      }
      break;
    }
    case OBJECT_INSTANCE: {
      
      ObjectInstance* instance = (ObjectInstance*)object;
      objectMarkGarbage((Object*)instance->cclass);
      tableCollectGarbage(&instance->fields);
      break;
    }
    case OBJECT_FUNCTION: {
      
      ObjectFunction* function = (ObjectFunction*)object;
      objectMarkGarbage((Object*)function->name);
      arrayMarkGarbage(&function->chunk.constants);
      break;
    }
    case OBJECT_UPVALUE: {

      valueMarkGarbage(((ObjectUpvalue*)object)->closed);
      break;
    }
    case OBJECT_NATIVE_FUNCTION:
    case OBJECT_STRING:

      break;
  }
}

objectFree(Object* object) {

#ifdef DEBUG_LOG_GARBAGE_COLLECTION
  printf("%p free type %d\n", (void*)object, object->type);
#endif

  switch (object->type) {

    case OBJECT_BOUND_FUNCTION: {

      FREE(ObjectBoundFunction, object);
      break;
    }
    case OBJECT_CLASS: {

      ObjectClass* cclass = (ObjectClass*)object;
      freeTable(&cclass->methods);
      FREE(ObjectClass, object);
      break;
    }
    case OBJECT_CLOSURE: {

      ObjectClosure* closure = (ObjectClosure*)object;
      FREE_ARRAY(ObjectUpvalue*, closure->upvalues, closure->upvalueCount);
      FREE(ObjectClosure, object);
      break;
    }
    case OBJECT_FUNCTION: {

      ObjectFunction* function = (ObjectFunction*)object;
      freeChunk(&function->chunk);
      FREE(ObjectFunction, object);
      break;
    }
    case OBJECT_INSTANCE: {

      ObjectInstance* instance = (ObjectInstance*)object;
      freeTable(&instance->fields);
      FREE(ObjectInstance, object);
      break;
    }
    case OBJECT_NATIVE_FUNCTION: {

      FREE(ObjectNativeFunction, object);
      break;
    }
    case OBJECT_STRING: {

      ObjectString* string = (ObjectString*)object;
      FREE_ARRAY(char, string->string, string->size + 1);
      FREE(ObjectString, object);
      break;
    }
    case OBJECT_UPVALUE: {

      FREE(ObjectUpvalue, object);
      break;
    }
  }
}

static void rootsCollectGarbage() {

  for (Value* slot = virtualmachine.stack; slot < virtualmachine.stackTop; slot++) {

    valueMarkGarbage(*slot);
  }

  for (int i = 0; i < virtualmachine.frameCount; i++) {

    objectMarkGarbage((Object*)virtualmachine.frames[i].closure);
  }

  for (ObjectUpvalue* upvalue = virtualmachine.openUpvalues;
       upvalue != NULL;
       upvalue = upvalue->next) {

    objectMarkGarbage((Object*)upvalue);
  }

  tableCollectGarbage(&virtualmachine.globals);
  compilerCollectGarbage();
  objectMarkGarbage((Object*)virtualmachine.initString);
}

static void referencesTrace() {

  while (virtualmachine.grayCount > 0) {

    Object* object = virtualmachine.grayStack[--virtualmachine.grayCount];
    objectBlacken(object);
  }
}

static void sweep() {
  
  Object* previous = NULL;
  Object* object = virtualmachine.objects;
  while (object != NULL) {

    if (object->isGarbage) {

      object->isGarbage = false;
      previous = object;
      object = object->next;
    } 
    else {

      Object* unreached = object;
      object = object->next;
      if (previous != NULL) {

        previous->next = object;
      } 
      else {

        virtualmachine.objects = object;
      }

      objectFree(unreached);
    }
  }
}

void collectGarbage() {

#ifdef DEBUG_LOG_GARBAGE_COLLECTION
  printf("-- gc begin\n");
  size_t before = virtualmachine.bytesAllocated;
#endif

  rootsCollectGarbage();
  referencesTrace();
  tableRemoveGarbage(&virtualmachine.strings);
  sweep();

  virtualmachine.nextGC = virtualmachine.bytesAllocated * GARBAGE_COLLECTOR_HEAP_SIZE_MULTIPLIER;

#ifdef DEBUG_LOG_GARBAGE_COLLECTION
  printf("-- gc end\n");
  printf(" collected %zu bytes (from %zu to %zu) next at %zu\n",
         before - virtualmachine.bytesAllocated, before, 
         virtualmachine.bytesAllocated, virtualmachine.nextGC);
#endif

}

void freeObjects() {
  
  Object* object = virtualmachine.objects;
  while (object != NULL) {

    Object* next = object->next;
    objectFree(object);
    object = next;
  }

  free(virtualmachine.grayStack);
}