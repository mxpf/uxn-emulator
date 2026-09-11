#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
	printf '%s\n' "usage: $0 uxnemu Mote.app" >&2
	exit 64
fi

if [ "$(uname -s)" != "Darwin" ]; then
	printf '%s\n' 'The macOS app bundle can only be built on macOS.' >&2
	exit 69
fi

binary_path=$1
app_path=$2
script_path=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_path=$(CDPATH= cd -- "$script_path/../.." && pwd)
icon_source="$project_path/assets/branding/mote-app-icon.png"

case "$app_path" in
	*.app) ;;
	*)
		printf '%s\n' 'The app output path must end in .app.' >&2
		exit 64
		;;
esac

if [ ! -x "$binary_path" ]; then
	printf '%s\n' "The emulator binary is missing: $binary_path" >&2
	exit 66
fi

if [ ! -f "$icon_source" ]; then
	printf '%s\n' "The app icon source is missing: $icon_source" >&2
	exit 66
fi

sdl_path=$(otool -L "$binary_path" | awk '/libSDL2[^ ]*\.dylib/ { print $1; exit }')
if [ -z "$sdl_path" ] || [ ! -f "$sdl_path" ]; then
	printf '%s\n' 'Could not find the SDL2 library used by uxnemu.' >&2
	exit 69
fi
sdl_name=$(basename -- "$sdl_path")
sdl3_path=
sdl3_name=
if strings "$sdl_path" | grep -q '@loader_path/libSDL3.dylib'; then
	sdl3_libdir=$(pkg-config --variable=libdir sdl3 2>/dev/null || true)
	sdl3_path="$sdl3_libdir/libSDL3.dylib"
	if [ ! -f "$sdl3_path" ]; then
		printf '%s\n' \
			'Could not find SDL3, which this SDL2 compatibility library uses.' >&2
		exit 69
	fi
	sdl3_name=libSDL3.dylib
fi

rm -rf "$app_path"
mkdir -p "$app_path/Contents/MacOS"
mkdir -p "$app_path/Contents/Frameworks"
mkdir -p "$app_path/Contents/Resources"

icon_temp=$(mktemp -d "${TMPDIR:-/tmp}/mote-icon.XXXXXX")
trap 'rm -rf "$icon_temp"' EXIT HUP INT TERM
iconset_path="$icon_temp/Mote.iconset"
mkdir -p "$iconset_path"

sips -z 16 16 "$icon_source" --out "$iconset_path/icon_16x16.png" >/dev/null
sips -z 32 32 "$icon_source" --out "$iconset_path/icon_16x16@2x.png" >/dev/null
sips -z 32 32 "$icon_source" --out "$iconset_path/icon_32x32.png" >/dev/null
sips -z 64 64 "$icon_source" --out "$iconset_path/icon_32x32@2x.png" >/dev/null
sips -z 128 128 "$icon_source" --out "$iconset_path/icon_128x128.png" >/dev/null
sips -z 256 256 "$icon_source" --out "$iconset_path/icon_128x128@2x.png" >/dev/null
sips -z 256 256 "$icon_source" --out "$iconset_path/icon_256x256.png" >/dev/null
sips -z 512 512 "$icon_source" --out "$iconset_path/icon_256x256@2x.png" >/dev/null
sips -z 512 512 "$icon_source" --out "$iconset_path/icon_512x512.png" >/dev/null
sips -z 1024 1024 "$icon_source" --out "$iconset_path/icon_512x512@2x.png" >/dev/null
iconutil -c icns "$iconset_path" -o "$icon_temp/Mote.icns"

cp "$script_path/Info.plist" "$app_path/Contents/Info.plist"
cp "$script_path/launch.sh" "$app_path/Contents/MacOS/Mote"
cp "$binary_path" "$app_path/Contents/MacOS/uxnemu"
cp "$sdl_path" "$app_path/Contents/Frameworks/$sdl_name"
if [ -n "$sdl3_path" ]; then
	cp -L "$sdl3_path" "$app_path/Contents/Frameworks/$sdl3_name"
fi
cp README.md LICENSE "$app_path/Contents/Resources/"
cp "$icon_temp/Mote.icns" "$app_path/Contents/Resources/Mote.icns"

chmod 0755 "$app_path/Contents/MacOS/Mote"
chmod 0755 "$app_path/Contents/MacOS/uxnemu"
chmod u+w "$app_path/Contents/MacOS/uxnemu"
chmod u+w "$app_path/Contents/Frameworks/$sdl_name"
if [ -n "$sdl3_name" ]; then
	chmod u+w "$app_path/Contents/Frameworks/$sdl3_name"
fi

install_name_tool -change "$sdl_path" \
	"@executable_path/../Frameworks/$sdl_name" \
	"$app_path/Contents/MacOS/uxnemu"
install_name_tool -id "@rpath/$sdl_name" \
	"$app_path/Contents/Frameworks/$sdl_name"
codesign --force --sign - --timestamp=none \
	"$app_path/Contents/Frameworks/$sdl_name"
codesign --force --sign - --timestamp=none \
	"$app_path/Contents/MacOS/uxnemu"
codesign --force --sign - --timestamp=none "$app_path"

printf '%s\n' "Created $app_path"
