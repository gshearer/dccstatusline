// dccstatusline — MIT
// sections: the six producers, and the assembly of the line itself

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SECTIONS_INTERNAL
#include "sections.h"

static int
tok_index(const section_desc_t *desc, const char *name)
{
  int i;

  for(i = 0; desc->tokens[i]; i++)
    if(strcmp(desc->tokens[i], name) == 0) return(i);

  return(-1);
}

static void
add_tok(tokset_t *ts, section_id_t id, const config_t *cfg, const char *name,
        sv_t value)
{
  int i = tok_index(&dcc_sections[id], name);

  if(i < 0 || ts->n >= DCC_MAX_TOKENS) return;

  ts->toks[ts->n].name = name;
  ts->toks[ts->n].value = value;
  ts->toks[ts->n].fg = cfg->sec[id].tok_fg[i];
  ts->toks[ts->n].bg = cfg->sec[id].tok_bg[i];
  ts->n++;
}

static void
run_fmt(sbuf_t *sb, section_id_t id, const config_t *cfg, const tokset_t *ts)
{
  fmt_render(sb, cfg->sec[id].format, ts->toks, ts->n,
             cfg->sec[id].fg, cfg->sec[id].bg);
}

static sv_t
fmt_pct(char *dst, size_t cap, uint32_t pct)
{
  int n = snprintf(dst, cap, "%u%%", pct);
  sv_t out = { dst, n > 0 && (size_t)n < cap ? (size_t)n : 0 };

  return(out);
}

// strftime("%a") would answer in the locale's language and width; this line
// is English throughout, and the weekday must stay three columns wide.
static const char *const weekday_name[7] =
  { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// The reset as a local wall-clock instant: "19:20", "7:20 PM", and with day
// set, "Mon 19:20" / "Mon 7:20 PM".
static sv_t
fmt_clock(char *dst, size_t cap, int64_t at, bool ampm, bool day)
{
  time_t t = (time_t)at;
  struct tm tm;
  sv_t out = { dst, 0 };
  char pre[8] = "";
  int n;

  if(!localtime_r(&t, &tm)) return(out);

  if(day && tm.tm_wday >= 0 && tm.tm_wday < 7)
    snprintf(pre, sizeof pre, "%s ", weekday_name[tm.tm_wday]);

  if(ampm)
  {
    int h = tm.tm_hour % 12;

    n = snprintf(dst, cap, "%s%d:%02d %s", pre, h ? h : 12, tm.tm_min,
                 tm.tm_hour < 12 ? "AM" : "PM");
  }

  else n = snprintf(dst, cap, "%s%02d:%02d", pre, tm.tm_hour, tm.tm_min);

  if(n > 0 && (size_t)n < cap) out.n = (size_t)n;

  return(out);
}

// Time remaining, floored to the minute: "04:12", and with day set,
// "03:19:47". A reset already past reads "00:00" rather than counting
// backwards, and a caller with no clock (now <= 0) renders nothing at all —
// the swallow rule then takes the literal run with it.
static sv_t
fmt_countdown(char *dst, size_t cap, int64_t at, int64_t now, bool day)
{
  sv_t out = { dst, 0 };
  int64_t left;
  int n;

  if(now <= 0) return(out);

  left = at > now ? at - now : 0;

  if(day)
    n = snprintf(dst, cap, "%02lld:%02lld:%02lld",
                 (long long)(left / 86400), (long long)(left % 86400 / 3600),
                 (long long)(left % 3600 / 60));

  else
    n = snprintf(dst, cap, "%02lld:%02lld", (long long)(left / 3600),
                 (long long)(left % 3600 / 60));

  if(n > 0 && (size_t)n < cap) out.n = (size_t)n;

  return(out);
}

// "claude-fable-5" → "5", "claude-opus-4-1-20250805" → "4.1",
// "claude-3-5-sonnet-20241022" → "3.5": join the numeric dash-tokens with
// dots, dropping a trailing 8-digit date. Unrecognized shapes yield empty,
// and the swallow rule tidies up after them.
static sv_t
derive_ver(char *dst, size_t cap, sv_t id)
{
  sv_t out = { dst, 0 };
  size_t i = 0, w = 0;

  if(id.n > 7 && memcmp(id.p, "claude-", 7) == 0) i = 7;

  while(i < id.n)
  {
    size_t start = i, len;
    bool digits = true;

    while(i < id.n && id.p[i] != '-')
    {
      if(id.p[i] < '0' || id.p[i] > '9') digits = false;

      i++;
    }

    len = i - start;

    if(i < id.n) i++;   // the dash

    if(!digits || !len) continue;

    if(len == 8 && i >= id.n) continue;   // trailing date stamp

    if(w + len + 2 > cap) return(out);    // never a torn version

    if(w) dst[w++] = '.';

    memcpy(dst + w, id.p + start, len);
    w += len;
  }

  out.n = w;

  return(out);
}

// Current payloads carry the version inside display_name ("Fable 5"); older
// ones did not ("Opus"). {ver} exists for the older shape — when the name
// already ends with the derived version as its own word, it goes out empty
// and the swallow rule spares us "Fable 5 5".
static bool
name_carries_ver(sv_t name, sv_t ver)
{
  if(!ver.n || name.n < ver.n) return(false);

  if(memcmp(name.p + name.n - ver.n, ver.p, ver.n) != 0) return(false);

  return(name.n == ver.n || name.p[name.n - ver.n - 1] == ' ');
}

static void
sec_cwd(sbuf_t *sb, const payload_t *pay, const config_t *cfg)
{
  static char abbrev[PATH_MAX];
  tokset_t ts = { .n = 0 };
  sv_t path = pay->cwd;

  if(!pay->has_cwd) return;

  switch(cfg->cwd_style)
  {
    case CWD_ABBREV:
      path = path_abbrev(abbrev, sizeof abbrev, path,
                         sv_from_cstr(getenv("HOME")));
      break;

    case CWD_BASENAME:
      path = path_basename(path);
      break;

    case CWD_FULL:
      break;
  }

  add_tok(&ts, SEC_CWD, cfg, "label", cfg->sec[SEC_CWD].label);
  add_tok(&ts, SEC_CWD, cfg, "path", path);
  run_fmt(sb, SEC_CWD, cfg, &ts);
}

static void
sec_git(sbuf_t *sb, const config_t *cfg, const gitinfo_t *git)
{
  tokset_t ts = { .n = 0 };
  sv_t branch = { git->name, git->n };

  if(!git->present) return;

  add_tok(&ts, SEC_GIT, cfg, "label", cfg->sec[SEC_GIT].label);
  add_tok(&ts, SEC_GIT, cfg, "branch", branch);
  run_fmt(sb, SEC_GIT, cfg, &ts);
}

static void
sec_model(sbuf_t *sb, const payload_t *pay, const config_t *cfg)
{
  char verbuf[16];
  tokset_t ts = { .n = 0 };
  sv_t name = pay->model_name;
  sv_t ver;

  if(!pay->has_model) return;

  if(!name.n) name = pay->model_id;

  ver = derive_ver(verbuf, sizeof verbuf, pay->model_id);

  if(name_carries_ver(name, ver)) ver.n = 0;

  add_tok(&ts, SEC_MODEL, cfg, "label", cfg->sec[SEC_MODEL].label);
  add_tok(&ts, SEC_MODEL, cfg, "name", name);
  add_tok(&ts, SEC_MODEL, cfg, "ver", ver);
  add_tok(&ts, SEC_MODEL, cfg, "effort", pay->effort);
  add_tok(&ts, SEC_MODEL, cfg, "id", pay->model_id);
  run_fmt(sb, SEC_MODEL, cfg, &ts);
}

static void
sec_context(sbuf_t *sb, const payload_t *pay, const config_t *cfg)
{
  char used[64], ceil[64], pctb[8];
  tokset_t ts = { .n = 0 };
  sv_t usedsv, ceilsv = { NULL, 0 }, pctsv = { NULL, 0 };

  if(!pay->has_context) return;

  usedsv.p = used;
  usedsv.n = u64_grouped(used, sizeof used, pay->ctx_used, cfg->thousands);

  if(pay->ctx_ceiling)
  {
    ceilsv.p = ceil;
    ceilsv.n = u64_grouped(ceil, sizeof ceil, pay->ctx_ceiling, cfg->thousands);
  }

  if(pay->has_pct) pctsv = fmt_pct(pctb, sizeof pctb, pay->ctx_pct);

  else if(pay->ctx_ceiling)
  {
    uint64_t pct = pay->ctx_used >= pay->ctx_ceiling
                     ? 100
                     : pay->ctx_used * 100 / pay->ctx_ceiling;

    pctsv = fmt_pct(pctb, sizeof pctb, (uint32_t)pct);
  }

  add_tok(&ts, SEC_CONTEXT, cfg, "label", cfg->sec[SEC_CONTEXT].label);
  add_tok(&ts, SEC_CONTEXT, cfg, "used", usedsv);
  add_tok(&ts, SEC_CONTEXT, cfg, "ceiling", ceilsv);
  add_tok(&ts, SEC_CONTEXT, cfg, "pct", pctsv);
  run_fmt(sb, SEC_CONTEXT, cfg, &ts);
}

static void
sec_plan(sbuf_t *sb, const payload_t *pay, const config_t *cfg,
         plan_slot_t slot, section_id_t id, int64_t now)
{
  char pctb[8], clock[32];
  tokset_t ts = { .n = 0 };
  const plan_window_t *w = &pay->plan[slot];
  const bool day = slot == PLAN_LONG;   // only the long window spans days
  sv_t resets = { NULL, 0 };

  if(!w->present) return;

  if(w->has_resets)
  {
    switch(cfg->sec[id].resets)
    {
      case RESETS_COUNTDOWN:
        resets = fmt_countdown(clock, sizeof clock, w->resets_at, now, day);
        break;

      case RESETS_CLOCK:
        resets = fmt_clock(clock, sizeof clock, w->resets_at, false, day);
        break;

      case RESETS_CLOCK12:
        resets = fmt_clock(clock, sizeof clock, w->resets_at, true, day);
        break;
    }
  }

  add_tok(&ts, id, cfg, "label", cfg->sec[id].label);
  add_tok(&ts, id, cfg, "window", w->window);
  add_tok(&ts, id, cfg, "pct", fmt_pct(pctb, sizeof pctb, w->pct));
  add_tok(&ts, id, cfg, "resets", resets);
  run_fmt(sb, id, cfg, &ts);
}

static void
render_section(sbuf_t *sb, section_id_t id, const payload_t *pay,
               const config_t *cfg, const gitinfo_t *git, int64_t now)
{
  switch(id)
  {
    case SEC_CWD:        sec_cwd(sb, pay, cfg); break;
    case SEC_GIT:        sec_git(sb, cfg, git); break;
    case SEC_MODEL:      sec_model(sb, pay, cfg); break;
    case SEC_CONTEXT:    sec_context(sb, pay, cfg); break;
    case SEC_PLAN_SHORT: sec_plan(sb, pay, cfg, PLAN_SHORT, id, now); break;
    case SEC_PLAN_LONG:  sec_plan(sb, pay, cfg, PLAN_LONG, id, now); break;
    case SEC_COUNT:      break;
  }
}

void
statusline_render(sbuf_t *out, const payload_t *pay, const config_t *cfg,
                  const gitinfo_t *git, int64_t now)
{
  static char scratch_mem[1024];
  size_t i;
  bool first = true;

  for(i = 0; i < cfg->norder; i++)
  {
    section_id_t id = (section_id_t)cfg->order[i];
    sbuf_t scratch;

    if(id >= SEC_COUNT) continue;

    sbuf_init(&scratch, scratch_mem, sizeof scratch_mem, 0);
    render_section(&scratch, id, pay, cfg, git, now);

    if(!scratch.len) continue;

    if(!first && cfg->separator.n)
    {
      color_sgr(out, cfg->sep_fg, cfg->sep_bg);
      sbuf_append_sv(out, cfg->separator);
    }

    sbuf_append(out, scratch.p, scratch.len);
    first = false;
  }
}
