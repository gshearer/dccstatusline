#ifndef DCC_PAYLOAD_H
#define DCC_PAYLOAD_H

#include <stdbool.h>
#include <stdint.h>

#include "util.h"

// The short and long plan-usage windows. Claude Code reports these as
// five_hour / seven_day today; the wire-key table below is the single place
// that mapping lives, so a future window change touches nothing else.
typedef enum
{
  PLAN_SHORT = 0,
  PLAN_LONG,
  PLAN_COUNT,
} plan_slot_t;

typedef struct
{
  bool present;
  bool has_resets;
  uint32_t pct;        // rounded, clamped 0-100
  int64_t resets_at;   // unix seconds
  sv_t window;         // human tag for the slot: "5h" / "7d"
} plan_window_t;

// Best-effort mapping of the stdin payload. Every view aliases the parse
// buffer; absent fields leave their has_* flag false and their view empty.
typedef struct
{
  bool has_cwd;
  sv_t cwd;            // workspace.current_dir, else top-level cwd

  bool has_model;
  sv_t model_name;     // model.display_name
  sv_t model_id;       // model.id
  sv_t effort;         // effort.level; absent ⇒ n == 0

  bool has_context;
  bool has_pct;
  uint64_t ctx_used;   // total_input_tokens + total_output_tokens, saturating
  uint64_t ctx_ceiling;
  uint32_t ctx_pct;

  bool has_cost;       // parsed for the future; no section renders it yet
  double cost_usd;
  uint64_t duration_ms;
  uint64_t lines_added;
  uint64_t lines_removed;

  plan_window_t plan[PLAN_COUNT];
} payload_t;

// Parses buf (mutably — string unescapes happen in place) into *out. False
// means the payload was not even a JSON object; true means best effort stands,
// including partial fills cut short by malformed tails.
bool payload_parse(char *, size_t, payload_t *);

#ifdef PAYLOAD_INTERNAL
#include "json.h"

typedef struct
{
  const char *wire;   // rate_limits key as Claude Code spells it today
  const char *tag;    // what {window} renders
} plan_wire_t;

static const plan_wire_t plan_wire[PLAN_COUNT] =
{
  [PLAN_SHORT] = { "five_hour", "5h" },
  [PLAN_LONG]  = { "seven_day", "7d" },
};

static void parse_workspace(json_t *, sv_t *);
static void parse_model(json_t *, payload_t *);
static void parse_effort(json_t *, payload_t *);
static void parse_context(json_t *, payload_t *);
static void parse_cost(json_t *, payload_t *);
static void parse_rate_limits(json_t *, payload_t *);
static void parse_window(json_t *, plan_window_t *);
static uint64_t num_u64(double);
static uint32_t num_pct(double);
#endif

#endif
