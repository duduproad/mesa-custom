/*
 * Copyright © 2009,2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifndef _HASH_TABLE_H
#define _HASH_TABLE_H

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include "macros.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hash_entry {
   uint32_t hash;
   const void *key;
   void *data;
};

struct hash_table {
   struct hash_entry *table;
   uint32_t (*key_hash_function)(const void *key);
   bool (*key_equals_function)(const void *a, const void *b);
   const void *deleted_key;
   uint32_t size;
   uint32_t rehash;
   uint64_t size_magic;
   uint64_t rehash_magic;
   uint32_t max_entries;
   uint32_t size_index;
   uint32_t entries;
   uint32_t deleted_entries;
};

struct hash_table *
_mesa_hash_table_create(void *mem_ctx,
                        uint32_t (*key_hash_function)(const void *key),
                        bool (*key_equals_function)(const void *a,
                                                    const void *b));

bool
_mesa_hash_table_init(struct hash_table *ht,
                      void *mem_ctx,
                      uint32_t (*key_hash_function)(const void *key),
                      bool (*key_equals_function)(const void *a,
                                                  const void *b));

struct hash_table *
_mesa_hash_table_create_u32_keys(void *mem_ctx);

struct hash_table *
_mesa_hash_table_clone(struct hash_table *src, void *dst_mem_ctx);
void _mesa_hash_table_destroy(struct hash_table *ht,
                              void (*delete_function)(struct hash_entry *entry));
void _mesa_hash_table_clear(struct hash_table *ht,
                            void (*delete_function)(struct hash_entry *entry));
void _mesa_hash_table_set_deleted_key(struct hash_table *ht,
                                      const void *deleted_key);

static inline uint32_t _mesa_hash_table_num_entries(const struct hash_table *ht)
{
   return ht->entries;
}

struct hash_entry *
_mesa_hash_table_insert(struct hash_table *ht, const void *key, void *data);
struct hash_entry *
_mesa_hash_table_insert_pre_hashed(struct hash_table *ht, uint32_t hash,
                                   const void *key, void *data);
struct hash_entry *
_mesa_hash_table_search(const struct hash_table *ht, const void *key);
struct hash_entry *
_mesa_hash_table_search_pre_hashed(struct hash_table *ht, uint32_t hash,
                                  const void *key);
void _mesa_hash_table_remove(struct hash_table *ht,
                             struct hash_entry *entry);
void _mesa_hash_table_remove_key(struct hash_table *ht,
                                 const void *key);

struct hash_entry *_mesa_hash_table_next_entry(struct hash_table *ht,
                                               struct hash_entry *entry);
struct hash_entry *_mesa_hash_table_next_entry_unsafe(const struct hash_table *ht,
                                               struct hash_entry *entry);
struct hash_entry *
_mesa_hash_table_random_entry(struct hash_table *ht,
                              bool (*predicate)(struct hash_entry *entry));

uint32_t _mesa_hash_data(const void *data, size_t size);
uint32_t _mesa_hash_data_with_seed(const void *data, size_t size, uint32_t seed);

uint32_t _mesa_hash_int(const void *key);
uint32_t _mesa_hash_uint(const void *key);
uint32_t _mesa_hash_u32(const void *key);
uint32_t _mesa_hash_u64(const void *key);
uint32_t _mesa_hash_string(const void *key);
uint32_t _mesa_hash_string_with_length(const void *_key, unsigned length);
uint32_t _mesa_hash_pointer(const void *pointer);

bool _mesa_key_int_equal(const void *a, const void *b);
bool _mesa_key_uint_equal(const void *a, const void *b);
bool _mesa_key_u32_equal(const void *a, const void *b);
bool _mesa_key_u64_equal(const void *a, const void *b);
bool _mesa_key_string_equal(const void *a, const void *b);
bool _mesa_key_pointer_equal(const void *a, const void *b);

struct hash_table *
_mesa_pointer_hash_table_create(void *mem_ctx);

static inline struct hash_table *
_mesa_string_hash_table_create(void *mem_ctx)
{
   return _mesa_hash_table_create(mem_ctx, _mesa_hash_string,
                                  _mesa_key_string_equal);
}

bool
_mesa_hash_table_reserve(struct hash_table *ht, unsigned size);
/**
 * This foreach function is safe against deletion (which just replaces
 * an entry's data with the deleted marker), but not against insertion
 * (which may rehash the table, making entry a dangling pointer).
 */
#define hash_table_foreach(ht, entry)                                      \
   for (struct hash_entry *entry = _mesa_hash_table_next_entry(ht, NULL);  \
        entry != NULL;                                                     \
        entry = _mesa_hash_table_next_entry(ht, entry))
/**
 * This foreach function destroys the table as it iterates.
 * It is not safe to use when inserting or removing entries.
 */
#define hash_table_foreach_remove(ht, entry)                                      \
   for (struct hash_entry *entry = _mesa_hash_table_next_entry_unsafe(ht, NULL);  \
        (ht)->entries;                                                     \
        entry->hash = 0, entry->key = (void*)NULL, entry->data = NULL,      \
        (ht)->entries--, entry = _mesa_hash_table_next_entry_unsafe(ht, entry))

static inline void
hash_table_call_foreach(struct hash_table *ht,
                        void (*callback)(const void *key,
                                         void *data,
                                         void *closure),
                        void *closure)
{
   hash_table_foreach(ht, entry)
      callback(entry->key, entry->data, closure);
}

/**
 * This helper macro generates the boilerplate required to use a hash table with
 * a fixed-size struct as the key.
 */
#define DERIVE_HASH_TABLE(T)                                                   \
   static uint32_t T##_hash(const void *key)                                   \
   {                                                                           \
      return _mesa_hash_data(key, sizeof(struct T));                           \
   }                                                                           \
                                                                               \
   static bool T##_equal(const void *a, const void *b)                         \
   {                                                                           \
      return memcmp(a, b, sizeof(struct T)) == 0;                              \
   }                                                                           \
                                                                               \
   static struct hash_table *T##_table_create(void *memctx)                    \
   {                                                                           \
      return _mesa_hash_table_create(memctx, T##_hash, T##_equal);             \
   }

/**
 * Hash table wrapper which supports 64-bit keys.
 */
struct hash_table_u64 {
   struct hash_table *table;
   void *freed_key_data;
   void *deleted_key_data;
};

struct hash_entry_u64 {
   uint64_t key;
   void *data;
   struct hash_entry *_entry;
};

struct hash_table_u64 *
_mesa_hash_table_u64_create(void *mem_ctx);

void
_mesa_hash_table_u64_destroy(struct hash_table_u64 *ht);

void
_mesa_hash_table_u64_insert(struct hash_table_u64 *ht, uint64_t key,
                            void *data);

void *
_mesa_hash_table_u64_search(struct hash_table_u64 *ht, uint64_t key);

void
_mesa_hash_table_u64_remove(struct hash_table_u64 *ht, uint64_t key);

void
_mesa_hash_table_u64_replace(struct hash_table_u64 *ht,
                             const struct hash_entry_u64 *he,
                             void *new_data);

void
_mesa_hash_table_u64_clear(struct hash_table_u64 *ht);

struct hash_entry_u64
_mesa_hash_table_u64_next_entry(struct hash_table_u64 *ht,
                                struct hash_entry_u64 *ent);

static inline uint32_t
_mesa_hash_table_u64_num_entries(struct hash_table_u64 *ht)
{
   return (!!ht->freed_key_data) + (!!ht->deleted_key_data) +
          _mesa_hash_table_num_entries(ht->table);
}

/**
 * This foreach function is safe against deletion (which just replaces
 * an entry's data with the deleted marker), but not against insertion
 * (which may rehash the table, making entry a dangling pointer).
 */
#define hash_table_u64_foreach(ht, entry)                                      \
   for (struct hash_entry_u64 entry =                                          \
         _mesa_hash_table_u64_next_entry(ht, NULL);                            \
        entry.data != NULL;                                                    \
        entry = _mesa_hash_table_u64_next_entry(ht, &entry))


#ifdef __cplusplus
} /* extern C */
#endif

#endif /* _HASH_TABLE_H */
