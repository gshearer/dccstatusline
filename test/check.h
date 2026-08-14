#ifndef DCC_TEST_CHECK_H
#define DCC_TEST_CHECK_H

#include <stdio.h>
#include <string.h>

static int check_failures;

#define CHECK(cond, fmt, ...)                                              \
  do                                                                       \
  {                                                                        \
    if(!(cond))                                                            \
    {                                                                      \
      check_failures++;                                                    \
      fprintf(stderr, "FAIL %s:%d: " fmt "\n",                             \
              __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__);               \
    }                                                                      \
  }                                                                        \
  while(0)

#define CHECK_MEM(got, gotn, want)                                         \
  CHECK((gotn) == strlen(want) && memcmp((got), (want), (gotn)) == 0,      \
        "got \"%.*s\" want \"%s\"", (int)(gotn), (got), (want))

#endif
