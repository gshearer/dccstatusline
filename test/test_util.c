// dccstatusline — MIT
// test_util: string views, the saturating buffer, and formatting helpers

#include <stdint.h>

#include "util.h"

#include "check.h"

static void test_sv(void);
static void test_sbuf(void);
static void test_grouped(void);
static void test_abbrev(void);
static void test_basename(void);

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

int
main(void)
{
  test_sv();
  test_sbuf();
  test_grouped();
  test_abbrev();
  test_basename();

  return(check_failures ? 1 : 0);
}
