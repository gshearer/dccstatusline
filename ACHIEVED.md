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
