// dccstatusline — MIT
// test_config: INI dialect, overlay semantics, per-key tolerance

#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "check.h"

static int tok(section_id_t, const char *);
static bool color_eq(color_t, color_t);
static bool sv_same(sv_t, sv_t);
static bool style_eq(const config_t *, const config_t *);
static bool is_profile(const config_t *, const char *const [SEC_COUNT]);
static void test_defaults(void);
static void test_overlay(void);
static void test_values(void);
static void test_cwd_shortening(void);
static void test_profile(void);
static void test_theme(void);
static void test_path(void);

// color_t is four bytes of enum followed by three of rgb, so it carries a byte
// of tail padding. Compare the fields the type declares, never its bytes: what
// lands in padding is the compiler's business, and gcc and clang disagree.
static bool
color_eq(color_t a, color_t b)
{
  return(a.kind == b.kind && a.r == b.r && a.g == b.g && a.b == b.b);
}

static bool
sv_same(sv_t a, sv_t b)
{
  return(a.n == b.n && (!a.n || memcmp(a.p, b.p, a.n) == 0));
}

// The line's whole look: the separator, every format, every color.
static bool
style_eq(const config_t *a, const config_t *b)
{
  size_t s, t;

  if(!sv_same(a->separator, b->separator) || !color_eq(a->sep_fg, b->sep_fg)
     || !color_eq(a->sep_bg, b->sep_bg))
    return(false);

  for(s = 0; s < SEC_COUNT; s++)
  {
    const section_cfg_t *x = &a->sec[s], *y = &b->sec[s];

    if(!sv_same(x->format, y->format) || !color_eq(x->fg, y->fg)
       || !color_eq(x->bg, y->bg))
      return(false);

    for(t = 0; t < DCC_MAX_TOKENS; t++)
      if(!color_eq(x->tok_fg[t], y->tok_fg[t])
         || !color_eq(x->tok_bg[t], y->tok_bg[t]))
        return(false);
  }

  return(true);
}

// All six sections in schema order, each with the given format.
static bool
is_profile(const config_t *cfg, const char *const want[SEC_COUNT])
{
  size_t s;

  if(cfg->norder != SEC_COUNT) return(false);

  for(s = 0; s < SEC_COUNT; s++)
    if(cfg->order[s] != s || !sv_same(cfg->sec[s].format, sv_from_cstr(want[s])))
      return(false);

  return(true);
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
test_profile(void)
{
  static const char *const basic[SEC_COUNT] =
  {
    "{path}", "{branch}", "{name} {ver} \xc2\xb7 {effort}", "{label} {pct}",
    "{window} {pct}", "{window} {pct}",
  };
  static const char *const detailed[SEC_COUNT] =
  {
    "{path}", "{branch}", "{name} {ver} \xc2\xb7 {effort}",
    "{label} {used}/{ceiling} {pct}",
    "{window} {pct} {resets}", "{window} {pct} {resets}",
  };
  // glyphs = on: the ⎇ before the branch, and the ↻ wherever a reset shows.
  static const char *const basic_glyphs[SEC_COUNT] =
  {
    "{path}", "\xe2\x8e\x87 {branch}", "{name} {ver} \xc2\xb7 {effort}",
    "{label} {pct}", "{window} {pct}", "{window} {pct}",
  };
  static const char *const detailed_glyphs[SEC_COUNT] =
  {
    "{path}", "\xe2\x8e\x87 {branch}", "{name} {ver} \xc2\xb7 {effort}",
    "{label} {used}/{ceiling} {pct}",
    "{window} {pct} \xe2\x86\xbb{resets}", "{window} {pct} \xe2\x86\xbb{resets}",
  };
  // glyphs = nerdfont: U+E0A0 for the ⎇, U+F021 and a space for the ↻, and the
  // model as a pill with the effort beside it instead of after the dot.
  static const char *const basic_nerdfont[SEC_COUNT] =
  {
    "{path}", "\xee\x82\xa0 {branch}", "{model_tag} {effort_plain}",
    "{label} {pct}", "{window} {pct}", "{window} {pct}",
  };
  static const char *const detailed_nerdfont[SEC_COUNT] =
  {
    "{path}", "\xee\x82\xa0 {branch}", "{model_tag} {effort_plain}",
    "{label} {used}/{ceiling} {pct}",
    "{window} {pct} \xef\x80\xa1 {resets}", "{window} {pct} \xef\x80\xa1 {resets}",
  };
  config_t cfg, defaults;
  size_t s;

  config_defaults(&defaults);
  CHECK(defaults.profile == PROFILE_NONE, "no profile by default");
  CHECK(!defaults.glyphs, "no glyphs by default");
  CHECK_MEM(defaults.sec[SEC_GIT].format.p, defaults.sec[SEC_GIT].format.n,
            "{branch}");

  {
    char ini[] = "[statusline]\nprofile = basic\n";

    config_defaults(&cfg);
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(cfg.profile == PROFILE_BASIC && is_profile(&cfg, basic), "profile basic");
  }

  {
    char ini[] = "[statusline]\nprofile = detailed\n";
    config_t want;

    config_defaults(&cfg);
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(cfg.profile == PROFILE_DETAILED && is_profile(&cfg, detailed),
          "profile detailed");

    // Only the list and the formats: every color, the separator, labels,
    // thousands, resets and cwd shortening stay where the config left them.
    want = defaults;

    for(s = 0; s < SEC_COUNT; s++) want.sec[s].format = cfg.sec[s].format;

    CHECK(style_eq(&cfg, &want), "detailed leaves the colors and separator");
    CHECK(sv_same(cfg.thousands, defaults.thousands)
          && cfg.cwd_style == defaults.cwd_style, "detailed leaves the rest");

    for(s = 0; s < SEC_COUNT; s++)
      CHECK(sv_same(cfg.sec[s].label, defaults.sec[s].label)
            && cfg.sec[s].resets == defaults.sec[s].resets,
            "detailed leaves %s label and resets", dcc_sections[s].name);
  }

  {
    // Wherever the keys sit, before the profile line or after it.
    char ini[] =
      "[statusline]\nsections = git\n"
      "[git]\nformat = X\n"
      "[statusline]\nprofile = basic\n"
      "[context]\nformat = Y\nlabel = tok\n"
      "[statusline]\nsections = model\n";

    config_defaults(&cfg);
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(is_profile(&cfg, basic), "profile beats sections and format keys");
    CHECK_MEM(cfg.sec[SEC_CONTEXT].label.p, cfg.sec[SEC_CONTEXT].label.n, "tok");
  }

  {
    char back[] = "[statusline]\nprofile = basic\nprofile = detailed\n";
    char bad[] = "[statusline]\nprofile = basic\nprofile = sparse\n";

    config_defaults(&cfg);
    config_load(&cfg, back, sizeof back - 1);
    CHECK(cfg.profile == PROFILE_DETAILED && is_profile(&cfg, detailed),
          "a later profile wins");

    config_defaults(&cfg);
    config_load(&cfg, bad, sizeof bad - 1);
    CHECK(cfg.profile == PROFILE_BASIC && is_profile(&cfg, basic),
          "bad profile keeps its value");
  }

  {
    // --profile: over whatever the config loaded, a profile key included.
    char ini[] = "[statusline]\nprofile = detailed\nsections = cwd\n";

    config_defaults(&cfg);
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(config_profile(&cfg, sv_from_cstr("basic")), "config_profile basic");
    CHECK(cfg.profile == PROFILE_BASIC && is_profile(&cfg, basic),
          "config_profile replaces the loaded profile");
    CHECK(config_profile(&cfg, sv_from_cstr("detailed"))
          && is_profile(&cfg, detailed), "config_profile detailed");

    CHECK(!config_profile(&cfg, sv_from_cstr("sparse")), "sparse is no profile");
    CHECK(!config_profile(&cfg, sv_from_cstr("none")), "none is no profile");
    CHECK(cfg.profile == PROFILE_DETAILED && is_profile(&cfg, detailed),
          "an unknown name leaves the config");
  }

  {
    // The glyphs are the profile's: without one, the key draws nothing.
    char ini[] = "[statusline]\nglyphs = on\n";

    config_defaults(&cfg);
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(cfg.glyphs, "glyphs = on is recorded");

    for(s = 0; s < SEC_COUNT; s++)
      CHECK(sv_same(cfg.sec[s].format, defaults.sec[s].format),
            "glyphs without a profile leave the %s format", dcc_sections[s].name);
  }

  {
    char after[] = "[statusline]\nprofile = basic\nglyphs = on\n";
    char before[] = "[statusline]\nglyphs = on\nprofile = detailed\n";
    char off[] = "[statusline]\nglyphs = on\nprofile = detailed\nglyphs = off\n";
    char bad[] = "[statusline]\nglyphs = on\nprofile = basic\nglyphs = yes\n";

    config_defaults(&cfg);
    config_load(&cfg, after, sizeof after - 1);
    CHECK(is_profile(&cfg, basic_glyphs), "basic with glyphs: the branch glyph alone");

    config_defaults(&cfg);
    config_load(&cfg, before, sizeof before - 1);
    CHECK(is_profile(&cfg, detailed_glyphs), "glyphs before the profile line");

    config_defaults(&cfg);
    config_load(&cfg, off, sizeof off - 1);
    CHECK(!cfg.glyphs && is_profile(&cfg, detailed), "a later glyphs = off wins");

    config_defaults(&cfg);
    config_load(&cfg, bad, sizeof bad - 1);
    CHECK(cfg.glyphs && is_profile(&cfg, basic_glyphs), "bad glyphs keeps its value");
  }

  {
    // As with on: nothing without a profile.
    char ini[] = "[statusline]\nglyphs = nerdfont\n";

    config_defaults(&cfg);
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(cfg.glyphs == GLYPHS_NERDFONT, "glyphs = nerdfont is recorded");

    for(s = 0; s < SEC_COUNT; s++)
      CHECK(sv_same(cfg.sec[s].format, defaults.sec[s].format),
            "nerdfont without a profile leaves the %s format", dcc_sections[s].name);
  }

  {
    char after[] = "[statusline]\nprofile = basic\nglyphs = nerdfont\n";
    char before[] = "[statusline]\nglyphs = nerdfont\nprofile = detailed\n";
    char off[] = "[statusline]\nglyphs = nerdfont\nprofile = detailed\nglyphs = off\n";
    char on[] = "[statusline]\nglyphs = nerdfont\nprofile = detailed\nglyphs = on\n";
    char bad[] = "[statusline]\nglyphs = nerdfont\nprofile = basic\nglyphs = yes\n";

    config_defaults(&cfg);
    config_load(&cfg, after, sizeof after - 1);
    CHECK(is_profile(&cfg, basic_nerdfont), "basic with nerdfont: the branch glyph alone");

    config_defaults(&cfg);
    config_load(&cfg, before, sizeof before - 1);
    CHECK(is_profile(&cfg, detailed_nerdfont), "nerdfont before the profile line");

    config_defaults(&cfg);
    config_load(&cfg, off, sizeof off - 1);
    CHECK(!cfg.glyphs && is_profile(&cfg, detailed), "a later glyphs = off beats nerdfont");

    config_defaults(&cfg);
    config_load(&cfg, on, sizeof on - 1);
    CHECK(cfg.glyphs == GLYPHS_ON && is_profile(&cfg, detailed_glyphs),
          "a later glyphs = on beats nerdfont");

    config_defaults(&cfg);
    config_load(&cfg, bad, sizeof bad - 1);
    CHECK(cfg.glyphs == GLYPHS_NERDFONT && is_profile(&cfg, basic_nerdfont),
          "bad glyphs keeps nerdfont");
  }

  {
    // --glyphs: over whatever the config loaded, and kept by --profile.
    char ini[] = "[statusline]\nprofile = basic\nglyphs = on\n";

    config_defaults(&cfg);
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(config_profile(&cfg, sv_from_cstr("detailed"))
          && is_profile(&cfg, detailed_glyphs), "config_profile keeps the glyphs");
    CHECK(config_glyphs(&cfg, sv_from_cstr("off")) && !cfg.glyphs
          && is_profile(&cfg, detailed), "config_glyphs off");
    CHECK(config_glyphs(&cfg, sv_from_cstr("on")) && cfg.glyphs
          && is_profile(&cfg, detailed_glyphs), "config_glyphs on");
    CHECK(!config_glyphs(&cfg, sv_from_cstr("yes")) && cfg.glyphs
          && is_profile(&cfg, detailed_glyphs), "yes is no switch");
    CHECK(config_glyphs(&cfg, sv_from_cstr("nerdfont"))
          && cfg.glyphs == GLYPHS_NERDFONT
          && is_profile(&cfg, detailed_nerdfont), "config_glyphs nerdfont");
    CHECK(config_profile(&cfg, sv_from_cstr("basic"))
          && is_profile(&cfg, basic_nerdfont), "config_profile keeps nerdfont");
  }
}

static void
test_theme(void)
{
  static const color_t nord_frost = { COLOR_RGB, 0x88, 0xc0, 0xd0 };
  config_t cfg, defaults;
  const char *name;
  size_t i;

  config_defaults(&defaults);

  {
    char ini[] = "[statusline]\ntheme = nord\n";

    config_defaults(&cfg);
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(cfg.themed, "nord applied");
    CHECK(color_eq(cfg.sec[SEC_CWD].fg, nord_frost), "nord cwd fg");
  }

  {
    // In order, like every other key: before the theme loses, after it wins.
    char ini[] =
      "[cwd]\nfg = red\n"
      "[statusline]\ntheme = nord\n"
      "[git]\nfg = 208\n";

    config_defaults(&cfg);
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(color_eq(cfg.sec[SEC_CWD].fg, nord_frost), "key before the theme loses");
    CHECK(cfg.sec[SEC_GIT].fg.kind == COLOR_IDX256 && cfg.sec[SEC_GIT].fg.r == 208,
          "key after the theme wins");
  }

  {
    char ini[] = "[statusline]\ntheme = nosuch\n";

    config_defaults(&cfg);
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(!cfg.themed && style_eq(&cfg, &defaults), "unknown theme keeps defaults");
  }

  {
    char ini[] = "[statusline]\ntheme = nord\ntheme = gruvbox\n";

    config_defaults(&cfg);
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(color_eq(cfg.sec[SEC_CWD].fg, nord_frost), "first theme wins");
  }

  {
    // --theme: the config's own theme key yields without a word, and its
    // color keys still land on top.
    char ini[] = "[statusline]\ntheme = dracula\n[cwd]\nfg = red\n";

    config_defaults(&cfg);
    CHECK(config_theme(&cfg, sv_from_cstr("nord")), "nord applies");
    cfg.theme_pinned = true;
    config_load(&cfg, ini, sizeof ini - 1);
    CHECK(cfg.themed, "a pinned theme stays applied");
    CHECK(color_eq(cfg.sec[SEC_GIT].fg, nord_frost), "the pinned theme wins");
    CHECK(cfg.sec[SEC_CWD].fg.kind == COLOR_NAMED16 && cfg.sec[SEC_CWD].fg.r == 1,
          "a color key lands on the pinned theme");
  }

  // Every preset applies and changes nothing but color; each recolors
  // something, except default, which is the defaults exactly.
  for(i = 0; (name = config_theme_name(i)); i++)
  {
    size_t s;

    config_defaults(&cfg);
    CHECK(config_theme(&cfg, sv_from_cstr(name)), "%s applies", name);
    CHECK(cfg.norder == defaults.norder
          && memcmp(cfg.order, defaults.order, sizeof cfg.order) == 0,
          "%s leaves the section order", name);
    CHECK(cfg.cwd_style == defaults.cwd_style && cfg.cwd_depth == 0
          && cfg.cwd_max_len == 0, "%s leaves cwd shortening", name);
    CHECK(sv_same(cfg.thousands, defaults.thousands), "%s leaves thousands", name);
    CHECK(sv_same(cfg.separator, defaults.separator), "%s leaves the separator", name);
    CHECK(cfg.profile == PROFILE_NONE, "%s leaves the profile", name);
    CHECK((strcmp(name, "default") == 0) == style_eq(&cfg, &defaults),
          "%s restyles exactly when it is not default", name);

    for(s = 0; s < SEC_COUNT; s++)
      CHECK(cfg.sec[s].resets == defaults.sec[s].resets
            && sv_same(cfg.sec[s].label, defaults.sec[s].label)
            && sv_same(cfg.sec[s].format, defaults.sec[s].format),
            "%s leaves %s resets, label and format", name, dcc_sections[s].name);

    // The pill leaves its fill unset, so it takes the model's color, under text
    // of its own. {effort} stays unfilled, and {effort_plain} has no color, so
    // it takes the model's too.
    CHECK(cfg.sec[SEC_MODEL].tok_bg[tok(SEC_MODEL, "model_tag")].kind == COLOR_NONE
          && cfg.sec[SEC_MODEL].tok_fg[tok(SEC_MODEL, "model_tag")].kind != COLOR_NONE
          && cfg.sec[SEC_MODEL].tok_bg[tok(SEC_MODEL, "effort")].kind == COLOR_NONE
          && cfg.sec[SEC_MODEL].tok_fg[tok(SEC_MODEL, "effort_plain")].kind == COLOR_NONE
          && cfg.sec[SEC_MODEL].tok_bg[tok(SEC_MODEL, "effort_plain")].kind == COLOR_NONE,
          "%s fills the model pill and only the pill", name);

    // The usage figures wear the model's color. Each needs its own key, or the
    // defaults' cyan and yellow show through.
    if(strcmp(name, "default") != 0)
    {
      const color_t second = cfg.sec[SEC_MODEL].fg;

      CHECK(color_eq(cfg.sec[SEC_CONTEXT].tok_fg[tok(SEC_CONTEXT, "used")], second)
            && color_eq(cfg.sec[SEC_CONTEXT].tok_fg[tok(SEC_CONTEXT, "pct")], second)
            && color_eq(cfg.sec[SEC_PLAN_SHORT].tok_fg[tok(SEC_PLAN_SHORT, "pct")], second)
            && color_eq(cfg.sec[SEC_PLAN_LONG].tok_fg[tok(SEC_PLAN_LONG, "pct")], second),
            "%s paints the usage figures in its second color", name);
    }
  }

  CHECK(i >= 2, "presets listed (%zu)", i);
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
  test_profile();
  test_theme();
  test_path();

  return(check_failures ? 1 : 0);
}
