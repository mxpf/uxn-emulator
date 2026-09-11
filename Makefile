CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -Iinclude

BIN := bin/uxncli
EMU_BIN := bin/uxnemu
CONSTELLATION_BIN := bin/constellation-v0
TEST_BIN := build/test_uxn
EMU_INPUT_TEST_BIN := build/test_emu_input
CONSTELLATION_TEST_BIN := build/test_constellation
REAL_ROM_TEST_BIN := build/test_real_roms
LEFT_WORKFLOW_TEST_BIN := build/test_left_workflow
LESSON_MEMORY_BIN := build/lesson-memory
PIXEL_ROM := build/pixel.rom
ASSEMBLER_ROM := build/drifblim.rom
HELLO_ROM := build/hello.rom
CORE_SOURCES := src/uxn.c src/varvara.c src/varvara_screen.c \
	src/varvara_file.c src/varvara_datetime.c src/varvara_audio.c \
	src/varvara_exec.c
ROM_SOURCE := src/rom.c
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)
MACOS_APP := dist/Uxn Emulator.app
MACOS_ARCHIVE := dist/Uxn-Emulator-macOS-$(shell uname -m).zip
MACOS_DMG := dist/Uxn-Emulator-macOS-$(shell uname -m).dmg

.PHONY: all app app-check package dmg dmg-check check test constellation compatibility verify-online real-roms example assembler lesson-memory clean

all: $(BIN) $(EMU_BIN)

app: $(EMU_BIN) packaging/macos/build-app.sh packaging/macos/launch.sh packaging/macos/Info.plist assets/branding/uxn-app-icon-v1.png README.md LICENSE
	sh ./packaging/macos/build-app.sh "$(EMU_BIN)" "$(MACOS_APP)"

package: app
	rm -f "$(MACOS_ARCHIVE)"
	ditto -c -k --norsrc --keepParent "$(MACOS_APP)" "$(MACOS_ARCHIVE)"
	@printf '%s\n' 'Created $(MACOS_ARCHIVE)'

dmg: app packaging/macos/build-dmg.sh
	sh ./packaging/macos/build-dmg.sh "$(MACOS_APP)" "$(MACOS_DMG)"

$(BIN): src/main.c $(ROM_SOURCE) $(CORE_SOURCES) include/uxn.h include/varvara.h include/rom.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c $(ROM_SOURCE) $(CORE_SOURCES) -o $@

$(EMU_BIN): src/emu.c $(ROM_SOURCE) $(CORE_SOURCES) include/uxn.h include/varvara.h include/rom.h | bin
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) src/emu.c $(ROM_SOURCE) $(CORE_SOURCES) $(SDL_LIBS) -o $@

$(CONSTELLATION_BIN): src/constellation_main.c src/constellation.c src/uxn.c include/constellation.h include/uxn.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) src/constellation_main.c src/constellation.c src/uxn.c -o $@

$(TEST_BIN): tests/test_uxn.c $(ROM_SOURCE) $(CORE_SOURCES) include/uxn.h include/varvara.h include/rom.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_uxn.c $(ROM_SOURCE) $(CORE_SOURCES) -o $@

$(EMU_INPUT_TEST_BIN): tests/test_emu_input.c src/emu.c $(ROM_SOURCE) $(CORE_SOURCES) include/uxn.h include/varvara.h include/rom.h | build
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) tests/test_emu_input.c $(ROM_SOURCE) $(CORE_SOURCES) $(SDL_LIBS) -o $@

$(CONSTELLATION_TEST_BIN): tests/test_constellation.c src/constellation.c src/uxn.c include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_constellation.c src/constellation.c src/uxn.c -o $@

$(REAL_ROM_TEST_BIN): tests/test_real_roms.c $(ROM_SOURCE) $(CORE_SOURCES) include/uxn.h include/varvara.h include/rom.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_real_roms.c $(ROM_SOURCE) $(CORE_SOURCES) -o $@

$(LEFT_WORKFLOW_TEST_BIN): tests/test_left_workflow.c src/emu.c $(ROM_SOURCE) $(CORE_SOURCES) include/uxn.h include/varvara.h include/rom.h | build
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) tests/test_left_workflow.c $(ROM_SOURCE) $(CORE_SOURCES) $(SDL_LIBS) -o $@

$(LESSON_MEMORY_BIN): lessons/01-memory.c | build
	$(CC) $(CFLAGS) lessons/01-memory.c -o $@

$(PIXEL_ROM): examples/pixel.rom.hex | build
	xxd -r -p $< $@

$(ASSEMBLER_ROM): | build
	curl -fsS https://wiki.xxiivv.com/etc/drifblim.rom.txt -o build/drifblim.rom.hex
	xxd -r -p build/drifblim.rom.hex $@

$(HELLO_ROM): examples/hello.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@

bin build:
	mkdir -p $@

test: $(TEST_BIN) $(EMU_INPUT_TEST_BIN) $(CONSTELLATION_TEST_BIN)
	./$(TEST_BIN)
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./$(EMU_INPUT_TEST_BIN)
	./$(CONSTELLATION_TEST_BIN)

build/constellation-%.rom: examples/constellation-%.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@

constellation: $(CONSTELLATION_BIN) $(CONSTELLATION_TEST_BIN) build/constellation-ping.rom build/constellation-pong.rom
	./$(CONSTELLATION_BIN) build/constellation-ping.rom build/constellation-pong.rom
	./$(CONSTELLATION_TEST_BIN) build/constellation-ping.rom build/constellation-pong.rom

.PHONY: constellation-recovery
constellation-recovery: $(CONSTELLATION_BIN) $(CONSTELLATION_TEST_BIN) build/constellation-burst.rom build/constellation-collect.rom
	./$(CONSTELLATION_BIN) build/constellation-burst.rom build/constellation-collect.rom
	./$(CONSTELLATION_TEST_BIN) --recovery build/constellation-burst.rom build/constellation-collect.rom

check: all test $(PIXEL_ROM)
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./$(EMU_BIN) \
		--frames 2 --screenshot build/offline-screen.bmp $(PIXEL_ROM)
	test -s build/offline-screen.bmp
	@printf '%s\n' 'Offline window check: pass'

app-check: app $(PIXEL_ROM) | build
	test -s "$(MACOS_APP)/Contents/Resources/UxnEmulator.icns"
	test "$$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIconFile' "$(MACOS_APP)/Contents/Info.plist")" = "UxnEmulator.icns"
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
		"$(MACOS_APP)/Contents/MacOS/Uxn Emulator" \
		--frames 1 --screenshot build/app-wait-screen.bmp
	test -s build/app-wait-screen.bmp
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
		"$(MACOS_APP)/Contents/MacOS/Uxn Emulator" \
		--frames 2 --screenshot build/app-screen.bmp $(PIXEL_ROM)
	test -s build/app-screen.bmp
	codesign --verify --deep --strict "$(MACOS_APP)"
	! otool -L "$(MACOS_APP)/Contents/MacOS/uxnemu" | \
		grep -E '/(opt|usr/local)/.*libSDL2'
	@printf '%s\n' 'macOS app check: pass'

dmg-check: dmg $(PIXEL_ROM) packaging/macos/verify-dmg.sh | build
	sh ./packaging/macos/verify-dmg.sh "$(MACOS_DMG)" "$(PIXEL_ROM)"

compatibility: $(BIN) $(EMU_BIN)
	./tests/compatibility.sh

verify-online: compatibility real-roms

real-roms: $(BIN) $(EMU_BIN) $(ASSEMBLER_ROM) $(REAL_ROM_TEST_BIN) $(LEFT_WORKFLOW_TEST_BIN)
	./tests/real-roms.sh

assembler: $(ASSEMBLER_ROM)

example: $(HELLO_ROM)
	./$(BIN) $(HELLO_ROM)

lesson-memory: $(LESSON_MEMORY_BIN)
	./$(LESSON_MEMORY_BIN)

clean:
	rm -rf bin build dist
