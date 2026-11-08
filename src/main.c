// dccstatusline — MIT
// main: read the Claude Code payload from stdin, emit the status line, never fail

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "gitinfo.h"
#include "payload.h"
#include "reference_config.h"
#include "sections.h"
#include "util.h"
#include "version.h"

// Claude Code blanks the status line on a non-zero exit or empty stdout, so
// every path through main emits a line and returns success.

static size_t stdin_drain(void);
static bool write_all(const char *, size_t);
static bool want_git(const config_t *);
static bool print_themes(const char *);
static void parse_flags(int, char **, const char **, const char **,
                        const char **, const char **);

static char stdin_buf[64 * 1024];
static char config_buf[32 * 1024];
static char line_mem[8 * 1024];

// Reads until EOF, the buffer cap, or an unrecoverable error. Whatever arrived
// is whatever we parse; short data is the parser's problem, not a fatal one.
static size_t
stdin_drain(void)
{
  size_t used = 0;

  while(used < sizeof stdin_buf)
  {
    ssize_t got = read(STDIN_FILENO, stdin_buf + used, sizeof stdin_buf - used);

    if(got == 0) break;

    if(got < 0)
    {
      if(errno == EINTR) continue;

      break;
    }

    used += (size_t)got;
  }

  return(used);
}

static bool
write_all(const char *buf, size_t len)
{
  size_t done = 0;

  while(done < len)
  {
    ssize_t put = write(STDOUT_FILENO, buf + done, len - done);

    if(put < 0)
    {
      if(errno == EINTR) continue;

      return(false);
    }

    done += (size_t)put;
  }

  return(true);
}

// The .git walk is worth its syscalls only if something renders from it: the
// git section itself, or a cwd asked to be relative to the repository root.
static bool
want_git(const config_t *cfg)
{
  size_t i;

  for(i = 0; i < cfg->norder; i++)
    if(cfg->order[i] == SEC_GIT
       || (cfg->order[i] == SEC_CWD && cfg->cwd_style == CWD_REPO))
      return(true);

  return(false);
}

// Every preset over the stock line, one sample line each, labeled with its
// name, so they can be compared side by side. The user's config stays out of
// it: this shows the themes as shipped, with only the overlay on top — INI text
// laid over each preset as if it sat below the theme line. The name is the
// table's own, so it always lays and counts as the theme, and a theme key in the
// overlay yields as it would below a theme line. A palette key in the overlay
// lays the preset again in it. No theme is no preset and has no row: the plugin
// picker draws it with a plain run of the same sample, as a payload in
// plugin/hooks/register.tsx, so a change to the sample here goes there too. The
// clock is fixed too — the countdowns measure against the same instant the
// sample resets were built from.
static bool
print_themes(const char *overlay)
{
  const int64_t now = 1738411200;
  const payload_t pay =
  {
    .has_cwd = true,
    .cwd = sv_from_cstr("~/src/acme"),
    .has_model = true,
    .model_name = sv_from_cstr("Opus"),
    .model_id = sv_from_cstr("claude-opus-5-5"),
    .effort = sv_from_cstr("high"),
    .has_context = true,
    .ctx_used = 87333,
    .ctx_ceiling = 200000,
    .plan =
    {
      [PLAN_SHORT] = { true, true, 24, now + 9000, sv_from_cstr("5h") },
      [PLAN_LONG]  = { true, true, 41, now + 302400, sv_from_cstr("7d") },
    },
  };
  const gitinfo_t git = { true, "main", 4, 0 };
  const size_t on = strlen(overlay);
  const char *name;
  size_t i;

  if(on > sizeof config_buf) return(false);

  for(i = 0; (name = config_theme_name(i)); i++)
  {
    config_t cfg;
    sbuf_t line;
    char label[32];
    const int n = snprintf(label, sizeof label, "%-12s", name);

    config_defaults(&cfg);
    config_theme(&cfg, sv_from_cstr(name));
    memcpy(config_buf, overlay, on);   // the loader writes as it parses
    config_load(&cfg, config_buf, on);

    sbuf_init(&line, line_mem, sizeof line_mem, 5);   // tail: reset + newline
    statusline_render(&line, &pay, &cfg, &git, now);
    sbuf_append_tail(&line, "\x1b[0m\n", 5);

    if(n < 0 || (size_t)n >= sizeof label
       || !write_all(label, (size_t)n) || !write_all(line.p, line.len))
      return(false);
  }

  return(true);
}

// The live run's flags: --theme, --palette, --profile and --glyphs each take
// the next word, and a repeat wins. Anything else is noted and passed over — a
// typo in the settings.json command must not cost the line.
static void
parse_flags(int argc, char **argv, const char **theme, const char **palette,
            const char **profile, const char **glyphs)
{
  int i;

  for(i = 1; i < argc; i++)
  {
    if(i + 1 < argc && strcmp(argv[i], "--theme") == 0) *theme = argv[++i];

    else if(i + 1 < argc && strcmp(argv[i], "--palette") == 0)
      *palette = argv[++i];

    else if(i + 1 < argc && strcmp(argv[i], "--profile") == 0)
      *profile = argv[++i];

    else if(i + 1 < argc && strcmp(argv[i], "--glyphs") == 0)
      *glyphs = argv[++i];

    else fprintf(stderr, "dccstatusline: ignored argument: %s\n", argv[i]);
  }
}

int
main(int argc, char **argv)
{
  static const char fallback[] = "\x1b[0;2m" DCC_BUILD_STRING "\x1b[0m\n";
  config_t cfg;
  payload_t pay;
  gitinfo_t git = { false, "", 0, 0 };
  sbuf_t line;
  size_t n;
  const char *theme = NULL, *palette = NULL, *profile = NULL, *glyphs = NULL;

  if(argc > 1 && strcmp(argv[1], "--version") == 0)
  {
    puts(DCC_BUILD_STRING);
    return(0);
  }

  if(argc > 1 && strcmp(argv[1], "--print-config") == 0)
    return(write_all((const char *)reference_config,
                     sizeof reference_config) ? 0 : 1);

  if(argc > 1 && strcmp(argv[1], "--themes") == 0)
    return(print_themes(argc > 2 ? argv[2] : "") ? 0 : 1);

  parse_flags(argc, argv, &theme, &palette, &profile, &glyphs);
  signal(SIGPIPE, SIG_IGN);   // a cancelled in-flight refresh must not kill us

  n = stdin_drain();
  config_defaults(&cfg);

  // Before --theme, so the theme lays in it; the config's palette key then
  // yields, and its color keys still land on top.
  if(palette)
  {
    if(config_palette(&cfg, sv_from_cstr(palette))) cfg.palette_pinned = true;

    else fprintf(stderr, "dccstatusline: --palette: bad palette: %s\n", palette);
  }

  // As if the config's first line named it: its own theme key then yields, and
  // its color keys still land on top.
  if(theme)
  {
    if(config_theme(&cfg, sv_from_cstr(theme))) cfg.theme_pinned = true;

    else fprintf(stderr, "dccstatusline: --theme: unknown theme: %s\n", theme);
  }

  {
    char path[PATH_MAX];

    if(config_path(path, sizeof path))
    {
      size_t got = file_slurp(path, config_buf, sizeof config_buf);

      if(got) config_load(&cfg, config_buf, got);
    }
  }

  if(profile && !config_profile(&cfg, sv_from_cstr(profile)))
    fprintf(stderr, "dccstatusline: --profile: bad profile: %s\n", profile);

  // Reapplies the profile, so it lands the same either side of --profile.
  if(glyphs && !config_glyphs(&cfg, sv_from_cstr(glyphs)))
    fprintf(stderr, "dccstatusline: --glyphs: bad glyphs: %s\n", glyphs);

  sbuf_init(&line, line_mem, sizeof line_mem, 5);   // tail: reset + newline

  if(payload_parse(stdin_buf, n, &pay))
  {
    struct timespec ts;
    int64_t now = 0;   // no clock: the countdowns render empty, the line stands

    if(clock_gettime(CLOCK_REALTIME, &ts) == 0) now = (int64_t)ts.tv_sec;

    if(pay.has_cwd && want_git(&cfg)) gitinfo_read(pay.cwd, &git);

    statusline_render(&line, &pay, &cfg, &git, now);
  }

  if(!line.len)
  {
    write_all(fallback, sizeof fallback - 1);

    return(0);
  }

  sbuf_append_tail(&line, "\x1b[0m\n", 5);
  write_all(line.p, line.len);

  return(0);
}
