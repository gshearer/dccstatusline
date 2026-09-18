# ACHIEVED.md

Ledger of shipped chunks — one row each. Optional verbose ship notes live in
`achieved/YYYY-MM.md` shards; find a section with `grep -n '^## <CHUNK-ID>'`.

| chunk | date | commit | outcome | shard |
|---|---|---|---|---|
| C1 · Scaffolding | 2026-08-14 | 6ad591c | meson/ninja skeleton with hardened flags, buildnum counter + version.h generation, minimal always-exit-0 binary, LICENSE/DESIGN.md/BUILD.md | — |
| C2 · Core primitives | 2026-08-14 | d653739 | util (sv/saturating sbuf/thousands/abbrev), color (3 spellings + SGR), fmt (templates, swallow rule); suites green in dev/ASan/release | — |
| C3 · Parsers | 2026-08-14 | 691e6e0 | json pull cursor (iterative skip, in-place shrink-only unescape), payload map (presence flags, window table), INI config (quote/comment dialect, per-key tolerance, XDG) | — |
| C4 · Program | 2026-08-14 | 30e2240 | gitinfo (one open/level, gitdir hop), six section producers + assembly, full main, examples/config, posix_spawn CLI harness; 9 suites green everywhere; ~285 µs/cycle | — |
| C5 · Hardening | 2026-08-14 | ae9e31a | libFuzzer harnesses through the full render path: 2.26M payload + 1.06M config execs under ASan/UBSan, zero findings; 49 syscalls, ~254 µs/cycle | — |
| C6 · Ship & shine | 2026-08-14 | d40e128 | flagship README with SVG terminal screenshot + badges + measured perf table; binary installed to ~/.local/bin; v0.0.1 complete | — |
| C7 · Live-fix & first light | 2026-08-14 | 9a3c64b | {ver} suppressed when display_name already carries the version ("Fable 5 5" bug from live test); repo published to github.com/gshearer/dccstatusline; v0.0.1 tagged and released | — |
| C8 · Static binaries + CI | 2026-08-14 | 8f510b8 | musl-static build proven locally (build-musl), tag-push release workflow (Alpine on x86_64 + arm64 runners, plain git clone, stripped assets + sha256), v0.0.1 backfilled: 66 KB x86_64 / 130 KB aarch64 at stable /latest/ URLs, README quick start gains the no-toolchain path | — |
| C9 · Reset styles | 2026-08-14 | 159a1e2 | `{resets}` gains a per-section `resets` key (countdown \| clock \| clock12); countdown is the new default and both default formats carry ↻{resets}; long window gains weekday + day count; render takes the wall clock as a parameter; 12-row style table, 2.7M fuzz execs clean | — |
| C10 · macOS release binary | 2026-09-17 | 7ccff83 | first external PR (kenzik #1) merged: hardening flags feature-detected via `cc.get_supported_arguments()` so Apple clang builds under `werror=true` (Linux keeps both flags, verified in `compile_commands.json`), `build-macos` job on macos-15 publishes `dccstatusline-arm64-macos`, checkout@v5; follow-up 3257daf verifies the packaged asset instead of the build-tree binary | — |
| C11 · CI + portability floor | 2026-09-17 | c93aa0e | `ci.yml` on every push/PR (gcc-14 + clang-18 × dev/release, ASan/UBSan, Apple clang); Linux hardening became a configure-time floor instead of a silent downgrade; `release` job ends the three-way create race; CI's first run found two latent bugs it then fixed — clang could never build the suite (CHECK's empty variadic tail, 2667372) and test_config byte-compared a padded `color_t` (6eef71e); v0.0.1 Linux assets rebuilt, macOS asset impossible on that tag (source predates the fix) | — |
| C12 · v0.0.2 | 2026-09-17 | b748787 | version bumped and tagged so the macOS asset has a release to live in (v0.0.1 could never carry it — the backfill builds the tagged source, which predates the fix); all four release jobs green, `/latest/` now serves `dccstatusline-arm64-macos` (51 KB Mach-O arm64 PIE) beside 67 KB x86_64 / 130 KB aarch64 musl-static, every checksum verified on download; README's macOS URL went 404 → 200 | — |
