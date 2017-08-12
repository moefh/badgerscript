/* util.c */

#include <stdarg.h>
#include <stdio.h>

#include "fh_internal.h"
#include "bytecode.h"

struct fh_src_loc fh_make_src_loc(uint16_t line, uint16_t col)
{
  struct fh_src_loc ret;
  ret.line = line;
  ret.col = col;
  return ret;
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
  case FH_VAL_NULL: printf("NULL"); return;
  case FH_VAL_NUMBER: printf("NUMBER(%f)", val->data.num); return;
  case FH_VAL_STRING: printf("STRING("); fh_dump_string(fh_get_string(val)); printf(")"); return;
  case FH_VAL_ARRAY: printf("ARRAY(size=%d)", fh_get_array_len(val));
#if 0
    printf("[");
    for (int i = 0; i < fh_get_array_len(val); i++) {
      fh_dump_value(fh_get_array_item((struct fh_value*)val, i));
      printf(",");
    }
    printf("]");
#endif
    return;
  case FH_VAL_FUNC: printf("FUNC(%p)", (void *) GET_VAL_FUNC(val)); return;
  case FH_VAL_C_FUNC: printf("C_FUNC"); return;
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
