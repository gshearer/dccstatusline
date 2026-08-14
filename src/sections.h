#ifndef DCC_SECTIONS_H
#define DCC_SECTIONS_H

#include "config.h"
#include "gitinfo.h"
#include "payload.h"
#include "util.h"

// Renders the whole status line (no trailing reset — main owns the tail)
// into sb: sections in configured order, styled separators between non-empty
// ones. A section without its datum produces nothing and takes its separator
// with it.
void statusline_render(sbuf_t *, const payload_t *, const config_t *,
                       const gitinfo_t *);

#ifdef SECTIONS_INTERNAL
#include "fmt.h"
#include "schema.h"

typedef struct
{
  fmt_token_t toks[DCC_MAX_TOKENS];
  size_t n;
} tokset_t;

static int tok_index(const section_desc_t *, const char *);
static void add_tok(tokset_t *, section_id_t, const config_t *, const char *, sv_t);
static void run_fmt(sbuf_t *, section_id_t, const config_t *, const tokset_t *);
static sv_t fmt_pct(char *, size_t, uint32_t);
static sv_t fmt_clock(char *, size_t, int64_t);
static sv_t derive_ver(char *, size_t, sv_t);
static bool name_carries_ver(sv_t, sv_t);
static void sec_cwd(sbuf_t *, const payload_t *, const config_t *);
static void sec_git(sbuf_t *, const config_t *, const gitinfo_t *);
static void sec_model(sbuf_t *, const payload_t *, const config_t *);
static void sec_context(sbuf_t *, const payload_t *, const config_t *);
static void sec_plan(sbuf_t *, const payload_t *, const config_t *,
                     plan_slot_t, section_id_t);
static void render_section(sbuf_t *, section_id_t, const payload_t *,
                           const config_t *, const gitinfo_t *);
#endif

#endif
