#ifndef DCC_COLOR_H
#define DCC_COLOR_H

#include <stdbool.h>
#include <stdint.h>

#include "util.h"

typedef enum
{
  COLOR_NONE = 0,   // no override: inherit the base style / stay unstyled
  COLOR_NAMED16,
  COLOR_IDX256,
  COLOR_RGB,
} color_kind_t;

typedef struct
{
  color_kind_t kind;
  uint8_t r, g, b;   // NAMED16 and IDX256 carry their index in r
} color_t;

// Accepts the 16 classic names ("cyan", "bright_magenta"), a 0-255 palette
// index, "#rrggbb", or "default" (parses to COLOR_NONE). Rejects anything
// else without touching *out.
bool color_parse(sv_t, color_t *);

// Appends one complete SGR sequence selecting fg/bg, always beginning from a
// reset ("\x1b[0...m") so every styled run starts from a clean slate. Both
// COLOR_NONE yields the bare reset.
void color_sgr(sbuf_t *, color_t, color_t);

#ifdef COLOR_INTERNAL
typedef struct
{
  const char *name;
  uint8_t idx;
} color_name_t;

static const color_name_t color_names[] =
{
  { "black",           0 }, { "red",             1 },
  { "green",           2 }, { "yellow",          3 },
  { "blue",            4 }, { "magenta",         5 },
  { "cyan",            6 }, { "white",           7 },
  { "bright_black",    8 }, { "bright_red",      9 },
  { "bright_green",   10 }, { "bright_yellow",  11 },
  { "bright_blue",    12 }, { "bright_magenta", 13 },
  { "bright_cyan",    14 }, { "bright_white",   15 },
};

static bool parse_named(sv_t, color_t *);
static bool parse_index(sv_t, color_t *);
static bool parse_rgb(sv_t, color_t *);
static int hex_nibble(char);
static size_t put_u8(char *, uint8_t);
static size_t sgr_component(char *, color_t, bool);
#endif

#endif
