// dccstatusline — MIT
// test_cli: the real binary, spawned with pipes — the program never forks,
// but its test harness may

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "check.h"

extern char **environ;

static const char *dcc_bin;

static bool run_bin(const char *, size_t, char *, size_t, size_t *);
static void expect_line(const char *, const char *, const char *);
static void expect_fallback(const char *, size_t, const char *);
static void test_styled(void);
static void test_failure_modes(void);

static bool
run_bin(const char *in, size_t inlen, char *out, size_t outcap, size_t *outlen)
{
  posix_spawn_file_actions_t fa;
  pid_t pid;
  int inpipe[2], outpipe[2], status;
  char *argv[] = { (char *)dcc_bin, NULL };
  size_t used = 0;

  if(pipe(inpipe) != 0 || pipe(outpipe) != 0) return(false);

  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_adddup2(&fa, inpipe[0], 0);
  posix_spawn_file_actions_adddup2(&fa, outpipe[1], 1);
  posix_spawn_file_actions_addclose(&fa, inpipe[1]);
  posix_spawn_file_actions_addclose(&fa, outpipe[0]);

  if(posix_spawn(&pid, dcc_bin, &fa, NULL, argv, environ) != 0)
  {
    posix_spawn_file_actions_destroy(&fa);

    return(false);
  }

  posix_spawn_file_actions_destroy(&fa);
  close(inpipe[0]);
  close(outpipe[1]);

  {
    size_t done = 0;

    while(done < inlen)
    {
      ssize_t put = write(inpipe[1], in + done, inlen - done);

      if(put < 0)
      {
        if(errno == EINTR) continue;

        break;   // EPIPE when the child stopped reading: fine
      }

      done += (size_t)put;
    }
  }

  close(inpipe[1]);

  for(;;)
  {
    ssize_t got = read(outpipe[0], out + used, outcap - used);

    if(got < 0 && errno == EINTR) continue;

    if(got <= 0) break;

    used += (size_t)got;

    if(used == outcap) break;
  }

  close(outpipe[0]);
  *outlen = used;

  if(waitpid(pid, &status, 0) != pid) return(false);

  return(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void
expect_line(const char *payload, const char *want, const char *what)
{
  char out[8192];
  size_t n = 0;

  CHECK(run_bin(payload, strlen(payload), out, sizeof out, &n),
        "%s: exit 0", what);
  CHECK(n == strlen(want) && memcmp(out, want, n) == 0,
        "%s: got \"%.*s\" want \"%s\"", what, (int)n, out, want);
}

static void
expect_fallback(const char *payload, size_t len, const char *what)
{
  static const char prefix[] = "\x1b[0;2mdccstatusline v";
  char out[8192];
  size_t n = 0;

  CHECK(run_bin(payload, len, out, sizeof out, &n), "%s: exit 0", what);
  CHECK(n >= sizeof prefix - 1 && memcmp(out, prefix, sizeof prefix - 1) == 0,
        "%s: fallback line (got %zu bytes)", what, n);
  CHECK(n && out[n - 1] == '\n', "%s: newline-terminated", what);
}

static void
test_styled(void)
{
  static const char ini[] =
    "[statusline]\n"
    "sections = context plan_short\n"
    "separator = \" | \"\n"
    "[context]\n"
    "format = {used}/{ceiling} {pct}\n"
    "fg = default\n"
    "used_fg = default\n"
    "pct_fg = default\n"
    "[plan_short]\n"
    "format = {window} {pct}\n"
    "fg = default\n"
    "pct_fg = red\n";
  static const char payload[] =
    "{\"context_window\":{\"total_input_tokens\":50000,"
    "\"context_window_size\":200000},"
    "\"rate_limits\":{\"five_hour\":{\"used_percentage\":23.5}}}";
  FILE *f = fopen("cli_config.ini", "w");

  CHECK(f && fputs(ini, f) >= 0 && fclose(f) == 0, "fixture config written");
  CHECK(setenv("DCCSTATUSLINE_CONFIG", "cli_config.ini", 1) == 0, "env");

  expect_line(payload,
              "\x1b[0m50,000"
              "\x1b[0m/"
              "\x1b[0m200,000"
              "\x1b[0m "
              "\x1b[0m25%"
              "\x1b[0;90m | "
              "\x1b[0m5h"
              "\x1b[0m "
              "\x1b[0;31m24%"
              "\x1b[0m\n",
              "styled end-to-end");
}

static void
test_failure_modes(void)
{
  static char big[100 * 1024];

  expect_fallback("this is not json at all", 23, "garbage stdin");
  expect_fallback("", 0, "empty stdin");
  expect_fallback("{}", 2, "empty object");
  expect_fallback("[1,2,3]", 7, "non-object json");

  memset(big, 'x', sizeof big);
  expect_fallback(big, sizeof big, "oversized stdin");
}

int
main(int argc, char **argv)
{
  if(argc != 2)
  {
    fprintf(stderr, "usage: %s <path-to-dccstatusline>\n", argv[0]);
    return(1);
  }

  dcc_bin = argv[1];
  signal(SIGPIPE, SIG_IGN);   // the oversized-stdin case writes into a wall

  test_styled();
  test_failure_modes();

  return(check_failures ? 1 : 0);
}
