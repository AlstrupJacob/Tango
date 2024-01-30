#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"

void initValueArray(ValueArray* array) {

  array->values = NULL;
  array->size = 0;
  array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {

  if (array->size < array->count + 1) {

    int oldSize = array->size;
    array->size = INCREASE_SIZE(oldSize);
    array->values = GROW_ARRAY(Value, array->values, 
                               oldSize, array->size);
  }
  
  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(ValueArray* array) {

  FREE_ARRAY(Value, array->values, array->size);
  initValueArray(array);
}

void valuePrint(Value value) {

  if (IS_BOOL(value)) {

    printf(AS_BOOL(value) ? "true" : "false");
  } 
  else if (IS_NIL(value)) {

    printf("nil");
  } 
  else if (IS_NUMBER(value)) {

    printf("%g", AS_NUMBER(value));
  } 
  else if (IS_OBJECT(value)) {

    objectPrint(value);
  }
}

bool valuesEqual(Value a, Value b) {
    
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    
    return AS_NUMBER(a) == AS_NUMBER(b);
  }
  return a == b;
}