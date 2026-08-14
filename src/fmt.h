#ifndef DCC_FMT_H
#define DCC_FMT_H

#include <stddef.h>

#include "color.h"
#include "util.h"

typedef struct
{
  const char *name;   // template spelling, e.g. "used" for {used}
  sv_t value;         // n == 0 means absent — see the swallow rule below
  color_t fg, bg;     // COLOR_NONE inherits the section base style
} fmt_token_t;

// Renders a template ("{used}/{ceiling} {pct}") into sb. Literal runs carry
// the base style; each token carries its own, falling back to base. The
// swallow rule: a token that is absent or unknown emits nothing AND consumes
// the literal run immediately before it, so dangling punctuation vanishes
// with its datum. {{ and }} are literal braces; an unclosed { renders
// literally to the end.
void fmt_render(sbuf_t *, sv_t, const fmt_token_t *, size_t, color_t, color_t);

#ifdef FMT_INTERNAL
static const fmt_token_t *token_find(sv_t, const fmt_token_t *, size_t);
static void emit_literal(sbuf_t *, sv_t, size_t, size_t, color_t, color_t);
#endif

#endif
