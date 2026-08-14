// dccstatusline — MIT
// json: schema-blind pull cursor with in-place unescape and iterative skip

#include <stdlib.h>
#include <string.h>

#define JSON_INTERNAL
#include "json.h"

void
json_init(json_t *j, char *buf, size_t n)
{
  j->p = buf;
  j->n = n;
  j->i = 0;
  j->err = false;

  if(n >= 3 && memcmp(buf, "\xef\xbb\xbf", 3) == 0) j->i = 3;
}

bool
json_ok(const json_t *j)
{
  return(!j->err);
}

static void
skip_ws(json_t *j)
{
  while(j->i < j->n)
  {
    char c = j->p[j->i];

    if(c != ' ' && c != '\t' && c != '\n' && c != '\r') break;

    j->i++;
  }
}

// Raw scan past a string, honoring backslash pairs — no unescape needed when
// the content is being discarded.
static void
skip_string_raw(json_t *j)
{
  j->i++;   // opening quote

  while(j->i < j->n && j->p[j->i] != '"')
    j->i += j->p[j->i] == '\\' ? 2 : 1;

  if(j->i >= j->n)
  {
    j->i = j->n;
    j->err = true;
    return;
  }

  j->i++;   // closing quote
}

static void
skip_primitive(json_t *j)
{
  while(j->i < j->n)
  {
    char c = j->p[j->i];

    if(c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
       c == ',' || c == ':' || c == '}' || c == ']')
      break;

    j->i++;
  }
}

// Skips exactly one value. Iterative with a hard depth cap: recursion here
// would hand hostile stdin a stack overflow.
void
json_skip(json_t *j)
{
  uint32_t depth = 0;

  if(j->err) return;

  do
  {
    skip_ws(j);

    if(j->i >= j->n)
    {
      j->err = true;
      return;
    }

    switch(j->p[j->i])
    {
      case '{':
      case '[':
        if(++depth > JSON_MAX_DEPTH)
        {
          j->err = true;
          return;
        }

        j->i++;
        break;

      case '}':
      case ']':
        if(!depth)
        {
          j->err = true;   // skip was asked for a value, not a closer
          return;
        }

        depth--;
        j->i++;
        break;

      case '"':
        skip_string_raw(j);
        break;

      case ',':
      case ':':
        if(!depth)
        {
          j->err = true;
          return;
        }

        j->i++;
        break;

      default:
        skip_primitive(j);
    }
  }
  while(depth && !j->err);
}

static uint32_t
hex4(const char *p)
{
  uint32_t v = 0;
  size_t i;

  for(i = 0; i < 4; i++)
  {
    char c = p[i];
    uint32_t nib;

    if(c >= '0' && c <= '9') nib = (uint32_t)(c - '0');

    else if(c >= 'a' && c <= 'f') nib = (uint32_t)(c - 'a' + 10);

    else if(c >= 'A' && c <= 'F') nib = (uint32_t)(c - 'A' + 10);

    else return(UINT32_MAX);

    v = v << 4 | nib;
  }

  return(v);
}

static size_t
utf8_put(char *dst, uint32_t cp)
{
  if(cp < 0x80)
  {
    dst[0] = (char)cp;

    return(1);
  }

  if(cp < 0x800)
  {
    dst[0] = (char)(0xc0 | cp >> 6);
    dst[1] = (char)(0x80 | (cp & 0x3f));

    return(2);
  }

  if(cp < 0x10000)
  {
    dst[0] = (char)(0xe0 | cp >> 12);
    dst[1] = (char)(0x80 | (cp >> 6 & 0x3f));
    dst[2] = (char)(0x80 | (cp & 0x3f));

    return(3);
  }

  dst[0] = (char)(0xf0 | cp >> 18);
  dst[1] = (char)(0x80 | (cp >> 12 & 0x3f));
  dst[2] = (char)(0x80 | (cp >> 6 & 0x3f));
  dst[3] = (char)(0x80 | (cp & 0x3f));

  return(4);
}

#define UTF8_REPLACEMENT "\xef\xbf\xbd"

// In-place unescape between the quotes: the write cursor trails the read
// cursor because every rewrite shrinks (\n 2→1, \uXXXX 6→≤3, pairs 12→4).
// Unpaired surrogates become U+FFFD (6→3); unknown escapes and bad \u hexes
// keep their character — the one rewrite that must never grow.
static bool
string_unescape(json_t *j, sv_t *out)
{
  char *w, *start;

  j->i++;   // opening quote
  start = w = j->p + j->i;

  while(j->i < j->n && j->p[j->i] != '"')
  {
    char c = j->p[j->i];

    if(c != '\\')
    {
      *w++ = c;
      j->i++;
      continue;
    }

    j->i++;   // the backslash

    if(j->i >= j->n) break;

    c = j->p[j->i];
    j->i++;

    switch(c)
    {
      case 'n': *w++ = '\n'; break;
      case 't': *w++ = '\t'; break;
      case 'r': *w++ = '\r'; break;
      case 'b': *w++ = '\b'; break;
      case 'f': *w++ = '\f'; break;

      case 'u':
      {
        uint32_t cp = j->n - j->i >= 4 ? hex4(j->p + j->i) : UINT32_MAX;

        if(cp == UINT32_MAX)
        {
          // Bad or short hex: fall back to the unknown-escape rule and keep
          // the 'u'. Emitting U+FFFD here would grow 2 consumed bytes into 3
          // written ones and let the write cursor overtake the read cursor.
          *w++ = 'u';
          break;
        }

        j->i += 4;

        if(cp >= 0xd800 && cp <= 0xdbff)
        {
          uint32_t lo = UINT32_MAX;

          if(j->n - j->i >= 6 && j->p[j->i] == '\\' && j->p[j->i + 1] == 'u')
            lo = hex4(j->p + j->i + 2);

          if(lo >= 0xdc00 && lo <= 0xdfff)
          {
            j->i += 6;
            cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
          }

          else cp = 0xfffd;
        }

        else if(cp >= 0xdc00 && cp <= 0xdfff) cp = 0xfffd;

        w += utf8_put(w, cp);
        break;
      }

      default: *w++ = c;   // \" \\ \/ and anything nonstandard: keep the char
    }
  }

  if(j->i >= j->n)
  {
    j->err = true;
    return(false);
  }

  j->i++;   // closing quote
  out->p = start;
  out->n = (size_t)(w - start);

  return(true);
}

bool
json_enter_object(json_t *j)
{
  if(j->err) return(false);

  skip_ws(j);

  if(j->i < j->n && j->p[j->i] == '{')
  {
    j->i++;

    return(true);
  }

  json_skip(j);

  return(false);
}

bool
json_next_key(json_t *j, sv_t *key)
{
  if(j->err) return(false);

  skip_ws(j);

  while(j->i < j->n && j->p[j->i] == ',')
  {
    j->i++;
    skip_ws(j);
  }

  if(j->i >= j->n)
  {
    j->err = true;
    return(false);
  }

  if(j->p[j->i] == '}')
  {
    j->i++;

    return(false);
  }

  if(j->p[j->i] != '"')
  {
    j->err = true;
    return(false);
  }

  if(!string_unescape(j, key)) return(false);

  skip_ws(j);

  if(j->i >= j->n || j->p[j->i] != ':')
  {
    j->err = true;
    return(false);
  }

  j->i++;

  return(true);
}

bool
json_read_string(json_t *j, sv_t *out)
{
  if(j->err) return(false);

  skip_ws(j);

  if(j->i < j->n && j->p[j->i] == '"') return(string_unescape(j, out));

  json_skip(j);

  return(false);
}

bool
json_read_number(json_t *j, double *out)
{
  char buf[64];
  size_t start, len;

  if(j->err) return(false);

  skip_ws(j);

  if(j->i < j->n &&
     (j->p[j->i] == '{' || j->p[j->i] == '[' || j->p[j->i] == '"'))
  {
    json_skip(j);

    return(false);
  }

  start = j->i;
  skip_primitive(j);
  len = j->i - start;

  if(!len)
  {
    json_skip(j);

    return(false);
  }

  {
    char *end = NULL;
    double v;

    if(len >= sizeof buf) len = sizeof buf - 1;

    memcpy(buf, j->p + start, len);
    buf[len] = '\0';
    v = strtod(buf, &end);

    if(end == buf) return(false);   // a primitive, but not a number (true/null/…)

    *out = v;
  }

  return(true);
}

bool
json_read_bool(json_t *j, bool *out)
{
  size_t start, len;

  if(j->err) return(false);

  skip_ws(j);

  if(j->i < j->n &&
     (j->p[j->i] == '{' || j->p[j->i] == '[' || j->p[j->i] == '"'))
  {
    json_skip(j);

    return(false);
  }

  start = j->i;
  skip_primitive(j);
  len = j->i - start;

  if(len == 4 && memcmp(j->p + start, "true", 4) == 0)
  {
    *out = true;

    return(true);
  }

  if(len == 5 && memcmp(j->p + start, "false", 5) == 0)
  {
    *out = false;

    return(true);
  }

  if(!len) json_skip(j);   // not a primitive at all: a container or closer

  return(false);
}
