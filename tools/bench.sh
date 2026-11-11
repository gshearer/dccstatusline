#!/bin/sh
# Measures a full refresh (spawn, payload on stdin, line out, exit) the way
# README.md reports it, and prints the figures its "Measured, not promised"
# table quotes. Linux only: the static build needs musl-gcc.
#
# It times the latest static musl release against this tree built the same
# way, run by run in alternation, so a change shows up as the gap between the
# two. A dynamic glibc build and an empty static program are timed on their
# own; the empty one is the cost of exec itself.
#
# usage: tools/bench.sh [work-dir]
#   Reusing a work dir rebuilds the tree incrementally instead of from scratch.
#   RUNS (5000), REPEATS (3), CPU (2, the core it pins to) and TAG (latest, or
#   a release such as v0.0.7) tune it.

set -eu

[ $# -le 1 ] || { echo "usage: tools/bench.sh [work-dir]" >&2; exit 2; }

root=$(cd "$(dirname "$0")/.." && pwd)
runs=${RUNS:-5000}
repeats=${REPEATS:-3}
cpu=${CPU:-2}
tag=${TAG:-latest}

for tool in cc musl-gcc meson ninja curl sha256sum strip; do
  command -v "$tool" >/dev/null 2>&1 || { echo "bench.sh: needs $tool" >&2; exit 1; }
done

case "$(uname -s)/$(uname -m)" in
  Linux/x86_64)              asset=dccstatusline-x86_64-linux-musl ;;
  Linux/aarch64|Linux/arm64) asset=dccstatusline-aarch64-linux-musl ;;
  *)
    echo "bench.sh: no static release binary for $(uname -s) $(uname -m)" >&2
    exit 1 ;;
esac

if [ "$tag" = latest ]; then
  base=https://github.com/gshearer/dccstatusline/releases/latest/download
else
  base=https://github.com/gshearer/dccstatusline/releases/download/$tag
fi

if [ $# -eq 1 ]; then
  mkdir -p "$1"
  work=$(cd "$1" && pwd)
else
  work=$(mktemp -d "${TMPDIR:-/tmp}/dccbench.XXXXXX")
fi

cd "$work"
mkdir -p norepo/a/b/c

if command -v git >/dev/null 2>&1 &&
   git -C norepo/a/b/c rev-parse --git-dir >/dev/null 2>&1; then
  echo "bench.sh: $work is inside a git repository; pick a work dir outside one" >&2
  exit 1
fi

# Pinning to one core keeps the scheduler from moving runs between cores.
pinned()
{
  if command -v taskset >/dev/null 2>&1; then taskset -c "$cpu" "$@"; else "$@"; fi
}

echo "fetching $asset ($tag)" >&2
curl -fsSL -o "$asset" "$base/$asset"
curl -fsSL -o "$asset.sha256" "$base/$asset.sha256"
sha256sum -c "$asset.sha256" >/dev/null
chmod +x "$asset"
cp "$asset" release

# Build output goes to build.log, shown only when a step fails.
quiet()
{
  "$@" > build.log 2>&1 || { cat build.log >&2; exit 1; }
}

echo "building this tree: static musl, dynamic glibc" >&2
[ -d musl/meson-info ] ||
  quiet env CC=musl-gcc meson setup musl "$root" -Doptimization=2 -Ddebug=false \
    -Db_lto=true -Dc_link_args=-static
[ -d glibc/meson-info ] ||
  quiet meson setup glibc "$root" -Doptimization=2 -Ddebug=false -Db_lto=true
quiet ninja -C musl src/dccstatusline
quiet ninja -C glibc src/dccstatusline bench
strip -o local musl/src/dccstatusline
cp glibc/src/dccstatusline dynamic
cp glibc/bench bench
printf 'int main(void){return 0;}\n' > empty.c
musl-gcc -static -O2 -o empty empty.c
strip empty

# A Claude Code status payload that fills every section. The binary looks for
# .git from the payload's cwd, not its own, so the cwd is what makes one
# payload a repository refresh and the other a walk all the way to /.
now=$(date +%s)
payload()
{
  cat <<EOF
{"hook_event_name":"Status","session_id":"3f6c1a2e-9b7d-4e21-a5c8-0d4f2b8e7a19","transcript_path":"/home/you/.claude/projects/acme/3f6c1a2e-9b7d-4e21-a5c8-0d4f2b8e7a19.jsonl","cwd":"$1","model":{"id":"claude-opus-5-5","display_name":"Opus"},"workspace":{"current_dir":"$1","project_dir":"$1"},"version":"2.1.312","output_style":{"name":"default"},"cost":{"total_cost_usd":1.8342,"total_duration_ms":2843117,"total_api_duration_ms":612004,"total_lines_added":214,"total_lines_removed":37},"exceeds_200k_tokens":false,"effort":{"level":"high"},"context_window":{"total_input_tokens":87333,"total_output_tokens":14021,"context_window_size":200000,"used_percentage":43.7,"remaining_percentage":56.3},"rate_limits":{"five_hour":{"used_percentage":24.0,"resets_at":$((now + 9000))},"seven_day":{"used_percentage":41.0,"resets_at":$((now + 260000))}}}
EOF
}
payload "$root/src" > repo.json
payload "$work/norepo/a/b/c" > norepo.json

export DCCSTATUSLINE_CONFIG=/nonexistent
version=$(./release --version)
./release < repo.json > release.out
./local < repo.json > local.out
cmp -s release.out local.out ||
  echo "note: this tree renders the payload differently from $version" >&2

# measure <scenario> <payload> <binary> [binary]: REPEATS bench runs, raw output
# to results.txt, and one "scenario binary median faults" line per binary and
# repeat to medians.txt.
: > results.txt
: > medians.txt
measure()
{
  scenario=$1 json=$2
  shift 2
  echo "timing $scenario: $*" >&2
  i=1

  while [ "$i" -le "$repeats" ]; do
    pinned ./bench -n "$runs" -p "$json" "$@" | tee -a results.txt |
      awk -v s="$scenario" '{
        for(i = 1; i < NF; i++)
        {
          if($i == "med") med = $(i + 1)
          if($i == "faults") faults = $(i + 1)
        }
        sub("^\\./", "", $1)
        print s, $1, med, faults
      }' >> medians.txt
    i=$((i + 1))
  done
}

measure repo    repo.json   ./release ./local
measure no-repo norepo.json ./release ./local
measure repo    repo.json   ./dynamic
measure floor   repo.json   ./empty

if command -v strace >/dev/null 2>&1; then
  syscalls()
  {
    strace -o strace.txt "$1" < "$2" >/dev/null
    grep -vc '^+++' strace.txt
  }
  sc="release $(syscalls ./release repo.json) (no repo: $(syscalls ./release norepo.json)), dynamic $(syscalls ./dynamic repo.json)"
else
  sc="(install strace to count them)"
fi

machine=$(sed -n 's/^model name[[:space:]]*: //p' /proc/cpuinfo 2>/dev/null | head -n 1)

echo
echo "$version release against this tree, $runs runs x $repeats, ${machine:-$(uname -m)}"
echo
awk '{
  k = $1 " " $2
  if(!(k in med)) order[n++] = k
  med[k] = med[k] sprintf("%7.1f", $3)
  faults[k] += $4
  count[k]++
}
END {
  printf "%-8s %-8s %-28s %s\n", "cwd", "binary", "median us, each repeat", "faults"
  for(i = 0; i < n; i++)
  {
    k = order[i]
    split(k, part, " ")
    printf "%-8s %-8s %-28s %6.1f\n", part[1], part[2], med[k], faults[k] / count[k]
  }
}' medians.txt
echo
echo "syscalls  $sc"
awk -v r="$(wc -c < release)" -v l="$(wc -c < local)" 'BEGIN {
  printf "size      release %d bytes (%.0f KB), local %d bytes\n", r, r / 1024, l
}'
echo "raw       $work/results.txt"
