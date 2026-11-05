---
name: theme
description: Show dccstatusline's preset themes in color and pick one. Use when the user wants a theme or color scheme for dccstatusline, names a preset (catppuccin, nord, gruvbox, tokyonight, dracula, onedark, rosepine, kanagawa, solarized), or asks what the themes look like.
---

# Pick a dccstatusline theme

A theme is a compiled-in preset of colors, selected with one `theme = <name>`
line. dccstatusline re-reads its config on every refresh, so the preset the
user picks shows up on the real status line as soon as the turn ends.

When the user types `/dccstatusline:theme` on its own, the plugin opens its
picker before this skill runs: a panel with a box previewing a sample
line over a row each for the theme, separator, profile and glyphs. The arrow
keys move between the rows, and Enter moves a row to its next value and redraws
the preview. The theme row goes round every preset, and the preview takes that
preset's colors. The
separator row chooses the separator (`│` by default), and the profile row
chooses none, basic or detailed. With none, the sections and formats are the
config's own. A profile replaces them all: basic draws every section trimmed
to the figures, and detailed adds the token counts and the reset times. Neither
draws a glyph until the glyphs row below turns the profile's `glyphs` key on:
`⎇` before the branch and, under detailed, `↻` before both reset times. Its
third value, `nerdfont`, draws U+E0A0 and U+F021 in their place and the model
as a filled pill between U+E0B6 and U+E0B4, and the separator row includes a
thin arrow, U+E0B1. These are private-use characters: empty boxes without a
Nerd Font, which the picker's preview shows. With no profile the glyphs row is
dimmed, reads `needs a profile`, and does nothing. Each row starts at what the
config already sets. Enter on the Apply button comes back as
`/dccstatusline:theme <name> [sep=<glyph>|sep=U+E0B1] [profile=none|basic|detailed] [glyphs=off|on|nerdfont]`:
the theme row's preset, then each option the user set or the config already
set. It names `glyphs` only under a profile. Esc closes the picker with nothing
picked.

So you run with no preset named only when there was no picker: the user asked
in their own words, Claude Code has no mods, or the picker couldn't open.
Then the preview is the colored `--themes` output in the transcript, and a
menu picks: a menu can't show color, and the status line hides while it's
open.

## 0. Find the binary and the config

Follow steps 0 and 1 of `${CLAUDE_PLUGIN_ROOT}/skills/configure/SKILL.md`.
They give you `BIN` and the config path, or a reason to stop.

If the `statusLine` command passes `--theme`, that flag wins over the
config's `theme` key, so a pick written to the config won't show. Tell the
user, and offer to drop the flag from the command before applying. The same
goes for `--profile` when the pick names `profile`, and for `--glyphs` when
it names `glyphs`.

If the user already named a preset, skip to step 2.

## 1. Show the presets and ask

Run `"BIN" --themes </dev/null`. It prints one sample line per preset, side by
side in real color, and that output is what the user compares. If it's
collapsed, ctrl+o expands it. The first word of each line is the preset's name.

If the output is a single dimmed line reading `dccstatusline v… (build #…)`
instead, the binary predates themes and ignored the flag. Stop and offer an
update: `/dccstatusline:setup` if `BIN` is `${CLAUDE_PLUGIN_DATA}/dccstatusline`,
otherwise the binary they installed by hand.

Then ask with one AskUserQuestion call. Ask nothing about glyphs or the
profile, but a request in the user's own words, such as "nord, basic" or
"basic with glyphs" or "detailed with nerd font glyphs", maps to the same
options as the picker's. The glyphs come only with a profile: asked for them without one, say so and offer basic or
detailed, and point a request to drop the `↻` from the config's own formats to
`/dccstatusline:configure`. If you can't show a menu, list the names and ask
in one line instead. The options are the presets other than the one in use, in
`--themes` order, first four. Describe each by its palette in a few words. A
menu holds only four, so name the rest in the question text: Other lets the
user type any name. Add no previews: the pane can't show color.

Put the step 2 question about keys in the same call when it applies. If the
user dismisses the menu, stop. Nothing has been written.

## 2. Apply

**Keys in the way.** The theme applies where its line sits, and every key after
it wins. If the config holds keys that paint over a preset (`separator`,
`profile`, `format`, or any `fg`, `bg`, `*_fg`, or `*_bg`), ask whether to
remove them.
Leave out the keys the options below write: the pick already chose those.
When the pick sets a profile, count `[statusline] sections` too: the profile
replaces it and every `format`, so they no longer do anything.
Name them, and offer "Remove them (Recommended)" and "Keep them". A pasted
`--print-config` restates all of them, which is the usual reason a theme seems
to do nothing. Ask once a session. If the user dismisses the question, stop.

Before the first write, copy any existing config to `<config>.bak`, and note
whether there was no config. Later picks in the same session don't refresh the
backup, so undo always returns to where the user started.

Remove the keys if the user chose to. Then write the theme as the first key
the config reads:

- If the first section header is `[statusline]`, put `theme = <name>` on the
  line right after it.
- Otherwise insert `[statusline]` and `theme = <name>` right before the first
  section header, or at the end of a file with no header. Create the file and
  its directory if needed.

If a `theme` key already exists, change that line instead. Never leave a
second `theme` key: only the first one counts.

Then write each option the pick names, changing the key if the config has it:

- `sep=<glyph>`: `separator = " <glyph> "` on the line right after `theme =`.
- `profile=basic` or `profile=detailed`: `profile = <value>` on the line
  right after `theme =`, or after the `separator` line if there is one.
- `glyphs=on` or `glyphs=nerdfont`: `glyphs = <value>` on the line right after
  `profile =`.

A pick in the user's own words had no preview to show a missing font's boxes.
When one names `nerdfont` or the thin arrow, say in your closing line that it
needs a Nerd Font.

`sep=U+E0B1` is a private-use character, which tool output hides and you can't
type reliably. Write the `separator` line with any glyph, then swap in its bytes
from the shell, and confirm they landed:

```sh
sed -i.tmp "s/^\([[:space:]]*separator[[:space:]]*=\).*/\1 \" $(printf '\356\202\261') \"/" "<config>" && rm "<config>.tmp"
grep -c "$(printf '\356\202\261')" "<config>"
```

A `separator` line that reads as blank between its quotes already holds one.
Change or remove it by its key with `sed`, never by matching its text.

`sep=│`, `profile=none` and `glyphs=off` are the defaults, named when the
config set something else: remove the lines the other value writes instead. `profile=none` also removes any `glyphs =` line in
`[statusline]`, which does nothing without a profile.

## 3. End the turn

End with one short line, such as "nord is on your status line now. Say
another to pick again, or undo to restore your previous config." The status
line redraws once the turn ends.

## 4. Respond

- **another**: ask the step 1 question again, running `--themes` first if its
  output has scrolled out of view. If the picker made the last pick, say
  instead that `/dccstatusline:theme` opens it again.
- **undo**: restore `<config>.bak` over the config, or delete the config if
  there was none before step 2.
- **Anything else**: the pick stands. If step 2 made a `.bak`, tell them it
  holds their previous config. Mention that `/dccstatusline:configure` can
  tune single colors on top of the theme.
