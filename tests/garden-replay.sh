#!/bin/sh
# Exercise the public recording/replay interface without a visible window.
set -eu

replay_dir=$(mktemp -d "${TMPDIR:-/tmp}/garden-replay.XXXXXX")
trap 'rm -rf -- "$replay_dir"' EXIT
trap 'exit 1' HUP INT TERM
export SDL_VIDEODRIVER=dummy

./bin/garden --script EEEEESSSSG --record "$replay_dir/walk.txt" \
    --screenshot "$replay_dir/recorded.bmp" > "$replay_dir/recorded.out"
test "$(wc -l < "$replay_dir/walk.txt" | tr -d ' ')" = 11
grep -F 'tick=10 player=7,6 creature=8,7 friend=1 trace=' "$replay_dir/recorded.out" > /dev/null

./bin/garden --replay "$replay_dir/walk.txt" \
    --screenshot "$replay_dir/replayed.bmp" > "$replay_dir/replayed.out"
cmp "$replay_dir/recorded.out" "$replay_dir/replayed.out"
test -s "$replay_dir/recorded.bmp"
cmp "$replay_dir/recorded.bmp" "$replay_dir/replayed.bmp"

# Change the last fingerprint while preserving valid file syntax. This must
# reach the replay comparison, not merely fail parsing or opening the file.
awk 'NR == 11 { $2 = (substr($2, 1, 1) == "0" ? "1" : "0") substr($2, 2) } { print }' \
    "$replay_dir/walk.txt" > "$replay_dir/corrupt.txt"
if ./bin/garden --replay "$replay_dir/corrupt.txt" \
    > "$replay_dir/corrupt.out" 2> "$replay_dir/corrupt.err"; then
    echo 'FAIL: replay accepted a corrupted fingerprint' >&2
    exit 1
fi
grep -F 'replay diverged at tick 10' "$replay_dir/corrupt.err" > /dev/null
test ! -s "$replay_dir/corrupt.out"

echo 'Saved replay passed: matching state/trace summary and screenshot; corrupted fingerprint rejected at tick 10.'
