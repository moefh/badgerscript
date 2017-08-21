/* src_loc.c */

#include <stdlib.h>
#include <limits.h>

#include "fh_internal.h"
#include "value.h"

struct fh_src_loc fh_get_addr_src_loc(struct fh_func_def *func_def, int addr)
{
  struct fh_src_loc loc = fh_make_src_loc(0,0,0);
  //printf("decoding src_loc for address %d\n", addr);
  fh_decode_src_loc(func_def->code_src_loc, func_def->code_src_loc_size, &loc, addr);
  return loc;
}

const void *fh_decode_src_loc(const void *encoded, int encoded_len, struct fh_src_loc *src_loc, int n_instr)
{
  struct fh_src_loc loc = *src_loc;
  const uint8_t *p = encoded;
  const uint8_t *end = p + encoded_len;

  for (int pc = 0; pc < n_instr; pc++) {
    if (p >= end)
      break;
    uint8_t b = *p++;
    if ((b & 0xc0) == 0xc0) {
      if (p + 6 > end)
        return NULL;
      loc.col     = p[0] | (p[1] << 8);
      loc.line    = p[2] | (p[3] << 8);
      loc.file_id = p[4] | (p[5] << 8);
      p += 6;
      //printf("decoded absolute %d:%d:%d\n", loc.file_id, loc.line, loc.col);
      continue;
    }
    if ((b & 0xc0) == 0x80) {
      if (p + 1 > end)
        return NULL;
      uint8_t c = *p++;
      int delta_line = ((b & 0x3f) << 1) | (c >> 7);
      int delta_col = c & 0x7f;
      loc.col += delta_col - 63;
      loc.line += delta_line - 63;
      //printf("decoded relative %3d:%-3d [%02x %02x]\n", delta_col-63, delta_line-63, b, c);
      continue;
    }
    loc.col += (int)b - 63;
    //printf("decoded relative %3d     [%02x]\n", (int)b - 63, b);
  }
  *src_loc = loc;
  return p;
}

static int get_encoded_delta(uint16_t old, uint16_t new)
{
  if (old < new) {
    if (new - old > 64)
      return -1;
    return (new - old) + 63;
  } else {
    if (old - new > 63)
      return -1;
    return (new + 63) - old;
  }
}

int fh_encode_src_loc_change(struct fh_buffer *buf, struct fh_src_loc *old_loc, struct fh_src_loc *new_loc)
{
  //printf("---- old=%3d:%-3d, new=%3d:%-3d    ", old_loc->line, old_loc->col, new_loc->line, new_loc->col);
  
  int delta_line = get_encoded_delta(old_loc->line, new_loc->line);
  int delta_col = get_encoded_delta(old_loc->col, new_loc->col);
  if (buf->size == 0 || old_loc->file_id != new_loc->file_id || delta_col < 0 || delta_line < 0) {
    // absolute
    //printf("-> encoding absolute %d:%d:%d\n", new_loc->file_id, new_loc->line, new_loc->col);
    if (fh_buf_add_byte(buf, 0xff) < 0
        || fh_buf_add_u16(buf, new_loc->col) < 0
        || fh_buf_add_u16(buf, new_loc->line) < 0
        || fh_buf_add_u16(buf, new_loc->file_id) < 0)
      return -1;
  } else if (delta_line != 63) {
    // relative line, col
    //printf("-> encoding relative %3d:%-3d [%02x,%02x]\n", delta_line-63, delta_col-63, delta_line, delta_col);
    if (fh_buf_add_byte(buf, 0x80 | (delta_line>>1)) < 0
        || fh_buf_add_byte(buf, (delta_line<<7) | (delta_col&0x7f)) < 0)
      return -1;
  } else {
    // relative col
    //printf("-> encoding relative %3d     [%02x]\n", delta_col-63, delta_col);
    if (fh_buf_add_byte(buf, delta_col) < 0)
      return -1;
  }
  return 0;
}
