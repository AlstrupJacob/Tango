#ifndef tango_value_h
#define tango_value_h

#include <string.h>
#include "util.h"

typedef struct Object Object;
typedef struct ObjectString ObjectString;

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN ((uint64_t)0x7ffc000000000000)

#define TAG_NIL 1
#define TAG_FALSE 2
#define TAG_TRUE 3

typedef uint64_t Value;

typedef struct {
  int count;
  int size;
  Value* values;
} ValueArray;

#define IS_BOOL(value) (((value) | 1) == TRUE_VALUE)
#define IS_NIL(value) ((value) == NIL_VAL)
#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define IS_OBJECT(value) \
  (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value) ((value) == TRUE_VALUE)
#define AS_NUMBER(value) valueToNum(value)
#define AS_OBJECT(value) \
  ((Object*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOLEAN_VALUE(b) ((b) ? TRUE_VALUE : FALSE_VALUE)
#define FALSE_VALUE ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VALUE ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL ((Value)(uint64_t)(QNAN | TAG_NIL))
#define NUMBER_VALUE(num) numToValue(num)
#define OBJECT_VALUE(object) \
  (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(object))

static inline double valueToNum(Value value) {
  
  double number;
  memcpy(&number, &value, sizeof(Value));
  return number;
}

static inline Value numToValue(double number) {

  Value value;
  memcpy(&value, &number, sizeof(double));
  return value;
}

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void valuePrint(Value value);

#endif 