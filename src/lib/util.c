/* util.c */

#include <stdarg.h>
#include <stdio.h>

#include "fh_i.h"
#include "ast.h"
#include "bytecode.h"

struct fh_output {
  FILE *f;
};

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
  case FH_VAL_NUMBER: printf("NUMBER(%f)", val->data.num); return;
  case FH_VAL_STRING: printf("STRING("); fh_dump_string(val->data.str); printf(")"); return;
  case FH_VAL_FUNC: printf("FUNC(%p)", (void *) val->data.func); return;
  case FH_VAL_C_FUNC: printf("C_FUNC"); return;
  }
  printf("INVALID_VALUE(type=%d)", val->type);
}

void fh_make_number(struct fh_value *val, double num)
{
  val->type = FH_VAL_NUMBER;
  val->data.num = num;
}

void fh_make_string(struct fh_value *val, const char *str)
{
  val->type = FH_VAL_STRING;
  val->data.str = (char *) str;
}

void fh_make_c_func(struct fh_value *val, fh_c_func func)
{
  val->type = FH_VAL_C_FUNC;
  val->data.c_func = func;
}

struct fh_src_loc fh_make_src_loc(uint16_t line, uint16_t col)
{
  struct fh_src_loc ret = {
    .line = line,
    .col = col,
  };

  return ret;
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

void fh_output(struct fh_output *out, char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  if (out && out->f)
    vfprintf(out->f, fmt, ap);
  else
    vprintf(fmt, ap);
  va_end(ap);
}
