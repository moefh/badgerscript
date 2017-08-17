/* map.c */

#include <string.h>
#include <stdio.h>

#include "program.h"
#include "value.h"

int fh_delete_map_object_entry(struct fh_map *map, struct fh_value *key)
{
  // TODO
  UNUSED(map);
  UNUSED(key);
  return -1;
}

int fh_get_map_object_value(struct fh_map *map, struct fh_value *key, struct fh_value *val)
{
  // TODO
  UNUSED(map);
  UNUSED(key);
  UNUSED(val);
  return -1;
}

int fh_add_map_object_entry(struct fh_map *map, struct fh_value *key, struct fh_value *val)
{
  // TODO
  UNUSED(map);
  UNUSED(key);
  UNUSED(val);
  return -1;
}

/* value functions */

int fh_delete_map_entry(struct fh_value *map, struct fh_value *key)
{
  return fh_delete_map_object_entry(GET_VAL_MAP(map), key);
}

int fh_get_map_value(struct fh_value *map, struct fh_value *key, struct fh_value *val)
{
  return fh_get_map_object_value(GET_VAL_MAP(map), key, val);
}

int fh_add_map_entry(struct fh_value *map, struct fh_value *key, struct fh_value *val)
{
  return fh_add_map_object_entry(GET_VAL_MAP(map), key, val);
}
