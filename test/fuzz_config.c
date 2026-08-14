// dccstatusline — MIT
// fuzz_config: hostile INI through config_load, then rendered — fuzzed
// formats and separators exercise the template engine too

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "config.h"
#include "payload.h"
#include "sections.h"

int LLVMFuzzerTestOneInput(const uint8_t *, size_t);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  static char buf[32 * 1024];
  static char line[8 * 1024];
  config_t cfg;
  payload_t pay;
  gitinfo_t git = { true, "main", 4 };
  sbuf_t sb;

  if(size > sizeof buf) size = sizeof buf;

  if(size) memcpy(buf, data, size);

  config_defaults(&cfg);
  config_load(&cfg, buf, size);

  memset(&pay, 0, sizeof pay);
  pay.has_cwd = true;
  pay.cwd = sv_from_cstr("/home/u/proj");
  pay.has_model = true;
  pay.model_name = sv_from_cstr("Fable");
  pay.model_id = sv_from_cstr("claude-fable-5");
  pay.effort = sv_from_cstr("max");
  pay.has_context = true;
  pay.ctx_used = 16700;
  pay.ctx_ceiling = 200000;
  pay.has_pct = true;
  pay.ctx_pct = 8;
  pay.plan[PLAN_SHORT].present = true;
  pay.plan[PLAN_SHORT].pct = 24;
  pay.plan[PLAN_SHORT].window = sv_from_cstr("5h");
  pay.plan[PLAN_SHORT].has_resets = true;
  pay.plan[PLAN_SHORT].resets_at = 1738425600;
  pay.plan[PLAN_LONG].present = true;
  pay.plan[PLAN_LONG].pct = 41;
  pay.plan[PLAN_LONG].window = sv_from_cstr("7d");

  sbuf_init(&sb, line, sizeof line, 5);
  statusline_render(&sb, &pay, &cfg, &git);

  if(sb.len > sizeof line - 5) __builtin_trap();   // reserve invariant

  sbuf_append_tail(&sb, "\x1b[0m\n", 5);

  if(sb.len > sizeof line) __builtin_trap();       // cap invariant

  return(0);
}
