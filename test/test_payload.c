// dccstatusline — MIT
// test_payload: schema mapping, presence flags, clamps, window slots

#include <string.h>

#include "payload.h"

#include "check.h"

static void test_full(void);
static void test_minimal(void);
static void test_cwd_fallback(void);
static void test_context_edges(void);
static void test_windows(void);
static void test_hostility(void);

static void
test_full(void)
{
  char doc[] =
    "{"
    "\"cwd\":\"/top\","
    "\"session_id\":\"abc\","
    "\"model\":{\"id\":\"claude-fable-5\",\"display_name\":\"Fable\"},"
    "\"workspace\":{\"current_dir\":\"/mnt/proj\",\"project_dir\":\"/mnt/proj\","
    "  \"added_dirs\":[],\"repo\":{\"host\":\"github.com\"}},"
    "\"version\":\"2.1.90\","
    "\"output_style\":{\"name\":\"default\"},"
    "\"effort\":{\"level\":\"max\"},"
    "\"cost\":{\"total_cost_usd\":0.5,\"total_duration_ms\":45000,"
    "  \"total_lines_added\":156,\"total_lines_removed\":23},"
    "\"context_window\":{\"total_input_tokens\":15500,\"total_output_tokens\":1200,"
    "  \"context_window_size\":200000,\"used_percentage\":8,"
    "  \"current_usage\":{\"input_tokens\":8500}},"
    "\"exceeds_200k_tokens\":false,"
    "\"rate_limits\":{"
    "  \"five_hour\":{\"used_percentage\":23.5,\"resets_at\":1738425600},"
    "  \"seven_day\":{\"used_percentage\":41.2,\"resets_at\":1738857600}}"
    "}";
  payload_t p;

  CHECK(payload_parse(doc, sizeof doc - 1, &p), "full payload parses");
  CHECK(p.has_cwd, "cwd present");
  CHECK_MEM(p.cwd.p, p.cwd.n, "/mnt/proj");   // workspace wins over top-level
  CHECK(p.has_model, "model present");
  CHECK_MEM(p.model_name.p, p.model_name.n, "Fable");
  CHECK_MEM(p.model_id.p, p.model_id.n, "claude-fable-5");
  CHECK_MEM(p.effort.p, p.effort.n, "max");
  CHECK(p.has_context && p.ctx_used == 16700 && p.ctx_ceiling == 200000,
        "context sums input+output (used=%llu)",
        (unsigned long long)p.ctx_used);
  CHECK(p.has_pct && p.ctx_pct == 8, "precomputed pct kept");
  CHECK(p.has_cost && p.duration_ms == 45000 && p.lines_added == 156 &&
        p.lines_removed == 23, "cost block");
  CHECK(p.plan[PLAN_SHORT].present && p.plan[PLAN_SHORT].pct == 24,
        "5h pct rounds (got %u)", p.plan[PLAN_SHORT].pct);
  CHECK(p.plan[PLAN_SHORT].has_resets &&
        p.plan[PLAN_SHORT].resets_at == 1738425600, "5h resets_at");
  CHECK(p.plan[PLAN_LONG].present && p.plan[PLAN_LONG].pct == 41, "7d pct");
  CHECK_MEM(p.plan[PLAN_SHORT].window.p, p.plan[PLAN_SHORT].window.n, "5h");
  CHECK_MEM(p.plan[PLAN_LONG].window.p, p.plan[PLAN_LONG].window.n, "7d");
}

static void
test_minimal(void)
{
  char doc[] = "{}";
  payload_t p;

  CHECK(payload_parse(doc, sizeof doc - 1, &p), "empty object parses");
  CHECK(!p.has_cwd && !p.has_model && !p.has_context && !p.has_cost,
        "nothing present");
  CHECK(!p.plan[PLAN_SHORT].present && !p.plan[PLAN_LONG].present,
        "no windows");
  CHECK(p.effort.n == 0, "no effort");
}

static void
test_cwd_fallback(void)
{
  char top_only[] = "{\"cwd\":\"/x\"}";
  char both[] = "{\"cwd\":\"/x\",\"workspace\":{\"current_dir\":\"/y\"}}";
  char dupe[] = "{\"cwd\":\"/a\",\"cwd\":\"/b\"}";
  payload_t p;

  CHECK(payload_parse(top_only, sizeof top_only - 1, &p), "top only");
  CHECK_MEM(p.cwd.p, p.cwd.n, "/x");

  CHECK(payload_parse(both, sizeof both - 1, &p), "both");
  CHECK_MEM(p.cwd.p, p.cwd.n, "/y");

  CHECK(payload_parse(dupe, sizeof dupe - 1, &p), "dupe");
  CHECK_MEM(p.cwd.p, p.cwd.n, "/b");   // last wins
}

static void
test_context_edges(void)
{
  char nullpct[] = "{\"context_window\":{\"total_input_tokens\":100,"
                   "\"context_window_size\":200000,\"used_percentage\":null}}";
  char nasty[] = "{\"context_window\":{\"total_input_tokens\":-50,"
                 "\"total_output_tokens\":1e30,\"used_percentage\":150}}";
  char lowpct[] = "{\"context_window\":{\"used_percentage\":-3}}";
  payload_t p;

  CHECK(payload_parse(nullpct, sizeof nullpct - 1, &p), "null pct doc");
  CHECK(p.has_context && !p.has_pct && p.ctx_used == 100, "null pct absent");

  CHECK(payload_parse(nasty, sizeof nasty - 1, &p), "nasty numbers doc");
  CHECK(p.ctx_used == UINT64_MAX, "negative floors, huge saturates");
  CHECK(p.has_pct && p.ctx_pct == 100, "pct clamps high");

  CHECK(payload_parse(lowpct, sizeof lowpct - 1, &p), "low pct doc");
  CHECK(p.has_pct && p.ctx_pct == 0, "pct clamps low");
}

static void
test_windows(void)
{
  char shortonly[] = "{\"rate_limits\":{\"five_hour\":{\"used_percentage\":10},"
                     "\"mystery_window\":{\"used_percentage\":50}}}";
  char badresets[] = "{\"rate_limits\":{\"seven_day\":{\"used_percentage\":5,"
                     "\"resets_at\":-7}}}";
  payload_t p;

  CHECK(payload_parse(shortonly, sizeof shortonly - 1, &p), "short only");
  CHECK(p.plan[PLAN_SHORT].present && !p.plan[PLAN_SHORT].has_resets,
        "5h present, no resets");
  CHECK(!p.plan[PLAN_LONG].present, "7d absent; unknown window skipped");

  CHECK(payload_parse(badresets, sizeof badresets - 1, &p), "bad resets doc");
  CHECK(p.plan[PLAN_LONG].present && !p.plan[PLAN_LONG].has_resets,
        "negative resets_at dropped");
}

static void
test_hostility(void)
{
  char arr[] = "[1,2]";
  char wrongtype[] = "{\"model\":\"just a string\",\"cwd\":\"/ok\"}";
  char cut[] = "{\"model\":{\"display_name\":\"Fable\"},\"cwd\":\"/ok\",\"x\":\"cut";
  payload_t p;

  CHECK(!payload_parse(arr, sizeof arr - 1, &p), "non-object rejected");

  CHECK(payload_parse(wrongtype, sizeof wrongtype - 1, &p), "wrong type doc");
  CHECK(!p.has_model && p.has_cwd, "string model skipped, cwd still lands");

  CHECK(payload_parse(cut, sizeof cut - 1, &p), "truncated doc best-effort");
  CHECK(p.has_model, "fields before the cut survive");
  CHECK_MEM(p.cwd.p, p.cwd.n, "/ok");
}

int
main(void)
{
  test_full();
  test_minimal();
  test_cwd_fallback();
  test_context_edges();
  test_windows();
  test_hostility();

  return(check_failures ? 1 : 0);
}
