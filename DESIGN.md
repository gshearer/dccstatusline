# dccstatusline — Design

Pure-C status line for Claude Code. Claude Code pipes a JSON payload to our stdin on each
status refresh (event-driven, debounced 300 ms, in-flight runs cancelled); we emit one
ANSI-styled line on stdout and exit. This document is the architecture reference; BUILD.md
covers building, TODO.md holds the work queue.

## The contract

- Non-zero exit or empty stdout blanks the status line. Therefore: **always exit 0,
  always emit something** — on malformed input, a dim fallback line (the build string),
  never silence.
- The payload evolves; unknown fields must be skipped without complaint. Absent and
  null fields are first-class states, not errors.
- Latency is the product: sub-millisecond end-to-end. Zero heap allocation, fixed static
  buffers, single read loop in / single write out, **no fork/exec**, no `setlocale`.

## Data flow

```
main: signal(SIGPIPE, SIG_IGN)
  → read(0) loop, EINTR-safe → stdin_buf[64 KiB]     overflow ⇒ treated as malformed
  → config: compiled defaults ← INI file overlay      bad file/key ⇒ that default holds
  → payload_parse: best-effort → payload_t            presence flag per field
  → gitinfo_read                                      only if the git section is enabled
  → statusline_render: sections in configured order,
    styled separators between non-empty sections
  → empty line ⇒ fallback (dim build string)
  → write(1) loop; EPIPE fine → return 0              single exit path, every route
```

## Modules

Core modules build into a `dcccore` static library shared by the binary, the unit tests,
and the fuzz harnesses. House style throughout: public API outside guards, internals
behind `#ifdef <MODULE>_INTERNAL`, `.c` files hold function bodies only.

| module | knows about | responsibility |
|---|---|---|
| `main.c` | orchestration | flow above; `--version`; sole includer of generated `version.h` |
| `json.c` | JSON, nothing else | pull cursor: enter object, iterate keys, read scalars; **iterative** skip of any value with depth cap 64; in-place string unescape |
| `payload.c` | Claude Code's schema | drives `json` over known key paths → `payload_t` (string views, numbers, `has_*` flags); best-effort partial fill |
| `config.c` | the INI dialect | file → `config_t` overlaying compiled defaults; per-key tolerance; owns the defaults table |
| `color.c` | terminal color | `color_t {none, named16, idx256, rgb}`: parse `bright_cyan` / `208` / `#ff8700`; SGR emission |
| `fmt.c` | templates | renders a format string against a token table `{name, value, fg, bg}` |
| `sections.c` | the six sections | producers build token tables from `payload_t` (+ gitinfo) and call `fmt`; `statusline_render` assembles the line |
| `gitinfo.c` | `.git` internals | walk up from cwd; `.git` dir or gitdir-file (one hop); `HEAD` → branch or 8-hex short SHA |
| `util.c` | primitives | `sv_t` string views; saturating `sbuf_t`; u64 → thousands-separated; `~`-abbreviation. Nothing else |

## Sections and templates

Six sections, rendered in config order: `cwd`, `git`, `model`, `context`, `plan_short`,
`plan_long`. The plan sections are deliberately window-agnostic: Claude Code currently
reports 5-hour and 7-day rate-limit windows, and a single mapping table in `payload.c`
feeds them into the short/long slots — if the windows ever change, the mapping changes,
no user's config does.

Each section renders a `format` template. Tokens:

| section | tokens |
|---|---|
| `cwd` | `{path}` (+ key `style = abbrev\|full\|basename`) |
| `git` | `{branch}` |
| `model` | `{name}` `{ver}` `{effort}` `{id}` |
| `context` | `{used}` `{ceiling}` `{pct}` |
| `plan_short` / `plan_long` | `{pct}` `{resets}` (HH:MM local) `{window}` ("5h"/"7d" today) |
| all | `{label}` (text from the section's `label =` key) |

Template semantics:

- **Swallow rule**: a token that renders empty also consumes the literal run immediately
  before it. `"{name} {ver} ·{effort}"` without effort → `Fable 5`; `"{used}/{ceiling}"`
  without a ceiling drops the `/`. One rule, every dangling-punctuation case.
- `{{` and `}}` are literal braces; an unclosed `{` renders literally; an unknown token
  renders empty (and swallows). An empty render makes the section vanish, separator
  included.
- Colors: the section's `fg`/`bg` style literal template text; `<token>_fg`/`<token>_bg`
  override per token. Every styled run is emitted as one SGR sequence starting `0;` —
  each element starts from a clean slate, so state never leaks, even under truncation.

Derivations: model version comes from the model id (`claude-fable-5` → "5";
unrecognized shapes leave `{ver}` empty, and so does a `{name}` that already ends
with the version — `Fable 5`, never `Fable 5 5`). Context used = `total_input_tokens +
total_output_tokens`; pct = payload's `used_percentage` clamped 0–100, else derived only
when `context_window_size > 0`.

## Configuration

Lookup order: `$DCCSTATUSLINE_CONFIG`, else `$XDG_CONFIG_HOME/dccstatusline/config`, else
`~/.config/dccstatusline/config`. No file → compiled-in defaults, which are identical to
the shipped, fully commented `examples/config`. A config error never blanks the line: a
bad value means that key keeps its default (one stderr note, visible in `claude --debug`).

INI dialect: `[section]`, `key = value`, `#` comments, quoted values for significant
whitespace (`separator = " │ "`), UTF-8 passed through as bytes, unknown sections and
keys ignored (forward compatibility). `[statusline]` holds `sections` (order list),
`separator` (+ `separator_fg/bg`), `thousands`.

## Buffers

All static, single-threaded, zero heap. stdin 64 KiB (payloads run ~1–2 KiB), config
32 KiB, line 8 KiB, per-section scratch 1 KiB, path scratch PATH_MAX. The `sbuf_t`
appender **saturates** — it never overflows, flags truncation, and reserves a tail so the
closing SGR reset always lands. String views point into the stdin/config buffers, which
outlive rendering. JSON strings unescape **in place** (unescaping only shrinks), so even
escaped strings stay zero-copy. A section commits to the line only if it produced bytes —
that is what makes vanished sections take their separator with them.

## Git without forking

From `workspace.current_dir` (fallback `cwd`), stat `<dir>/.git` walking toward `/`
(64-level cap). Directory → that is the gitdir; regular file → read `gitdir: <path>`,
resolve relative, follow exactly one hop (worktrees, submodules). Read `<gitdir>/HEAD`:
`ref: refs/heads/X` → branch `X` (prefix stripped, embedded slashes kept); 40/64 hex →
first 8 characters. HEAD is always a loose file — packed-refs never needed. Anything odd
(no repo, unreadable, oversize) → the section vanishes.

## Build and versioning

meson + ninja; `c_std=gnu23`, `warning_level=3`, `werror=true`, `-Wvla -Wshadow
-Wformat=2`, stack protector + clash protection always; `_FORTIFY_SOURCE=3` only in
optimized, unsanitized configs. Release: `-Doptimization=2 -Db_lto=true` (the program is
syscall-dominated; -O3 buys nothing).

Build number: a native C helper (`tools/buildnum.c`) runs as a `build_always_stale`
custom target, incrementing `build_number.txt` (source root, gitignored — survives
build-dir wipes) and generating `version.h` with `DCC_BUILD_STRING "dccstatusline v0.0.1
(build #N)"`. N counts ninja invocations that build; only `main.c` includes the header,
so each bump costs one tiny recompile and a relink.

## Testing

Table-driven unit tests per module (one row per distinct code path), `mkdtemp` fixtures
for gitinfo, colors-off plain-string tests for section producers plus one ANSI-exact case
pinning SGR emission, and a posix_spawn integration harness piping fixture payloads
through the real binary against golden outputs (the *program* never spawns; the test
may). Both parsers that face external input — payload JSON and config INI — get libFuzzer
harnesses (`-Dfuzz=true`) driving the full parse→render path under ASan/UBSan. Everything
ships only after the suite is green in the ASan build.

## Non-goals (v1)

COLUMNS-aware truncation, git dirty state, multi-row layouts, cost/duration/lines
sections (parsed, unrendered), NO_COLOR, macOS/Windows, CI + musl-static release
binaries, and any SIGSEGV catch-all (sanitizers and fuzzing are the defense — a handler
papering over corruption is not).
