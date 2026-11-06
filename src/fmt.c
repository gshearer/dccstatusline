// dccstatusline — MIT
// fmt: the token template engine behind every section's `format` key

#define FMT_INTERNAL
#include "fmt.h"

static const fmt_token_t *
token_find(sv_t name, const fmt_token_t *tokens, size_t ntokens)
{
  size_t i;

  for(i = 0; i < ntokens; i++)
    if(sv_eq_cstr(name, tokens[i].name)) return(&tokens[i]);

  return(NULL);
}

// Emits template bytes [from, to) under one SGR prefix, collapsing the {{ and
// }} escapes. Empty runs emit nothing at all — not even the prefix.
static void
emit_literal(sbuf_t *sb, sv_t tpl, size_t from, size_t to, color_t fg, color_t bg)
{
  size_t i = from, chunk = from;

  if(from >= to) return;

  color_sgr(sb, fg, bg);

  while(i < to)
  {
    if((tpl.p[i] == '{' || tpl.p[i] == '}') && i + 1 < to && tpl.p[i + 1] == tpl.p[i])
    {
      sbuf_append(sb, tpl.p + chunk, i + 1 - chunk);
      i += 2;
      chunk = i;
      continue;
    }

    i++;
  }

  sbuf_append(sb, tpl.p + chunk, i - chunk);
}

// A pill's rounded end, part of the token and not a literal in the template, so
// the swallow rule can't take it with a missing token after the pill. It wears
// the fill as text, over whatever surrounds the pill.
static void
emit_cap(sbuf_t *sb, const char *cap, color_t fill, color_t base_bg)
{
  color_sgr(sb, fill, base_bg);
  sbuf_append_sv(sb, sv_from_cstr(cap));
}

void
fmt_render(sbuf_t *sb, sv_t tpl, const fmt_token_t *tokens, size_t ntokens,
           color_t base_fg, color_t base_bg)
{
  size_t i = 0, lit = 0;

  while(i < tpl.n)
  {
    if(tpl.p[i] == '{')
    {
      if(i + 1 < tpl.n && tpl.p[i + 1] == '{')
      {
        i += 2;
        continue;
      }

      {
        size_t close = i + 1;
        sv_t name;
        const fmt_token_t *t;

        while(close < tpl.n && tpl.p[close] != '}') close++;

        if(close == tpl.n) break;   // unclosed: the tail renders literally

        name.p = tpl.p + i + 1;
        name.n = close - i - 1;
        t = token_find(name, tokens, ntokens);

        if(t && t->value.n)
        {
          const color_t fg = t->fg.kind != COLOR_NONE ? t->fg : base_fg;
          const color_t bg = t->bg.kind != COLOR_NONE ? t->bg
                             : t->pill ? base_fg : base_bg;

          emit_literal(sb, tpl, lit, i, base_fg, base_bg);

          if(t->pill) emit_cap(sb, "\xee\x82\xb6", bg, base_bg);   // U+E0B6

          color_sgr(sb, fg, bg);
          sbuf_append_sv(sb, t->value);

          if(t->pill) emit_cap(sb, "\xee\x82\xb4", bg, base_bg);   // U+E0B4
        }

        i = close + 1;
        lit = i;
      }

      continue;
    }

    if(tpl.p[i] == '}' && i + 1 < tpl.n && tpl.p[i + 1] == '}')
    {
      i += 2;
      continue;
    }

    i++;
  }

  emit_literal(sb, tpl, lit, tpl.n, base_fg, base_bg);
}
