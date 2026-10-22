# Building dccstatusline

Linux is the primary platform. The code is plain POSIX C and also builds and passes its
test suite on macOS (Apple clang 17, arm64); other POSIX systems are expected to work but
are not exercised.

## Requirements

- meson ≥ 1.4 and ninja
- gcc ≥ 14 or clang ≥ 18 (C23; Apple clang ≥ 16 works), glibc ≥ 2.38, musl, or a BSD/macOS
  libc (all provide `strlcpy`)
- clang only: the optional fuzz harnesses (`-Dfuzz=true`)

The hardening flags are feature-detected, so a toolchain that lacks
`-fstack-clash-protection` (notably Apple clang) still builds — it keeps
`-fstack-protector-strong` and drops only the unsupported flag. **On Linux this
is a hard floor, not a preference**: both flags are required, and `meson setup`
fails naming the missing one rather than quietly shipping a softer binary than
this file claims. Static linking (`-Dc_link_args=-static`) is Linux/musl only; a
macOS build is dynamically linked.

## Quick start

```sh
meson setup build-dev
ninja -C build-dev
./build-dev/src/dccstatusline --version
```

## Build configurations

| directory | setup line | purpose |
|---|---|---|
| `build-dev` | `meson setup build-dev` | debugoptimized default; day-to-day work |
| `build-release` | `meson setup build-release -Doptimization=2 -Ddebug=false -Db_lto=true` | shipping binary: -O2, LTO, `_FORTIFY_SOURCE=3`, stack protections |
| `build-asan` | `meson setup build-asan -Dbuildtype=debug -Db_sanitize=address,undefined` | the suite must be green here before anything ships |
| `build-fuzz` | `CC=clang meson setup build-fuzz -Dbuildtype=debug -Db_sanitize=address,undefined -Dfuzz=true` | libFuzzer harnesses for the payload and config parsers |
| `build-musl` | `CC=musl-gcc meson setup build-musl -Doptimization=2 -Ddebug=false -Db_lto=true -Dc_link_args=-static` | fully static binary (needs the `musl` package) |

Run tests with `meson test -C <dir>`.

## Continuous integration

`.github/workflows/ci.yml` runs on every push to `main` and every pull request:
both supported compilers (gcc 14, clang 18) against both the dev and release
configurations, the ASan/UBSan suite, the fuzz harnesses replaying their
committed corpora, and a macOS build on Apple clang. macOS is there
deliberately — `release.yml` is the only other place it appears, and finding a
break there means finding it with a tag already pushed.

## Release binaries

`.github/workflows/release.yml` builds static x86_64 and aarch64 binaries in
Alpine containers on every version-tag push and attaches them (with sha256
sums) to the GitHub release; asset names are unversioned so
`/releases/latest/download/dccstatusline-<arch>-linux-musl` is a stable URL.
Backfill an existing tag with `gh workflow run release.yml -f tag=<tag>`.
A release bumps `version` in `plugin/.claude-plugin/plugin.json` alongside `meson.build`
— CI fails until the two agree — because that version is what offers plugin users the
update.

The same workflow also builds a native macOS Apple Silicon binary on a
`macos-15` runner and attaches it as `dccstatusline-arm64-macos` (macOS has no
static libc, so it is dynamically linked against `libSystem`). It is unsigned,
which only matters for browser downloads: those carry `com.apple.quarantine` and
need `xattr -d com.apple.quarantine <file>` once before Gatekeeper will run them.
`curl` sets no such flag. Intel Macs build from source.

One honest caveat: `_FORTIFY_SOURCE` is a glibc-headers mechanism, so on musl
the flag compiles as a no-op — static binaries keep `-fstack-protector-strong`
and `-fstack-clash-protection` but carry no fortify checks. The glibc release
build retains all three.

## Fuzzing

The fuzz build produces three libFuzzer harnesses, all under ASan/UBSan. Two
cover the parsers that face external input — the stdin payload (JSON) and the
config file (INI) — each driven through the full render path. The third,
`fuzz_cwd`, takes five knob bytes (style, depth, max_len, repository root, buffer
cap) and then a hostile working directory, and asserts what the cwd shorteners
promise rather than merely that they survive: never over budget, never wider
than their input, never a codepoint cut in half.

The sanitize build lets UBSan report and carry on, so tell it to halt or its
findings scroll past as log lines instead of landing as crash files. Give each
harness a scratch corpus dir first (it receives newly discovered units) and the
pristine seeds second:

```sh
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mkdir -p /tmp/dccfuzz-payload /tmp/dccfuzz-config /tmp/dccfuzz-cwd
./build-fuzz/test/fuzz_payload /tmp/dccfuzz-payload test/corpus/payload -max_total_time=60 -max_len=65536
./build-fuzz/test/fuzz_config  /tmp/dccfuzz-config  test/corpus/config  -max_total_time=60 -max_len=32768
./build-fuzz/test/fuzz_cwd     /tmp/dccfuzz-cwd     test/corpus/cwd     -max_total_time=60 -max_len=4200
```

`fuzz_cwd`'s length cap sits just past PATH_MAX on purpose: a path too long for
the scratch buffers has to take the refusal paths too. CI builds all three
harnesses and replays every committed unit once (`-runs=0`) on each push; new
fuzzing stays a local job.

## Versioning

The build string is `dccstatusline v<version>`, plus ` (build #N)` in a tree that keeps a
build counter. The counter is opt-in by presence: N lives in `build_number.txt` at the repo
root (gitignored, survives build-dir wipes) and increments on **every ninja invocation that
builds** — including no-op rebuilds and `meson test` runs; N counts builds, not releases.
Start counting with `echo 0 > build_number.txt`, reset by writing a number, stop by
deleting it. Release tarballs, CI clones and distro packages have no counter, so they build
as plain `v<version>`, byte-identical from one build to the next. `dccstatusline --version`
prints the string.

## Install

```sh
meson setup build-release -Doptimization=2 -Ddebug=false -Db_lto=true --prefix ~/.local
ninja -C build-release install    # installs ~/.local/bin/dccstatusline
```

## Wiring into Claude Code

Add to `~/.claude/settings.json`:

```json
{
  "statusLine": {
    "type": "command",
    "command": "/home/you/.local/bin/dccstatusline",
    "padding": 0
  }
}
```

Configuration is read from `$DCCSTATUSLINE_CONFIG`, else
`$XDG_CONFIG_HOME/dccstatusline/config`, else `~/.config/dccstatusline/config`; with no
file present, the built-in defaults apply. See `examples/config` for the commented
reference; the binary embeds it at build time and prints it with `--print-config`, and
`test_cli` holds it to the compiled-in defaults.

## Testing the plugin

The plugin lives in `plugin/`; `.claude-plugin/marketplace.json` at the repo root
lists it. Its scripts need no Claude session:

```sh
claude plugin validate .                                 # marketplace and plugin together
sh plugin/scripts/install.sh /tmp/dcc                    # prints the path and version
sh plugin/skills/configure/preview.sh /tmp/dcc/dccstatusline
printf '[git]\nbranch_fg = magenta\n' > /tmp/dcc/draft
sh plugin/skills/configure/preview.sh /tmp/dcc/dccstatusline /tmp/dcc/draft
```

`install.sh` always fetches the latest *published* release, so it tests the
plugin, not your working tree's C code. A value that fails to parse in a draft
shows up on stderr. `configure` learns the dialect from `--print-config`, so
to exercise it against unreleased C changes, copy your own build over the
installed binary in `~/.claude/plugins/data/dccstatusline-dccstatusline/`.

When editing a `SKILL.md`, remember that Claude Code substitutes every
`${CLAUDE_PLUGIN_ROOT}` and `${CLAUDE_PLUGIN_DATA}` in the file as it loads,
prose included. A sentence that names one to warn against it arrives naming
the real path instead.

To try the skills, load the plugin from the checkout for one session:

```sh
claude --plugin-dir ./plugin
```

Then run `/dccstatusline:setup`. Ask for a change in plain words to exercise
`configure`, including one it cannot express ("show my battery"), which it
should decline rather than write. Restart the session to pick up edits to a
`SKILL.md` or script.

To take the path users take instead, add the checkout as a marketplace:

```
/plugin marketplace add ./
/plugin install dccstatusline@dccstatusline
```

Undo it with `/plugin uninstall dccstatusline@dccstatusline`, then
`/plugin marketplace remove dccstatusline`, and delete `statusLine` from your
settings.

`setup` writes `statusLine` into your real settings. To keep them untouched, run
under a scratch config dir: `CLAUDE_CONFIG_DIR=/tmp/cc-test claude --plugin-dir
./plugin`, with one `/login` inside. `configure` still edits your real
dccstatusline config, but backs up the old file to `.bak` first.
