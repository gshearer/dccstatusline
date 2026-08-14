# Building dccstatusline

Linux is the primary platform; the code is plain POSIX C and is expected to port to other
POSIX systems, but only Linux is exercised today.

## Requirements

- meson ≥ 1.4 and ninja
- gcc ≥ 14 or clang ≥ 18 (C23), glibc ≥ 2.38 or musl (`strlcpy`)
- clang only: the optional fuzz harnesses (`-Dfuzz=true`)

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

Run tests with `meson test -C <dir>`.

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
