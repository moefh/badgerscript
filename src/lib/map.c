/* map.c */

#include <string.h>
#include <stdio.h>

#include "program.h"
#include "value.h"

#define OCCUPIED(e) ((e)->key.type != FH_VAL_NULL)

// hash used by ELF
uint32_t hash(const unsigned char *s, size_t len)
{
  uint32_t high;
  const unsigned char *end = s + len;
  uint32_t h = 0;
  while (s < end) {
    h = (h << 4) + *s++;
    if ((high = h & 0xF0000000) != 0)
      h ^= high >> 24;
    h &= ~high;
  }
  return h;
}

uint32_t val_hash(struct fh_value *val, uint32_t cap)
{
  // WARNING: don't use uint8_t because it may not be considered a
  // char for strict aliasing purposes
  const unsigned char *p;
  size_t len;
  if (val->type == FH_VAL_STRING) {
    len = GET_VAL_STRING(val)->size;
    p = (unsigned char *) GET_VAL_STRING_DATA(val);
  } else {
    switch (val->type) {
    case FH_VAL_NULL:   len = 0; break;
    case FH_VAL_BOOL:   len = sizeof(bool); break;
    case FH_VAL_NUMBER: len = sizeof(double); break;
    case FH_VAL_C_FUNC: len = sizeof(fh_c_func); break;
    default:            len = sizeof(void *); break;
    }
    p = (unsigned char *) &val->data;
  }
  uint32_t h = hash(p, len);
  //printf("hash(p, %5zu) returns %08x --> pos=%d\n", len, h, h&(cap-1));
  return h & (cap - 1);
}


static uint32_t find_slot(struct fh_map_entry *entries, uint32_t cap, struct fh_value *key)
{
  uint32_t i = val_hash(key, cap);
  while (OCCUPIED(&entries[i]) && ! fh_vals_are_equal(key, &entries[i].key))
    i = (i+1) & (cap-1);
  return i;
}

static uint32_t insert(struct fh_map_entry *entries, uint32_t cap, struct fh_value *key, struct fh_value *val)
{
  uint32_t i = find_slot(entries, cap, key);
  //printf("-> inserting k="); fh_dump_value(key);
  //printf("; val="); fh_dump_value(val);
  //printf(" at position %u (occupied=%d)\n", i, OCCUPIED(&entries[i]));
  if (! OCCUPIED(&entries[i]))
    entries[i].key = *key;
  entries[i].val = *val;
  return i;
}

static int rebuild(struct fh_map *map, uint32_t cap)
{
  struct fh_map_entry *entries = malloc(cap * sizeof(struct fh_map_entry));
  if (! entries)
    return -1;
  memset(entries, 0, cap * sizeof(struct fh_map_entry));

  //printf("rebuilding map with cap %u\n", cap);
  for (uint32_t i = 0; i < map->cap; i++) {
    struct fh_map_entry *e = &map->entries[i];
    if (e->key.type != FH_VAL_NULL)
      insert(entries, cap, &e->key, &e->val);
  }
  //printf("done rebuilding\n");
  
  free(map->entries);
  map->entries = entries;
  map->cap = cap;
  return 0;
}

int fh_get_map_object_value(struct fh_map *map, struct fh_value *key, struct fh_value *val)
{
  if (map->cap == 0)
    return -1;
  
  uint32_t i = find_slot(map->entries, map->cap, key);
  if (! OCCUPIED(&map->entries[i]))
    return -1;
  *val = map->entries[i].val;
  return 0;
}

int fh_add_map_object_entry(struct fh_program *prog, struct fh_map *map, struct fh_value *key, struct fh_value *val)
{
  if (key->type == FH_VAL_NULL) {
    fh_set_error(prog, "can't insert null key in map");
    return -1;
  }

  uint32_t i;
  if (map->cap > 0) {
    i = find_slot(map->entries, map->cap, key);
    if (OCCUPIED(&map->entries[i])) {
      map->entries[i].val = *val;
      return 0;
    }
  }

  if (map->cap == 0 || map->len+1 > map->cap/2) {
    if (rebuild(map, (map->cap == 0) ? 8 : 2*map->cap) < 0) {
      fh_set_error(prog, "out of memory");
      return -1;
    }
    i = find_slot(map->entries, map->cap, key);
  }
  map->len++;
  map->entries[i].key = *key;
  map->entries[i].val = *val;
  return 0;
}

int fh_next_map_object_key(struct fh_map *map, struct fh_value *key, struct fh_value *next_key)
{
  if (key->type == FH_VAL_NULL) {
    *next_key = map->entries[0].key;
    return 0;
  }
    
  for (uint8_t i = 0; i < map->cap-1; i++)
    if (OCCUPIED(&map->entries[i]) && fh_vals_are_equal(key, &map->entries[i].key)) {
      *next_key = map->entries[(i+1)&(map->cap-1)].key;
      return 0;
    }
  *next_key = map->entries[0].key;
  return 0;
}

int fh_delete_map_object_entry(struct fh_map *map, struct fh_value *key)
{
  if (map->cap == 0)
    return -1;
  
  uint32_t i = find_slot(map->entries, map->cap, key);
  if (! OCCUPIED(&map->entries[i]))
    return -1;
  uint32_t j = i;
  while (true) {
    map->entries[i].key.type = FH_VAL_NULL;
  start:
    j = (j+1) & (map->cap-1);
    if (! OCCUPIED(&map->entries[j]))
      break;
    uint32_t k = val_hash(&map->entries[j].key, map->cap);
    if ((i < j) ? (i<k)&&(k<=j) : (i<k)||(k<=j))
      goto start;
    map->entries[i] = map->entries[j];
    i = j;
  }
  map->len--;
  return 0;
}

int fh_alloc_map_object_len(struct fh_map *map, uint32_t len)
{
  // round len up to the nearest power of 2
  len--;
  len |= len >> 1;
  len |= len >> 2;
  len |= len >> 4;
  len |= len >> 8;
  len |= len >> 16;
  len++;

  if (len < map->len)
    return -1;
  return rebuild(map, 2*len);
}

/* value functions */

int fh_alloc_map_len(struct fh_value *map, uint32_t len)
{
  struct fh_map *m = GET_VAL_MAP(map);
  if (! m)
    return -1;
  return fh_alloc_map_object_len(m, len);
}

int fh_delete_map_entry(struct fh_value *map, struct fh_value *key)
{
  struct fh_map *m = GET_VAL_MAP(map);
  if (! m)
    return -1;
  return fh_delete_map_object_entry(m, key);
}

int fh_next_map_key(struct fh_value *map, struct fh_value *key, struct fh_value *next_key)
{
  struct fh_map *m = GET_VAL_MAP(map);
  if (! m)
    return -1;
  return fh_next_map_object_key(m, key, next_key);
}

int fh_get_map_value(struct fh_value *map, struct fh_value *key, struct fh_value *val)
{
  struct fh_map *m = GET_VAL_MAP(map);
  if (! m)
    return -1;
  return fh_get_map_object_value(m, key, val);
}

int fh_add_map_entry(struct fh_program *prog, struct fh_value *map, struct fh_value *key, struct fh_value *val)
{
  struct fh_map *m = GET_VAL_MAP(map);
  if (! m)
    return -1;
  return fh_add_map_object_entry(prog, m, key, val);
}
