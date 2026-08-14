// dccstatusline — MIT
// gitinfo: branch or short SHA from .git/HEAD, walked up and read by hand

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define GITINFO_INTERNAL
#include "gitinfo.h"

static ssize_t
read_small(const char *path, char *dst, size_t cap)
{
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  size_t used = 0;

  if(fd < 0) return(-1);

  while(used < cap)
  {
    ssize_t got = read(fd, dst + used, cap - used);

    if(got < 0)
    {
      int e = errno;

      close(fd);
      errno = e;   // the caller reads errno; close must not clobber it

      return(-1);
    }

    if(got == 0) break;

    used += (size_t)got;
  }

  close(fd);

  return((ssize_t)used);
}

static bool
hex_run(const char *p, size_t n)
{
  size_t i;

  for(i = 0; i < n; i++)
    if(!((p[i] >= '0' && p[i] <= '9') || (p[i] >= 'a' && p[i] <= 'f')))
      return(false);

  return(n > 0);
}

static bool
parse_head(const char *buf, size_t n, gitinfo_t *out)
{
  sv_t s = { buf, n };

  while(s.n && (s.p[s.n - 1] == '\n' || s.p[s.n - 1] == '\r')) s.n--;

  if(s.n > 5 && memcmp(s.p, "ref: ", 5) == 0)
  {
    s.p += 5;
    s.n -= 5;

    if(s.n > 11 && memcmp(s.p, "refs/heads/", 11) == 0)
    {
      s.p += 11;
      s.n -= 11;
    }

    if(!s.n || s.n >= sizeof out->name) return(false);

    memcpy(out->name, s.p, s.n);
    out->n = s.n;
    out->name[s.n] = '\0';
    out->present = true;

    return(true);
  }

  if((s.n == 40 || s.n == 64) && hex_run(s.p, s.n))
  {
    memcpy(out->name, s.p, 8);
    out->n = 8;
    out->name[8] = '\0';
    out->present = true;

    return(true);
  }

  return(false);
}

void
gitinfo_read(sv_t start_dir, gitinfo_t *out)
{
  char dir[PATH_MAX], path[PATH_MAX], gitdir[PATH_MAX], head[512];
  size_t dirlen = start_dir.n, level;

  out->present = false;
  out->n = 0;
  out->name[0] = '\0';

  if(!dirlen || dirlen >= sizeof dir) return;

  memcpy(dir, start_dir.p, dirlen);

  while(dirlen > 1 && dir[dirlen - 1] == '/') dirlen--;

  for(level = 0; level < GITINFO_MAX_LEVELS; level++)
  {
    ssize_t got;
    int n = snprintf(path, sizeof path, "%.*s/.git/HEAD", (int)dirlen, dir);

    if(n <= 0 || (size_t)n >= sizeof path) return;

    got = read_small(path, head, sizeof head - 1);

    if(got > 0 && parse_head(head, (size_t)got, out)) return;

    if(got < 0 && errno == ENOTDIR)
    {
      // <dir>/.git is a file: worktree or submodule. One gitdir: hop.
      char link[PATH_MAX];

      n = snprintf(path, sizeof path, "%.*s/.git", (int)dirlen, dir);

      if(n <= 0 || (size_t)n >= sizeof path) return;

      got = read_small(path, link, sizeof link - 1);

      if(got <= 0) return;

      {
        sv_t s = { link, (size_t)got };

        while(s.n && (s.p[s.n - 1] == '\n' || s.p[s.n - 1] == '\r')) s.n--;

        if(s.n <= 8 || memcmp(s.p, "gitdir: ", 8) != 0) return;

        s.p += 8;
        s.n -= 8;

        if(s.p[0] == '/')
          n = snprintf(gitdir, sizeof gitdir, "%.*s/HEAD", (int)s.n, s.p);

        else
          n = snprintf(gitdir, sizeof gitdir, "%.*s/%.*s/HEAD",
                       (int)dirlen, dir, (int)s.n, s.p);

        if(n <= 0 || (size_t)n >= sizeof gitdir) return;

        got = read_small(gitdir, head, sizeof head - 1);

        if(got > 0) parse_head(head, (size_t)got, out);

        return;
      }
    }

    // No repository at this level: step to the parent. A dirlen of 1 was the
    // root (probed as "//.git/HEAD", which POSIX resolves fine) or a bare
    // relative name — either way there is no further up.
    if(dirlen <= 1) return;

    while(dirlen > 1 && dir[dirlen - 1] != '/') dirlen--;

    while(dirlen > 1 && dir[dirlen - 1] == '/') dirlen--;
  }
}
