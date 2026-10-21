---
name: configure
description: Customize dccstatusline, however it was installed. When dccstatusline is the active status line, use this instead of the built-in status line setup whenever the user wants to change their status line's colors, sections, order, separator, formats, cwd shortening, or reset-time style, or asks what it can show.
---

# Configure dccstatusline

dccstatusline reads one INI file. The plugin does not manage that file, so it
survives plugin updates and uninstalls. The reader skips unknown keys and
sections without a word, and a bad value silently keeps its default. Because
of that, every change is previewed before it is applied.

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
