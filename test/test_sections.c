// dccstatusline — MIT
// test_sections: producers, derivations, assembly — mostly with colors off

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sections.h"

#include "check.h"

static void plain(config_t *);
static size_t strip_sgr(const char *, size_t, char *);
static payload_t sample(void);
static void render_plain(const payload_t *, const config_t *,
                         const gitinfo_t *, char *, size_t *);
static void test_full_line(void);
static void test_vanishing(void);
static void test_derived_pct(void);
static void test_ver_table(void);
static void test_resets_clock(void);
static void test_ansi_exact(void);

// Colors off everywhere: expectations read as plain text. SGR bytes are
// pinned by test_color/test_fmt and one exact case below.
static void
plain(config_t *cfg)
{
  color_t none = { COLOR_NONE, 0, 0, 0 };
  size_t s, t;

  cfg->sep_fg = none;
  cfg->sep_bg = none;

  for(s = 0; s < SEC_COUNT; s++)
  {
    cfg->sec[s].fg = none;
    cfg->sec[s].bg = none;

    for(t = 0; t < DCC_MAX_TOKENS; t++)
    {
      cfg->sec[s].tok_fg[t] = none;
      cfg->sec[s].tok_bg[t] = none;
    }
  }
}

static size_t
strip_sgr(const char *in, size_t n, char *out)
{
  size_t i = 0, w = 0;

  while(i < n)
  {
    if(in[i] == '\x1b' && i + 1 < n && in[i + 1] == '[')
    {
      i += 2;

      while(i < n && in[i] != 'm') i++;

      if(i < n) i++;

      continue;
    }

    out[w++] = in[i++];
  }

  return(w);
}

static payload_t
sample(void)
{
  payload_t p;

  memset(&p, 0, sizeof p);
  p.has_cwd = true;
  p.cwd = sv_from_cstr("/home/u/proj");
  p.has_model = true;
  p.model_name = sv_from_cstr("Fable");
  p.model_id = sv_from_cstr("claude-fable-5");
  p.effort = sv_from_cstr("max");
  p.has_context = true;
  p.ctx_used = 16700;
  p.ctx_ceiling = 200000;
  p.has_pct = true;
  p.ctx_pct = 8;
  p.plan[PLAN_SHORT].present = true;
  p.plan[PLAN_SHORT].pct = 24;
  p.plan[PLAN_SHORT].window = sv_from_cstr("5h");
  p.plan[PLAN_LONG].present = true;
  p.plan[PLAN_LONG].pct = 41;
  p.plan[PLAN_LONG].window = sv_from_cstr("7d");

  return(p);
}

static void
render_plain(const payload_t *pay, const config_t *cfg, const gitinfo_t *git,
             char *out, size_t *outn)
{
  static char mem[4096];
  sbuf_t sb;

  sbuf_init(&sb, mem, sizeof mem, 0);
  statusline_render(&sb, pay, cfg, git);
  *outn = strip_sgr(sb.p, sb.len, out);
}

static void
test_full_line(void)
{
  config_t cfg;
  payload_t pay = sample();
  gitinfo_t git = { true, "main", 4 };
  char out[1024];
  size_t n;

  CHECK(setenv("HOME", "/home/u", 1) == 0, "setenv HOME");
  config_defaults(&cfg);
  plain(&cfg);
  render_plain(&pay, &cfg, &git, out, &n);
  CHECK_MEM(out, n,
            "~/proj \xe2\x94\x82 main \xe2\x94\x82 Fable 5 \xc2\xb7max \xe2\x94\x82 "
            "ctx 16,700/200,000 8% \xe2\x94\x82 5h 24% \xe2\x94\x82 7d 41%");
}

static void
test_vanishing(void)
{
  config_t cfg;
  payload_t pay = sample();
  gitinfo_t nogit = { false, "", 0 };
  char out[1024];
  size_t n;

  config_defaults(&cfg);
  plain(&cfg);

  // no git: its separator goes with it — no doubling
  pay.effort.n = 0;   // and no effort: "Fable 5", never "Fable 5 ·"
  render_plain(&pay, &cfg, &nogit, out, &n);
  CHECK_MEM(out, n,
            "~/proj \xe2\x94\x82 Fable 5 \xe2\x94\x82 "
            "ctx 16,700/200,000 8% \xe2\x94\x82 5h 24% \xe2\x94\x82 7d 41%");

  // nothing at all: empty render (main turns that into the fallback line)
  {
    payload_t empty;

    memset(&empty, 0, sizeof empty);
    render_plain(&empty, &cfg, &nogit, out, &n);
    CHECK(n == 0, "all-absent renders empty (got %zu bytes)", n);
  }
}

static void
test_derived_pct(void)
{
  config_t cfg;
  payload_t pay;
  gitinfo_t nogit = { false, "", 0 };
  char ini[] = "[statusline]\nsections = context\n";
  char out[256];
  size_t n;

  memset(&pay, 0, sizeof pay);
  pay.has_context = true;
  pay.ctx_used = 50000;
  pay.ctx_ceiling = 200000;

  config_defaults(&cfg);
  plain(&cfg);
  config_load(&cfg, ini, sizeof ini - 1);
  render_plain(&pay, &cfg, &nogit, out, &n);
  CHECK_MEM(out, n, "ctx 50,000/200,000 25%");

  pay.ctx_used = 300000;   // over the ceiling: clamps, still renders
  render_plain(&pay, &cfg, &nogit, out, &n);
  CHECK_MEM(out, n, "ctx 300,000/200,000 100%");
}

static void
test_ver_table(void)
{
  static const struct
  {
    const char *id, *want;
  } rows[] =
  {
    { "claude-fable-5",             "5"   },
    { "claude-opus-4-1-20250805",   "4.1" },
    { "claude-3-5-sonnet-20241022", "3.5" },
    { "claude-haiku-4-5-20251001",  "4.5" },
    { "utterly-alien",              ""    },
  };
  config_t cfg;
  gitinfo_t nogit = { false, "", 0 };
  char ini[] = "[statusline]\nsections = model\n[model]\nformat = {ver}\n";
  size_t i;

  config_defaults(&cfg);
  plain(&cfg);
  config_load(&cfg, ini, sizeof ini - 1);

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    payload_t pay;
    char out[256];
    size_t n;

    memset(&pay, 0, sizeof pay);
    pay.has_model = true;
    pay.model_name = sv_from_cstr("X");
    pay.model_id = sv_from_cstr(rows[i].id);
    render_plain(&pay, &cfg, &nogit, out, &n);
    CHECK_MEM(out, n, rows[i].want);
  }
}

static void
test_resets_clock(void)
{
  config_t cfg;
  payload_t pay;
  gitinfo_t nogit = { false, "", 0 };
  char ini[] = "[statusline]\nsections = plan_short\n"
               "[plan_short]\nformat = {window} {pct} {resets}\n";
  char out[256];
  size_t n;

  CHECK(setenv("TZ", "UTC", 1) == 0, "setenv TZ");
  tzset();

  memset(&pay, 0, sizeof pay);
  pay.plan[PLAN_SHORT].present = true;
  pay.plan[PLAN_SHORT].pct = 24;
  pay.plan[PLAN_SHORT].window = sv_from_cstr("5h");
  pay.plan[PLAN_SHORT].has_resets = true;
  pay.plan[PLAN_SHORT].resets_at = 3600;

  config_defaults(&cfg);
  plain(&cfg);
  config_load(&cfg, ini, sizeof ini - 1);
  render_plain(&pay, &cfg, &nogit, out, &n);
  CHECK_MEM(out, n, "5h 24% 01:00");

  pay.plan[PLAN_SHORT].resets_at = 86399;
  render_plain(&pay, &cfg, &nogit, out, &n);
  CHECK_MEM(out, n, "5h 24% 23:59");
}

static void
test_ansi_exact(void)
{
  config_t cfg;
  payload_t pay;
  gitinfo_t git = { true, "main", 4 };
  char ini[] = "[statusline]\nsections = git\n";
  static char mem[256];
  sbuf_t sb;

  memset(&pay, 0, sizeof pay);
  config_defaults(&cfg);   // git default: bright_green fg, no token override
  config_load(&cfg, ini, sizeof ini - 1);
  sbuf_init(&sb, mem, sizeof mem, 0);
  statusline_render(&sb, &pay, &cfg, &git);
  CHECK_MEM(sb.p, sb.len, "\x1b[0;92mmain");
}

int
main(void)
{
  test_full_line();
  test_vanishing();
  test_derived_pct();
  test_ver_table();
  test_resets_clock();
  test_ansi_exact();

  return(check_failures ? 1 : 0);
}
