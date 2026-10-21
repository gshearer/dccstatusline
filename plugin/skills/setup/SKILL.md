---
name: setup
description: Install or update the dccstatusline binary and point Claude Code's statusLine setting at it.
disable-model-invocation: true
---

# Set up dccstatusline

A plugin cannot set `statusLine` itself, so this skill installs the binary and
writes the setting. Re-running it updates the binary in place.

1. Install the release binary for this platform:

   ```sh
   sh "${CLAUDE_PLUGIN_ROOT}/scripts/install.sh" "${CLAUDE_PLUGIN_DATA}"
   ```

   The first line of output is the binary's absolute path and the second is
   its version. If the script fails, show its output verbatim and stop. On an
   unsupported platform, it prints the build-from-source pointer.

2. Read the user settings file: `$CLAUDE_CONFIG_DIR/settings.json` if that
   variable is set, otherwise `~/.claude/settings.json`. If the file does not
   exist, treat it as `{}`.
   - If `statusLine.command` already equals the path from step 1, leave the
     file alone. The update is done.
   - If a different `statusLine` is set, show it to the user and ask before
     replacing it.
   - Otherwise set the following, keeping every other key exactly as it was:

     ```json
     "statusLine": { "type": "command", "command": "<path from step 1>", "padding": 0 }
     ```

   Use the expanded absolute path, never the literal `${CLAUDE_PLUGIN_DATA}`.
   Nothing documents that plugin variables expand inside `statusLine`.

3. Tell the user:
   - The installed version, and that the status line appears on the next
     refresh.
   - The defaults need no config file. To customize colors, sections, or the
     cwd, they can ask for a change in plain words or run
     `/dccstatusline:configure`.
   - Running `/dccstatusline:setup` again updates the binary.
   - Uninstalling the plugin deletes the binary, so they should then remove
     `statusLine` from that settings file.
