// dccstatusline — MIT
// bench: time full refresh cycles (spawn, payload in, line out, exit) of one
// binary, or of two interleaved run by run so drift lands on both alike

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

typedef struct
{
  const char *bin;
  uint64_t *ns;      // per-run wall time
  uint64_t faults;   // minor faults, summed over measured runs
} subject_t;

static bool run_once(const char *, const char *, size_t, uint64_t *, uint64_t *);
static bool slurp(const char *, char **, size_t *);
static int cmp_u64(const void *, const void *);
static void report(const subject_t *, size_t);
static int usage(const char *);

static uint64_t
now_ns(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return((uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec);
}

// One refresh as Claude Code drives it: payload on stdin, stdout drained to
// EOF, exit reaped. Timing spans posix_spawn through wait4.
static bool
run_once(const char *bin, const char *in, size_t inlen, uint64_t *ns,
         uint64_t *faults)
{
  posix_spawn_file_actions_t fa;
  struct rusage ru;
  pid_t pid;
  int inpipe[2], outpipe[2], status;
  char *argv[] = { (char *)bin, NULL };
  char sink[8192];
  uint64_t t0;
  size_t done = 0;

  if(pipe(inpipe) != 0) return(false);

  if(pipe(outpipe) != 0)
  {
    close(inpipe[0]);
    close(inpipe[1]);

    return(false);
  }

  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_adddup2(&fa, inpipe[0], 0);
  posix_spawn_file_actions_adddup2(&fa, outpipe[1], 1);
  posix_spawn_file_actions_addclose(&fa, inpipe[1]);
  posix_spawn_file_actions_addclose(&fa, outpipe[0]);

  t0 = now_ns();

  if(posix_spawn(&pid, bin, &fa, NULL, argv, environ) != 0)
  {
    posix_spawn_file_actions_destroy(&fa);

    return(false);
  }

  posix_spawn_file_actions_destroy(&fa);
  close(inpipe[0]);
  close(outpipe[1]);

  while(done < inlen)
  {
    ssize_t put = write(inpipe[1], in + done, inlen - done);

    if(put < 0)
    {
      if(errno == EINTR) continue;

      break;
    }

    done += (size_t)put;
  }

  close(inpipe[1]);

  for(;;)
  {
    ssize_t got = read(outpipe[0], sink, sizeof sink);

    if(got < 0 && errno == EINTR) continue;

    if(got <= 0) break;
  }

  close(outpipe[0]);

  if(wait4(pid, &status, 0, &ru) != pid) return(false);

  *ns = now_ns() - t0;
  *faults = (uint64_t)ru.ru_minflt;

  return(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static bool
slurp(const char *path, char **out, size_t *outlen)
{
  FILE *f = fopen(path, "rb");
  char *buf = NULL;
  size_t cap = 0, used = 0;

  if(!f) return(false);

  for(;;)
  {
    size_t got;

    if(used == cap)
    {
      char *grown = realloc(buf, cap ? cap * 2 : 4096);

      if(!grown)
      {
        free(buf);
        fclose(f);

        return(false);
      }

      buf = grown;
      cap = cap ? cap * 2 : 4096;
    }

    got = fread(buf + used, 1, cap - used, f);
    used += got;

    if(got == 0) break;
  }

  fclose(f);
  *out = buf;
  *outlen = used;

  return(true);
}

static int
cmp_u64(const void *a, const void *b)
{
  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;

  return((x > y) - (x < y));
}

static void
report(const subject_t *s, size_t n)
{
  uint64_t sum = 0;
  size_t i;

  qsort(s->ns, n, sizeof *s->ns, cmp_u64);

  for(i = 0; i < n; i++) sum += s->ns[i];

  printf("%-40s  min %7.1f  med %7.1f  mean %7.1f  p95 %7.1f  p99 %7.1f us"
         "  faults %6.1f\n",
         s->bin,
         (double)s->ns[0] / 1e3,
         (double)s->ns[n / 2] / 1e3,
         (double)sum / (double)n / 1e3,
         (double)s->ns[n * 95 / 100] / 1e3,
         (double)s->ns[n * 99 / 100] / 1e3,
         (double)s->faults / (double)n);
}

static int
usage(const char *argv0)
{
  fprintf(stderr,
          "usage: %s [-n runs] [-w warmup] -p payload binary [binary]\n"
          "  the binary's environment is bench's own: set DCCSTATUSLINE_CONFIG\n"
          "  (or HOME) before running to choose the config it reads\n",
          argv0);

  return(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
  subject_t subj[2];
  const char *payload_path = NULL;
  char *payload = NULL;
  size_t payload_len = 0, runs = 5000, warmup = 500, nsubj, i, k;
  int opt;

  while((opt = getopt(argc, argv, "n:w:p:")) != -1)
  {
    if(opt == 'n') runs = strtoul(optarg, NULL, 10);

    else if(opt == 'w') warmup = strtoul(optarg, NULL, 10);

    else if(opt == 'p') payload_path = optarg;

    else return(usage(argv[0]));
  }

  nsubj = (size_t)(argc - optind);
  signal(SIGPIPE, SIG_IGN);   // a subject that exits unread must not kill us

  if(!payload_path || runs < 100 || nsubj < 1 || nsubj > 2)
    return(usage(argv[0]));

  if(!slurp(payload_path, &payload, &payload_len))
  {
    fprintf(stderr, "bench: %s: %s\n", payload_path, strerror(errno));
    return(EXIT_FAILURE);
  }

  for(k = 0; k < nsubj; k++)
  {
    subj[k].bin = argv[optind + (int)k];
    subj[k].faults = 0;
    subj[k].ns = calloc(runs, sizeof *subj[k].ns);

    if(!subj[k].ns) return(EXIT_FAILURE);
  }

  for(i = 0; i < warmup + runs; i++)
    for(k = 0; k < nsubj; k++)
    {
      // Alternate which subject goes first, so neither always runs warm.
      subject_t *s = &subj[(k + i) % nsubj];
      uint64_t ns, faults;

      if(!run_once(s->bin, payload, payload_len, &ns, &faults))
      {
        fprintf(stderr, "bench: %s: run failed\n", s->bin);
        return(EXIT_FAILURE);
      }

      if(i >= warmup)
      {
        s->ns[i - warmup] = ns;
        s->faults += faults;
      }
    }

  for(k = 0; k < nsubj; k++) report(&subj[k], runs);

  return(EXIT_SUCCESS);
}
