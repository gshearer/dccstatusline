// dccstatusline — MIT
// test_util: string views, the saturating buffer, and formatting helpers

#include <stdint.h>

#include "util.h"

#define PATH_CAP 256

#include "check.h"

static void test_sv(void);
static void test_sbuf(void);
static void test_grouped(void);
static void test_abbrev(void);
static void test_basename(void);
static void test_cols(void);
static void test_tail(void);
static void test_shrink(void);
static void test_clamp(void);

static void
test_sv(void)
{
  sv_t a = sv_from_cstr("abc"), empty = sv_from_cstr(""), null = sv_from_cstr(NULL);

  CHECK(a.n == 3 && a.p[0] == 'a', "sv_from_cstr length/content");
  CHECK(empty.n == 0, "sv_from_cstr empty");
  CHECK(null.n == 0 && null.p == NULL, "sv_from_cstr NULL");
  CHECK(sv_eq(a, sv_from_cstr("abc")), "sv_eq equal");
  CHECK(!sv_eq(a, sv_from_cstr("ab")), "sv_eq length mismatch");
  CHECK(!sv_eq(a, sv_from_cstr("abd")), "sv_eq content mismatch");
  CHECK(sv_eq(empty, null), "sv_eq both empty");
  CHECK(sv_eq_cstr(a, "abc"), "sv_eq_cstr");
}

static void
test_sbuf(void)
{
  char mem[8];
  sbuf_t sb;

  sbuf_init(&sb, mem, sizeof mem, 0);
  sbuf_append_cstr(&sb, "hello");
  sbuf_append_byte(&sb, '!');
  CHECK_MEM(sb.p, sb.len, "hello!");
  CHECK(!sb.truncated, "no truncation within cap");

  sbuf_append_cstr(&sb, "xyz");   // 3 bytes into 2 remaining: dropped whole
  CHECK(sb.len == 6 && sb.truncated, "overflow is all-or-nothing");

  sbuf_append(&sb, "ab", 2);      // exactly fits after the failed append
  CHECK(sb.len == 8, "saturation is per-piece, not sticky");

  sbuf_init(&sb, mem, sizeof mem, 4);
  sbuf_append_cstr(&sb, "abcde");
  CHECK(sb.len == 0 && sb.truncated, "reserve excluded from normal appends");

  sbuf_init(&sb, mem, sizeof mem, 4);
  sbuf_append_cstr(&sb, "abcd");
  sbuf_append_tail(&sb, "wxyz", 4);
  CHECK_MEM(sb.p, sb.len, "abcdwxyz");
  CHECK(!sb.truncated, "tail append reaches the reserve");

  sbuf_append_tail(&sb, "!", 1);
  CHECK(sb.len == 8 && sb.truncated, "tail append saturates at cap");

  sbuf_append(&sb, "", 0);
  CHECK(sb.len == 8, "zero-length append is a no-op");
}

static void
test_grouped(void)
{
  static const struct
  {
    uint64_t v;
    const char *sep;
    const char *want;
  } rows[] =
  {
    { 0,                    ",",  "0"                          },
    { 999,                  ",",  "999"                        },
    { 1000,                 ",",  "1,000"                      },
    { 1234567,              ",",  "1,234,567"                  },
    { UINT64_MAX,           ",",  "18,446,744,073,709,551,615" },
    { 1234567,              "",   "1234567"                    },
    { 1000,                 "..", "1..000"                     },
  };
  char buf[32];
  size_t i, n;

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    n = u64_grouped(buf, sizeof buf, rows[i].v, sv_from_cstr(rows[i].sep));
    CHECK_MEM(buf, n, rows[i].want);
  }

  n = u64_grouped(buf, 5, 1000, sv_from_cstr(","));   // "1,000" needs 6
  CHECK(n == 0 && buf[0] == '\0', "insufficient cap yields empty");
}

static void
test_abbrev(void)
{
  static const struct
  {
    const char *path, *home, *want;
  } rows[] =
  {
    { "/home/doc",     "/home/doc",  "~"          },
    { "/home/doc/src", "/home/doc",  "~/src"      },
    { "/home/docs",    "/home/doc",  "/home/docs" },   // sibling, not a prefix
    { "/etc",          "/home/doc",  "/etc"       },
    { "/home/doc/src", "/home/doc/", "~/src"      },   // trailing slash in HOME
    { "/home/doc",     "",           "/home/doc"  },
  };
  char buf[64];
  size_t i;
  sv_t got;

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    got = path_abbrev(buf, sizeof buf,
                      sv_from_cstr(rows[i].path), sv_from_cstr(rows[i].home));
    CHECK_MEM(got.p, got.n, rows[i].want);
  }

  got = path_abbrev(buf, 3, sv_from_cstr("/home/doc/src"), sv_from_cstr("/home/doc"));
  CHECK_MEM(got.p, got.n, "/home/doc/src");   // cap too small: original view
}

static void
test_basename(void)
{
  static const struct
  {
    const char *path, *want;
  } rows[] =
  {
    { "a/b/c",     "c"    },
    { "/usr/bin/", "bin"  },
    { "/",         "/"    },
    { "///",       "/"    },
    { "name",      "name" },
    { "",          ""     },
  };
  size_t i;
  sv_t got;

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    got = path_basename(sv_from_cstr(rows[i].path));
    CHECK_MEM(got.p, got.n, rows[i].want);
  }
}


static void
test_cols(void)
{
  CHECK(path_cols(sv_from_cstr("/usr/bin")) == 8, "path_cols ascii");
  CHECK(path_cols(sv_from_cstr("")) == 0, "path_cols empty");
  CHECK(path_cols(sv_from_cstr("\xe2\x80\xa6")) == 1, "path_cols ellipsis is one column");
  CHECK(path_cols(sv_from_cstr("pr\xc3\xb6j")) == 4, "path_cols multibyte");
}

static void
test_tail(void)
{
  static const struct
  {
    const char *path;
    unsigned depth;
    const char *want;
  } rows[] =
  {
    { "/a/b/c/d",   2, "\xe2\x80\xa6/c/d"   },
    { "/a/b/c/d",   1, "\xe2\x80\xa6/d"     },
    { "/a/b/c/d",   4, "/a/b/c/d"           },   // as many components as we have
    { "/a/b",       9, "/a/b"               },   // fewer: nothing to drop
    { "/a/b/c/",    2, "\xe2\x80\xa6/b/c"   },   // trailing slash is not a component
    { "/a//b//c",   2, "\xe2\x80\xa6/b//c"  },
    { "a/b/c",      2, "\xe2\x80\xa6/b/c"   },   // relative paths shorten too
    { "~/src/proj", 1, "\xe2\x80\xa6/proj"  },
    { "/a/b/c/d",   0, "/a/b/c/d"           },   // depth 0 is off
    { "/",          2, "/"                  },
    { "",           2, ""                   },
  };
  char buf[PATH_CAP];
  size_t i;
  sv_t got;

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    got = path_tail(buf, sizeof buf, sv_from_cstr(rows[i].path), rows[i].depth);
    CHECK_MEM(got.p, got.n, rows[i].want);
  }

  got = path_tail(buf, 4, sv_from_cstr("/a/b/c"), 1);
  CHECK_MEM(got.p, got.n, "/a/b/c");   // cap too small: original view
}

static void
test_shrink(void)
{
  static const struct
  {
    const char *path;
    unsigned keep;
    const char *want;
  } rows[] =
  {
    { "/mnt/volumes/source/proj", 0, "/m/v/s/proj"              },
    { "/mnt/volumes/source/proj", 1, "/m/v/s/proj"              },
    { "/mnt/volumes/source/proj", 2, "/m/v/source/proj"         },
    { "/mnt/volumes/source/proj", 9, "/mnt/volumes/source/proj" },
    { "~/src/deep/proj",              1, "~/s/d/proj"                    },
    { "~/proj",                       1, "~/proj"                        },
    { "~",                            1, "~"                             },
    { "/proj",                        1, "/proj"                         },
    { "proj",                         1, "proj"                          },
    { "/usr/bin/",                    1, "/u/bin"                        },
    { "/",                            1, "/"                             },
    { "",                             1, ""                              },
    // A leading component's first codepoint, never its first byte.
    { "/mnt/\xc3\xa4rchive/proj",     1, "/m/\xc3\xa4/proj"          },
  };
  char buf[PATH_CAP];
  size_t i;
  sv_t got;

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    got = path_shrink(buf, sizeof buf, sv_from_cstr(rows[i].path), rows[i].keep);
    CHECK_MEM(got.p, got.n, rows[i].want);
  }

  got = path_shrink(buf, 4, sv_from_cstr("/a/b/long"), 1);
  CHECK_MEM(got.p, got.n, "/a/b/long");   // cap too small: original view
}

static void
test_clamp(void)
{
  static const struct
  {
    const char *path;
    size_t max;
    const char *want;
  } rows[] =
  {
    { "/a/b/c/dir",  10, "/a/b/c/dir"              },   // already fits
    { "/a/b/c/dir",   9, "\xe2\x80\xa6/b/c/dir"    },
    { "/a/b/c/dir",   7, "\xe2\x80\xa6/c/dir"      },
    { "/a/b/c/dir",   5, "\xe2\x80\xa6/dir"        },
    { "/a/b/c/dir",   3, "\xe2\x80\xa6ir"          },   // the last component alone overflows
    { "/a/b/c/dir",   1, "\xe2\x80\xa6"            },
    { "/a/b/c/dir",   0, "/a/b/c/dir"              },   // 0 is off
    { "/a/b/c/dir",  99, "/a/b/c/dir"              },
    { "dir",          2, "\xe2\x80\xa6r"           },   // no parent to drop
    { "/",            1, "/"                       },
    { "",             1, ""                        },
    // Columns, not bytes: "…/pröj" is six of them, and fits in six.
    { "/a/b/pr\xc3\xb6j",  6, "\xe2\x80\xa6/pr\xc3\xb6j" },
    // Under that, not even the last component fits whole: cut on a codepoint.
    { "/a/b/pr\xc3\xb6j",  4, "\xe2\x80\xa6r\xc3\xb6j"   },
  };
  char buf[PATH_CAP];
  size_t i;
  sv_t got;

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    got = path_clamp(buf, sizeof buf, sv_from_cstr(rows[i].path), rows[i].max);
    CHECK_MEM(got.p, got.n, rows[i].want);
  }

  got = path_clamp(buf, 3, sv_from_cstr("/a/b/c/dir"), 5);
  CHECK_MEM(got.p, got.n, "/a/b/c/dir");   // cap too small: original view
}

int
main(void)
{
  test_sv();
  test_sbuf();
  test_grouped();
  test_abbrev();
  test_basename();
  test_cols();
  test_tail();
  test_shrink();
  test_clamp();

  return(check_failures ? 1 : 0);
}
