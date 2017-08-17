/* map.c */

#include <string.h>
#include <stdio.h>

#include "program.h"
#include "value.h"

/*
 * Dumb implementation of a map: we simply put all entries in a vector
 * and scan the entire vector on every key lookup.
 */

static int rehash(struct fh_map *map)
{
  int cap = 2 * map->cap;
  struct fh_map_entry *entries = malloc(cap * sizeof(struct fh_map_entry));
  if (! entries)
    return -1;

  if (map->len)
    memcpy(entries, map->entries, map->len * sizeof(struct fh_map_entry));
  for (int i = map->len; i < cap; i++)
    entries[i].key.type = FH_VAL_NULL;

  free(map->entries);
  map->entries = entries;
  map->cap = cap;
  return 0;
}

static struct fh_map_entry *find_key_entry(struct fh_map *map, struct fh_value *key)
{
  if (key->type == FH_VAL_NULL)
    return NULL;

  for (int i = 0; i < map->len; i++) {
    if (fh_vals_are_equal(&map->entries[i].key, key))
      return &map->entries[i];
  }
  return NULL;
}

int fh_delete_map_object_entry(struct fh_program *prog, struct fh_map *map, struct fh_value *key)
{
  UNUSED(prog);
  
  struct fh_map_entry *e = find_key_entry(map, key);
  if (! e)
    return -1;

  if (e - map->entries < map->len)
    memmove(e, e+1, (map->len - (e - map->entries) - 1) * sizeof(struct fh_map_entry));
  map->entries[map->len].key.type = FH_VAL_NULL;
  map->len--;
  return 0;
}

int fh_get_map_object_value(struct fh_program *prog, struct fh_map *map, struct fh_value *key, struct fh_value *val)
{
  UNUSED(prog);
  
  struct fh_map_entry *e = find_key_entry(map, key);
  if (! e)
    return -1;
  *val = e->val;
  return 0;
}

int fh_add_map_object_entry(struct fh_program *prog, struct fh_map *map, struct fh_value *key, struct fh_value *val)
{
  if (key->type == FH_VAL_NULL) {
    fh_set_error(prog, "can't insert null key in map");
    return -1;
  }
  
  struct fh_map_entry *e = find_key_entry(map, key);
  if (! e) {
    if (map->len >= map->cap) {
      if (rehash(map) < 0)
        return -1;
    }
    
    e = &map->entries[map->len++];
    e->key = *key;
  }
  
  e->val = *val;
  return 0;
}

/* value functions */

int fh_delete_map_entry(struct fh_program *prog, struct fh_value *map, struct fh_value *key)
{
  return fh_delete_map_object_entry(prog, GET_VAL_MAP(map), key);
}

int fh_get_map_value(struct fh_program *prog, struct fh_value *map, struct fh_value *key, struct fh_value *val)
{
  return fh_get_map_object_value(prog, GET_VAL_MAP(map), key, val);
}

int fh_add_map_entry(struct fh_program *prog, struct fh_value *map, struct fh_value *key, struct fh_value *val)
{
  return fh_add_map_object_entry(prog, GET_VAL_MAP(map), key, val);
}
