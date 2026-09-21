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

// Width of path as the cwd budget counts it: one column per codepoint, never
// per byte. Exact for most scripts; CJK and emoji render two columns wide.
size_t path_cols(sv_t);

// Shorteners for the cwd section. Each builds into dst and returns a view of
// it, or returns the input view untouched when nothing applies or dst is too
// small — so a short buffer costs detail, never correctness.

// The last `depth` components behind a "…/". Unchanged when depth is 0, or
// when eliding would save no width: no more components than that, or only a
// "/" or "~/" ahead of them.
sv_t path_tail(char *, size_t, sv_t, unsigned);

// Every component ahead of the last `keep` (at least one) collapsed to its
// first codepoint, a dot-directory to its first two: "/m/v/s/proj",
// "~/.c/nvim". A leading "/" or "~/" is kept as punctuation.
sv_t path_shrink(char *, size_t, sv_t, unsigned);

// Clamped to max_cols columns: leading components give way to "…/" first, and
// a last component that still overflows keeps its final columns behind a "…".
sv_t path_clamp(char *, size_t, sv_t, size_t);

#ifdef UTIL_INTERNAL
// The one elision mark the cwd shorteners use: three bytes, one column.
#define DCC_ELLIPSIS     "\xe2\x80\xa6"
#define DCC_ELLIPSIS_LEN 3

static size_t cp_len(const char *, size_t, size_t);
static size_t tail_start(sv_t, unsigned);
#endif

#endif
