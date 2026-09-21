#ifndef DCC_GITINFO_H
#define DCC_GITINFO_H

#include <stdbool.h>
#include <stddef.h>

#include "util.h"

typedef struct
{
  bool present;
  char name[256];   // branch name or 8-hex short SHA, owned here
  size_t n;
  size_t root_n;    // bytes of start_dir that are the repository root; 0 if none
} gitinfo_t;

// Walks up from start_dir toward / (64-level cap) looking for .git, reads
// HEAD directly — a .git *file* (worktree, submodule) is followed exactly one
// gitdir: hop. Never forks, never touches packed-refs (HEAD is always loose).
// Anything odd leaves present false.
void gitinfo_read(sv_t, gitinfo_t *);

#ifdef GITINFO_INTERNAL
#define GITINFO_MAX_LEVELS 64

static ssize_t read_small(const char *, char *, size_t);
static bool parse_head(const char *, size_t, size_t, gitinfo_t *);
static bool hex_run(const char *, size_t);
#endif

#endif
