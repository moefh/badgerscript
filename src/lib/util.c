/* util.c */

#include <stdarg.h>
#include <stdio.h>

#include "fh_internal.h"
#include "bytecode.h"

uint32_t fh_hash(const void *data, size_t len)
{
  // this is the hash used by ELF
  uint32_t high;
  const unsigned char *s = data;
  const unsigned char *end = s + len;
  uint32_t h = 0;
  while (s < end) {
    h = (h << 4) + *s++;
    if ((high = h & 0xF0000000) != 0)
      h ^= high >> 24;
    h &= ~high;
  }

  // this is an additional bit mix
  uint32_t r = h;
  r += r << 16;
  r ^= r >> 13;
  r += r << 4;
  r ^= r >> 7;
  r += r << 10;
  r ^= r >> 5;
  r += r << 8;
  r ^= r >> 16;
  return r;
}

void fh_dump_string(const char *str)
{
  printf("\"");
  for (const char *p = str; *p != '\0'; p++) {
    switch (*p) {
    case '\n': printf("\\n"); break;
    case '\r': printf("\\r"); break;
    case '\t': printf("\\t"); break;
    case '\\': printf("\\\\"); break;
    case '"': printf("\\\""); break;
    default:
      if (*p < 32)
        printf("\\x%02x", (unsigned char) *p);
      else
        printf("%c", *p);
      break;
    }
  }
  printf("\"");
}

void fh_dump_value(const struct fh_value *val)
{
  switch (val->type) {
  case FH_VAL_NULL:     printf("NULL"); return;
  case FH_VAL_BOOL:     printf("BOOL(%s)", (val->data.b) ? "true" : "false"); return;
  case FH_VAL_NUMBER:   printf("NUMBER(%f)", val->data.num); return;
  case FH_VAL_STRING:   printf("STRING("); fh_dump_string(fh_get_string(val)); printf(")"); return;
  case FH_VAL_ARRAY:    printf("ARRAY(len=%d)", fh_get_array_len(val)); return;
  case FH_VAL_MAP:      printf("MAP(len=%d,cap=%d)", GET_OBJ_MAP(val->data.obj)->len, GET_OBJ_MAP(val->data.obj)->cap); break;
  case FH_VAL_UPVAL:    printf("UPVAL("); fh_dump_value(GET_OBJ_UPVAL(val->data.obj)->val); printf(")"); return;
  case FH_VAL_CLOSURE:  printf("CLOSURE(%p)", val->data.obj); return;
  case FH_VAL_FUNC_DEF: printf("FUNC_DEF(%p)", val->data.obj); return;
  case FH_VAL_C_FUNC:   printf("C_FUNC"); return;
  }
  printf("INVALID_VALUE(type=%d)", val->type);
}

int fh_utf8_len(char *str, size_t str_size)
{
  int len = 0;
  uint8_t *p = (uint8_t *) str;
  uint8_t *end = (uint8_t *) str + str_size;

  while (p < end) {
    uint8_t c = *p++;
    if (c == 0)
      break;
    if ((c & 0x80) == 0) {
      len++;
    } else if ((c & 0xe0) == 0xc0) {
      len += 2;
      if (p >= end || (*p++ & 0xc0) != 0x80) return -1;
    } else if ((c & 0xf0) == 0xe0) {
      len += 3;
      if (p >= end || (*p++ & 0xc0) != 0x80) return -1;
      if (p >= end || (*p++ & 0xc0) != 0x80) return -1;
    } else if ((c & 0xf8) == 0xf0) {
      len += 4;
      if (p >= end || (*p++ & 0xc0) != 0x80) return -1;
      if (p >= end || (*p++ & 0xc0) != 0x80) return -1;
      if (p >= end || (*p++ & 0xc0) != 0x80) return -1;
    } else {
      return -1;
    }
  }

  return len;
}
