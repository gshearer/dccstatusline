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
`-fstack-protector-strong` and drops only the unsupported flag. Static linking
(`-Dc_link_args=-static`) is Linux/musl only; a macOS build is dynamically linked.

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

## Release binaries

`.github/workflows/release.yml` builds static x86_64 and aarch64 binaries in
Alpine containers on every version-tag push and attaches them (with sha256
sums) to the GitHub release; asset names are unversioned so
`/releases/latest/download/dccstatusline-<arch>-linux-musl` is a stable URL.
Backfill an existing tag with `gh workflow run release.yml -f tag=<tag>`.

The same workflow also builds a native macOS Apple Silicon binary on a
`macos-15` runner and attaches it as `dccstatusline-arm64-macos` (macOS has no
static libc, so it is dynamically linked against `libSystem`). It is unsigned;
after downloading, clear the Gatekeeper quarantine flag once with
`xattr -d com.apple.quarantine <file>`. Intel Macs build from source.

One honest caveat: `_FORTIFY_SOURCE` is a glibc-headers mechanism, so on musl
the flag compiles as a no-op — static binaries keep `-fstack-protector-strong`
and `-fstack-clash-protection` but carry no fortify checks. The glibc release
build retains all three.

## Fuzzing

The fuzz build produces libFuzzer harnesses for the two parsers that face
external input — the stdin payload (JSON) and the config file (INI) — both
driven through the full render path under ASan/UBSan. Give each a scratch
corpus dir first (it receives newly discovered units) and the pristine seeds
second:

```sh
mkdir -p /tmp/dccfuzz-payload /tmp/dccfuzz-config
./build-fuzz/test/fuzz_payload /tmp/dccfuzz-payload test/corpus/payload -max_total_time=60 -max_len=65536
./build-fuzz/test/fuzz_config  /tmp/dccfuzz-config  test/corpus/config  -max_total_time=60 -max_len=32768
```

## Versioning

The build string is `dccstatusline v<version> (build #N)`. N lives in `build_number.txt`
at the repo root (gitignored, survives build-dir wipes) and increments on **every ninja
invocation that builds** — including no-op rebuilds and `meson test` runs. That is the
accepted semantic: N counts builds, not releases. Reset it by writing a number to the
file, or delete it to restart at 1. `dccstatusline --version` prints the string.

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
file present, the built-in defaults apply. See `examples/config` (lands with the sections
chunk) for the commented reference.
