// dccstatusline — MIT
// test_color: the three color spellings and SGR emission

#include "color.h"

#include "check.h"

static void test_parse(void);
static void test_sgr(void);

static void
test_parse(void)
{
  static const struct
  {
    const char *text;
    bool ok;
    color_kind_t kind;
    uint8_t r, g, b;
  } rows[] =
  {
    { "cyan",           true,  COLOR_NAMED16,   6, 0, 0 },
    { "black",          true,  COLOR_NAMED16,   0, 0, 0 },
    { "bright_magenta", true,  COLOR_NAMED16,  13, 0, 0 },
    { "default",        true,  COLOR_NONE,      0, 0, 0 },
    { "chartreuse",     false, COLOR_NONE,      0, 0, 0 },
    { "",               false, COLOR_NONE,      0, 0, 0 },
    { "0",              true,  COLOR_IDX256,    0, 0, 0 },
    { "255",            true,  COLOR_IDX256,  255, 0, 0 },
    { "256",            false, COLOR_NONE,      0, 0, 0 },
    { "1000",           false, COLOR_NONE,      0, 0, 0 },
    { "12a",            false, COLOR_NONE,      0, 0, 0 },
    { "#ff8700",        true,  COLOR_RGB,     255, 135, 0 },
    { "#FF8700",        true,  COLOR_RGB,     255, 135, 0 },
    { "#ff870",         false, COLOR_NONE,      0, 0, 0 },
    { "#gg0000",        false, COLOR_NONE,      0, 0, 0 },
    { "ff8700",         false, COLOR_NONE,      0, 0, 0 },
  };
  size_t i;

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    color_t c = { COLOR_NONE, 77, 77, 77 };   // sentinel: untouched on reject
    bool ok = color_parse(sv_from_cstr(rows[i].text), &c);

    CHECK(ok == rows[i].ok, "\"%s\" accept/reject", rows[i].text);

    if(ok && rows[i].ok)
    {
      CHECK(c.kind == rows[i].kind, "\"%s\" kind", rows[i].text);

      if(c.kind == COLOR_NAMED16 || c.kind == COLOR_IDX256)
        CHECK(c.r == rows[i].r, "\"%s\" index", rows[i].text);

      if(c.kind == COLOR_RGB)
        CHECK(c.r == rows[i].r && c.g == rows[i].g && c.b == rows[i].b,
              "\"%s\" rgb", rows[i].text);
    }
  }
}

static void
test_sgr(void)
{
  static const struct
  {
    color_t fg, bg;
    const char *want;
  } rows[] =
  {
    { { COLOR_NAMED16,   6, 0, 0 }, { COLOR_NONE,     0, 0, 0 }, "\x1b[0;36m"              },
    { { COLOR_NAMED16,   9, 0, 0 }, { COLOR_NONE,     0, 0, 0 }, "\x1b[0;91m"              },
    { { COLOR_NONE,      0, 0, 0 }, { COLOR_NAMED16, 15, 0, 0 }, "\x1b[0;107m"             },
    { { COLOR_IDX256,  208, 0, 0 }, { COLOR_NONE,     0, 0, 0 }, "\x1b[0;38;5;208m"        },
    { { COLOR_RGB, 255, 135, 0 },   { COLOR_NAMED16,  4, 0, 0 }, "\x1b[0;38;2;255;135;0;44m" },
    { { COLOR_NONE,      0, 0, 0 }, { COLOR_NONE,     0, 0, 0 }, "\x1b[0m"                 },
  };
  char mem[64];
  size_t i;
  sbuf_t sb;

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    sbuf_init(&sb, mem, sizeof mem, 0);
    color_sgr(&sb, rows[i].fg, rows[i].bg);
    CHECK_MEM(sb.p, sb.len, rows[i].want);
  }
}

int
main(void)
{
  test_parse();
  test_sgr();

  return(check_failures ? 1 : 0);
}
