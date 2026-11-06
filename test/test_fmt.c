// dccstatusline — MIT
// test_fmt: template rendering, styles, and the empty-token swallow rule

#include "fmt.h"

#include "check.h"

static const color_t none = { COLOR_NONE, 0, 0, 0 };
static const color_t red = { COLOR_NAMED16, 1, 0, 0 };
static const color_t cyan = { COLOR_NAMED16, 6, 0, 0 };

static void try_plain(const char *, const fmt_token_t *, size_t, const char *);
static void test_swallow(void);
static void test_escapes(void);
static void test_styles(void);
static void test_truncation(void);
static void test_pill(void);

static void
try_plain(const char *tpl, const fmt_token_t *toks, size_t n, const char *want)
{
  char mem[256];
  sbuf_t sb;

  sbuf_init(&sb, mem, sizeof mem, 0);
  fmt_render(&sb, sv_from_cstr(tpl), toks, n, none, none);
  CHECK_MEM(sb.p, sb.len, want);
}

static void
test_swallow(void)
{
  const fmt_token_t one[] =
  {
    { "a", { "A", 1 }, { COLOR_NONE, 0, 0, 0 }, { COLOR_NONE, 0, 0, 0 }, false },
  };
  const fmt_token_t pair[] =
  {
    { "a", { "A", 1 }, { COLOR_NONE, 0, 0, 0 }, { COLOR_NONE, 0, 0, 0 }, false },
    { "b", { NULL, 0 }, { COLOR_NONE, 0, 0, 0 }, { COLOR_NONE, 0, 0, 0 }, false },
  };
  const fmt_token_t ctx[] =
  {
    { "u", { "U", 1 }, { COLOR_NONE, 0, 0, 0 }, { COLOR_NONE, 0, 0, 0 }, false },
    { "c", { "C", 1 }, { COLOR_NONE, 0, 0, 0 }, { COLOR_NONE, 0, 0, 0 }, false },
    { "p", { NULL, 0 }, { COLOR_NONE, 0, 0, 0 }, { COLOR_NONE, 0, 0, 0 }, false },
  };

  try_plain("hi", NULL, 0, "\x1b[0mhi");
  try_plain("{a}", one, 1, "\x1b[0mA");
  try_plain("x {a}", one, 1, "\x1b[0mx \x1b[0mA");
  try_plain("{a}!", one, 1, "\x1b[0mA\x1b[0m!");
  try_plain("{a} \xc2\xb7 {b}", pair, 2, "\x1b[0mA");            // middle swallowed
  try_plain("ctx {b}", pair, 2, "");                             // leading swallowed
  try_plain("{u}/{c} {p}", ctx, 3, "\x1b[0mU\x1b[0m/\x1b[0mC");  // chain survives
  try_plain("{zz} x {a}", one, 1, "\x1b[0m x \x1b[0mA");         // unknown swallows
  try_plain("", one, 1, "");
}

static void
test_escapes(void)
{
  try_plain("{{x}}", NULL, 0, "\x1b[0m{x}");
  try_plain("a{{b", NULL, 0, "\x1b[0ma{b");
  try_plain("{oops", NULL, 0, "\x1b[0m{oops");
}

static void
test_styles(void)
{
  const fmt_token_t styled[] =
  {
    { "a", { "A", 1 }, { COLOR_NAMED16, 6, 0, 0 }, { COLOR_NONE, 0, 0, 0 }, false },
  };
  const fmt_token_t plain[] =
  {
    { "a", { "A", 1 }, { COLOR_NONE, 0, 0, 0 }, { COLOR_NONE, 0, 0, 0 }, false },
  };
  char mem[256];
  sbuf_t sb;

  sbuf_init(&sb, mem, sizeof mem, 0);
  fmt_render(&sb, sv_from_cstr("L{a}"), styled, 1, red, none);
  CHECK_MEM(sb.p, sb.len, "\x1b[0;31mL\x1b[0;36mA");   // token overrides base

  sbuf_init(&sb, mem, sizeof mem, 0);
  fmt_render(&sb, sv_from_cstr("{a}"), plain, 1, red, none);
  CHECK_MEM(sb.p, sb.len, "\x1b[0;31mA");              // token inherits base
}

static void
test_truncation(void)
{
  const fmt_token_t one[] =
  {
    { "a", { "AAAAA", 5 }, { COLOR_NONE, 0, 0, 0 }, { COLOR_NONE, 0, 0, 0 }, false },
  };
  char mem[8];
  sbuf_t sb;

  sbuf_init(&sb, mem, sizeof mem, 0);
  fmt_render(&sb, sv_from_cstr("{a}"), one, 1, none, none);
  CHECK(sb.truncated, "tiny buffer flags truncation");
  CHECK(sb.len <= sizeof mem, "never overflows");
}

// The caps wear the fill over the base bg, and survive a swallow after the pill.
static void
test_pill(void)
{
  const fmt_token_t toks[] =
  {
    { "p", { "P", 1 }, red, cyan, true },
    { "e", { NULL, 0 }, red, cyan, true },
    { "b", { NULL, 0 }, none, none, false },
  };
  const fmt_token_t unfilled[] = { { "u", { "U", 1 }, red, none, true } };
  char mem[256];
  sbuf_t sb;

  sbuf_init(&sb, mem, sizeof mem, 0);
  fmt_render(&sb, sv_from_cstr("{p}"), toks, 3, none, none);
  CHECK_MEM(sb.p, sb.len,
            "\x1b[0;36m\xee\x82\xb6\x1b[0;31;46mP\x1b[0;36m\xee\x82\xb4");

  sbuf_init(&sb, mem, sizeof mem, 0);
  fmt_render(&sb, sv_from_cstr("{p}"), toks, 3, none, red);
  CHECK_MEM(sb.p, sb.len,
            "\x1b[0;36;41m\xee\x82\xb6\x1b[0;31;46mP\x1b[0;36;41m\xee\x82\xb4");

  sbuf_init(&sb, mem, sizeof mem, 0);
  fmt_render(&sb, sv_from_cstr("{p} {b}"), toks, 3, none, none);
  CHECK_MEM(sb.p, sb.len,
            "\x1b[0;36m\xee\x82\xb6\x1b[0;31;46mP\x1b[0;36m\xee\x82\xb4");

  sbuf_init(&sb, mem, sizeof mem, 0);
  fmt_render(&sb, sv_from_cstr("{e}"), toks, 3, none, none);
  CHECK(sb.len == 0, "an empty pill draws no caps");

  // With no bg of its own, the pill fills with the base fg.
  sbuf_init(&sb, mem, sizeof mem, 0);
  fmt_render(&sb, sv_from_cstr("{u}"), unfilled, 1, cyan, none);
  CHECK_MEM(sb.p, sb.len,
            "\x1b[0;36m\xee\x82\xb6\x1b[0;31;46mU\x1b[0;36m\xee\x82\xb4");
}

int
main(void)
{
  (void)cyan;

  test_swallow();
  test_escapes();
  test_styles();
  test_truncation();
  test_pill();

  return(check_failures ? 1 : 0);
}
