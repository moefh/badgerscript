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
  case FH_VAL_FUNC: printf("FUNC(addr=%u)", val->data.func->addr); return;
  case FH_VAL_C_FUNC: printf("C_FUNC(%p)", val->data.c_func); return;
  }
  printf("INVALID_VALUE(type=%d)", val->type);
}

void fh_make_number(struct fh_value *val, double num)
{
  val->type = FH_VAL_NUMBER;
  val->data.num = num;
}

void fh_make_c_func(struct fh_value *val, fh_c_func func)
{
  val->type = FH_VAL_C_FUNC;
  val->data.c_func = func;
}

const char *fh_dump_token(struct fh_tokenizer *t, struct fh_token *tok)
{
  static char str[256];
  
  switch (tok->type) {
  case TOK_EOF:
    snprintf(str, sizeof(str), "<end-of-file>");
    break;
    
  case TOK_KEYWORD:
    snprintf(str, sizeof(str), "%s", fh_get_token_keyword(t, tok));
    break;
    
  case TOK_SYMBOL:
    snprintf(str, sizeof(str), "%s", fh_get_token_symbol(t, tok));
    break;
    
  case TOK_OP:
    snprintf(str, sizeof(str), "%s", fh_get_token_op(t, tok));
    break;
    
  case TOK_PUNCT:
    snprintf(str, sizeof(str), "%c", tok->data.punct);
    break;
    
  case TOK_STRING:
    snprintf(str, sizeof(str), "\"%s\"", fh_get_token_string(t, tok));
    break;
    
  case TOK_NUMBER:
    snprintf(str, sizeof(str), "%g", tok->data.num);
    break;
    
  default:
    snprintf(str, sizeof(str), "<unknown token type %d>", tok->type);
    break;
  }

  return str;
}

struct fh_src_loc fh_make_src_loc(uint16_t line, uint16_t col)
{
  struct fh_src_loc ret = {
    .line = line,
    .col = col,
  };

  return ret;
}

ssize_t fh_utf8_len(char *str, size_t str_size)
{
  size_t len = 0;
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

void fh_dump_mem(const char *label, const void *data, size_t len)
{
  char str[18];
  size_t cur, str_len;

  if (label == NULL)
    printf("* dumping %u bytes\n", (unsigned int) len);
  else
    printf("%s (%u bytes)\n", label, (unsigned int) len);
  
  cur = 0;
  while (cur < len) {
    const uint8_t *line = (const uint8_t *) data + cur;
    size_t i, si;

    printf("| ");
    str_len = (len - cur > 16) ? 16 : len - cur;
    for (si = i = 0; i < str_len; i++) {
      printf("%02x ", line[i]);
      str[si++] = (line[i] >= 32 && line[i] < 127) ? line[i] : '.';
      if (i == 7) {
        printf(" ");
        str[si++] = ' ';
      }
    }
    str[si++] = '\0';
    cur += str_len;

    for (i = str_len; i < 16; i++) {
      printf("   ");
      if (i == 7)
        printf(" ");
    }
    printf("| %-17s |\n", str);
  }
}
