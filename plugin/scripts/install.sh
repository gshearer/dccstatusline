#!/bin/sh
# Fetches the dccstatusline release binary for this platform into <dest-dir>,
# verifies it against its published sha256, and prints the installed path.
#
# Re-running it is how the binary is updated. The new binary replaces the old
# one with a single rename, so a status line refreshing mid-install runs either
# the old file or the new one, never half of either.

set -eu

[ $# -eq 1 ] || { echo "usage: install.sh <dest-dir>" >&2; exit 2; }

dest=$1
base=https://github.com/gshearer/dccstatusline/releases/latest/download

case "$(uname -s)/$(uname -m)" in
  Linux/x86_64)              asset=dccstatusline-x86_64-linux-musl ;;
  Linux/aarch64|Linux/arm64) asset=dccstatusline-aarch64-linux-musl ;;
  Darwin/arm64)              asset=dccstatusline-arm64-macos ;;
  *)
    echo "no release binary for $(uname -s) $(uname -m); build from source:" >&2
    echo "  https://github.com/gshearer/dccstatusline#quick-start" >&2
    exit 1 ;;
esac

if command -v sha256sum >/dev/null 2>&1; then
  sha() { sha256sum "$1" | cut -d' ' -f1; }
else
  sha() { shasum -a 256 "$1" | cut -d' ' -f1; }
fi

mkdir -p "$dest"
dest=$(cd "$dest" && pwd)
tmp="$dest/.dccstatusline.$$"
trap 'rm -f "$tmp" "$tmp.sha256"' EXIT
trap 'exit 1' HUP INT TERM   # dash skips the EXIT trap on a fatal signal

curl -fsSL -o "$tmp" "$base/$asset"
curl -fsSL -o "$tmp.sha256" "$base/$asset.sha256"

# The .sha256 file names the asset, not the temp file: compare hashes only.
want=$(cut -d' ' -f1 < "$tmp.sha256")
got=$(sha "$tmp")
[ "$want" = "$got" ] || { echo "checksum mismatch for $asset" >&2; exit 1; }

chmod +x "$tmp"
version=$("$tmp" --version)
mv -f "$tmp" "$dest/dccstatusline"

echo "$dest/dccstatusline"
echo "$version"
