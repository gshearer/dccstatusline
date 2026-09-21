// dccstatusline — MIT
// test_gitinfo: HEAD variants over mkdtemp fixtures, no git binary involved

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gitinfo.h"

#include "check.h"

static char root[64];

static bool join(char *, size_t, const char *);
static bool put_file(const char *, const char *);
static bool put_dir(const char *);
static void expect(const char *, const char *, const char *);
static void test_variants(void);
static void test_root(void);

static bool
join(char *dst, size_t cap, const char *rel)
{
  int n = snprintf(dst, cap, "%s/%s", root, rel);

  return(n > 0 && (size_t)n < cap);
}

static bool
put_dir(const char *rel)
{
  char path[PATH_MAX];

  if(!join(path, sizeof path, rel)) return(false);

  return(mkdir(path, 0755) == 0);
}

static bool
put_file(const char *rel, const char *content)
{
  char path[PATH_MAX];
  FILE *f;

  if(!join(path, sizeof path, rel)) return(false);

  f = fopen(path, "w");

  if(!f) return(false);

  if(fputs(content, f) < 0)
  {
    fclose(f);

    return(false);
  }

  return(fclose(f) == 0);
}

static void
expect(const char *start_rel, const char *want, const char *what)
{
  char path[PATH_MAX];
  gitinfo_t g;

  CHECK(join(path, sizeof path, start_rel), "%s: path fits", what);
  gitinfo_read(sv_from_cstr(path), &g);

  if(!want)
  {
    CHECK(!g.present, "%s: expected absent, got \"%.*s\"", what, (int)g.n, g.name);
    return;
  }

  CHECK(g.present, "%s: expected present", what);

  if(g.present)
    CHECK(g.n == strlen(want) && memcmp(g.name, want, g.n) == 0,
          "%s: got \"%.*s\" want \"%s\"", what, (int)g.n, g.name, want);
}

static void
test_variants(void)
{
  // repo1: symref HEAD, probed from a nested subdirectory
  CHECK(put_dir("repo1") && put_dir("repo1/.git") &&
        put_dir("repo1/a") && put_dir("repo1/a/b"), "repo1 dirs");
  CHECK(put_file("repo1/.git/HEAD", "ref: refs/heads/main\n"), "repo1 HEAD");
  expect("repo1", "main", "symref");
  expect("repo1/a/b", "main", "walk-up");

  // repo2: branch name containing slashes
  CHECK(put_dir("repo2") && put_dir("repo2/.git"), "repo2 dirs");
  CHECK(put_file("repo2/.git/HEAD", "ref: refs/heads/feature/x\n"), "repo2 HEAD");
  expect("repo2", "feature/x", "nested branch");

  // repo3: detached 40-hex
  CHECK(put_dir("repo3") && put_dir("repo3/.git"), "repo3 dirs");
  CHECK(put_file("repo3/.git/HEAD",
                 "0123456789abcdef0123456789abcdef01234567\n"), "repo3 HEAD");
  expect("repo3", "01234567", "detached sha1");

  // repo4: detached 64-hex (sha256 repository)
  CHECK(put_dir("repo4") && put_dir("repo4/.git"), "repo4 dirs");
  CHECK(put_file("repo4/.git/HEAD",
                 "aabbccddeeff00112233445566778899"
                 "aabbccddeeff00112233445566778899\n"), "repo4 HEAD");
  expect("repo4", "aabbccdd", "detached sha256");

  // repo5: unborn branch — HEAD symref is all we ever read
  CHECK(put_dir("repo5") && put_dir("repo5/.git"), "repo5 dirs");
  CHECK(put_file("repo5/.git/HEAD", "ref: refs/heads/newborn"), "repo5 HEAD");
  expect("repo5", "newborn", "unborn, no trailing newline");

  // repo6: .git FILE with a relative gitdir hop (worktree layout)
  CHECK(put_dir("repo6") && put_dir("repo6/wt") && put_dir("repo6/priv"),
        "repo6 dirs");
  CHECK(put_file("repo6/wt/.git", "gitdir: ../priv\n"), "repo6 gitdir file");
  CHECK(put_file("repo6/priv/HEAD", "ref: refs/heads/wt-branch\n"), "repo6 HEAD");
  expect("repo6/wt", "wt-branch", "gitdir hop");

  // repo7: gitdir hop to a missing target
  CHECK(put_dir("repo7"), "repo7 dirs");
  CHECK(put_file("repo7/.git", "gitdir: /nonexistent/nowhere\n"), "repo7 file");
  expect("repo7", NULL, "hop to nothing");

  // repo8: garbage HEAD
  CHECK(put_dir("repo8") && put_dir("repo8/.git"), "repo8 dirs");
  CHECK(put_file("repo8/.git/HEAD", "what even is this\n"), "repo8 HEAD");
  expect("repo8", NULL, "garbage HEAD");

  // No repository anywhere above. The fixture root lives inside the build
  // tree — inside this project's own repo — so an in-tree path would truly
  // find a branch; use an absolute path with nothing above it instead.
  {
    gitinfo_t g;

    gitinfo_read(sv_from_cstr("/nonexistent-dccstatusline-void"), &g);
    CHECK(!g.present, "no repo above /");

    gitinfo_read(sv_from_cstr(""), &g);
    CHECK(!g.present, "empty start dir");
  }
}

static void
test_root(void)
{
  char path[PATH_MAX];
  gitinfo_t g;

  // root_n is the prefix of start_dir that is the repository root, however
  // many levels up it was found — it is what the cwd `repo` style measures.
  CHECK(join(path, sizeof path, "repo1"), "root: path fits");
  gitinfo_read(sv_from_cstr(path), &g);
  CHECK(g.present && g.root_n == strlen(path),
        "root at the start dir (got %zu want %zu)", g.root_n, strlen(path));

  CHECK(join(path, sizeof path, "repo1/a/b"), "root: nested path fits");
  gitinfo_read(sv_from_cstr(path), &g);
  CHECK(g.present && g.root_n == strlen(path) - 4,
        "root two levels up (got %zu want %zu)", g.root_n, strlen(path) - 4);

  // A trailing slash is not part of the root.
  CHECK(join(path, sizeof path, "repo1/a/"), "root: trailing slash fits");
  gitinfo_read(sv_from_cstr(path), &g);
  CHECK(g.present && g.root_n == strlen(path) - 3,
        "trailing slash ignored (got %zu want %zu)", g.root_n, strlen(path) - 3);

  // The worktree hop reports the directory holding the .git file.
  CHECK(join(path, sizeof path, "repo6/wt"), "root: worktree path fits");
  gitinfo_read(sv_from_cstr(path), &g);
  CHECK(g.present && g.root_n == strlen(path),
        "worktree root (got %zu want %zu)", g.root_n, strlen(path));

  gitinfo_read(sv_from_cstr("/nonexistent-dccstatusline-void"), &g);
  CHECK(!g.present && g.root_n == 0, "no repo leaves root_n zero");
}

int
main(void)
{
  snprintf(root, sizeof root, "dccgit-XXXXXX");

  if(!mkdtemp(root))
  {
    fprintf(stderr, "mkdtemp failed\n");
    return(1);
  }

  test_variants();
  test_root();

  return(check_failures ? 1 : 0);
}
