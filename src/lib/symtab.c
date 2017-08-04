/* symtab.c */

#include <string.h>

#include "fh_i.h"

struct fh_symtab {
  int num;
  int cap;
  size_t *entries;
  struct fh_buffer symbols;
};

struct fh_symtab *fh_symtab_new(void)
{
  struct fh_symtab *s = malloc(sizeof(struct fh_symtab));
  if (s == NULL)
    return NULL;
  s->num = 0;
  s->cap = 0;
  s->entries = NULL;
  fh_init_buffer(&s->symbols);
  return s;
}

void fh_symtab_free(struct fh_symtab *s)
{
  if (s->entries != NULL)
    free(s->entries);
  fh_free_buffer(&s->symbols);
  free(s);
}

fh_symbol_id fh_symtab_add(struct fh_symtab *s, char *symbol)
{
  int cur = fh_symtab_get_id(s, symbol);
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

  ssize_t entry_pos = fh_buf_add_string(&s->symbols, symbol, strlen((char *) symbol));
  if (entry_pos < 0)
    return -1;
  s->entries[s->num] = entry_pos;
  return s->num++;
}

fh_symbol_id fh_symtab_get_id(struct fh_symtab *s, char *symbol)
{
  for (int32_t i = 0; i < s->num; i++) {
    if (strcmp(symbol, (char*) s->symbols.p + s->entries[i]) == 0)
      return i;
  }
  return -1;
}

const char *fh_symtab_get_symbol(struct fh_symtab *s, fh_symbol_id id)
{
  if (id >= 0 && id < s->num)
    return s->symbols.p + s->entries[id];
  return NULL;
}
