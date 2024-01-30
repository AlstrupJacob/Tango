#ifndef tango_object_h
#define tango_object_h

#include "util.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_BOUND_FUNCTION(value) objectIsType(value, OBJECT_BOUND_FUNCTION)
#define IS_CLASS(value) objectIsType(value, OBJECT_CLASS)
#define IS_CLOSURE(value) objectIsType(value, OBJECT_CLOSURE)
#define IS_FUNCTION(value) objectIsType(value, OBJECT_FUNCTION)
#define IS_INSTANCE(value) objectIsType(value, OBJECT_INSTANCE)
#define IS_NATIVE_FUNCTION(value) objectIsType(value, OBJECT_NATIVE_FUNCTION)
#define IS_STRING(value) objectIsType(value, OBJECT_STRING)

#define AS_BOUND_FUNCTION(value) ((ObjectBoundFunction*)AS_OBJECT(value))
#define AS_CLASS(value) ((ObjectClass*)AS_OBJECT(value))
#define AS_CLOSURE(value) ((ObjectClosure*)AS_OBJECT(value))
#define AS_FUNCTION(value) ((ObjectFunction*)AS_OBJECT(value))
#define AS_INSTANCE(value) ((ObjectInstance*)AS_OBJECT(value))
#define AS_NATIVE_FUNCTION(value) \
    (((ObjectNativeFunction*)AS_OBJECT(value))->function)
#define AS_STRING(value) ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString*)AS_OBJECT(value))->string)

typedef enum {
  OBJECT_FUNCTION,  
  OBJECT_BOUND_FUNCTION,
  OBJECT_NATIVE_FUNCTION,
  OBJECT_CLASS,
  OBJECT_CLOSURE,
  OBJECT_INSTANCE,
  OBJECT_STRING,
  OBJECT_UPVALUE,
} ObjectType;

struct Object {
  ObjectType type;
  bool isGarbage;
  struct Object* next;
};

typedef struct {
  Object object;
  int arity;
  int upvalueCount;
  Chunk chunk;
  ObjectString* name;
} ObjectFunction;

typedef Value (*NativeFunction)(int argCount, Value* args);

typedef struct {
  Object object;
  NativeFunction function;
} ObjectNativeFunction;

struct ObjectString {
  Object object;
  int size;
  char* string;
  uint32_t hash;
};

typedef struct ObjectUpvalue {
  Object object;
  Value* location;
  Value closed;
  struct ObjectUpvalue* next;
} ObjectUpvalue;

typedef struct {
  Object object;
  ObjectFunction* function;
  ObjectUpvalue** upvalues;
  int upvalueCount;
} ObjectClosure;

typedef struct {
  Object object;
  ObjectString* name;
  Table methods;
} ObjectClass;

typedef struct {
  Object object;
  ObjectClass* cclass;
  Table fields;
} ObjectInstance;

typedef struct {
  Object object;
  Value receiver;
  ObjectClosure* function;
} ObjectBoundFunction;

ObjectBoundFunction* newBoundFunction(Value receiver,
                                      ObjectClosure* function);
ObjectClass* newClass(ObjectString* name);
ObjectClosure* newClosure(ObjectFunction* function);
ObjectFunction* newFunction();
ObjectInstance* newInstance(ObjectClass* cclass);
ObjectNativeFunction* newNativeFunction(NativeFunction function);
ObjectUpvalue* newUpvalue(Value* slot);
ObjectString* stringTake(char* string, int size);
ObjectString* stringCopy(const char* string, int size);

void objectPrint(Value value);

static inline bool objectIsType(Value value, ObjectType type) {
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif