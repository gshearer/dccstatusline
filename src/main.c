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
#include "sections.h"
#include "util.h"
#include "version.h"

// Claude Code blanks the status line on a non-zero exit or empty stdout, so
// every path through main emits a line and returns success.

static size_t stdin_drain(void);
static bool write_all(const char *, size_t);
static bool want_git(const config_t *);

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

int
main(int argc, char **argv)
{
  static const char fallback[] = "\x1b[0;2m" DCC_BUILD_STRING "\x1b[0m\n";
  config_t cfg;
  payload_t pay;
  gitinfo_t git = { false, "", 0, 0 };
  sbuf_t line;
  size_t n;

  if(argc > 1 && strcmp(argv[1], "--version") == 0)
  {
    puts(DCC_BUILD_STRING);
    return(0);
  }

  signal(SIGPIPE, SIG_IGN);   // a cancelled in-flight refresh must not kill us

  n = stdin_drain();
  config_defaults(&cfg);

  {
    char path[PATH_MAX];

    if(config_path(path, sizeof path))
    {
      size_t got = file_slurp(path, config_buf, sizeof config_buf);

      if(got) config_load(&cfg, config_buf, got);
    }
  }

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
