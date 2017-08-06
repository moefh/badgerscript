/* tokenizer.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "fh_i.h"

#define BUF_SIZE 1024

#define FH_IS_SPACE(c) ((c) == ' ' || (c) == '\r' || (c) == '\n' || (c) == '\t')
#define FH_IS_ALPHA(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') || (c) == '_')
#define FH_IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define FH_IS_ALNUM(c) (FH_IS_ALPHA(c) || FH_IS_DIGIT(c))

static const struct keyword {
  enum fh_keyword_type type;
  const char *name;
} keywords[] = {
  { KW_FUNCTION,   "function" },
  { KW_RETURN,     "return" },
  { KW_VAR,        "var" },
  { KW_IF,         "if" },
  { KW_ELSE,       "else" },
  { KW_WHILE,      "while" },
  { KW_BREAK,      "break" },
  { KW_CONTINUE,   "continue" },
};

struct fh_tokenizer {
  struct fh_input *in;
  struct fh_ast *ast;

  struct fh_src_loc cur_loc;
  uint32_t buf_pos;
  uint32_t buf_len;
  char buf[BUF_SIZE];

  char last_err_msg[256];
  struct fh_src_loc last_err_loc;
  
  int saved_byte;
  struct fh_src_loc saved_loc;

  struct fh_buffer tmp;
};

struct fh_tokenizer *fh_new_tokenizer(struct fh_input *in, struct fh_ast *ast)
{
  struct fh_tokenizer *t = malloc(sizeof(struct fh_tokenizer));
  if (! t)
    return NULL;
  
  t->in = in;
  t->ast = ast;
  t->cur_loc = fh_make_src_loc(1, 0);
  
  t->buf_pos = 0;
  t->buf_len = 0;
  t->saved_byte = -1;

  t->last_err_msg[0] = '\0';
  t->last_err_loc = fh_make_src_loc(0,0);

  fh_init_buffer(&t->tmp);
  
  return t;
}

void fh_free_tokenizer(struct fh_tokenizer *t)
{
  fh_free_buffer(&t->tmp);
  free(t);
}

static void set_error(struct fh_tokenizer *t, struct fh_src_loc loc, char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf((char *) t->last_err_msg, sizeof(t->last_err_msg), fmt, ap);
  va_end(ap);

  t->last_err_loc = loc;
}

const char *fh_get_tokenizer_error(struct fh_tokenizer *t)
{
  return t->last_err_msg;
}

struct fh_src_loc fh_get_tokenizer_error_loc(struct fh_tokenizer *t)
{
  return t->last_err_loc;
}

const char *fh_get_token_keyword(struct fh_tokenizer *t, struct fh_token *tok)
{
  UNUSED(t);
  for (int i = 0; i < ARRAY_SIZE(keywords); i++) {
    if (keywords[i].type == tok->data.keyword)
      return keywords[i].name;
  }
  return NULL;
}

const char *fh_get_token_symbol(struct fh_tokenizer *t, struct fh_token *tok)
{
  return fh_get_symbol_name(t->ast->symtab, tok->data.symbol_id);
}

const char *fh_get_token_op(struct fh_tokenizer *t, struct fh_token *tok)
{
  UNUSED(t);
  return tok->data.op_name;
}

const char *fh_get_token_string(struct fh_tokenizer *t, struct fh_token *tok)
{
  if (tok->type == TOK_STRING)
    return t->ast->string_pool.p + tok->data.str;
  return NULL;
}

static int next_byte(struct fh_tokenizer *t)
{
  if (t->saved_byte >= 0) {
    uint8_t ret = t->saved_byte;
    struct fh_src_loc tmp = t->cur_loc;
    t->cur_loc = t->saved_loc;
    t->saved_loc = tmp;
    t->saved_byte = -2;
    return ret;
  }

  if (t->saved_byte == -2) {
    t->cur_loc = t->saved_loc;
    t->saved_byte = -1;
  }
  
  if (t->buf_pos == t->buf_len) {
    ssize_t r = fh_input_read(t->in, t->buf, BUF_SIZE);
    if (r < 0)
      return -1;
    t->buf_len = (uint32_t) r;
    t->buf_pos = 0;
  }
  uint8_t ret = (uint8_t) t->buf[t->buf_pos++];
  if (ret == '\n') {
    t->cur_loc.line++;
    t->cur_loc.col = 0;
  } else {
    t->cur_loc.col++;
  }
  return ret;
}

static void unget_byte(struct fh_tokenizer *t, uint8_t b)
{
  if (t->saved_byte >= 0) {
    fprintf(stderr, "ERROR: can't unget byte: buffer full");
    return;
  }

  t->saved_byte = b;
}

static bool is_op(struct fh_tokenizer *t, char *name)
{
  return fh_get_op(&t->ast->op_table, name) != NULL;
}

static int find_keyword(char *keyword, size_t keyword_size, enum fh_keyword_type *ret)
{
  for (int i = 0; i < ARRAY_SIZE(keywords); i++) {
    if (strncmp((char *) keyword, keywords[i].name, keyword_size) == 0 && keywords[i].name[keyword_size] == '\0') {
      *ret = keywords[i].type;
      return 1;
    }
  }
  return 0;
}

int fh_read_token(struct fh_tokenizer *t, struct fh_token *tok)
{
  int c;

  while (1) {
    c = next_byte(t);
    if (c < 0) {
      tok->loc = t->cur_loc;
      tok->type = TOK_EOF;
      return 0;
    }
    if (FH_IS_SPACE(c))
      continue;

    if (c == '#') {
      while (c != '\n') {
        c = next_byte(t);
        if (c < 0) {
          tok->loc = t->cur_loc;
          tok->type = TOK_EOF;
          return 0;
        }
      }
      continue;
    }

    break;
  }

  tok->loc = t->cur_loc;

  // string
  if (c == '"') {
    t->tmp.size = 0;
    while (1) {
      c = next_byte(t);
      if (c < 0) {
        set_error(t, tok->loc, "unterminated string");
        return -1;
      }
      if (c == '"')
        break;
      if (c == '\\') {
        int next = next_byte(t);
        if (next < 0) {
          set_error(t, tok->loc, "unterminated string");
          return -1;
        }
        switch (next) {
        case '"': c = '"'; break;
        case '\\': c = '\\'; break;
        case '\'': c = '\''; break;
        case 'n': c = '\n'; break;
        case 't': c = '\t'; break;
        case 'r': c = '\r'; break;
        default:
          set_error(t, t->cur_loc, "bad escape sequence");
          return -1;
        }
      }
      if (fh_buf_add_byte(&t->tmp, c) < 0) {
        set_error(t, tok->loc, "out of memory");
        return -1;
      }
    }
    if (fh_utf8_len(t->tmp.p, t->tmp.size) < 0) {
      set_error(t, tok->loc, "invalid utf-8 string");
      return -1;
    }
    fh_string_id str_pos = fh_buf_add_string(&t->ast->string_pool, t->tmp.p, t->tmp.size);
    if (str_pos < 0) {
      set_error(t, tok->loc, "out of memory");
      return -1;
    }
    tok->type = TOK_STRING;
    tok->data.str = str_pos;
    return 0;
  }

  // number
  if (FH_IS_DIGIT(c)) {
    t->tmp.size = 0;
    int got_point = 0;
    while (FH_IS_DIGIT(c) || c == '.') {
      if (c == '.') {
        if (got_point)
          break;
        got_point = 1;
      }
      if (fh_buf_add_byte(&t->tmp, c) < 0) {
        set_error(t, tok->loc, "out of memory");
        return -1;
      }
      c = next_byte(t);
    }
    if (c >= 0)
      unget_byte(t, c);

    if (fh_buf_add_byte(&t->tmp, '\0') < 0) {
      set_error(t, tok->loc, "out of memory");
      return -1;
    }
    
    char *end = NULL;
    double num = strtod((char *) t->tmp.p, &end);
    if ((char *) t->tmp.p == end) {
      set_error(t, tok->loc, "invalid number");
      return -1;
    }
    tok->type = TOK_NUMBER;
    tok->data.num = num;
    return 0;
  }

  // keyword or symbol
  if (FH_IS_ALPHA(c)) {
    t->tmp.size = 0;
    while (FH_IS_ALNUM(c)) {
      if (fh_buf_add_byte(&t->tmp, c) < 0) {
        set_error(t, tok->loc, "out of memory");
        return -1;
      }
      c = next_byte(t);
    }
    if (c >= 0)
      unget_byte(t, c);

    enum fh_keyword_type keyword;
    if (find_keyword(t->tmp.p, t->tmp.size, &keyword) > 0) {
      // keyword
      tok->type = TOK_KEYWORD;
      tok->data.keyword = keyword;
    } else {
      // other symbol
      if (fh_buf_add_byte(&t->tmp, '\0') < 0) {
        set_error(t, tok->loc, "out of memory");
        return -1;
      }
      fh_symbol_id symbol_id = fh_add_symbol(t->ast->symtab, t->tmp.p);
      if (symbol_id < 0) {
        set_error(t, tok->loc, "out of memory");
        return -1;
      }
      tok->type = TOK_SYMBOL;
      tok->data.symbol_id = symbol_id;
    }
    
    return 0;
  }

  // punctuation
  if (c == ',' || c == ';' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}') {
    tok->type = TOK_PUNCT;
    tok->data.punct = c;
    return 0;
  }
  
  // operator
  struct fh_src_loc op_loc = t->cur_loc;
  char op_name[4] = { c };
  int op_len = 1;
  while (1) {
    op_name[op_len] = '\0';
    if (! is_op(t, op_name)) {
      op_name[--op_len] = '\0';
      unget_byte(t, c);
      break;
    }

    if (op_len == sizeof(op_name)-1)
      break;
    c = next_byte(t);
    if (c < 0)
      break;
    op_name[op_len++] = c;
  }
  if (op_len > 0) {
    tok->type = TOK_OP;
    strcpy(tok->data.op_name, op_name);
    return 0;
  }

  if (c >= 32 && c < 128)
    set_error(t, op_loc, "invalid character: '%c'", c);
  else
    set_error(t, op_loc, "invalid byte: 0x%02x", c);
  return -1;
}
