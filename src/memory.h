#ifndef tango_memory_h
#define tango_memory_h

#include "util.h"
#include "object.h"

#define ARRAY_SIZE_INCREASE_MULTIPLIER 2
#define ARRAY_SIZE_DECREASE_MULTIPLIER 0.5 // ?

#define ALLOCATE(type, count) \
  (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define INCREASE_SIZE(size) \
  ((size) < 8 ? 8 : (size) * ARRAY_SIZE_INCREASE_MULTIPLIER)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
  (type*)reallocate(pointer, sizeof(type) * (oldCount), \
                    sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
  reallocate(pointer, sizeof(type) * oldCount, 0)

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void objectMarkGarbage(Object* object);
void valueMarkGarbage(Value value);
void collectGarbage();
void freeObjects();

#endif