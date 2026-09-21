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
  uint16_t cwd_depth;      // trailing components kept whole; 0 = keep them all
  uint16_t cwd_max_len;    // hard column budget for the rendered path; 0 = none
  section_cfg_t sec[SEC_COUNT];
} config_t;

// Defaults are the shipped theme — identical to examples/config. config_load
// overlays an INI buffer onto them per key: an unparseable value keeps that
// key's default (one stderr note), unknown sections and keys are ignored.
// Views alias the config buffer, which must outlive the render.
void config_defaults(config_t *);
void config_load(config_t *, char *, size_t);

// $DCCSTATUSLINE_CONFIG, else $XDG_CONFIG_HOME/dccstatusline/config, else
// ~/.config/dccstatusline/config. False when no path can be resolved or fit.
bool config_path(char *, size_t);

#ifdef CONFIG_INTERNAL
#define CUR_NONE   (-2)
#define CUR_GLOBAL (-1)

static void handle_line(config_t *, int *, char *, size_t);
static sv_t parse_value(char *, size_t);
static sv_t trim(sv_t);
static void set_global(config_t *, sv_t, sv_t);
static void set_section(config_t *, int, sv_t, sv_t);
static void set_resets(section_cfg_t *, sv_t);
static bool parse_u16(sv_t, uint16_t *);
static bool token_color(section_cfg_t *, const section_desc_t *, sv_t, sv_t);
static void set_color(color_t *, sv_t, sv_t);
static void parse_order(config_t *, sv_t);
static int token_index(const section_desc_t *, sv_t);
static color_t named16(uint8_t);
static void set_tok_fg(config_t *, section_id_t, const char *, color_t);
#endif

#endif
