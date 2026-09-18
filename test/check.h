#ifndef DCC_TEST_CHECK_H
#define DCC_TEST_CHECK_H

#include <stdio.h>
#include <string.h>

static int check_failures;

// The format string rides inside __VA_ARGS__ rather than sitting in a named
// parameter, so a CHECK with nothing to interpolate still passes one variadic
// argument. The obvious spelling — CHECK(cond, fmt, ...) — leaves that tail
// empty, which C23 permits but clang still rejects under -Wpedantic; two
// fprintf calls in a test harness cost nothing and keep the macro ISO-clean.
#define CHECK(cond, ...)                                                   \
  do                                                                       \
  {                                                                        \
    if(!(cond))                                                            \
    {                                                                      \
      check_failures++;                                                    \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);                 \
      fprintf(stderr, __VA_ARGS__);                                        \
      fputc('\n', stderr);                                                 \
    }                                                                      \
  }                                                                        \
  while(0)

#define CHECK_MEM(got, gotn, want)                                         \
  CHECK((gotn) == strlen(want) && memcmp((got), (want), (gotn)) == 0,      \
        "got \"%.*s\" want \"%s\"", (int)(gotn), (got), (want))

#endif
