// dccstatusline — MIT
// payload: map Claude Code's stdin JSON onto payload_t, best effort

#include <string.h>

#define PAYLOAD_INTERNAL
#include "payload.h"

static uint64_t
num_u64(double d)
{
  if(!(d > 0)) return(0);   // negatives and NaN alike

  if(d >= 18446744073709551615.0) return(UINT64_MAX);

  return((uint64_t)d);
}

static uint32_t
num_pct(double d)
{
  if(!(d > 0)) return(0);

  if(d >= 100) return(100);

  return((uint32_t)(d + 0.5));
}

static void
parse_workspace(json_t *j, sv_t *current_dir)
{
  sv_t key;

  if(!json_enter_object(j)) return;

  while(json_next_key(j, &key))
  {
    if(sv_eq_cstr(key, "current_dir")) json_read_string(j, current_dir);

    else json_skip(j);
  }
}

static void
parse_model(json_t *j, payload_t *out)
{
  sv_t key;

  if(!json_enter_object(j)) return;

  while(json_next_key(j, &key))
  {
    if(sv_eq_cstr(key, "display_name")) json_read_string(j, &out->model_name);

    else if(sv_eq_cstr(key, "id")) json_read_string(j, &out->model_id);

    else json_skip(j);
  }

  out->has_model = out->model_name.n || out->model_id.n;
}

static void
parse_effort(json_t *j, payload_t *out)
{
  sv_t key;

  if(!json_enter_object(j)) return;

  while(json_next_key(j, &key))
  {
    if(sv_eq_cstr(key, "level")) json_read_string(j, &out->effort);

    else json_skip(j);
  }
}

static void
parse_context(json_t *j, payload_t *out)
{
  double in_tok = 0, out_tok = 0, size = 0, pct = 0;
  bool has_pct = false;
  sv_t key;

  if(!json_enter_object(j)) return;

  while(json_next_key(j, &key))
  {
    if(sv_eq_cstr(key, "total_input_tokens")) json_read_number(j, &in_tok);

    else if(sv_eq_cstr(key, "total_output_tokens")) json_read_number(j, &out_tok);

    else if(sv_eq_cstr(key, "context_window_size")) json_read_number(j, &size);

    else if(sv_eq_cstr(key, "used_percentage"))
      has_pct = json_read_number(j, &pct);   // null reads false: stays absent

    else json_skip(j);
  }

  out->has_context = true;
  out->ctx_ceiling = num_u64(size);

  {
    uint64_t a = num_u64(in_tok), b = num_u64(out_tok);

    out->ctx_used = a > UINT64_MAX - b ? UINT64_MAX : a + b;
  }

  if(has_pct)
  {
    out->has_pct = true;
    out->ctx_pct = num_pct(pct);
  }
}

static void
parse_cost(json_t *j, payload_t *out)
{
  double usd = 0, dur = 0, add = 0, del = 0;
  sv_t key;

  if(!json_enter_object(j)) return;

  while(json_next_key(j, &key))
  {
    if(sv_eq_cstr(key, "total_cost_usd")) json_read_number(j, &usd);

    else if(sv_eq_cstr(key, "total_duration_ms")) json_read_number(j, &dur);

    else if(sv_eq_cstr(key, "total_lines_added")) json_read_number(j, &add);

    else if(sv_eq_cstr(key, "total_lines_removed")) json_read_number(j, &del);

    else json_skip(j);
  }

  out->has_cost = true;
  out->cost_usd = usd > 0 ? usd : 0;
  out->duration_ms = num_u64(dur);
  out->lines_added = num_u64(add);
  out->lines_removed = num_u64(del);
}

static void
parse_window(json_t *j, plan_window_t *w)
{
  double pct = 0, resets = 0;
  bool has_pct = false, has_resets = false;
  sv_t key;

  if(!json_enter_object(j)) return;

  while(json_next_key(j, &key))
  {
    if(sv_eq_cstr(key, "used_percentage")) has_pct = json_read_number(j, &pct);

    else if(sv_eq_cstr(key, "resets_at")) has_resets = json_read_number(j, &resets);

    else json_skip(j);
  }

  w->present = true;
  w->pct = has_pct ? num_pct(pct) : 0;

  if(has_resets && resets > 0 && resets < 9007199254740992.0)   // 2^53: exact
  {
    w->has_resets = true;
    w->resets_at = (int64_t)resets;
  }
}

static void
parse_rate_limits(json_t *j, payload_t *out)
{
  sv_t key;

  if(!json_enter_object(j)) return;

  while(json_next_key(j, &key))
  {
    size_t slot;

    for(slot = 0; slot < PLAN_COUNT; slot++)
      if(sv_eq_cstr(key, plan_wire[slot].wire)) break;

    if(slot < PLAN_COUNT) parse_window(j, &out->plan[slot]);

    else json_skip(j);
  }
}

bool
payload_parse(char *buf, size_t len, payload_t *out)
{
  json_t j;
  sv_t top_cwd = { NULL, 0 }, ws_dir = { NULL, 0 };
  sv_t key;
  size_t slot;

  memset(out, 0, sizeof *out);

  for(slot = 0; slot < PLAN_COUNT; slot++)
    out->plan[slot].window = sv_from_cstr(plan_wire[slot].tag);

  json_init(&j, buf, len);

  if(!json_enter_object(&j)) return(false);

  while(json_next_key(&j, &key))
  {
    if(sv_eq_cstr(key, "cwd")) json_read_string(&j, &top_cwd);

    else if(sv_eq_cstr(key, "workspace")) parse_workspace(&j, &ws_dir);

    else if(sv_eq_cstr(key, "model")) parse_model(&j, out);

    else if(sv_eq_cstr(key, "effort")) parse_effort(&j, out);

    else if(sv_eq_cstr(key, "context_window")) parse_context(&j, out);

    else if(sv_eq_cstr(key, "cost")) parse_cost(&j, out);

    else if(sv_eq_cstr(key, "rate_limits")) parse_rate_limits(&j, out);

    else json_skip(&j);
  }

  out->cwd = ws_dir.n ? ws_dir : top_cwd;
  out->has_cwd = out->cwd.n > 0;

  return(true);
}
