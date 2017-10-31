/**
* simple hashmap implementation
* @author
*/

#ifndef lhashmap_h
#define lhashmap_h

#include <glua/lprefix.h>

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

/**
 * function to compare two keys
 */
typedef int(*LHashmap_compare)(const void *a, const void *b);

/**
 * function to hash keys
 */
typedef unsigned int(*LHashmap_hash)(const void *key);

typedef void* (*LHashmap_key_copy)(const void *key);

typedef void(*LHashmap_key_free)(const void *key);

typedef struct LArrayList
{
    size_t length;
    void **elements;
    size_t element_size;
    size_t max_size;
} LArrayList;

LArrayList *LArraylist_create(size_t element_size, size_t init_size);

size_t LArraylist_count(LArrayList *alist);

void LArraylist_destroy(LArrayList *alist);

void LArraylist_append(LArrayList *alist, void *data);

void *LArraylist_get(LArrayList *alist, size_t index);

void *LArraylist_pop(LArrayList *alist);

void LArraylist_set(LArrayList *alist, size_t index, void *data);

void *LArraylist_last(LArrayList *alist);

void *LArraylist_first(LArrayList *alist);

typedef struct LHashMapEntry
{
    void *key;
    void *data;
    size_t element_size;
    unsigned int hash;
} LHashMapEntry;

typedef struct LHashMap
{
    LHashmap_compare compare;
    LHashmap_hash hash;
    LHashmap_key_copy key_copy;
    LHashmap_key_free key_free;
    size_t element_size;
    LArrayList *buckets; // every bucket contains a list of LHashMapEntry entries, hash to buckets index
} LHashMap;

typedef int(*LHashmap_traverse_cb)(LHashMapEntry *entry);

LHashMap *LHashmap_create(LHashmap_compare compare, LHashmap_hash hash,
    LHashmap_key_copy key_copy, LHashmap_key_free key_free, size_t element_size);
void LHashmap_destroy(LHashMap *map);

int LHashmap_put(LHashMap *map, const void *key, void *data);
void *LHashmap_get(LHashMap *map, const void *key);

int LHashmap_traverse(LHashMap *map, LHashmap_traverse_cb traverse_cb);

void *LHashmap_delete(LHashMap *map, const void *key);

#endif