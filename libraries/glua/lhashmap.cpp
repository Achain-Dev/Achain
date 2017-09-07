#include "glua/lprefix.h"
#include "glua/lhashmap.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static const size_t L_HASHMAP_BUCKETS_SIZE = 96;
static const size_t L_ARRAYLIST_EXPANSION_SIZE = 8;


#define check_mem(data)	if(nullptr==data) {\
	perror("out of memory\b"); \
	goto error;	 \
}

static void check(int cond, const char *error_format, ...)
{
    if (!cond)
    {
        va_list vap;
        va_start(vap, error_format);
        printf(error_format, vap);
        va_end(vap);
    }
}

static int default_compare(const void *a, const void *b)
{
    char *as = (char*)a;
    char *bs = (char*)b;
    if (as == bs)
        return 0;
    if (!as && !bs)
        return 0;
    if (!as || !bs)
        return 1;
    return strcmp(as, bs);
}

static unsigned int default_hash(const void *key)
{
    if (!key)
        return 0;
    char *keystr = (char*)key;
    size_t len = strlen(keystr);
    unsigned int hash, i;
    for (hash = i = 0; i < len; ++i)
    {
        hash += keystr[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

static void *default_key_copy(const void *key)
{
    char *key_str = (char*)key;
    char *copied = (char*)calloc(strlen(key_str) + 1, sizeof(char));
    memcpy(copied, key_str, sizeof(char) * (strlen(key_str) + 1));
    return copied;
}

static void default_key_free(const void *key)
{
    if (key)
    {
        char *key_str = (char *)key;
        free(key_str);
    }
}

LArrayList *LArraylist_create(size_t element_size, size_t init_size)
{
    init_size = init_size >= 0 ? init_size : 0;
    LArrayList *alist = (LArrayList *)calloc(1, sizeof(LArrayList));
    alist->element_size = element_size;
    alist->length = 0;
    alist->elements = (void**)calloc(element_size, init_size);
    alist->max_size = init_size;
    check_mem(alist);
    return alist;
error:
    return nullptr;
}

size_t LArraylist_count(LArrayList *alist)
{
    if (!alist || alist->length < 0)
        return 0;
    else
        return alist->length;
}

void LArraylist_destroy(LArrayList *alist)
{
    if (alist)
        free(alist->elements);
}

void LArraylist_append(LArrayList *alist, void *data)
{
    if (alist)
    {
        void **items = alist->elements;
        // if not enought size, make new array
        if (alist->length >= alist->max_size)
        {
            // realloc array
            void **old_items = items;
            size_t old_length = alist->length;
            size_t old_max = alist->max_size;
            size_t new_size = alist->max_size + L_ARRAYLIST_EXPANSION_SIZE;
            alist->elements = (void**)calloc(new_size, alist->element_size);
            memcpy(alist->elements, old_items, alist->element_size * old_max);
            alist->max_size = new_size;
            free(old_items);
        }
        items = (void**)alist->elements;
        items[alist->length++] = data;
    }
}

void *LArraylist_get(LArrayList *alist, size_t index)
{
    if (!alist)
        return nullptr;
    if (index >= alist->length)
    {
        return nullptr;
    }
    void **items = (void **)alist->elements;
    return items[index];
}

void *LArraylist_last(LArrayList *alist)
{
    if (!alist || alist->length < 1)
        return nullptr;
    return alist->elements[alist->length - 1];
}

void *LArraylist_first(LArrayList *alist)
{
    if (!alist || alist->length < 1)
        return nullptr;
    return alist->elements[0];
}

void *LArraylist_pop(LArrayList *alist)
{
    if (!alist || alist->length < 1)
        return nullptr;
    void **items = alist->elements;
    void *item = items[alist->length - 1];
    memset(items + alist->length - 1, 0, alist->element_size);
    alist->length -= 1;
    return item;
}

void (LArraylist_set)(LArrayList *alist, size_t index, void *data)
{
    if (!alist || alist->length <= index)
        return;
    void **items = alist->elements;
    items[index] = data;
}

LHashMap *LHashmap_create(LHashmap_compare compare, LHashmap_hash hash,
    LHashmap_key_copy key_copy, LHashmap_key_free key_free, size_t element_size)
{
    LHashMap *map = (LHashMap*)calloc(1, sizeof(LHashMap));
    check_mem(map);

    map->compare = compare == nullptr ? default_compare : compare;
    map->hash = hash == nullptr ? default_hash : hash;
    map->key_copy = key_copy == nullptr ? default_key_copy : key_copy;
    map->key_free = key_free == nullptr ? default_key_free : key_free;
    map->element_size = element_size;
    map->buckets = LArraylist_create(sizeof(LArrayList *), L_HASHMAP_BUCKETS_SIZE);
    map->buckets->length = L_HASHMAP_BUCKETS_SIZE;
    check_mem(map->buckets);
    return map;

error:
    if (map) {
        LHashmap_destroy(map);
    }

    return nullptr;
}


void LHashmap_destroy(LHashMap *map)
{
    if (map) {
        if (map->buckets) {
            LArrayList *buckets = map->buckets;
            for (size_t i = 0; i < buckets->length; i++) {
                LArrayList *bucket = (LArrayList*)(((LArrayList**)(buckets->elements))[i]);
                if (nullptr != bucket) {
                    LHashMapEntry **entries = (LHashMapEntry **)bucket->elements;
                    for (size_t j = 0; j < bucket->length; j++) {
                        map->key_free(entries[j]->key);
                        free(entries[j]);
                    }
                    LArraylist_destroy(bucket);
                }
            }
            LArraylist_destroy(map->buckets);
        }

        free(map);
    }
}

static LHashMapEntry *LHashmap_entry_create(LHashMap *map, int hash, const void *key, void *data, size_t element_size)
{
    LHashMapEntry *node = (LHashMapEntry*)calloc(1, sizeof(LHashMapEntry));
    check_mem(node);
    node->key = map->key_copy(key);
    node->data = data;
    node->hash = hash;
    node->element_size = element_size;
    return node;

error:
    return nullptr;
}


static LArrayList *LHashmap_find_bucket(LHashMap *map, const void *key,
    int create, unsigned int *hash_out)
{

    unsigned int hash = map->hash(key);
    int bucket_n = hash % L_HASHMAP_BUCKETS_SIZE;
    check(bucket_n >= 0, "Invalid bucket found: %d", bucket_n);
    *hash_out = hash;

    LArrayList *bucket = (LArrayList*)LArraylist_get(map->buckets, bucket_n);

    if (!bucket && create) {
        // new bucket, set it up
        bucket = LArraylist_create(sizeof(void *), 0);
        check_mem(bucket);
        ((LArrayList**)(map->buckets->elements))[bucket_n] = bucket;
    }

    return bucket;

error:
    return nullptr;
}


int LHashmap_put(LHashMap *map, const void *key, void *data)
{
    if (!map)
        return 0;
    unsigned int hash = 0;
    LArrayList *bucket = LHashmap_find_bucket(map, key, 1, &hash);
    check(nullptr != bucket, "Error can't create bucket.");

    LHashMapEntry *node = LHashmap_entry_create(map, hash, key, data, map->element_size);
    check_mem(node);

    LArraylist_append(bucket, node);

    return 0;

error:
    return -1;
}

static int LHashmap_get_node(LHashMap *map, unsigned int hash, LArrayList *bucket, const void *key)
{
    for (size_t i = 0; i < bucket->length; i++) {
        LHashMapEntry *node = (LHashMapEntry *)LArraylist_get(bucket, i);
        if (node->hash == hash && map->compare(node->key, key) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}


void *LHashmap_get(LHashMap *map, const void *key)
{
    unsigned int hash = 0;
    LArrayList *bucket = LHashmap_find_bucket(map, key, 0, &hash);
    if (!bucket) return nullptr;

    int i = LHashmap_get_node(map, hash, bucket, key);
    if (i == -1) return nullptr;

    LHashMapEntry *node = (LHashMapEntry*)LArraylist_get(bucket, i);
    check_mem(node);
    check(node != nullptr, "Failed to get node from bucket when it should exist.");

    return node->data;

error: // fallthrough
    return nullptr;
}

int LHashmap_traverse(LHashMap *map, LHashmap_traverse_cb traverse_cb)
{
    int rc = 0;
    for (size_t i = 0; i < map->buckets->length; i++) {
        LArrayList *bucket = (LArrayList*)LArraylist_get(map->buckets, i);
        if (bucket) {
            for (size_t j = 0; j < bucket->length; j++) {
                LHashMapEntry *node = (LHashMapEntry*)LArraylist_get(bucket, j);
                rc = traverse_cb(node);
                if (rc != 0) return rc;
            }
        }
    }
    return 0;
}

void *LHashmap_delete(LHashMap *map, const void *key)
{
    unsigned int hash = 0;
    LArrayList *bucket = LHashmap_find_bucket(map, key, 0, &hash);
    if (!bucket) return nullptr;

    int i = LHashmap_get_node(map, hash, bucket, key);
    if (i == -1) return nullptr;

    LHashMapEntry *node = (LHashMapEntry*)LArraylist_get(bucket, i);
    void *data = node->data;
    map->key_free(node->key);
    free(node);

    LHashMapEntry *ending = (LHashMapEntry*)LArraylist_pop(bucket);

    if (ending != node) {
        // alright looks like it's not the last one, swap it
        LArraylist_set(bucket, i, ending);
    }

    return data;
}