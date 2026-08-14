#ifndef DCC_SCHEMA_H
#define DCC_SCHEMA_H

#include <stddef.h>

// The six sections, their config-file names, and their template tokens.
// Token order here is the storage order for per-token color overrides in
// config_t, so config parsing and section rendering agree by construction.

typedef enum
{
  SEC_CWD = 0,
  SEC_GIT,
  SEC_MODEL,
  SEC_CONTEXT,
  SEC_PLAN_SHORT,
  SEC_PLAN_LONG,
  SEC_COUNT,
} section_id_t;

#define DCC_MAX_TOKENS 5

typedef struct
{
  const char *name;
  const char *tokens[DCC_MAX_TOKENS + 1];   // NULL-terminated
} section_desc_t;

static const section_desc_t dcc_sections[SEC_COUNT] =
{
  [SEC_CWD]        = { "cwd",        { "label", "path", NULL } },
  [SEC_GIT]        = { "git",        { "label", "branch", NULL } },
  [SEC_MODEL]      = { "model",      { "label", "name", "ver", "effort", "id", NULL } },
  [SEC_CONTEXT]    = { "context",    { "label", "used", "ceiling", "pct", NULL } },
  [SEC_PLAN_SHORT] = { "plan_short", { "label", "window", "pct", "resets", NULL } },
  [SEC_PLAN_LONG]  = { "plan_long",  { "label", "window", "pct", "resets", NULL } },
};

#endif
