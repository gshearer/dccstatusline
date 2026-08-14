// dccstatusline — MIT
// test_json: cursor alignment, in-place unescape, iterative skip, hostility

#include "json.h"

#include "check.h"

static void test_scalars(void);
static void test_mismatch_alignment(void);
static void test_escapes(void);
static void test_depth(void);
static void test_truncation(void);
static void test_tolerance(void);

static void
test_scalars(void)
{
  char doc[] = "\xef\xbb\xbf"
               "{\"s\":\"hi\",\"n\":42,\"neg\":-3.5,\"b\":true,\"z\":null,"
               "\"after\":\"ok\"}";
  json_t j;
  sv_t key, s = { NULL, 0 }, after = { NULL, 0 };
  double n = 0, neg = 0;
  bool b = false, got_z = true;

  json_init(&j, doc, sizeof doc - 1);
  CHECK(json_enter_object(&j), "BOM then object accepted");

  while(json_next_key(&j, &key))
  {
    if(sv_eq_cstr(key, "s")) CHECK(json_read_string(&j, &s), "read s");

    else if(sv_eq_cstr(key, "n")) CHECK(json_read_number(&j, &n), "read n");

    else if(sv_eq_cstr(key, "neg")) CHECK(json_read_number(&j, &neg), "read neg");

    else if(sv_eq_cstr(key, "b")) CHECK(json_read_bool(&j, &b), "read b");

    else if(sv_eq_cstr(key, "z")) got_z = json_read_string(&j, &s);

    else if(sv_eq_cstr(key, "after")) json_read_string(&j, &after);

    else json_skip(&j);
  }

  CHECK(json_ok(&j), "clean parse");
  CHECK_MEM(s.p, s.n, "hi");
  CHECK(n == 42 && neg == -3.5 && b, "numbers and bool");
  CHECK(!got_z, "null is not a string");
  CHECK_MEM(after.p, after.n, "ok");   // cursor stayed aligned past the null
}

static void
test_mismatch_alignment(void)
{
  char doc[] = "{\"o\":{\"x\":1},\"a\":[1,[2],3],\"s\":\"txt\",\"n\":5}";
  json_t j;
  sv_t key;
  double n = 0;
  int misses = 0;

  json_init(&j, doc, sizeof doc - 1);
  CHECK(json_enter_object(&j), "enter");

  while(json_next_key(&j, &key))
  {
    if(sv_eq_cstr(key, "n")) CHECK(json_read_number(&j, &n), "read n");

    else if(!json_read_number(&j, &n)) misses++;   // containers + string skip
  }

  CHECK(json_ok(&j) && misses == 3 && n == 5,
        "read_number skips containers whole (misses=%d n=%g)", misses, n);
}

static void
test_escapes(void)
{
  static const struct
  {
    const char *raw;    // string body as it appears in the document
    const char *want;   // unescaped bytes
  } rows[] =
  {
    { "a\\nb",               "a\nb"              },
    { "\\t\\r\\b\\f",        "\t\r\b\f"          },
    { "\\\"x\\\\",           "\"x\\"             },
    { "\\/slash",            "/slash"            },
    { "\\u0041",             "A"                 },
    { "\\u00e9",             "\xc3\xa9"          },
    { "\\u20ac",             "\xe2\x82\xac"      },
    { "\\ud83d\\ude00",      "\xf0\x9f\x98\x80"  },
    { "\\ud800x",            "\xef\xbf\xbdx"     },   // unpaired high surrogate
    { "\\udc00",             "\xef\xbf\xbd"      },   // lone low surrogate
    { "\\uZZZZ",             "uZZZZ"             },   // bad hex keeps the u
    { "\\u12",               "u12"               },   // short hex at the end
    { "\\q",                 "q"                 },   // unknown escape
    { "caf\xc3\xa9",         "caf\xc3\xa9"       },   // raw UTF-8 passes through
  };
  size_t i;

  for(i = 0; i < sizeof rows / sizeof rows[0]; i++)
  {
    char doc[128];
    json_t j;
    sv_t key, got = { NULL, 0 };
    int n = snprintf(doc, sizeof doc, "{\"k\":\"%s\"}", rows[i].raw);

    CHECK(n > 0 && (size_t)n < sizeof doc, "fixture fits");
    json_init(&j, doc, (size_t)n);
    CHECK(json_enter_object(&j) && json_next_key(&j, &key) &&
          json_read_string(&j, &got), "row %zu parses", i);
    CHECK_MEM(got.p, got.n, rows[i].want);
  }
}

static void
test_depth(void)
{
  char deep[256], ok[160];
  json_t j;
  sv_t key, after = { NULL, 0 };
  size_t i, pos;

  // 70 nested arrays: over the cap, must go sticky-error without recursing
  pos = 0;
  memcpy(deep + pos, "{\"a\":", 5);
  pos += 5;

  for(i = 0; i < 70; i++) deep[pos++] = '[';

  deep[pos++] = '1';

  for(i = 0; i < 70; i++) deep[pos++] = ']';

  memcpy(deep + pos, ",\"after\":\"x\"}", 13);
  pos += 13;

  json_init(&j, deep, pos);
  CHECK(json_enter_object(&j), "enter deep");

  while(json_next_key(&j, &key))
  {
    if(sv_eq_cstr(key, "after")) json_read_string(&j, &after);

    else json_skip(&j);
  }

  CHECK(!json_ok(&j) && after.n == 0, "over-cap nesting is a sticky error");

  // 60 levels: under the cap, and the key after it still reads
  pos = 0;
  memcpy(ok + pos, "{\"a\":", 5);
  pos += 5;

  for(i = 0; i < 60; i++) ok[pos++] = '[';

  ok[pos++] = '1';

  for(i = 0; i < 60; i++) ok[pos++] = ']';

  memcpy(ok + pos, ",\"after\":\"x\"}", 13);
  pos += 13;

  json_init(&j, ok, pos);
  CHECK(json_enter_object(&j), "enter ok-depth");

  while(json_next_key(&j, &key))
  {
    if(sv_eq_cstr(key, "after")) json_read_string(&j, &after);

    else json_skip(&j);
  }

  CHECK(json_ok(&j), "under-cap nesting parses");
  CHECK_MEM(after.p, after.n, "x");
}

static void
test_truncation(void)
{
  char unterm[] = "{\"a\":\"never ends";
  char cutnum[] = "{\"a\":12";
  json_t j;
  sv_t key, s;
  double n = 0;

  json_init(&j, unterm, sizeof unterm - 1);
  CHECK(json_enter_object(&j) && json_next_key(&j, &key), "key before cut");
  CHECK(!json_read_string(&j, &s) && !json_ok(&j), "unterminated string errs");

  json_init(&j, cutnum, sizeof cutnum - 1);
  CHECK(json_enter_object(&j) && json_next_key(&j, &key), "key");
  CHECK(json_read_number(&j, &n) && n == 12, "number at EOF still lands");
  CHECK(!json_next_key(&j, &key), "then the object never closes");
  CHECK(!json_ok(&j), "and that is an error");
}

static void
test_tolerance(void)
{
  char sloppy[] = "{,\"a\":1,,\"b\":2,}";
  char notobj[] = "[1,2]";
  char closer[] = "}";
  json_t j;
  sv_t key;
  double a = 0, b = 0;

  json_init(&j, sloppy, sizeof sloppy - 1);
  CHECK(json_enter_object(&j), "enter sloppy");

  while(json_next_key(&j, &key))
  {
    if(sv_eq_cstr(key, "a")) json_read_number(&j, &a);

    else if(sv_eq_cstr(key, "b")) json_read_number(&j, &b);

    else json_skip(&j);
  }

  CHECK(json_ok(&j) && a == 1 && b == 2, "stray commas tolerated");

  json_init(&j, notobj, sizeof notobj - 1);
  CHECK(!json_enter_object(&j), "array is not an object");

  json_init(&j, closer, sizeof closer - 1);
  CHECK(!json_enter_object(&j) && !json_ok(&j), "lone closer errs");
}

int
main(void)
{
  test_scalars();
  test_mismatch_alignment();
  test_escapes();
  test_depth();
  test_truncation();
  test_tolerance();

  return(check_failures ? 1 : 0);
}
