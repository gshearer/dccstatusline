// dccstatusline — MIT
// color: parse the three color spellings and emit SGR escape sequences

#include <stddef.h>
#include <string.h>

#define COLOR_INTERNAL
#include "color.h"

static bool
parse_named(sv_t text, color_t *out)
{
  size_t i;

  for(i = 0; i < sizeof color_names / sizeof color_names[0]; i++)
  {
    if(sv_eq_cstr(text, color_names[i].name))
    {
      out->kind = COLOR_NAMED16;
      out->r = color_names[i].idx;

      return(true);
    }
  }

  return(false);
}

static bool
parse_index(sv_t text, color_t *out)
{
  uint32_t v = 0;
  size_t i;

  if(!text.n || text.n > 3) return(false);

  for(i = 0; i < text.n; i++)
  {
    if(text.p[i] < '0' || text.p[i] > '9') return(false);

    v = v * 10 + (uint32_t)(text.p[i] - '0');
  }

  if(v > 255) return(false);

  out->kind = COLOR_IDX256;
  out->r = (uint8_t)v;

  return(true);
}

static int
hex_nibble(char c)
{
  if(c >= '0' && c <= '9') return(c - '0');
  if(c >= 'a' && c <= 'f') return(c - 'a' + 10);
  if(c >= 'A' && c <= 'F') return(c - 'A' + 10);

  return(-1);
}

static bool
parse_rgb(sv_t text, color_t *out)
{
  int nib[6];
  size_t i;

  if(text.n != 7 || text.p[0] != '#') return(false);

  for(i = 0; i < 6; i++)
  {
    nib[i] = hex_nibble(text.p[i + 1]);

    if(nib[i] < 0) return(false);
  }

  out->kind = COLOR_RGB;
  out->r = (uint8_t)(nib[0] << 4 | nib[1]);
  out->g = (uint8_t)(nib[2] << 4 | nib[3]);
  out->b = (uint8_t)(nib[4] << 4 | nib[5]);

  return(true);
}

bool
color_parse(sv_t text, color_t *out)
{
  if(sv_eq_cstr(text, "default"))
  {
    out->kind = COLOR_NONE;

    return(true);
  }

  return(parse_named(text, out) || parse_index(text, out) || parse_rgb(text, out));
}

static size_t
put_u8(char *dst, uint8_t v)
{
  size_t n = 0;

  if(v >= 100) dst[n++] = (char)('0' + v / 100);
  if(v >= 10) dst[n++] = (char)('0' + v / 10 % 10);

  dst[n++] = (char)('0' + v % 10);

  return(n);
}

static size_t
sgr_component(char *dst, color_t c, bool bg)
{
  size_t n = 0;
  unsigned code;

  switch(c.kind)
  {
    case COLOR_NAMED16:
      code = c.r < 8 ? (bg ? 40u : 30u) + c.r : (bg ? 100u : 90u) + (c.r - 8u);
      dst[n++] = ';';
      n += put_u8(dst + n, (uint8_t)code);
      break;

    case COLOR_IDX256:
      memcpy(dst + n, bg ? ";48;5;" : ";38;5;", 6);
      n += 6;
      n += put_u8(dst + n, c.r);
      break;

    case COLOR_RGB:
      memcpy(dst + n, bg ? ";48;2;" : ";38;2;", 6);
      n += 6;
      n += put_u8(dst + n, c.r);
      dst[n++] = ';';
      n += put_u8(dst + n, c.g);
      dst[n++] = ';';
      n += put_u8(dst + n, c.b);
      break;

    case COLOR_NONE:
      break;
  }

  return(n);
}

void
color_sgr(sbuf_t *sb, color_t fg, color_t bg)
{
  char seq[48];
  size_t n = 0;

  seq[n++] = '\x1b';
  seq[n++] = '[';
  seq[n++] = '0';
  n += sgr_component(seq + n, fg, false);
  n += sgr_component(seq + n, bg, true);
  seq[n++] = 'm';

  sbuf_append(sb, seq, n);
}
