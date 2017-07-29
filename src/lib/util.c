/* util.c */

#include <stdio.h>

#include "fh_i.h"

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

static ssize_t file_read(void *src, uint8_t *line, ssize_t max_len)
{
  FILE *f = src;

  size_t ret = fread(line, 1, max_len, f);
  if (ret == 0) {
    if (ferror(f) || feof(f))
      return -1;
  }
  return ret;
}

int fh_input_open_file(struct fh_input *in, const char *filename)
{
  static struct fh_input_funcs file_input_funcs = {
    .read = file_read,
  };
  
  FILE *f = fopen(filename, "rb");
  if (f == NULL)
    return -1;
  in->src = f;
  in->funcs = &file_input_funcs;
  return 0;
}

int fh_input_close_file(struct fh_input *in)
{
  int ret = 0;
  
  if (in->src) {
    FILE *f = in->src;
    ret = fclose(f);
    in->src = NULL;
  }

  return ret;
}

ssize_t fh_utf8_len(uint8_t *str, size_t str_size)
{
  size_t len = 0;
  uint8_t *p = str;
  uint8_t *end = str + str_size;

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
