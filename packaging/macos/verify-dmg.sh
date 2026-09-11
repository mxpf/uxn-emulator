#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
	printf '%s\n' "usage: $0 Uxn-Emulator.dmg pixel.rom" >&2
	exit 64
fi

if [ "$(uname -s)" != "Darwin" ]; then
	printf '%s\n' 'The macOS disk image can only be checked on macOS.' >&2
	exit 69
fi

case $1 in
	/*) dmg_path=$1 ;;
	*) dmg_path=$(pwd)/$1 ;;
esac
case $2 in
	/*) rom_path=$2 ;;
	*) rom_path=$(pwd)/$2 ;;
esac

if [ ! -s "$dmg_path" ]; then
	printf '%s\n' "The disk image is missing: $dmg_path" >&2
	exit 66
fi
if [ ! -s "$rom_path" ]; then
	printf '%s\n' "The test ROM is missing: $rom_path" >&2
	exit 66
fi

check_root=$(mktemp -d "${TMPDIR:-/tmp}/uxn-emulator-dmg-check.XXXXXX")
mount_path="$check_root/mounted"
copy_path="$check_root/Applications/Uxn Emulator.app"
mounted=0

cleanup()
{
	status=$?
	trap - EXIT HUP INT TERM
	if [ "$mounted" -eq 1 ]; then
		hdiutil detach -quiet "$mount_path" >/dev/null 2>&1 || true
	fi
	rm -rf "$check_root"
	exit "$status"
}
trap cleanup EXIT HUP INT TERM

mkdir -p "$mount_path" "$(dirname -- "$copy_path")"
hdiutil attach -quiet -readonly -nobrowse -mountpoint "$mount_path" "$dmg_path"
mounted=1

mounted_app="$mount_path/Uxn Emulator.app"
applications_link="$mount_path/Applications"
if [ ! -d "$mounted_app" ]; then
	printf '%s\n' 'The mounted image does not contain Uxn Emulator.app.' >&2
	exit 65
fi
if [ ! -L "$applications_link" ] || [ "$(readlink "$applications_link")" != /Applications ]; then
	printf '%s\n' 'The Applications shortcut is missing or incorrect.' >&2
	exit 65
fi
entry_count=$(find "$mount_path" -mindepth 1 -maxdepth 1 | wc -l | tr -d ' ')
if [ "$entry_count" -ne 2 ]; then
	printf '%s\n' 'The mounted image contains unexpected top-level items.' >&2
	exit 65
fi

codesign --verify --deep --strict "$mounted_app"
if otool -L "$mounted_app/Contents/MacOS/uxnemu" | \
	grep -E '/(opt|usr/local)/.*libSDL2'; then
	printf '%s\n' 'The packaged emulator still uses an external SDL2 library.' >&2
	exit 65
fi

ditto "$mounted_app" "$copy_path"
codesign --verify --deep --strict "$copy_path"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
	"$copy_path/Contents/MacOS/Uxn Emulator" \
	--frames 2 --screenshot "$check_root/copied-app-screen.bmp" "$rom_path"
if [ ! -s "$check_root/copied-app-screen.bmp" ]; then
	printf '%s\n' 'The app copied from the disk image did not draw its test screen.' >&2
	exit 65
fi

hdiutil detach -quiet "$mount_path"
mounted=0
if hdiutil info | grep -F "$mount_path" >/dev/null; then
	printf '%s\n' 'The disk image remained mounted after detaching.' >&2
	exit 65
fi

printf '%s\n' 'macOS disk image check: pass'
