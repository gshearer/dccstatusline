#ifndef DCC_CONFIG_H
#define DCC_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "color.h"
#include "schema.h"
#include "util.h"

typedef enum
{
  CWD_ABBREV = 0,
  CWD_FULL,
  CWD_BASENAME,
  CWD_SHRINK,
  CWD_REPO,
} cwd_style_t;

// How {resets} renders. The countdown is time remaining; the clock styles are
// the local instant of the reset itself. The long window prefixes its weekday
// and counts down in days, since "19:00" alone cannot say which of seven.
typedef enum
{
  RESETS_COUNTDOWN = 0,
  RESETS_CLOCK,
  RESETS_CLOCK12,
} resets_style_t;

// A profile is a whole line, decided for you: set, it replaces the section
// list and every format. None leaves both to the config.
typedef enum
{
  PROFILE_NONE = 0,
  PROFILE_BASIC,
  PROFILE_DETAILED,
  PROFILE_COUNT,
} profile_t;

// What a profile draws for its glyphs. The Nerd Font ones are private-use code
// points, empty boxes without that font, so they are a choice and never the
// default.
typedef enum
{
  GLYPHS_OFF = 0,
  GLYPHS_ON,
  GLYPHS_NERDFONT,
} glyphs_t;

typedef struct
{
  sv_t format;
  sv_t label;
  color_t fg, bg;                    // base style: literals and un-overridden tokens
  color_t tok_fg[DCC_MAX_TOKENS];    // indexed per dcc_sections token order
  color_t tok_bg[DCC_MAX_TOKENS];
  resets_style_t resets;             // plan sections only
} section_cfg_t;

typedef struct
{
  uint8_t order[SEC_COUNT];
  uint8_t norder;
  sv_t separator;
  color_t sep_fg, sep_bg;
  sv_t thousands;
  cwd_style_t cwd_style;
  uint16_t cwd_depth;      // trailing components kept whole; 0 = off
  uint16_t cwd_max_len;    // codepoint budget for the rendered path; 0 = off
  bool themed;             // a theme key has been applied; the first one wins
  bool theme_pinned;       // --theme set it: the config's theme key yields quietly
  profile_t profile;       // applied after the whole load; see config_load
  glyphs_t glyphs;         // a profile's ⎇, ↻ and model pill; nothing without one
  section_cfg_t sec[SEC_COUNT];
} config_t;

// Defaults are the shipped theme — identical to examples/config. config_load
// overlays an INI buffer onto them per key: an unparseable value keeps that
// key's default (one stderr note), unknown sections and keys are ignored.
// A profile key, and the glyphs key with it, is applied after the whole
// buffer, so it wins wherever the sections and format keys sit. Views alias
// the config buffer, which must outlive the render.
void config_defaults(config_t *);
void config_load(config_t *, char *, size_t);

// Overlays a compiled-in preset — colors only — as if its INI text
// sat where the call happens, so later keys still override it. False for an
// unknown name. Colors are copied by value, so no view outlives the call.
bool config_theme(config_t *, sv_t);

// Sets the named profile and applies it now, over whatever the config loaded.
// False for an unknown name, leaving the config untouched.
bool config_profile(config_t *, sv_t);

// Sets the glyphs key from off, on or nerdfont and reapplies the profile, if
// any. False for anything else, leaving the config untouched.
bool config_glyphs(config_t *, sv_t);

// The i-th preset's name, in --themes order; NULL past the last.
const char *config_theme_name(size_t);

// $DCCSTATUSLINE_CONFIG, else $XDG_CONFIG_HOME/dccstatusline/config, else
// ~/.config/dccstatusline/config. False when no path can be resolved or fit.
bool config_path(char *, size_t);

#ifdef CONFIG_INTERNAL
#define CUR_NONE   (-2)
#define CUR_GLOBAL (-1)

typedef struct
{
  const char *name;
  const char *ini;
} theme_t;

// Each preset paints the line in two colors of its palette and one gray: the
// first for where you are (cwd and branch), the second for the model and every
// usage figure, and the gray for the separator and all literals. {model_tag}
// sets no fill, so it fills with the second color as the model's fg, and
// writes on the palette's background, the one dark text on the line.
// Never the separator, a format, the profile, sections, cwd shortening, or
// resets — a theme recolors the line, it does not rewrite or rearrange it.
static const theme_t themes[] =
{
  { "default", "" },
  { "catppuccin",
    "[statusline]\n"
    "separator_fg = #7f849c\n"
    "[cwd]\n"
    "fg = #cba6f7\n"
    "[git]\n"
    "fg = #cba6f7\n"
    "[model]\n"
    "fg = #89b4fa\n"
    "effort_fg = #7f849c\n"
    "model_tag_fg = #1e1e2e\n"
    "[context]\n"
    "fg = #7f849c\n"
    "used_fg = #89b4fa\n"
    "pct_fg = #89b4fa\n"
    "[plan_short]\n"
    "fg = #7f849c\n"
    "pct_fg = #89b4fa\n"
    "[plan_long]\n"
    "fg = #7f849c\n"
    "pct_fg = #89b4fa\n" },
  { "nord",
    "[statusline]\n"
    "separator_fg = #616e88\n"
    "[cwd]\n"
    "fg = #88c0d0\n"
    "[git]\n"
    "fg = #88c0d0\n"
    "[model]\n"
    "fg = #81a1c1\n"
    "effort_fg = #616e88\n"
    "model_tag_fg = #2e3440\n"
    "[context]\n"
    "fg = #616e88\n"
    "used_fg = #81a1c1\n"
    "pct_fg = #81a1c1\n"
    "[plan_short]\n"
    "fg = #616e88\n"
    "pct_fg = #81a1c1\n"
    "[plan_long]\n"
    "fg = #616e88\n"
    "pct_fg = #81a1c1\n" },
  { "gruvbox",
    "[statusline]\n"
    "separator_fg = #928374\n"
    "[cwd]\n"
    "fg = #fabd2f\n"
    "[git]\n"
    "fg = #fabd2f\n"
    "[model]\n"
    "fg = #8ec07c\n"
    "effort_fg = #928374\n"
    "model_tag_fg = #282828\n"
    "[context]\n"
    "fg = #928374\n"
    "used_fg = #8ec07c\n"
    "pct_fg = #8ec07c\n"
    "[plan_short]\n"
    "fg = #928374\n"
    "pct_fg = #8ec07c\n"
    "[plan_long]\n"
    "fg = #928374\n"
    "pct_fg = #8ec07c\n" },
  { "tokyonight",
    "[statusline]\n"
    "separator_fg = #737aa2\n"
    "[cwd]\n"
    "fg = #7aa2f7\n"
    "[git]\n"
    "fg = #7aa2f7\n"
    "[model]\n"
    "fg = #bb9af7\n"
    "effort_fg = #737aa2\n"
    "model_tag_fg = #1a1b26\n"
    "[context]\n"
    "fg = #737aa2\n"
    "used_fg = #bb9af7\n"
    "pct_fg = #bb9af7\n"
    "[plan_short]\n"
    "fg = #737aa2\n"
    "pct_fg = #bb9af7\n"
    "[plan_long]\n"
    "fg = #737aa2\n"
    "pct_fg = #bb9af7\n" },
  { "dracula",
    "[statusline]\n"
    "separator_fg = #6272a4\n"
    "[cwd]\n"
    "fg = #bd93f9\n"
    "[git]\n"
    "fg = #bd93f9\n"
    "[model]\n"
    "fg = #ff79c6\n"
    "effort_fg = #6272a4\n"
    "model_tag_fg = #282a36\n"
    "[context]\n"
    "fg = #6272a4\n"
    "used_fg = #ff79c6\n"
    "pct_fg = #ff79c6\n"
    "[plan_short]\n"
    "fg = #6272a4\n"
    "pct_fg = #ff79c6\n"
    "[plan_long]\n"
    "fg = #6272a4\n"
    "pct_fg = #ff79c6\n" },
  { "onedark",
    "[statusline]\n"
    "separator_fg = #767676\n"
    "[cwd]\n"
    "fg = #61afef\n"
    "[git]\n"
    "fg = #61afef\n"
    "[model]\n"
    "fg = #c678dd\n"
    "effort_fg = #767676\n"
    "model_tag_fg = #282c34\n"
    "[context]\n"
    "fg = #767676\n"
    "used_fg = #c678dd\n"
    "pct_fg = #c678dd\n"
    "[plan_short]\n"
    "fg = #767676\n"
    "pct_fg = #c678dd\n"
    "[plan_long]\n"
    "fg = #767676\n"
    "pct_fg = #c678dd\n" },
  { "rosepine",
    "[statusline]\n"
    "separator_fg = #6e6a86\n"
    "[cwd]\n"
    "fg = #ebbcba\n"
    "[git]\n"
    "fg = #ebbcba\n"
    "[model]\n"
    "fg = #c4a7e7\n"
    "effort_fg = #6e6a86\n"
    "model_tag_fg = #191724\n"
    "[context]\n"
    "fg = #6e6a86\n"
    "used_fg = #c4a7e7\n"
    "pct_fg = #c4a7e7\n"
    "[plan_short]\n"
    "fg = #6e6a86\n"
    "pct_fg = #c4a7e7\n"
    "[plan_long]\n"
    "fg = #6e6a86\n"
    "pct_fg = #c4a7e7\n" },
  { "kanagawa",
    "[statusline]\n"
    "separator_fg = #727169\n"
    "[cwd]\n"
    "fg = #7e9cd8\n"
    "[git]\n"
    "fg = #7e9cd8\n"
    "[model]\n"
    "fg = #957fb8\n"
    "effort_fg = #727169\n"
    "model_tag_fg = #1f1f28\n"
    "[context]\n"
    "fg = #727169\n"
    "used_fg = #957fb8\n"
    "pct_fg = #957fb8\n"
    "[plan_short]\n"
    "fg = #727169\n"
    "pct_fg = #957fb8\n"
    "[plan_long]\n"
    "fg = #727169\n"
    "pct_fg = #957fb8\n" },
  { "solarized",
    "[statusline]\n"
    "separator_fg = #586e75\n"
    "[cwd]\n"
    "fg = #268bd2\n"
    "[git]\n"
    "fg = #268bd2\n"
    "[model]\n"
    "fg = #2aa198\n"
    "effort_fg = #586e75\n"
    "model_tag_fg = #002b36\n"
    "[context]\n"
    "fg = #586e75\n"
    "used_fg = #2aa198\n"
    "pct_fg = #2aa198\n"
    "[plan_short]\n"
    "fg = #586e75\n"
    "pct_fg = #2aa198\n"
    "[plan_long]\n"
    "fg = #586e75\n"
    "pct_fg = #2aa198\n" },
};

typedef struct
{
  const char *name;
  const char *sections;
  const char *format[SEC_COUNT];
  const char *glyphs[SEC_COUNT];   // the format with glyphs on; NULL keeps it
  const char *nerdfont[SEC_COUNT]; // the same for glyphs = nerdfont
} profile_def_t;

// A profile is opinionated: it replaces the section list and every section's
// format, wherever the config set them. Never colors, labels, the separator,
// thousands, cwd shortening, or resets styles — those stay the config's. Basic
// trims the line to the figures; detailed draws everything. Neither draws a
// glyph unless the glyphs key turns them on: ⎇ before the branch, ↻ before
// the reset times. With nerdfont, U+E0A0 for the ⎇ and U+F021 for the ↻. U+F021
// keeps a space after it, since a Nerd Font icon can spill into the next cell.
// nerdfont also draws the model as a pill and the effort beside it in the model's
// color, with no dot: {model_tag}, which draws the rounded caps U+E0B6 and U+E0B4
// itself, and {effort_plain}.
static const profile_def_t profiles[PROFILE_COUNT] =
{
  [PROFILE_BASIC] =
  {
    "basic", "cwd git model context plan_short plan_long",
    {
      [SEC_CWD]        = "{path}",
      [SEC_GIT]        = "{branch}",
      [SEC_MODEL]      = "{name} {ver} \xc2\xb7 {effort}",
      [SEC_CONTEXT]    = "{label} {pct}",
      [SEC_PLAN_SHORT] = "{window} {pct}",
      [SEC_PLAN_LONG]  = "{window} {pct}",
    },
    {
      [SEC_GIT]        = "\xe2\x8e\x87 {branch}",              // "⎇ {branch}"
    },
    {
      [SEC_GIT]        = "\xee\x82\xa0 {branch}",              // U+E0A0
      [SEC_MODEL]      = "{model_tag} {effort_plain}",
    },
  },
  [PROFILE_DETAILED] =
  {
    "detailed", "cwd git model context plan_short plan_long",
    {
      [SEC_CWD]        = "{path}",
      [SEC_GIT]        = "{branch}",
      [SEC_MODEL]      = "{name} {ver} \xc2\xb7 {effort}",
      [SEC_CONTEXT]    = "{label} {used}/{ceiling} {pct}",
      [SEC_PLAN_SHORT] = "{window} {pct} {resets}",
      [SEC_PLAN_LONG]  = "{window} {pct} {resets}",
    },
    {
      [SEC_GIT]        = "\xe2\x8e\x87 {branch}",              // "⎇ {branch}"
      [SEC_PLAN_SHORT] = "{window} {pct} \xe2\x86\xbb{resets}",   // "↻"
      [SEC_PLAN_LONG]  = "{window} {pct} \xe2\x86\xbb{resets}",
    },
    {
      [SEC_GIT]        = "\xee\x82\xa0 {branch}",              // U+E0A0
      [SEC_MODEL]      = "{model_tag} {effort_plain}",
      [SEC_PLAN_SHORT] = "{window} {pct} \xef\x80\xa1 {resets}",  // U+F021
      [SEC_PLAN_LONG]  = "{window} {pct} \xef\x80\xa1 {resets}",
    },
  },
};

static void load(config_t *, char *, size_t);
static profile_t profile_find(sv_t);
static const char *profile_format(const profile_def_t *, glyphs_t, size_t);
static void apply_profile(config_t *);
static void handle_line(config_t *, int *, char *, size_t);
static sv_t parse_value(char *, size_t);
static sv_t trim(sv_t);
static void set_global(config_t *, sv_t, sv_t);
static void set_section(config_t *, int, sv_t, sv_t);
static void set_resets(section_cfg_t *, sv_t);
static void set_theme(config_t *, sv_t);
static void set_profile(config_t *, sv_t);
static void set_glyphs(config_t *, sv_t);
static bool parse_u16(sv_t, uint16_t *);
static bool parse_glyphs(sv_t, glyphs_t *);
static bool token_color(section_cfg_t *, const section_desc_t *, sv_t, sv_t);
static void set_color(color_t *, sv_t, sv_t);
static void parse_order(config_t *, sv_t);
static int token_index(const section_desc_t *, sv_t);
static color_t named16(uint8_t);
static void set_tok_fg(config_t *, section_id_t, const char *, color_t);
#endif

#endif
