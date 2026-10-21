#!/bin/sh
# Renders the status line through <binary> with a representative payload.
# With [config], that file is used instead of the user's own config.
#
# The payload is built here rather than shipped as a file because resets_at
# is an absolute epoch: a fixed value would render the plan sections stale.

set -eu

[ $# -ge 1 ] || { echo "usage: preview.sh <binary> [config]" >&2; exit 2; }

bin=$1
[ $# -ge 2 ] && export DCCSTATUSLINE_CONFIG="$2"

now=$(date +%s)

# $PWD goes into a JSON string: escape what JSON requires, and drop control
# characters, which a path almost never holds and JSON would need as \u00XX.
cwd=$(printf '%s' "$PWD" | tr -d '\000-\037' | sed 's/[\\"]/\\&/g')

"$bin" <<EOF
{
  "cwd": "$cwd",
  "workspace": { "current_dir": "$cwd" },
  "model": { "id": "claude-opus-5-5", "display_name": "Opus" },
  "effort": { "level": "high" },
  "context_window": {
    "total_input_tokens": 84213,
    "total_output_tokens": 3120,
    "context_window_size": 200000
  },
  "rate_limits": {
    "five_hour": { "used_percentage": 23.5, "resets_at": $((now + 9000)) },
    "seven_day": { "used_percentage": 41.0, "resets_at": $((now + 302400)) }
  }
}
EOF
