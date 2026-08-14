// dccstatusline — MIT
// main: read the Claude Code payload from stdin, emit the status line, never fail

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "version.h"

// Claude Code blanks the status line on a non-zero exit or empty stdout, so
// every path through main emits a line and returns success.

static size_t stdin_drain(void);
static bool write_all(const char *, size_t);

static char stdin_buf[64 * 1024];

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

int
main(int argc, char **argv)
{
  static const char fallback[] = "\x1b[0;2m" DCC_BUILD_STRING "\x1b[0m\n";

  if(argc > 1 && strcmp(argv[1], "--version") == 0)
  {
    puts(DCC_BUILD_STRING);
    return(0);
  }

  signal(SIGPIPE, SIG_IGN);   // a cancelled in-flight refresh must not kill us

  stdin_drain();              // payload parsing lands with the parser chunks

  write_all(fallback, sizeof fallback - 1);

  return(0);
}
