#ifndef DCC_JSON_H
#define DCC_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "util.h"

// Schema-blind pull cursor over a MUTABLE byte buffer: string reads unescape
// in place (unescaping only shrinks), so returned views alias the buffer and
// share its lifetime. The grammar is deliberately tolerant — extra or missing
// commas never derail it — because the only hard promises are: never read out
// of bounds, never hang, never misalign onto the wrong value, and go sticky-
// error instead of guessing. Readers consume their value on success; on a
// type mismatch they skip it and return false, so drivers never resynchronize
// by hand. null is "not the type you asked for" — absent, by design.
typedef struct
{
  char *p;
  size_t n;
  size_t i;
  bool err;
} json_t;

void json_init(json_t *, char *, size_t);
bool json_ok(const json_t *);

bool json_enter_object(json_t *);
bool json_next_key(json_t *, sv_t *);

bool json_read_string(json_t *, sv_t *);
bool json_read_number(json_t *, double *);
bool json_read_bool(json_t *, bool *);
void json_skip(json_t *);

#ifdef JSON_INTERNAL
#define JSON_MAX_DEPTH 64

static void skip_ws(json_t *);
static void skip_string_raw(json_t *);
static void skip_primitive(json_t *);
static bool string_unescape(json_t *, sv_t *);
static uint32_t hex4(const char *);
static size_t utf8_put(char *, uint32_t);
#endif

#endif
