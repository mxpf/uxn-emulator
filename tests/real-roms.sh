#!/bin/sh

set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
rom_dir=$(mktemp -d /tmp/uxn-real-roms.XXXXXX)
trap 'rm -rf -- "$rom_dir"' EXIT HUP INT TERM

apps="dexe left nebu noodle nasu oekaki turye m_pc"

cd "$rom_dir"

for app in $apps; do
	curl -fsS "https://wiki.xxiivv.com/etc/${app}.tal.txt" -o "${app}.tal"
	if ! "$project_dir/bin/uxncli" "$project_dir/build/drifblim.rom" \
		"${app}.tal" "${app}.rom" >"${app}.assemble.log" 2>&1; then
		cat "${app}.assemble.log" >&2
		exit 1
	fi
	if [ ! -s "${app}.rom" ]; then
		cat "${app}.assemble.log" >&2
		printf '%s\n' "${app}: assembler did not create a ROM" >&2
		exit 1
	fi

	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
		"$project_dir/bin/uxnemu" --frames 5 \
		--screenshot "${app}.bmp" "${app}.rom"
	test -s "${app}.bmp"

	colors=$(xxd -p -s 138 -c 4 "${app}.bmp" | sort -u | wc -l | tr -d ' ')
	if [ "$colors" -lt 2 ]; then
		printf '%s\n' "${app}: screen is blank" >&2
		exit 1
	fi
	printf '%s\n' "${app}: assembled and drew a screen"
done

curl -fsS "https://wiki.xxiivv.com/etc/cccc.tal.txt" -o cccc.tal
if ! "$project_dir/bin/uxncli" "$project_dir/build/drifblim.rom" \
	cccc.tal cccc.rom >cccc.assemble.log 2>&1; then
	cat cccc.assemble.log >&2
	exit 1
fi
test -s cccc.rom

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
	"$project_dir/bin/uxnemu" --frames 5 --screenshot left-file.bmp \
	left.rom left.tal
if cmp -s left.bmp left-file.bmp; then
	printf '%s\n' 'left: opening a text file did not change the screen' >&2
	exit 1
fi
printf '%s\n' 'left: opened its Uxntal source as text'

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
	"$project_dir/bin/uxnemu" --frames 5 --screenshot dexe-file.bmp \
	dexe.rom nasu.rom
if cmp -s dexe.bmp dexe-file.bmp; then
	printf '%s\n' 'dexe: opening a binary file did not change the screen' >&2
	exit 1
fi
printf '%s\n' 'dexe: opened the Nasu ROM as bytes'

"$project_dir/build/test_real_roms" left.rom noodle.rom cccc.rom
