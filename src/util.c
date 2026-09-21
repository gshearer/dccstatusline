// dccstatusline — MIT
// util: string views, the saturating output buffer, small formatting helpers

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define UTIL_INTERNAL
#include "util.h"

size_t
file_slurp(const char *path, char *dst, size_t cap)
{
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  size_t used = 0;

  if(fd < 0) return(0);

  while(used < cap)
  {
    ssize_t got = read(fd, dst + used, cap - used);

    if(got <= 0) break;

    used += (size_t)got;
  }

  close(fd);

  return(used);
}

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

size_t
path_cols(sv_t path)
{
  size_t i, cols = 0;

  // UTF-8 continuation bytes ride along with their lead byte: one column per
  // codepoint, so a multibyte directory name is not over-charged.
  for(i = 0; i < path.n; i++)
    if(((unsigned char)path.p[i] & 0xc0) != 0x80) cols++;

  return(cols);
}

// Bytes spanned by the codepoint starting at p[i] — one lead byte and its
// continuations, clamped to n. Never zero, so no caller can fail to advance.
static size_t
cp_len(const char *p, size_t n, size_t i)
{
  size_t len = 1;

  while(i + len < n && ((unsigned char)p[i + len] & 0xc0) == 0x80) len++;

  return(len);
}

// Index where the last `n` components of path begin, ignoring trailing
// slashes — or 0 when it holds no more than n, and so has no parent to drop.
static size_t
tail_start(sv_t path, unsigned n)
{
  unsigned seen = 0;
  size_t i;

  while(path.n > 1 && path.p[path.n - 1] == '/') path.n--;

  i = path.n;

  while(i)
  {
    size_t start = i;

    while(start && path.p[start - 1] != '/') start--;

    if(start < i && ++seen == n) return(start);

    if(!start) break;

    i = start - 1;   // step over the '/' separating this component from its parent
  }

  return(0);
}

sv_t
path_tail(char *dst, size_t cap, sv_t path, unsigned depth)
{
  size_t i, n;
  sv_t head, out;

  if(!depth) return(path);

  while(path.n > 1 && path.p[path.n - 1] == '/') path.n--;

  i = tail_start(path, depth);

  // Eliding has to buy width. "…/" costs two columns, so a bare root "/" or a
  // "~/" ahead of the kept components stays as it stands.
  head.p = path.p;
  head.n = i;

  if(path_cols(head) <= 2) return(path);

  n = path.n - i;

  if(cap < DCC_ELLIPSIS_LEN + 1 + n + 1) return(path);

  memcpy(dst, DCC_ELLIPSIS, DCC_ELLIPSIS_LEN);
  dst[DCC_ELLIPSIS_LEN] = '/';
  memcpy(dst + DCC_ELLIPSIS_LEN + 1, path.p + i, n);

  out.p = dst;
  out.n = DCC_ELLIPSIS_LEN + 1 + n;
  dst[out.n] = '\0';

  return(out);
}

sv_t
path_shrink(char *dst, size_t cap, sv_t path, unsigned keep)
{
  size_t bstart, i = 0, out;
  sv_t res;

  if(!keep) keep = 1;

  while(path.n > 1 && path.p[path.n - 1] == '/') path.n--;

  bstart = tail_start(path, keep);

  if(!bstart) return(path);   // nothing ahead of the kept components to collapse

  // A leading "/" or "~/" is punctuation, not a component: copy it as it
  // stands. A nonzero bstart means at least two bytes, so p[1] is there.
  if(path.p[0] == '/') i = 1;

  else if(path.p[0] == '~' && path.p[1] == '/') i = 2;

  if(i >= cap) return(path);

  memcpy(dst, path.p, i);
  out = i;

  while(i < bstart)
  {
    size_t start = i, len;

    while(i < bstart && path.p[i] != '/') i++;

    if(i > start)
    {
      len = cp_len(path.p, i, start);

      // A dot-directory keeps its dot and one codepoint more: a lone "."
      // would read as the current directory.
      if(path.p[start] == '.' && start + len < i)
        len += cp_len(path.p, i, start + len);

      if(out + len + 1 + 1 > cap) return(path);

      memcpy(dst + out, path.p + start, len);
      out += len;
      dst[out++] = '/';
    }

    while(i < bstart && path.p[i] == '/') i++;
  }

  if(out + (path.n - bstart) + 1 > cap) return(path);

  memcpy(dst + out, path.p + bstart, path.n - bstart);
  out += path.n - bstart;
  dst[out] = '\0';

  res.p = dst;
  res.n = out;

  return(res);
}

sv_t
path_clamp(char *dst, size_t cap, sv_t path, size_t max_cols)
{
  size_t i, bstart, want, cols, cut, total = path_cols(path), seen = 0;
  sv_t out;

  if(!max_cols || total <= max_cols) return(path);

  bstart = tail_start(path, 1);

  // Drop whole leading components while that is enough: "…/" plus the rest.
  // seen counts the columns of path[0..i], so whatever follows a '/' at i
  // spans total - seen of them: one pass, however deep the path.
  for(i = 0; i < bstart; i++)
  {
    size_t rest = path.n - i - 1;

    if(((unsigned char)path.p[i] & 0xc0) != 0x80) seen++;

    if(path.p[i] != '/' || total - seen + 2 > max_cols) continue;

    if(cap < DCC_ELLIPSIS_LEN + 1 + rest + 1) return(path);

    memcpy(dst, DCC_ELLIPSIS, DCC_ELLIPSIS_LEN);
    dst[DCC_ELLIPSIS_LEN] = '/';
    memcpy(dst + DCC_ELLIPSIS_LEN + 1, path.p + i + 1, rest);

    out.p = dst;
    out.n = DCC_ELLIPSIS_LEN + 1 + rest;
    dst[out.n] = '\0';

    return(out);
  }

  // Even the last component overflows: keep its final columns behind one "…".
  want = max_cols - 1;
  cols = 0;
  cut = path.n;

  while(cut > bstart && cols < want)
  {
    cut--;

    while(cut > bstart && ((unsigned char)path.p[cut] & 0xc0) == 0x80) cut--;

    cols++;
  }

  if(cap < DCC_ELLIPSIS_LEN + (path.n - cut) + 1) return(path);

  memcpy(dst, DCC_ELLIPSIS, DCC_ELLIPSIS_LEN);
  memcpy(dst + DCC_ELLIPSIS_LEN, path.p + cut, path.n - cut);

  out.p = dst;
  out.n = DCC_ELLIPSIS_LEN + (path.n - cut);
  dst[out.n] = '\0';

  return(out);
}
