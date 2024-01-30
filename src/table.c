#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {

  table->count = 0;
  table->size = 0;
  table->pairs = NULL;
}

void freeTable(Table* table) {

  FREE_ARRAY(Pair, table->pairs, table->size);
  initTable(table);
}

static Pair* getPair(Pair* pairs, int size, ObjectString* key) {
  
  uint32_t index = key->hash & (size - 1);
  Pair* tombstone = NULL;
  for (;;) {

    Pair* pair = &pairs[index];
    if (pair->key == NULL) {

      if (IS_NIL(pair->value)) {

        return tombstone != NULL ? tombstone : pair;
      } 
      else {

        if (tombstone == NULL) tombstone = pair;
      }
    } 
    else if (pair->key == key) {

      return pair;
    }

    index = (index + 1) & (size - 1);
  }
}

bool tableGetValue(Table* table, ObjectString* key, Value* value) {

  if (table->count == 0) return false;

  Pair* pair = getPair(table->pairs, table->size, key);
  if (pair->key == NULL) return false;

  *value = pair->value;
  return true;
}

static void resize(Table* table, int size) {

  Pair* pairs = ALLOCATE(Pair, size);
  table->count = 0;
  for (int i = 0; i < size; i++) {

    pairs[i].key = NULL;
    pairs[i].value = NIL_VAL;
  }

  for (int i = 0; i < table->size; i++) {

    Pair* pair = &table->pairs[i];
    if (pair->key == NULL) continue;

    Pair* destination = getPair(pairs, size, pair->key);
    destination->key = pair->key;
    destination->value = pair->value;
    table->count++;
  }

  FREE_ARRAY(Pair, table->pairs, table->size);
  table->pairs = pairs;
  table->size = size;
}

bool tableSetValue(Table* table, ObjectString* key, Value value) {
  
  if (table->count + 1 > table->size * TABLE_MAX_LOAD) {
    
    int size = INCREASE_SIZE(table->size);
    resize(table, size);
  }

  Pair* pair = getPair(table->pairs, table->size, key);
  bool newKey = pair->key == NULL;
  if (newKey && IS_NIL(pair->value)) table->count++;

  pair->key = key;
  pair->value = value;
  return newKey;
}

bool tableRemoveValue(Table* table, ObjectString* key) {

  if (table->count == 0) return false;

  Pair* pair = getPair(table->pairs, table->size, key);
  if (pair->key == NULL) return false;

  pair->key = NULL;
  pair->value = BOOLEAN_VALUE(true);
  return true;
}

void tableCopyTo(Table* from, Table* to) {

  for (int i = 0; i < from->size; i++) {

    Pair* pair = &from->pairs[i];
    if (pair->key != NULL) {

      tableSetValue(to, pair->key, pair->value);
    }
  }
}

ObjectString* tableGetString(Table* table, const char* string,
                              int size, uint32_t hash) {

  if (table->count == 0) return NULL;

  uint32_t index = hash & (table->size - 1);
  for (;;) {

    Pair* pair = &table->pairs[index];
    if (pair->key == NULL) {

      if (IS_NIL(pair->value)) return NULL;
    } 
    else if (pair->key->size == size &&
             pair->key->hash == hash &&
             memcmp(pair->key->string, string, size) == 0) {

        return pair->key;
    }

    index = (index + 1) & (table->size - 1);
  }
}

void tableCollectGarbage(Table* table) {

  for (int i = 0; i < table->size; i++) {

    Pair* pair = &table->pairs[i];
    objectMarkGarbage((Object*)pair->key);
    valueMarkGarbage(pair->value);
  }
}

void tableRemoveGarbage(Table* table) {

  for (int i = 0; i < table->size; i++) {

    Pair* pair = &table->pairs[i];
    if (pair->key != NULL && !pair->key->object.isGarbage) {

      tableRemoveValue(table, pair->key);
    }
  }
}