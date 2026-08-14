#ifndef DCC_UTIL_H
#define DCC_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A borrowed view into caller-owned bytes. The storage must outlive the view;
// views into the stdin and config buffers are valid through the whole render.
typedef struct
{
  const char *p;
  size_t n;
} sv_t;

// Fixed-capacity saturating appender. Appends are all-or-nothing: a piece that
// does not fit is dropped whole and `truncated` is set, so an escape sequence
// can never be torn. The `reserve` tail bytes are reachable only through
// sbuf_append_tail, guaranteeing the closing SGR reset always lands.
typedef struct
{
  char *p;
  size_t cap;
  size_t len;
  size_t reserve;
  bool truncated;
} sbuf_t;

sv_t sv_from_cstr(const char *);
bool sv_eq(sv_t, sv_t);
bool sv_eq_cstr(sv_t, const char *);

void sbuf_init(sbuf_t *, char *, size_t, size_t);
void sbuf_append(sbuf_t *, const char *, size_t);
void sbuf_append_sv(sbuf_t *, sv_t);
void sbuf_append_cstr(sbuf_t *, const char *);
void sbuf_append_byte(sbuf_t *, char);
void sbuf_append_tail(sbuf_t *, const char *, size_t);

// Writes v with `sep` between digit groups of three, NUL-terminated. Returns
// the length written, or 0 with an empty string when cap cannot hold it.
size_t u64_grouped(char *, size_t, uint64_t, sv_t);

// Reads a whole file into dst. Returns bytes read; 0 for missing, unreadable,
// or empty files alike — for a config, absent and empty mean the same thing.
size_t file_slurp(const char *, char *, size_t);

// Returns a view of path with an exact $HOME prefix rewritten to `~` (built in
// dst), or the original path view untouched when no abbreviation applies.
sv_t path_abbrev(char *, size_t, sv_t, sv_t);
sv_t path_basename(sv_t);

#endif
