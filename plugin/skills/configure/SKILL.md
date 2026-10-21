---
name: configure
description: Customize dccstatusline, the status line this plugin installs. When dccstatusline is the active status line, use this instead of the built-in status line setup whenever the user wants to change their status line's colors, sections, order, separator, formats, cwd shortening, or reset-time style, or asks what it can show.
---

# Configure dccstatusline

dccstatusline reads one INI file. The plugin does not manage that file, so it
survives plugin updates and uninstalls. The reader skips unknown keys and
sections without a word, and a bad value silently keeps its default. Because
of that, every change is previewed before it is applied.

## 0. Check that dccstatusline is the active status line

Another tool, such as the built-in `/statusline`, may have pointed
`statusLine` elsewhere. Then editing this config changes nothing on screen,
even though the preview below still shows the change.

Find the `statusLine` setting that wins. The first file that sets it wins:

1. `.claude/settings.local.json` in the project
2. `.claude/settings.json` in the project
3. `$CLAUDE_CONFIG_DIR/settings.json` if that variable is set, otherwise
   `~/.claude/settings.json`

If its `command` is not `${CLAUDE_PLUGIN_DATA}/dccstatusline`, or nothing sets
`statusLine`, stop. Tell the user which file sets it and what it runs, and
offer `/dccstatusline:setup` to switch back. Don't edit the config.

## 1. Find the binary and the config

- Binary: `${CLAUDE_PLUGIN_DATA}/dccstatusline`. If it is missing, run
  `/dccstatusline:setup` first.
- Config path, first match wins:
  1. `$DCCSTATUSLINE_CONFIG`
  2. `$XDG_CONFIG_HOME/dccstatusline/config`
  3. `~/.config/dccstatusline/config`

  Read it if it exists. It is fine if it doesn't: every key is optional.

## 2. Learn the dialect

Read `${CLAUDE_PLUGIN_ROOT}/skills/configure/config.example`. It is the full
commented reference: every section, token, key, and color spelling, set to the
compiled-in defaults. Use only keys it documents. If the user asks for
something it can't express, such as a section that doesn't exist, say so
instead of writing a key that would be ignored.

## 3. Draft

Write the proposed config to `${CLAUDE_PLUGIN_DATA}/config.draft`. Start from
the current config, or from an empty file, and change only what the request
needs. Don't paste in the whole reference: defaults don't need restating.

## 4. Preview

Render the current line and the draft with the same representative payload:

```sh
sh "${CLAUDE_PLUGIN_ROOT}/skills/configure/preview.sh" "${CLAUDE_PLUGIN_DATA}/dccstatusline"
sh "${CLAUDE_PLUGIN_ROOT}/skills/configure/preview.sh" "${CLAUDE_PLUGIN_DATA}/dccstatusline" "${CLAUDE_PLUGIN_DATA}/config.draft"
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
