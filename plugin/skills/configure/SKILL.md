---
name: configure
description: Customize dccstatusline, however it was installed. When dccstatusline is the active status line, use this instead of the built-in status line setup whenever the user wants to change their status line's colors, sections, order, separator, formats, profile (basic or detailed) and its glyphs (plain or Nerd Font), cwd shortening, or reset-time style, or asks what it can show.
---

# Configure dccstatusline

dccstatusline reads one INI file. The plugin does not manage that file, so it
survives plugin updates and uninstalls. The reader skips unknown keys and
sections without a word, and a bad value silently keeps its default. Because
of that, every change is previewed before it is applied.

To try or pick one of the preset themes, use `/dccstatusline:theme` instead.
It shows each preset in color and applies the one you pick.

## 0. Find the active dccstatusline binary

Another tool, such as the built-in `/statusline`, may have pointed
`statusLine` elsewhere. Then editing this config changes nothing on screen,
even though the preview below still shows the change.

Find the `statusLine` setting that wins. The first file that sets it wins:

1. `.claude/settings.local.json` in the project
2. `.claude/settings.json` in the project
3. `$CLAUDE_CONFIG_DIR/settings.json` if that variable is set, otherwise
   `~/.claude/settings.json`

Take the first word of its `command`. If that word's last path component is
`dccstatusline`, it is the binary: expand a leading `~`, and resolve a bare
`dccstatusline` with `command -v`. Call its absolute path `BIN` below. The
user may have installed it by hand or through this plugin. Either way it is
the binary to configure.

If nothing sets `statusLine`, or its command is something else, stop. Tell the
user which file sets it and what it runs, and offer `/dccstatusline:setup` to
switch to dccstatusline. Don't edit the config.

Note any `--theme <name>`, `--palette monochrome|duotone|tritone`,
`--profile <name>` or `--glyphs on|off|nerdfont` after the binary. Each flag
stands in for the config key of the same name and wins over it: `--theme` acts
as if `theme = <name>` were the config's first line, so the config's own
`theme` key is skipped while its color keys still apply on top, `--palette`
lays that theme in its palette and skips the config's `palette` key, `--profile`
beats the config's `profile` key, and `--glyphs` its `glyphs` key. A change to a key a flag
overrides won't show on screen, and the preview below runs without the
flags. When the request touches one, tell the user, and offer to drop the
flag from the command in the file that set it.

## 1. Find the config

First match wins:

1. `$DCCSTATUSLINE_CONFIG`
2. `$XDG_CONFIG_HOME/dccstatusline/config`
3. `~/.config/dccstatusline/config`

Read it if it exists. It is fine if it doesn't: every key is optional.

## 2. Learn the dialect

Run `"BIN" --print-config`. The binary prints its own commented reference:
every section, token, key, and color spelling, set to the compiled-in
defaults. Because it comes from the binary that will read the config, it
documents exactly what that build accepts. Use only keys it documents. If the
user asks for something it can't express, such as a section that doesn't
exist, say so instead of writing a key that would be ignored.

If the output is a single dimmed line reading `dccstatusline v… (build #…)`
rather than a reference, the binary predates `--print-config` (added in
v0.0.4) and ignored the flag. Stop and offer an
update: `/dccstatusline:setup` if `BIN` is
`${CLAUDE_PLUGIN_DATA}/dccstatusline`, otherwise the binary they installed by
hand.

## 3. Draft

Write the proposed config to `${CLAUDE_PLUGIN_DATA}/config.draft`. Start from
the current config, or from an empty file, and change only what the request
needs. Don't paste in the whole reference: defaults don't need restating.

A `separator` that reads as blank between its quotes, or a `format` with a gap
where a glyph should be, holds a private-use glyph such as the thin arrow
U+E0B1, which tool output hides. Make the draft with `cp` and keep that line, or
change it with `sed`: retyping the file loses the glyph. To set U+E0B1, write
its bytes from the shell, `printf '\356\202\261'`. It needs a Nerd Font, so ask
about the font first.

`[statusline] palette = monochrome|duotone|tritone` decides how many of the
theme's three colors the line wears beside its muted color: all three, with
the third on the usage figures (`tritone`, the default), the first and second,
or the first alone.
It lays the theme again where its line sits, so it paints over any color key
between it and `theme`. Keep it on the line right after `theme =`, with the
color keys the user asks for below it. With no theme it does nothing: the
stock colors take no palette.

A profile, set by `profile =` or by `--profile`, is a whole line decided for
the user. It replaces `[statusline] sections` and every `format`, wherever
those keys sit, and leaves the colors, labels, separator, `thousands`, cwd
shortening and reset styles to the config. Both profiles draw all six
sections: basic trims them to the figures
(`~/src/acme │ main │ Opus 5.5 · high │ ctx 43% │ 5h 24% │ 7d 41%`), and detailed adds the
token counts and the reset times. Neither draws a glyph until
`[statusline] glyphs = on` turns them on: `⎇` before the branch and, under
detailed, `↻` before both reset times. `glyphs = nerdfont` draws U+E0A0 and
U+F021 in their place, the model as a filled pill between U+E0B6 and U+E0B4, and
the effort beside it with no `·`: they need a Nerd Font and show as empty boxes without
one, so ask about the font first.
With no profile, the sections and formats are the config's own, glyphs included, and the `glyphs` key does
nothing: to add or drop one there, change the `format`. A Nerd Font glyph goes in
by its bytes, as the arrow does above: `printf '\356\202\240'` for U+E0A0 and
`printf '\357\200\241'` for U+F021. `{model_tag}` draws its own caps. If a profile is set and the
request changes the sections, their order or a format, those keys would do
nothing: ask whether to remove the profile first.

## 4. Preview

Render the current line and the draft with the same representative payload:

```sh
sh "${CLAUDE_PLUGIN_ROOT}/skills/configure/preview.sh" "BIN"
sh "${CLAUDE_PLUGIN_ROOT}/skills/configure/preview.sh" "BIN" "${CLAUDE_PLUGIN_DATA}/config.draft"
```

- Any `dccstatusline: config:` line on stderr is a value that failed to
  parse. Fix it.
- Compare the two outputs, including the escape codes, and confirm the
  requested change shows up. If it doesn't, a key is wrong. Fix it and preview
  again.
- Show the user the draft and describe what changed in the line.

## 5. Apply

On the user's go-ahead, copy any existing config to `<config>.bak`, then move
the draft to the config path, creating the directory if needed. The live
status line picks up the change on its next refresh. No restart is needed.
