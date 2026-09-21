// dccstatusline — MIT
// test_sections: producers, derivations, assembly — mostly with colors off

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sections.h"

#include "check.h"

// The clock the reset cases are written against: Sat 2025-02-01 12:00:00 UTC,
// with the two absolute stamps they compare into. TZ is pinned to UTC before
// any of them render.
#define TEST_NOW 1738411200
#define SAT_1600 1738425600
#define SUN_0000 1738454400
#define M 60
#define H 3600
#define D 86400

static void plain(config_t *);
static size_t strip_sgr(const char *, size_t, char *);
static payload_t sample(void);
static void render_at(const payload_t *, const config_t *, const gitinfo_t *,
                      int64_t, char *, size_t *);
static void render_plain(const payload_t *, const config_t *,
                         const gitinfo_t *, char *, size_t *);
static void test_full_line(void);
static void test_vanishing(void);
static void test_derived_pct(void);
static void test_ver_table(void);
static void test_ver_suppression(void);
static void test_resets_styles(void);
static void test_resets_default(void);
static void test_cwd_styles(void);
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
render_at(const payload_t *pay, const config_t *cfg, const gitinfo_t *git,
          int64_t now, char *out, size_t *outn)
{
  static char mem[4096];
  sbuf_t sb;

  sbuf_init(&sb, mem, sizeof mem, 0);
  statusline_render(&sb, pay, cfg, git, now);
  *outn = strip_sgr(sb.p, sb.len, out);
}

// Cases with nothing time-dependent to say still need a clock; they get the
// same fixed one.
static void
render_plain(const payload_t *pay, const config_t *cfg, const gitinfo_t *git,
             char *out, size_t *outn)
{
  render_at(pay, cfg, git, TEST_NOW, out, outn);
}

static void
test_full_line(void)
{
  config_t cfg;
  payload_t pay = sample();
  gitinfo_t git = { true, "main", 4, 0 };
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
  gitinfo_t nogit = { false, "", 0, 0 };
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
  gitinfo_t nogit = { false, "", 0, 0 };
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
  gitinfo_t nogit = { false, "", 0, 0 };
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
test_ver_suppression(void)
{
  static const struct
  {
    const char *name, *id, *want;
  } rows[] =
  {
    { "Fable 5", "claude-fable-5",           "Fable 5"  },   // name ends with ver
    { "Opus",    "claude-opus-4-1-20250805", "Opus 4.1" },   // bare name keeps ver
    { "4.1",     "claude-opus-4-1-20250805", "4.1"      },   // name IS the ver
    { "GPT-5",   "claude-fable-5",           "GPT-5 5"  },   // '-' is no word break
  };
  config_t cfg;
  gitinfo_t nogit = { false, "", 0, 0 };
  char ini[] = "[statusline]\nsections = model\n[model]\nformat = {name} {ver}\n";
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
    pay.model_name = sv_from_cstr(rows[i].name);
    pay.model_id = sv_from_cstr(rows[i].id);
    render_plain(&pay, &cfg, &nogit, out, &n);
    CHECK_MEM(out, n, rows[i].want);
  }
}

// Every reset style, both windows. Times are stated as offsets from TEST_NOW
// (Sat 2025-02-01 12:00:00 UTC) so the arithmetic reads at a glance; the two
// absolute stamps are that same Saturday at 16:00 and the Sunday midnight
// after it, which pin the AM/PM turn.
static void
test_resets_styles(void)
{
  static const struct
  {
    bool longw;         // plan_long: gains the weekday, counts days
    const char *style;
    int64_t at, now;
    const char *want;
  } rows[] =
  {
    { false, "countdown", TEST_NOW + 4 * H + 12 * M,      TEST_NOW, "5h 24% 04:12"     },
    { false, "countdown", TEST_NOW + 4 * H + 12 * M + 59, TEST_NOW, "5h 24% 04:12"     },
    { false, "countdown", TEST_NOW - 60,                  TEST_NOW, "5h 24% 00:00"     },
    { false, "countdown", TEST_NOW + 4 * H,               0,        "5h 24%"           },
    { true,  "countdown", TEST_NOW + 3 * D + 19 * H + 47 * M, TEST_NOW, "7d 41% 03:19:47" },
    { true,  "countdown", TEST_NOW + 5 * M,               TEST_NOW, "7d 41% 00:00:05"  },
    { false, "clock",     SAT_1600,                       TEST_NOW, "5h 24% 16:00"     },
    { true,  "clock",     SAT_1600,                       TEST_NOW, "7d 41% Sat 16:00" },
    { false, "clock12",   SAT_1600,                       TEST_NOW, "5h 24% 4:00 PM"   },
    { true,  "clock12",   SAT_1600,                       TEST_NOW, "7d 41% Sat 4:00 PM" },
    { false, "clock12",   SUN_0000,                       TEST_NOW, "5h 24% 12:00 AM"  },
    { false, "clock12",   TEST_NOW,                       TEST_NOW, "5h 24% 12:00 PM"  },
  };
  size_t i;

  CHECK(setenv("TZ", "UTC", 1) == 0, "setenv TZ");
  tzset();

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    const char *name = rows[i].longw ? "plan_long" : "plan_short";
    plan_slot_t slot = rows[i].longw ? PLAN_LONG : PLAN_SHORT;
    config_t cfg;
    payload_t pay;
    gitinfo_t nogit = { false, "", 0, 0 };
    char ini[256], out[256];
    size_t n;
    int len = snprintf(ini, sizeof ini,
                       "[statusline]\nsections = %s\n"
                       "[%s]\nformat = {window} {pct} {resets}\nresets = %s\n",
                       name, name, rows[i].style);

    CHECK(len > 0 && (size_t)len < sizeof ini, "row %zu: ini fits", i);

    memset(&pay, 0, sizeof pay);
    pay.plan[slot].present = true;
    pay.plan[slot].pct = rows[i].longw ? 41 : 24;
    pay.plan[slot].window = sv_from_cstr(rows[i].longw ? "7d" : "5h");
    pay.plan[slot].has_resets = true;
    pay.plan[slot].resets_at = rows[i].at;

    config_defaults(&cfg);
    plain(&cfg);
    config_load(&cfg, ini, (size_t)len);
    render_at(&pay, &cfg, &nogit, rows[i].now, out, &n);
    CHECK_MEM(out, n, rows[i].want);
  }
}

// The shipped default carries ↻{resets} and counts down; a window with no
// reset stamp still renders, the token and its literal run swallowed.
static void
test_resets_default(void)
{
  config_t cfg;
  payload_t pay;
  gitinfo_t nogit = { false, "", 0, 0 };
  char ini[] = "[statusline]\nsections = plan_short\n";
  char out[256];
  size_t n;

  memset(&pay, 0, sizeof pay);
  pay.plan[PLAN_SHORT].present = true;
  pay.plan[PLAN_SHORT].pct = 24;
  pay.plan[PLAN_SHORT].window = sv_from_cstr("5h");

  config_defaults(&cfg);
  plain(&cfg);
  config_load(&cfg, ini, sizeof ini - 1);
  render_at(&pay, &cfg, &nogit, TEST_NOW, out, &n);
  CHECK_MEM(out, n, "5h 24%");

  pay.plan[PLAN_SHORT].has_resets = true;
  pay.plan[PLAN_SHORT].resets_at = TEST_NOW + 2 * H + 5 * M;
  render_at(&pay, &cfg, &nogit, TEST_NOW, out, &n);
  CHECK_MEM(out, n, "5h 24% \xe2\x86\xbb""02:05");
}

static void
test_cwd_styles(void)
{
  static const struct
  {
    const char *cwd, *ini, *want;
  } rows[] =
  {
    // A repository two levels up: the root's own name leads the path.
    { "/src/repo/a/b", "[cwd]\nstyle = repo\n",            "repo/a/b"          },
    { "/src/repo",     "[cwd]\nstyle = repo\n",            "repo"              },
    // Outside a repository, repo has nothing to measure against: abbreviate.
    { "/home/u/proj",  "[cwd]\nstyle = repo\n",            "~/proj"            },

    { "/a/b/c/proj",   "[cwd]\ndepth = 2\n",               "\xe2\x80\xa6/c/proj" },
    { "/a/b/c/proj",   "[cwd]\nmax_len = 8\n",             "\xe2\x80\xa6/c/proj" },
    { "/a/b/c/proj",   "[cwd]\nmax_len = 7\n",             "\xe2\x80\xa6/proj"   },
    { "/a/bb/cc/proj", "[cwd]\nstyle = shrink\n",          "/a/b/c/proj"       },
    { "/a/bb/cc/proj", "[cwd]\nstyle = shrink\ndepth = 2\n", "/a/b/cc/proj"    },
    // The stages compose: shrink first, then the column budget.
    { "/aa/bb/cc/proj-with-a-long-name",
      "[cwd]\nstyle = shrink\nmax_len = 10\n",             "\xe2\x80\xa6long-name" },
    // depth on top of repo: the repository name gives way like any other.
    { "/src/repo/a/b", "[cwd]\nstyle = repo\ndepth = 2\n", "\xe2\x80\xa6/a/b"  },
  };
  size_t i;

  CHECK(setenv("HOME", "/home/u", 1) == 0, "setenv HOME");

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    config_t cfg;
    payload_t pay;
    gitinfo_t git = { true, "main", 4, 9 };   // "/src/repo" is the root
    char ini[128], out[256];
    size_t n;

    memset(&pay, 0, sizeof pay);
    pay.has_cwd = true;
    pay.cwd = sv_from_cstr(rows[i].cwd);

    if(strncmp(rows[i].cwd, "/src/repo", 9) != 0)
    {
      git.present = false;
      git.root_n = 0;
    }

    memcpy(ini, rows[i].ini, strlen(rows[i].ini) + 1);

    config_defaults(&cfg);
    plain(&cfg);
    config_load(&cfg, ini, strlen(rows[i].ini));
    cfg.norder = 1;
    cfg.order[0] = SEC_CWD;

    render_plain(&pay, &cfg, &git, out, &n);
    CHECK_MEM(out, n, rows[i].want);
  }
}

static void
test_ansi_exact(void)
{
  config_t cfg;
  payload_t pay;
  gitinfo_t git = { true, "main", 4, 0 };
  char ini[] = "[statusline]\nsections = git\n";
  static char mem[256];
  sbuf_t sb;

  memset(&pay, 0, sizeof pay);
  config_defaults(&cfg);   // git default: bright_green fg, no token override
  config_load(&cfg, ini, sizeof ini - 1);
  sbuf_init(&sb, mem, sizeof mem, 0);
  statusline_render(&sb, &pay, &cfg, &git, TEST_NOW);
  CHECK_MEM(sb.p, sb.len, "\x1b[0;92mmain");
}

int
main(void)
{
  test_full_line();
  test_vanishing();
  test_derived_pct();
  test_ver_table();
  test_ver_suppression();
  test_resets_styles();
  test_resets_default();
  test_cwd_styles();
  test_ansi_exact();

  return(check_failures ? 1 : 0);
}
