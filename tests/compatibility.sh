#!/bin/sh

set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
compat_dir=$(mktemp -d /tmp/uxn-compat.XXXXXX)
trap 'rm -rf -- "$compat_dir"' EXIT HUP INT TERM

cd "$compat_dir"

curl -fsS https://wiki.xxiivv.com/etc/drifblim.rom.txt -o drifblim.hex
xxd -r -p drifblim.hex drifblim.rom

for name in opctest varvara.console varvara.file.test varvara.screen \
	varvara.audio; do
	curl -fsS "https://wiki.xxiivv.com/etc/${name}.tal.txt" -o "${name}.tal"
	"$project_dir/bin/uxncli" drifblim.rom "${name}.tal" "${name}.rom"
done

opcode_output=$("$project_dir/bin/uxncli" opctest.rom)
printf '%s\n' "$opcode_output" | rg -q '^Opc-test: pass$'
printf '%s\n' "$opcode_output" | rg -q '^Jci-wrap: pass$'
if printf '%s\n' "$opcode_output" | rg -q ': fail$'; then
	printf '%s\n' 'The official opcode test reported a failure.' >&2
	exit 1
fi

file_output=$("$project_dir/bin/uxncli" varvara.file.test.rom)
[ "$file_output" = 'File: pass' ]

console_output=$(printf ghi | "$project_dir/bin/uxncli" \
	varvara.console.rom abc def)
[ "$console_output" = 'Console: pass' ]

window_console_output=$(printf ghi | SDL_VIDEODRIVER=dummy \
	SDL_AUDIODRIVER=dummy "$project_dir/bin/uxnemu" --frames 100 \
	varvara.console.rom abc def)
[ "$window_console_output" = 'Console: pass' ]

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
	"$project_dir/bin/uxnemu" --frames 2 --screenshot screen.bmp \
	varvara.screen.rom

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
	"$project_dir/bin/uxnemu" --frames 120 --screenshot audio.bmp \
	varvara.audio.rom
test -s audio.bmp

if command -v sips >/dev/null 2>&1; then
	curl -fsS https://wiki.xxiivv.com/media/generic/varvara_blend.png \
		-o official-screen.png
	sips -s format bmp official-screen.png --out official-screen.bmp >/dev/null
	sips -s format bmp screen.bmp --out actual-screen.bmp >/dev/null
	dd if=official-screen.bmp of=official-pixels bs=1 skip=138 2>/dev/null
	dd if=actual-screen.bmp of=actual-pixels bs=1 skip=138 2>/dev/null
	cmp official-pixels actual-pixels
	printf '%s\n' 'Official screen pixels: pass'
else
	printf '%s\n' 'Official screen pixels: skipped (the sips image tool is unavailable)'
fi

printf '%s\n' 'Official opcode test: pass'
printf '%s\n' 'Official Console test: pass'
printf '%s\n' 'Official File test: pass'
printf '%s\n' 'Windowed Console and Audio paths: pass'
