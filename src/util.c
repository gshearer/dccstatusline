// dccstatusline — MIT
// util: string views, the saturating output buffer, small formatting helpers

#include <string.h>

#define UTIL_INTERNAL
#include "util.h"

sv_t
sv_from_cstr(const char *s)
{
  sv_t out = { s, s ? strlen(s) : 0 };

  return(out);
}

bool
sv_eq(sv_t a, sv_t b)
{
  if(a.n != b.n) return(false);

  return(a.n == 0 || memcmp(a.p, b.p, a.n) == 0);
}

bool
sv_eq_cstr(sv_t a, const char *s)
{
  return(sv_eq(a, sv_from_cstr(s)));
}

void
sbuf_init(sbuf_t *sb, char *storage, size_t cap, size_t reserve)
{
  sb->p = storage;
  sb->cap = cap;
  sb->len = 0;
  sb->reserve = reserve > cap ? cap : reserve;
  sb->truncated = false;
}

void
sbuf_append(sbuf_t *sb, const char *src, size_t n)
{
  size_t limit = sb->cap - sb->reserve;

  if(n > limit - sb->len)
  {
    sb->truncated = true;
    return;
  }

  if(n) memcpy(sb->p + sb->len, src, n);

  sb->len += n;
}

void
sbuf_append_sv(sbuf_t *sb, sv_t sv)
{
  sbuf_append(sb, sv.p, sv.n);
}

void
sbuf_append_cstr(sbuf_t *sb, const char *s)
{
  sbuf_append(sb, s, strlen(s));
}

void
sbuf_append_byte(sbuf_t *sb, char c)
{
  sbuf_append(sb, &c, 1);
}

void
sbuf_append_tail(sbuf_t *sb, const char *src, size_t n)
{
  if(n > sb->cap - sb->len)
  {
    sb->truncated = true;
    return;
  }

  if(n) memcpy(sb->p + sb->len, src, n);

  sb->len += n;
}

size_t
u64_grouped(char *dst, size_t cap, uint64_t v, sv_t sep)
{
  char raw[20];
  size_t digits = 0, need, out = 0, i;

  do
  {
    raw[digits++] = (char)('0' + v % 10);
    v /= 10;
  }
  while(v);

  need = digits + ((digits - 1) / 3) * sep.n + 1;

  if(cap < need)
  {
    if(cap) dst[0] = '\0';

    return(0);
  }

  for(i = digits; i--;)
  {
    dst[out++] = raw[i];

    if(i && i % 3 == 0 && sep.n)
    {
      memcpy(dst + out, sep.p, sep.n);
      out += sep.n;
    }
  }

  dst[out] = '\0';

  return(out);
}

sv_t
path_abbrev(char *dst, size_t cap, sv_t path, sv_t home)
{
  size_t rest;
  sv_t out;

  while(home.n && home.p[home.n - 1] == '/') home.n--;

  if(!home.n || home.n > path.n || memcmp(path.p, home.p, home.n) != 0) return(path);
  if(path.n != home.n && path.p[home.n] != '/') return(path);

  rest = path.n - home.n;

  if(cap < rest + 2) return(path);

  dst[0] = '~';

  if(rest) memcpy(dst + 1, path.p + home.n, rest);

  dst[rest + 1] = '\0';
  out.p = dst;
  out.n = rest + 1;

  return(out);
}

sv_t
path_basename(sv_t path)
{
  sv_t out;
  size_t i;

  while(path.n > 1 && path.p[path.n - 1] == '/') path.n--;

  i = path.n;

  while(i && path.p[i - 1] != '/') i--;

  out.p = path.p + i;
  out.n = path.n - i;

  if(!out.n && path.n)
  {
    out.p = path.p;   // all slashes: the root itself is the basename
    out.n = 1;
  }

  return(out);
}
