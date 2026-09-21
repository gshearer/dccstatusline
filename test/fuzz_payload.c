// dccstatusline — MIT
// fuzz_payload: hostile stdin through the entire hot path, invariants asserted

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
  static char buf[64 * 1024];
  static char line[8 * 1024];
  payload_t pay;
  config_t cfg;
  gitinfo_t git = { false, "", 0, 0 };
  sbuf_t sb;

  if(size > sizeof buf) size = sizeof buf;

  if(size) memcpy(buf, data, size);

  config_defaults(&cfg);

  if(payload_parse(buf, size, &pay))
  {
    sbuf_init(&sb, line, sizeof line, 5);
    statusline_render(&sb, &pay, &cfg, &git, 1738411200);   // fixed: no wall clock in a fuzz run

    if(sb.len > sizeof line - 5) __builtin_trap();   // reserve invariant

    sbuf_append_tail(&sb, "\x1b[0m\n", 5);

    if(sb.len > sizeof line) __builtin_trap();       // cap invariant
  }

  return(0);
}
