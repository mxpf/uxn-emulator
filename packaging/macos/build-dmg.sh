#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
	printf '%s\n' "usage: $0 Mote.app Mote.dmg" >&2
	exit 64
fi

if [ "$(uname -s)" != "Darwin" ]; then
	printf '%s\n' 'The macOS disk image can only be built on macOS.' >&2
	exit 69
fi

app_path=$1
dmg_path=$2

case "$app_path" in
	*.app) ;;
	*)
		printf '%s\n' 'The app path must end in .app.' >&2
		exit 64
		;;
esac

case "$dmg_path" in
	*.dmg) ;;
	*)
		printf '%s\n' 'The disk-image path must end in .dmg.' >&2
		exit 64
		;;
esac

if [ ! -d "$app_path" ]; then
	printf '%s\n' "The app bundle is missing: $app_path" >&2
	exit 66
fi

stage_root=$(mktemp -d "${TMPDIR:-/tmp}/mote-dmg.XXXXXX")
trap 'rm -rf "$stage_root"' EXIT HUP INT TERM
volume_path="$stage_root/Mote"

mkdir -p "$volume_path"
ditto "$app_path" "$volume_path/Mote.app"
ln -s /Applications "$volume_path/Applications"
mkdir -p "$(dirname -- "$dmg_path")"
rm -f "$dmg_path"
hdiutil create -quiet -volname 'Mote' -srcfolder "$volume_path" \
	-format UDZO "$dmg_path"

if [ ! -s "$dmg_path" ]; then
	printf '%s\n' 'The disk image was not created.' >&2
	exit 74
fi

printf '%s\n' "Created $dmg_path"
