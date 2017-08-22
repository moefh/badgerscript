/* tokenizer.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "tokenizer.h"
#include "ast.h"

#define FH_IS_SPACE(c) ((c) == ' ' || (c) == '\r' || (c) == '\n' || (c) == '\t')
#define FH_IS_ALPHA(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') || (c) == '_')
#define FH_IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define FH_IS_ALNUM(c) (FH_IS_ALPHA(c) || FH_IS_DIGIT(c))

static const struct keyword {
  enum fh_keyword_type type;
  const char *name;
} keywords[] = {
  { KW_INCLUDE,    "include" },
  { KW_FUNCTION,   "function" },
  { KW_RETURN,     "return" },
  { KW_VAR,        "var" },
  { KW_IF,         "if" },
  { KW_ELSE,       "else" },
  { KW_WHILE,      "while" },
  { KW_BREAK,      "break" },
  { KW_CONTINUE,   "continue" },
};

void fh_init_tokenizer(struct fh_tokenizer *t, struct fh_program *prog, struct fh_input *in, struct fh_ast *ast, struct fh_buffer *tmp_buf, uint16_t file_id)
{
  t->next = NULL;
  t->prog = prog;
  t->in = in;
  t->ast = ast;
  t->tmp = tmp_buf;
  t->file_id = file_id;
  t->cur_loc = fh_make_src_loc(file_id, 1, 0);
  
  t->buf_pos = 0;
  t->buf_len = 0;
  t->saved_byte = -1;

  t->last_err_loc = t->cur_loc;
}

static void set_error(struct fh_tokenizer *t, struct fh_src_loc loc, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  fh_set_error(t->prog, "%s:%d:%d: %s", fh_get_ast_file_name(t->ast, loc.file_id), loc.line, loc.col, str);
  t->last_err_loc = loc;
}

struct fh_src_loc fh_get_tokenizer_error_loc(struct fh_tokenizer *t)
{
  return t->last_err_loc;
}

const char *fh_get_token_keyword(struct fh_token *tok)
{
  for (int i = 0; i < ARRAY_SIZE(keywords); i++) {
    if (keywords[i].type == tok->data.keyword)
      return keywords[i].name;
  }
  return NULL;
}

const char *fh_get_token_symbol(struct fh_ast *ast, struct fh_token *tok)
{
  return fh_get_symbol_name(&ast->symtab, tok->data.symbol_id);
}

const char *fh_get_token_op(struct fh_token *tok)
{
  return tok->data.op_name;
}

const char *fh_get_token_string(struct fh_ast *ast, struct fh_token *tok)
{
  if (tok->type == TOK_STRING)
    return fh_get_ast_string(ast, tok->data.str);
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
    int r = fh_read_input(t->in, t->buf, sizeof(t->buf));
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

  t->saved_loc = t->cur_loc;
  t->saved_byte = b;
}

static int find_keyword(const void *keyword, int keyword_size, enum fh_keyword_type *ret)
{
  for (int i = 0; i < ARRAY_SIZE(keywords); i++) {
    if (strncmp(keyword, keywords[i].name, keyword_size) == 0 && keywords[i].name[keyword_size] == '\0') {
      *ret = keywords[i].type;
      return 1;
    }
  }
  return 0;
}

const char *fh_dump_token(struct fh_ast *ast, struct fh_token *tok)
{
  static char str[256];
  
  switch (tok->type) {
  case TOK_EOF:
    snprintf(str, sizeof(str), "<end-of-file>");
    break;
    
  case TOK_KEYWORD:
    snprintf(str, sizeof(str), "%s", fh_get_token_keyword(tok));
    break;
    
  case TOK_SYMBOL:
    snprintf(str, sizeof(str), "%s", fh_get_token_symbol(ast, tok));
    break;
    
  case TOK_OP:
    snprintf(str, sizeof(str), "%s", fh_get_token_op(tok));
    break;
    
  case TOK_PUNCT:
    snprintf(str, sizeof(str), "%c", tok->data.punct);
    break;
    
  case TOK_STRING:
    snprintf(str, sizeof(str), "\"%s\"", fh_get_token_string(ast, tok));
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
    t->tmp->size = 0;
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
        case 'e': c = '\x1b'; break;
        case 'n': c = '\n'; break;
        case 't': c = '\t'; break;
        case 'r': c = '\r'; break;
        default:
          set_error(t, t->cur_loc, "bad escape sequence");
          return -1;
        }
      }
      if (fh_buf_add_byte(t->tmp, c) < 0) {
        set_error(t, tok->loc, "out of memory");
        return -1;
      }
    }
    if (fh_utf8_len(t->tmp->p, t->tmp->size) < 0) {
      set_error(t, tok->loc, "invalid utf-8 string");
      return -1;
    }
    fh_string_id str_pos = fh_buf_add_string(&t->ast->string_pool, t->tmp->p, t->tmp->size);
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
    t->tmp->size = 0;
    int got_point = 0;
    while (FH_IS_DIGIT(c) || c == '.') {
      if (c == '.') {
        if (got_point)
          break;
        got_point = 1;
      }
      if (fh_buf_add_byte(t->tmp, c) < 0) {
        set_error(t, tok->loc, "out of memory");
        return -1;
      }
      c = next_byte(t);
    }
    if (c >= 0)
      unget_byte(t, c);

    if (fh_buf_add_byte(t->tmp, '\0') < 0) {
      set_error(t, tok->loc, "out of memory");
      return -1;
    }
    
    char *end = NULL;
    double num = strtod(t->tmp->p, &end);
    if (t->tmp->p == end) {
      set_error(t, tok->loc, "invalid number");
      return -1;
    }
    tok->type = TOK_NUMBER;
    tok->data.num = num;
    return 0;
  }

  // keyword or symbol
  if (FH_IS_ALPHA(c)) {
    t->tmp->size = 0;
    while (FH_IS_ALNUM(c)) {
      if (fh_buf_add_byte(t->tmp, c) < 0) {
        set_error(t, tok->loc, "out of memory");
        return -1;
      }
      c = next_byte(t);
    }
    if (c >= 0)
      unget_byte(t, c);

    enum fh_keyword_type keyword;
    if (find_keyword(t->tmp->p, t->tmp->size, &keyword) > 0) {
      // keyword
      tok->type = TOK_KEYWORD;
      tok->data.keyword = keyword;
    } else {
      // other symbol
      if (fh_buf_add_byte(t->tmp, '\0') < 0) {
        set_error(t, tok->loc, "out of memory");
        return -1;
      }
      fh_symbol_id symbol_id = fh_add_symbol(&t->ast->symtab, t->tmp->p);
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
  if (c == ',' || c == '.' || c == ';' || c == ':' ||
      c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}') {
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
    if (! fh_get_op(op_name)) {
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
