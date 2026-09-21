// dccstatusline — MIT
// test_config: INI dialect, overlay semantics, per-key tolerance

#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "check.h"

static int tok(section_id_t, const char *);
static bool color_eq(color_t, color_t);
static void test_defaults(void);
static void test_overlay(void);
static void test_values(void);
static void test_cwd_shortening(void);
static void test_path(void);

// color_t is four bytes of enum followed by three of rgb, so it carries a byte
// of tail padding. Compare the fields the type declares, never its bytes: what
// lands in padding is the compiler's business, and gcc and clang disagree.
static bool
color_eq(color_t a, color_t b)
{
  return(a.kind == b.kind && a.r == b.r && a.g == b.g && a.b == b.b);
}

// Mirror of the internal token lookup, via the public schema table.
static int
tok(section_id_t id, const char *name)
{
  int i;

  for(i = 0; dcc_sections[id].tokens[i]; i++)
    if(strcmp(dcc_sections[id].tokens[i], name) == 0) return(i);

  return(-1);
}

static void
test_defaults(void)
{
  config_t cfg;

  config_defaults(&cfg);
  CHECK(cfg.norder == SEC_COUNT, "all sections on by default");
  CHECK_MEM(cfg.separator.p, cfg.separator.n, " \xe2\x94\x82 ");
  CHECK(cfg.sec[SEC_CONTEXT].format.n > 0, "context has a format");
  CHECK(cfg.cwd_style == CWD_ABBREV, "cwd abbreviates by default");
  CHECK(cfg.cwd_depth == 0 && cfg.cwd_max_len == 0, "cwd is unshortened by default");
}

static void
test_overlay(void)
{
  char ini[] =
    "# a comment\r\n"
    "stray = ignored\n"                        // before any section header
    "[statusline]\n"
    "sections = git cwd bogus\n"
    "separator = \" * \"\n"
    "separator_fg = red\n"
    "thousands = \"\"\n"
    "\n"
    "[git]\n"
    "format = \"g:{branch}\"\n"
    "branch_fg = 208\n"
    "label = SCM\n"
    "\n"
    "[cwd]\r\n"
    "style = basename\r\n"
    "fg = #102030\n"
    "\n"
    "[nope]\n"
    "key = value\n"
    "\n"
    "[model]\n"
    "effort_fg = not_a_color\n"
    "novel_key = ignored\n";
  config_t cfg, defaults;
  int bi = tok(SEC_GIT, "branch"), ei = tok(SEC_MODEL, "effort");

  config_defaults(&defaults);
  config_defaults(&cfg);
  config_load(&cfg, ini, sizeof ini - 1);

  CHECK(cfg.norder == 2 && cfg.order[0] == SEC_GIT && cfg.order[1] == SEC_CWD,
        "order parsed, bogus name dropped (n=%u)", cfg.norder);
  CHECK_MEM(cfg.separator.p, cfg.separator.n, " * ");
  CHECK(cfg.sep_fg.kind == COLOR_NAMED16 && cfg.sep_fg.r == 1, "separator_fg red");
  CHECK(cfg.thousands.n == 0, "thousands disabled via quoted empty");

  CHECK_MEM(cfg.sec[SEC_GIT].format.p, cfg.sec[SEC_GIT].format.n, "g:{branch}");
  CHECK(bi >= 0 && cfg.sec[SEC_GIT].tok_fg[bi].kind == COLOR_IDX256 &&
        cfg.sec[SEC_GIT].tok_fg[bi].r == 208, "branch_fg 208");
  CHECK_MEM(cfg.sec[SEC_GIT].label.p, cfg.sec[SEC_GIT].label.n, "SCM");

  CHECK(cfg.cwd_style == CWD_BASENAME, "style basename");
  CHECK(cfg.sec[SEC_CWD].fg.kind == COLOR_RGB &&
        cfg.sec[SEC_CWD].fg.r == 0x10 && cfg.sec[SEC_CWD].fg.g == 0x20 &&
        cfg.sec[SEC_CWD].fg.b == 0x30, "cwd truecolor fg");

  CHECK(ei >= 0 && color_eq(cfg.sec[SEC_MODEL].tok_fg[ei],
                            defaults.sec[SEC_MODEL].tok_fg[ei]),
        "bad color keeps that key's default");
}

static void
test_values(void)
{
  char ini[] =
    "[git]\n"
    "format = {branch} # trailing comment\n"
    "label = \"a\\\"b\\\\c\" # after the quote\n"
    "\n"
    "[cwd]\n"
    "format = a=b\n"
    "no_equals_line\n";
  config_t cfg;

  config_defaults(&cfg);
  config_load(&cfg, ini, sizeof ini - 1);

  CHECK_MEM(cfg.sec[SEC_GIT].format.p, cfg.sec[SEC_GIT].format.n, "{branch}");
  CHECK_MEM(cfg.sec[SEC_GIT].label.p, cfg.sec[SEC_GIT].label.n, "a\"b\\c");
  CHECK_MEM(cfg.sec[SEC_CWD].format.p, cfg.sec[SEC_CWD].format.n, "a=b");
}

static void
test_cwd_shortening(void)
{
  char ini[] =
    "[cwd]\n"
    "style   = shrink\n"
    "depth   = 3\n"
    "max_len = 40\n";
  char bad[] =
    "[cwd]\n"
    "style   = sideways\n"
    "depth   = 2x\n"
    "max_len = 70000\n";       // past uint16: refused whole
  config_t cfg;

  config_defaults(&cfg);
  config_load(&cfg, ini, sizeof ini - 1);
  CHECK(cfg.cwd_style == CWD_SHRINK, "style shrink");
  CHECK(cfg.cwd_depth == 3, "depth 3 (got %u)", cfg.cwd_depth);
  CHECK(cfg.cwd_max_len == 40, "max_len 40 (got %u)", cfg.cwd_max_len);

  {
    char repo[] = "[cwd]\nstyle = repo\n";

    config_defaults(&cfg);
    config_load(&cfg, repo, sizeof repo - 1);
    CHECK(cfg.cwd_style == CWD_REPO, "style repo");
  }

  config_defaults(&cfg);
  cfg.cwd_depth = 2;
  cfg.cwd_max_len = 30;
  config_load(&cfg, bad, sizeof bad - 1);
  CHECK(cfg.cwd_style == CWD_ABBREV, "bad style keeps the default");
  CHECK(cfg.cwd_depth == 2, "bad depth keeps its value (got %u)", cfg.cwd_depth);
  CHECK(cfg.cwd_max_len == 30, "overflowing max_len keeps its value (got %u)",
        cfg.cwd_max_len);
}

static void
test_path(void)
{
  char buf[256];

  CHECK(setenv("DCCSTATUSLINE_CONFIG", "/tmp/x.conf", 1) == 0, "setenv");
  CHECK(config_path(buf, sizeof buf), "explicit path resolves");
  CHECK(strcmp(buf, "/tmp/x.conf") == 0, "explicit path wins");

  CHECK(unsetenv("DCCSTATUSLINE_CONFIG") == 0, "unsetenv");
  CHECK(setenv("XDG_CONFIG_HOME", "/xdg", 1) == 0, "setenv xdg");
  CHECK(config_path(buf, sizeof buf), "xdg path resolves");
  CHECK(strcmp(buf, "/xdg/dccstatusline/config") == 0, "xdg layout");

  CHECK(unsetenv("XDG_CONFIG_HOME") == 0, "unsetenv xdg");
  CHECK(setenv("HOME", "/home/u", 1) == 0, "setenv home");
  CHECK(config_path(buf, sizeof buf), "home path resolves");
  CHECK(strcmp(buf, "/home/u/.config/dccstatusline/config") == 0, "home layout");

  CHECK(!config_path(buf, 8), "tiny cap refuses");
}

int
main(void)
{
  test_defaults();
  test_overlay();
  test_values();
  test_cwd_shortening();
  test_path();

  return(check_failures ? 1 : 0);
}
