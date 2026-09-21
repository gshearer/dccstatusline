// dccstatusline — MIT
// fuzz_cwd: hostile working directories through the shorteners and the cwd
// section, every promise the shorteners make asserted

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "gitinfo.h"
#include "payload.h"
#include "sections.h"
#include "util.h"

// Input: five knob bytes, then the working directory itself.
//   [0] style   [1] depth   [2] max_len   [3] root_n, 0 for no repository
//   [4] dst cap: under 128 a tight cap as given, else room past the path
#define KNOBS 5

int LLVMFuzzerInitialize(int *, char ***);
int LLVMFuzzerTestOneInput(const uint8_t *, size_t);

static bool utf8_whole(sv_t);
static bool built(sv_t, sv_t, const char *, size_t);
static void shorten(sv_t, unsigned, size_t, size_t);
static void render(sv_t, const uint8_t *);

// Lead bytes and their continuations, each run complete: enough to prove
// that no cut landed inside a codepoint.
static bool
utf8_whole(sv_t s)
{
  size_t i = 0;

  while(i < s.n)
  {
    unsigned char c = (unsigned char)s.p[i];
    size_t len, k;

    if(c < 0x80) len = 1;

    else if((c & 0xe0) == 0xc0) len = 2;

    else if((c & 0xf0) == 0xe0) len = 3;

    else if((c & 0xf8) == 0xf0) len = 4;

    else return(false);

    if(len > s.n - i) return(false);

    for(k = 1; k < len; k++)
      if(((unsigned char)s.p[i + k] & 0xc0) != 0x80) return(false);

    i += len;
  }

  return(true);
}

// A shortener hands back its input, trimmed of trailing slashes at most, or a
// NUL-terminated view built inside dst. True for the second.
static bool
built(sv_t in, sv_t out, const char *dst, size_t cap)
{
  if(out.p == in.p)
  {
    if(out.n > in.n) __builtin_trap();

    return(false);
  }

  if(out.p != dst || out.n >= cap || dst[out.n] != '\0') __builtin_trap();

  if(utf8_whole(in) && !utf8_whole(out)) __builtin_trap();

  return(true);
}

static void
shorten(sv_t in, unsigned depth, size_t max_len, size_t cap)
{
  char *dst = malloc(cap);   // exact size: a byte past cap is an ASan report
  sv_t out;

  if(!dst && cap) return;

  out = path_tail(dst, cap, in, depth);

  if(built(in, out, dst, cap) && path_cols(out) > path_cols(in)) __builtin_trap();

  out = path_shrink(dst, cap, in, depth);

  if(built(in, out, dst, cap) && path_cols(out) > path_cols(in)) __builtin_trap();

  out = path_clamp(dst, cap, in, max_len);

  if(built(in, out, dst, cap))
  {
    if(path_cols(out) > max_len) __builtin_trap();
  }

  // With room to build in, an over-budget path never comes back as it was.
  else if(max_len && cap >= in.n + 4 && path_cols(in) > max_len)
    __builtin_trap();

  free(dst);
}

// The cwd section alone on the line: sec_cwd's three stages and the scratch
// buffers between them, under the invariants the other harnesses hold.
static void
render(sv_t cwd, const uint8_t *knob)
{
  static char line[8 * 1024];
  config_t cfg;
  payload_t pay;
  gitinfo_t git = { knob[3] != 0, "main", 4, knob[3] };
  sbuf_t sb;

  config_defaults(&cfg);
  cfg.order[0] = SEC_CWD;
  cfg.norder = 1;
  cfg.cwd_style = (cwd_style_t)(knob[0] % (CWD_REPO + 1));   // repo is the last style
  cfg.cwd_depth = knob[1];
  cfg.cwd_max_len = knob[2];

  memset(&pay, 0, sizeof pay);
  pay.has_cwd = cwd.n > 0;
  pay.cwd = cwd;

  sbuf_init(&sb, line, sizeof line, 5);
  statusline_render(&sb, &pay, &cfg, &git, 1738411200);

  if(sb.len > sizeof line - 5) __builtin_trap();   // reserve invariant

  sbuf_append_tail(&sb, "\x1b[0m\n", 5);

  if(sb.len > sizeof line) __builtin_trap();       // cap invariant
}

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
  (void)argc;
  (void)argv;

  // abbrev and shrink rewrite an exact $HOME prefix: give them one to find.
  // libFuzzer ignores this return value, so a failure has to be loud here.
  if(setenv("HOME", "/home/u", 1) != 0) __builtin_trap();

  return(0);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  size_t n, cap;
  char *src;
  sv_t cwd;

  if(size < KNOBS) return(0);

  n = size - KNOBS;
  src = malloc(n);   // exact size again: a read past the path is a report

  if(!src && n) return(0);

  if(n) memcpy(src, data + KNOBS, n);

  cwd.p = src;
  cwd.n = n;
  cap = data[4] < 128 ? data[4] : n + 4 + (size_t)(data[4] - 128);

  shorten(cwd, data[1], data[2], cap);
  render(cwd, data);

  free(src);

  return(0);
}
