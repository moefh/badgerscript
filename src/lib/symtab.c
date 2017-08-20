/* symtab.c */

#include <string.h>

#include "fh_internal.h"

void fh_init_symtab(struct fh_symtab *s)
{
  s->num = 0;
  s->cap = 0;
  s->entries = NULL;
  fh_init_buffer(&s->symbols);
}

void fh_destroy_symtab(struct fh_symtab *s)
{
  if (s->entries)
    free(s->entries);
  fh_destroy_buffer(&s->symbols);
}

fh_symbol_id fh_add_symbol(struct fh_symtab *s, const void *symbol)
{
  fh_symbol_id cur = fh_get_symbol_id(s, symbol);
  if (cur >= 0)
    return cur;

  if (s->num == s->cap) {
    int new_cap = (s->cap + 1024 + 1) / 1024 * 1024;
    void *new_entries = realloc(s->entries, new_cap * sizeof(s->entries[0]));
    if (new_entries == NULL)
      return -1;
    s->entries = new_entries;
    s->cap = new_cap;
  }

  int entry_pos = fh_buf_add_string(&s->symbols, symbol, strlen(symbol));
  if (entry_pos < 0)
    return -1;
  s->entries[s->num] = entry_pos;
  return s->num++;
}

fh_symbol_id fh_get_symbol_id(struct fh_symtab *s, const void *symbol)
{
  for (fh_symbol_id i = 0; i < s->num; i++) {
    if (strcmp(symbol, (char*) s->symbols.p + s->entries[i]) == 0)
      return i;
  }
  return -1;
}

const char *fh_get_symbol_name(struct fh_symtab *s, fh_symbol_id id)
{
  if (id >= 0 && id < s->num)
    return s->symbols.p + s->entries[id];
  return NULL;
}
