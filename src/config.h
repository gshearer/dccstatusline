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

// The palette key: how many of a preset's three colors the line wears
// beside its muted one: the first and second, all three, or the first alone.
// Duotone is the default, the line every preset drew before palettes. It takes
// a theme: with none, the line keeps its stock colors under any palette.
typedef enum
{
  PALETTE_DUOTONE = 0,
  PALETTE_TRITONE,
  PALETTE_MONOCHROME,
} palette_t;

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
  uint8_t theme;           // 1 + the preset laid last; 0, no theme, until one is
  palette_t palette;       // how a preset lays its colors; see config_palette
  bool palette_pinned;     // --palette set it: the config's palette key yields quietly
  profile_t profile;       // applied after the whole load; see config_load
  glyphs_t glyphs;         // a profile's ⎇, ↻ and model pill; nothing without one
  section_cfg_t sec[SEC_COUNT];
} config_t;

// Defaults are the stock line, with no theme — identical to examples/config.
// config_load overlays an INI buffer onto them per key: an unparseable value
// keeps that key's default (one stderr note), unknown sections and keys are
// ignored. A profile key, and the glyphs key with it, is applied after the
// whole buffer, so it wins wherever the sections and format keys sit. Views
// alias the config buffer, which must outlive the render.
void config_defaults(config_t *);
void config_load(config_t *, char *, size_t);

// Lays a compiled-in preset's colors, and nothing else, in the palette the
// palette key holds, as if its keys sat where the call happens, so later keys
// still override it. False for an unknown name. `none` names the stock line,
// as `default` did in v0.0.6: no preset, it counts as the theme and lays
// nothing.
bool config_theme(config_t *, sv_t);

// Sets the palette key from monochrome, duotone or tritone and lays the preset
// in force again in it. With no theme it lays nothing: the stock colors take
// no palette. False for anything else, leaving the config untouched.
bool config_palette(config_t *, sv_t);

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
  color_t first, second, third, muted, base;
} theme_t;

#define RGB(v) { COLOR_RGB, ((v) >> 16) & 255, ((v) >> 8) & 255, (v) & 255 }

// Each preset is five colors: the first for where you are (cwd and branch), the
// second for the model, the third for every usage figure, its muted color for
// the separator and all literals, and its background, the one dark text on the
// line: {model_tag} sets no fill, so it fills with the model's color, and
// writes in the background. The palette key decides how many of the three the
// line wears: monochrome paints the model and the figures in the first, duotone
// both in the second, and tritone each in its own. With no theme the line is
// config_defaults' own, in the terminal's 16 colors, which no palette touches.
// Never the separator, a format, the profile, sections, cwd shortening, or
// resets — a theme recolors the line, it does not rewrite or rearrange it.
static const theme_t themes[] =
{
  { "catppuccin", RGB(0xcba6f7), RGB(0x89b4fa), RGB(0xfab387), RGB(0x7f849c), RGB(0x1e1e2e) },
  { "nord",       RGB(0x88c0d0), RGB(0x81a1c1), RGB(0x5e81ac), RGB(0x616e88), RGB(0x2e3440) },
  { "gruvbox",    RGB(0xfabd2f), RGB(0x8ec07c), RGB(0xfe8019), RGB(0x928374), RGB(0x282828) },
  { "tokyonight", RGB(0x7aa2f7), RGB(0xbb9af7), RGB(0xff9e64), RGB(0x737aa2), RGB(0x1a1b26) },
  { "dracula",    RGB(0xbd93f9), RGB(0xff79c6), RGB(0x8be9fd), RGB(0x6272a4), RGB(0x282a36) },
  { "onedark",    RGB(0x61afef), RGB(0xc678dd), RGB(0x98c379), RGB(0x767676), RGB(0x282c34) },
  { "rosepine",   RGB(0xebbcba), RGB(0xc4a7e7), RGB(0x9ccfd8), RGB(0x6e6a86), RGB(0x191724) },
  { "kanagawa",   RGB(0x7e9cd8), RGB(0x957fb8), RGB(0xe6c384), RGB(0x727169), RGB(0x1f1f28) },
  { "solarized",  RGB(0x268bd2), RGB(0x2aa198), RGB(0xb58900), RGB(0x586e75), RGB(0x002b36) },
};

#undef RGB

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

static void lay(config_t *, const theme_t *);
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
static void set_palette(config_t *, sv_t);
static bool parse_u16(sv_t, uint16_t *);
static bool parse_glyphs(sv_t, glyphs_t *);
static bool parse_palette(sv_t, palette_t *);
static bool token_color(section_cfg_t *, const section_desc_t *, sv_t, sv_t);
static void set_color(color_t *, sv_t, sv_t);
static void parse_order(config_t *, sv_t);
static int token_index(const section_desc_t *, sv_t);
static color_t named16(uint8_t);
static void set_tok_fg(config_t *, section_id_t, const char *, color_t);
#endif

#endif
