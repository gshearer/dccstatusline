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
main: parse flags, signal(SIGPIPE, SIG_IGN)
  → read(0) loop, EINTR-safe → stdin_buf[64 KiB]     overflow ⇒ treated as malformed
  → config: compiled defaults ← --theme ← INI file overlay ← --profile ← --glyphs
                                                      bad file/key/flag ⇒ the last value holds
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
| `main.c` | orchestration | flow above; `--version`, `--print-config`, `--themes`; the live run's `--theme` / `--profile` / `--glyphs`; sole includer of generated `version.h` |
| `json.c` | JSON, nothing else | pull cursor: enter object, iterate keys, read scalars; **iterative** skip of any value with depth cap 64; in-place string unescape |
| `payload.c` | Claude Code's schema | drives `json` over known key paths → `payload_t` (string views, numbers, `has_*` flags); best-effort partial fill |
| `config.c` | the INI dialect | file → `config_t` overlaying compiled defaults; per-key tolerance; owns the defaults, theme and profile tables |
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
| `cwd` | `{path}` (+ keys `style = abbrev\|full\|basename\|shrink\|repo`, `depth = N`, `max_len = N`) |
| `git` | `{branch}` |
| `model` | `{name}` `{ver}` `{effort}` `{effort_plain}` `{model_tag}` `{id}` |
| `context` | `{used}` `{ceiling}` `{pct}` |
| `plan_short` / `plan_long` | `{pct}` `{resets}` (+ key `resets = countdown\|clock\|clock12`) `{window}` ("5h"/"7d" today) |
| all | `{label}` (text from the section's `label =` key) |

Template semantics:

- **Swallow rule**: a token that renders empty also consumes the literal run immediately
  before it. `"{name} {ver} · {effort}"` without effort → `Fable 5`; `"{used}/{ceiling}"`
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
keys ignored (forward compatibility). `[statusline]` holds `theme` (a preset), `profile`
(`basic` or `detailed`, unset by default), `glyphs` (`off`, `on` or `nerdfont`, a
profile's, off by default), `sections` (order list), `separator` (+ `separator_fg/bg`), `thousands`.

Themes are compiled-in INI text, colors only, kept in a table beside the
defaults. Each named preset is two colors of its palette and a gray: the first for where
you are (cwd and branch), the second for the model and every usage figure, the gray for
the separator and all literals. `{model_tag}` fills with the second, which it inherits from
the model's `fg` (an unset pill `bg` falls back to the section's `fg`), and writes on the
palette's background. `theme = name` feeds that text through the same loader at
the line where it sits, so keys after it override the preset and keys before it are
overridden. The loader
writes into its buffer (quoted values collapse in place), so the text is first copied
into a static buffer in `config_theme`; every preset key is a color, parsed by value, so
no view points into that buffer once the preset is loaded. Only the first `theme` key
naming a known preset counts. `--themes` renders every preset over the defaults with a
fixed sample payload and clock, ignoring the user's config, so it shows the presets as
shipped. An optional second argument is INI text laid over each preset as if it sat
below the theme line, which is how the plugin's picker previews its separator, glyphs
and profile options; it borrows the config buffer, unused on that path, and is copied
in afresh per preset. One too big for that buffer exits 1, which the contract above
allows: `--themes` is not the status line's path. The picker's pick names a private-use separator by code point
(`sep=U+E0B1`): the skill that writes the config is Claude, which can't see the character
in tool output.

A profile is opinionated: a compiled-in section list and one format per section, kept in
a table beside the themes, and nothing else. Each profile carries two more tables, the
formats for `glyphs = on` and for `glyphs = nerdfont`, where a NULL keeps the plain
format: `⎇` before the branch and `↻` before a reset time, or U+E0A0 and U+F021. Those two
are private-use code points, empty boxes without a Nerd Font, so they are a value of
their own and `on` stays portable. U+F021 keeps a space after it, since a Nerd Font icon
can spill into the next cell; the swallow rule drops both when a window has no reset time.
`nerdfont` also draws the model as a pill and drops the `·`: `{model_tag} {effort_plain}`.
`{model_tag}` joins the name and version, since a literal space would punch a hole in the
fill. The token draws the rounded caps U+E0B6 and U+E0B4 itself, in its `bg`. As literals,
the closing cap would sit in the run before `{effort_plain}`, and the swallow rule would
take it with a missing effort. `{effort_plain}` is `{effort}` with no color of its own, so
it takes the section's `fg`. It is a token of its own because a theme's colors apply
whatever the glyphs, and `{effort}` keeps its gray on lines with no pill.
The `profile` and `glyphs` keys only record the choice;
`config_load` applies them once the whole buffer has been read, so the profile replaces
`sections` and every `format` wherever those keys sit, and `glyphs` counts on either side
of the `profile` line, while colors, labels, the separator, `thousands`, cwd shortening
and reset styles stay the config's. With no profile `glyphs` does nothing: the formats
are the config's, glyphs included. The views point at string literals, so applying never
touches a buffer. A preset's own load stops short of that step, so a profile is applied
once, by the outermost load. The renderer knows nothing of profiles: they work entirely
through formats.

On the live run `--theme`, `--profile` and `--glyphs` stand in for those three keys and
win over them. `--theme` is applied right after the defaults, before the file, as if it
were the config's first line, and pins the theme so the file's own `theme` key yields
without a note; the file's color keys still land on top. `--profile` is applied after the
file, so it beats the `profile` key, and `--glyphs` after that, reapplying the profile,
so it beats `glyphs` whichever flag comes first. An unknown name, a flag with no value,
or any other argument gets one stderr note and changes nothing, so a typo in the
`statusLine` command costs only that flag. `--version`, `--print-config` and `--themes`
count only as the first argument: later on they are any other argument, and none of the
three reads the live flags.

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
(no repo, unreadable, oversize) → the section vanishes. The walk also reports `root_n`,
the length of the start-dir prefix that is the repository root, which is what the cwd
`repo` style measures against — so no second walk and no extra syscall. The walk runs
only when something renders from it: the `git` section, or a `cwd` styled `repo`.

## Shortening the cwd

Three stages in `sec_cwd`, each a no-op when unconfigured, each a pure function over
views into PATH_MAX scratch: `style` picks the base form (`abbrev`, `full`, `basename`,
`shrink`, `repo`), `depth` decides how many trailing components survive whole — the rest
elided behind `…/` only where that saves width (a lone `/` or `~/` stays), or spelled one
letter apiece under `shrink`, a dot-directory keeping its dot — and `max_len` holds the
result to a budget, dropping leading components first and cutting the last one only if it
still overflows alone, all in one pass. The budget counts codepoints, which is exact for
most scripts and short for CJK and emoji, two columns wide each: a width table was judged
not worth carrying, and `wcwidth` answers nothing useful in the C locale. Cuts land on
codepoint boundaries, so a multibyte directory name is never sliced in half. A scratch
buffer too small to hold a stage's output leaves the path as it was: a short buffer costs
detail, never correctness.

## Build and versioning

meson + ninja; `c_std=gnu23`, `warning_level=3`, `werror=true`, `-Wvla -Wshadow
-Wformat=2`, stack protector + clash protection always; `_FORTIFY_SOURCE=3` only in
optimized, unsanitized configs. Release: `-Doptimization=2 -Db_lto=true` (the program is
syscall-dominated; -O3 buys nothing).

Build number: a native C helper (`tools/buildnum.c`) runs as a `build_always_stale`
custom target and generates `version.h` with `DCC_BUILD_STRING "dccstatusline v<version>"`.
Where `build_number.txt` exists (source root, gitignored — survives build-dir wipes) it is
incremented and the string gains ` (build #N)`; N counts ninja invocations that build, and
only `main.c` includes the header, so each bump costs one tiny recompile and a relink.
Without the file — release tarballs, CI clones, distro packages — the header is constant
and rewritten only when its text changes, so builds are reproducible and a second `ninja`
is a no-op.

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
