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
          emit_literal(sb, tpl, lit, i, base_fg, base_bg);
          color_sgr(sb,
                    t->fg.kind != COLOR_NONE ? t->fg : base_fg,
                    t->bg.kind != COLOR_NONE ? t->bg : base_bg);
          sbuf_append_sv(sb, t->value);
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
