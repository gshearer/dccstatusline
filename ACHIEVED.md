# ACHIEVED.md

Ledger of shipped chunks — one row each. Optional verbose ship notes live in
`achieved/YYYY-MM.md` shards; find a section with `grep -n '^## <CHUNK-ID>'`.

| chunk | date | commit | outcome | shard |
|---|---|---|---|---|
| C1 · Scaffolding | 2026-08-14 | 6ad591c | meson/ninja skeleton with hardened flags, buildnum counter + version.h generation, minimal always-exit-0 binary, LICENSE/DESIGN.md/BUILD.md | — |
| C2 · Core primitives | 2026-08-14 | d653739 | util (sv/saturating sbuf/thousands/abbrev), color (3 spellings + SGR), fmt (templates, swallow rule); suites green in dev/ASan/release | — |
| C3 · Parsers | 2026-08-14 | 691e6e0 | json pull cursor (iterative skip, in-place shrink-only unescape), payload map (presence flags, window table), INI config (quote/comment dialect, per-key tolerance, XDG) | — |
