// dccstatusline — MIT
// config: overlay the INI file onto compiled-in defaults, key by key

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_INTERNAL
#include "config.h"

static color_t
named16(uint8_t idx)
{
  color_t c = { COLOR_NAMED16, idx, 0, 0 };

  return(c);
}

static int
token_index(const section_desc_t *desc, sv_t name)
{
  int i;

  for(i = 0; desc->tokens[i]; i++)
    if(sv_eq_cstr(name, desc->tokens[i])) return(i);

  return(-1);
}

static void
set_tok_fg(config_t *cfg, section_id_t id, const char *token, color_t c)
{
  int i = token_index(&dcc_sections[id], sv_from_cstr(token));

  if(i >= 0) cfg->sec[id].tok_fg[i] = c;
}

// The shipped theme. examples/config documents exactly these values.
void
config_defaults(config_t *cfg)
{
  const color_t dim = named16(8);   // bright_black
  section_cfg_t *sc;
  size_t i;

  memset(cfg, 0, sizeof *cfg);

  for(i = 0; i < SEC_COUNT; i++) cfg->order[i] = (uint8_t)i;

  cfg->norder = SEC_COUNT;
  cfg->separator = sv_from_cstr(" \xe2\x94\x82 ");   // " │ "
  cfg->sep_fg = dim;
  cfg->thousands = sv_from_cstr(",");
  cfg->cwd_style = CWD_ABBREV;

  sc = &cfg->sec[SEC_CWD];
  sc->format = sv_from_cstr("{path}");
  sc->fg = named16(12);             // bright_blue

  sc = &cfg->sec[SEC_GIT];
  sc->format = sv_from_cstr("{branch}");
  sc->fg = named16(10);             // bright_green

  sc = &cfg->sec[SEC_MODEL];
  sc->format = sv_from_cstr("{name} {ver} \xc2\xb7{effort}");
  sc->fg = named16(13);             // bright_magenta
  set_tok_fg(cfg, SEC_MODEL, "effort", dim);

  sc = &cfg->sec[SEC_CONTEXT];
  sc->format = sv_from_cstr("{label} {used}/{ceiling} {pct}");
  sc->label = sv_from_cstr("ctx");
  sc->fg = dim;                     // the label, slash, and spaces stay subtle
  set_tok_fg(cfg, SEC_CONTEXT, "used", named16(6));    // cyan
  set_tok_fg(cfg, SEC_CONTEXT, "pct", named16(14));    // bright_cyan

  sc = &cfg->sec[SEC_PLAN_SHORT];
  sc->format = sv_from_cstr("{window} {pct} \xe2\x86\xbb{resets}");   // "↻"
  sc->fg = dim;
  sc->resets = RESETS_COUNTDOWN;
  set_tok_fg(cfg, SEC_PLAN_SHORT, "pct", named16(11)); // bright_yellow

  sc = &cfg->sec[SEC_PLAN_LONG];
  sc->format = sv_from_cstr("{window} {pct} \xe2\x86\xbb{resets}");
  sc->fg = dim;
  sc->resets = RESETS_COUNTDOWN;
  set_tok_fg(cfg, SEC_PLAN_LONG, "pct", named16(3));   // yellow
}

static sv_t
trim(sv_t s)
{
  while(s.n && (s.p[0] == ' ' || s.p[0] == '\t'))
  {
    s.p++;
    s.n--;
  }

  while(s.n && (s.p[s.n - 1] == ' ' || s.p[s.n - 1] == '\t')) s.n--;

  return(s);
}

// Values: a leading quote protects spaces and '#', and admits the \" and
// backslash-backslash escapes (collapsed in place); everything after the
// closing quote is ignored. Unquoted values are cut at the first '#' that
// follows whitespace — a leading '#' is data, so truecolor needs no quotes —
// then trimmed.
static sv_t
parse_value(char *p, size_t n)
{
  sv_t out;

  while(n && (p[0] == ' ' || p[0] == '\t'))
  {
    p++;
    n--;
  }

  if(n && p[0] == '"')
  {
    char *w = p + 1;
    size_t r = 1;

    while(r < n && p[r] != '"')
    {
      if(p[r] == '\\' && r + 1 < n && (p[r + 1] == '"' || p[r + 1] == '\\'))
      {
        *w++ = p[r + 1];
        r += 2;
        continue;
      }

      *w++ = p[r];
      r++;
    }

    out.p = p + 1;
    out.n = (size_t)(w - (p + 1));

    return(out);
  }

  {
    size_t k;

    for(k = 1; k < n; k++)
    {
      if(p[k] == '#' && (p[k - 1] == ' ' || p[k - 1] == '\t'))
      {
        n = k;
        break;
      }
    }
  }

  out.p = p;
  out.n = n;

  return(trim(out));
}

static void
set_color(color_t *dst, sv_t key, sv_t value)
{
  if(!color_parse(value, dst))
    fprintf(stderr, "dccstatusline: config: bad color for %.*s\n",
            (int)key.n, key.p);
}

static void
parse_order(config_t *cfg, sv_t value)
{
  size_t i = 0;

  cfg->norder = 0;

  while(i < value.n)
  {
    sv_t word;
    size_t start, s;

    while(i < value.n && (value.p[i] == ' ' || value.p[i] == '\t')) i++;

    start = i;

    while(i < value.n && value.p[i] != ' ' && value.p[i] != '\t') i++;

    word.p = value.p + start;
    word.n = i - start;

    if(!word.n) continue;

    for(s = 0; s < SEC_COUNT; s++)
    {
      if(sv_eq_cstr(word, dcc_sections[s].name))
      {
        if(cfg->norder < SEC_COUNT) cfg->order[cfg->norder++] = (uint8_t)s;

        break;
      }
    }

    if(s == SEC_COUNT)
      fprintf(stderr, "dccstatusline: config: unknown section in list: %.*s\n",
              (int)word.n, word.p);
  }
}

static void
set_global(config_t *cfg, sv_t key, sv_t value)
{
  if(sv_eq_cstr(key, "sections")) parse_order(cfg, value);

  else if(sv_eq_cstr(key, "separator")) cfg->separator = value;

  else if(sv_eq_cstr(key, "separator_fg")) set_color(&cfg->sep_fg, key, value);

  else if(sv_eq_cstr(key, "separator_bg")) set_color(&cfg->sep_bg, key, value);

  else if(sv_eq_cstr(key, "thousands")) cfg->thousands = value;
}

static bool
token_color(section_cfg_t *sc, const section_desc_t *desc, sv_t key, sv_t value)
{
  bool bg;
  sv_t stem = key;
  int i;

  if(key.n < 4) return(false);

  stem.n -= 3;

  if(memcmp(key.p + stem.n, "_fg", 3) == 0) bg = false;

  else if(memcmp(key.p + stem.n, "_bg", 3) == 0) bg = true;

  else return(false);

  i = token_index(desc, stem);

  if(i < 0) return(false);

  set_color(bg ? &sc->tok_bg[i] : &sc->tok_fg[i], key, value);

  return(true);
}

static void
set_resets(section_cfg_t *sc, sv_t value)
{
  if(sv_eq_cstr(value, "countdown")) sc->resets = RESETS_COUNTDOWN;

  else if(sv_eq_cstr(value, "clock")) sc->resets = RESETS_CLOCK;

  else if(sv_eq_cstr(value, "clock12")) sc->resets = RESETS_CLOCK12;

  else fprintf(stderr, "dccstatusline: config: bad resets style: %.*s\n",
               (int)value.n, value.p);
}

static void
set_section(config_t *cfg, int id, sv_t key, sv_t value)
{
  section_cfg_t *sc = &cfg->sec[id];

  if(sv_eq_cstr(key, "format")) sc->format = value;

  else if(sv_eq_cstr(key, "label")) sc->label = value;

  else if(sv_eq_cstr(key, "fg")) set_color(&sc->fg, key, value);

  else if(sv_eq_cstr(key, "bg")) set_color(&sc->bg, key, value);

  else if((id == SEC_PLAN_SHORT || id == SEC_PLAN_LONG)
          && sv_eq_cstr(key, "resets"))
    set_resets(sc, value);

  else if(id == SEC_CWD && sv_eq_cstr(key, "style"))
  {
    if(sv_eq_cstr(value, "abbrev")) cfg->cwd_style = CWD_ABBREV;

    else if(sv_eq_cstr(value, "full")) cfg->cwd_style = CWD_FULL;

    else if(sv_eq_cstr(value, "basename")) cfg->cwd_style = CWD_BASENAME;

    else fprintf(stderr, "dccstatusline: config: bad cwd style: %.*s\n",
                 (int)value.n, value.p);
  }

  else token_color(sc, &dcc_sections[id], key, value);
}

static void
handle_line(config_t *cfg, int *cur, char *p, size_t n)
{
  char *eq;
  sv_t key, value;

  while(n && (p[0] == ' ' || p[0] == '\t'))
  {
    p++;
    n--;
  }

  while(n && (p[n - 1] == ' ' || p[n - 1] == '\t')) n--;

  if(!n || p[0] == '#') return;

  if(p[0] == '[')
  {
    char *close = memchr(p, ']', n);
    sv_t name;
    size_t s;

    if(!close) return;

    name.p = p + 1;
    name.n = (size_t)(close - p) - 1;
    name = trim(name);
    *cur = CUR_NONE;

    if(sv_eq_cstr(name, "statusline"))
    {
      *cur = CUR_GLOBAL;
      return;
    }

    for(s = 0; s < SEC_COUNT; s++)
      if(sv_eq_cstr(name, dcc_sections[s].name)) *cur = (int)s;

    return;
  }

  eq = memchr(p, '=', n);

  if(!eq || *cur == CUR_NONE) return;

  key.p = p;
  key.n = (size_t)(eq - p);
  key = trim(key);
  value = parse_value(eq + 1, n - (size_t)(eq - p) - 1);

  if(!key.n) return;

  if(*cur == CUR_GLOBAL) set_global(cfg, key, value);

  else set_section(cfg, *cur, key, value);
}

void
config_load(config_t *cfg, char *buf, size_t n)
{
  size_t pos = 0;
  int cur = CUR_NONE;

  while(pos < n)
  {
    char *eol = memchr(buf + pos, '\n', n - pos);
    size_t raw = eol ? (size_t)(eol - (buf + pos)) : n - pos;
    size_t len = raw;

    if(len && buf[pos + len - 1] == '\r') len--;

    handle_line(cfg, &cur, buf + pos, len);
    pos += raw + 1;
  }
}

bool
config_path(char *dst, size_t cap)
{
  const char *env = getenv("DCCSTATUSLINE_CONFIG");
  int n;

  if(env && *env) return(strlcpy(dst, env, cap) < cap);

  env = getenv("XDG_CONFIG_HOME");

  if(env && *env) n = snprintf(dst, cap, "%s/dccstatusline/config", env);

  else
  {
    const char *home = getenv("HOME");

    if(!home || !*home) return(false);

    n = snprintf(dst, cap, "%s/.config/dccstatusline/config", home);
  }

  return(n > 0 && (size_t)n < cap);
}
