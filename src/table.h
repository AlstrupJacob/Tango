#ifndef tango_table_h
#define tango_table_h

#include "util.h"
#include "value.h"

typedef struct {
  ObjectString* key;
  Value value;
} Pair;

typedef struct {    
  int count;
  int size;
  Pair* pairs;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGetValue(Table* table, ObjectString* key, Value* value);
bool tableSetValue(Table* table, ObjectString* key, Value value);
bool tableRemoveValue(Table* table, ObjectString* key);
void tableCopyTo(Table* from, Table* to);
ObjectString* tableGetString(Table* table, const char* string, 
                             int size, uint32_t hash);
void tableRemoveGarbage(Table* table);
void tableCollectGarbage(Table* table);

#endif