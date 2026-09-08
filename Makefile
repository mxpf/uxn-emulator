CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -Iinclude

BIN := bin/uxncli
EMU_BIN := bin/uxnemu
CONSTELLATION_BIN := bin/constellation-v0
TEST_BIN := build/test_uxn
CONSTELLATION_TEST_BIN := build/test_constellation
REAL_ROM_TEST_BIN := build/test_real_roms
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

.PHONY: all check test constellation compatibility verify-online real-roms example assembler lesson-memory clean

all: $(BIN) $(EMU_BIN)

$(BIN): src/main.c $(ROM_SOURCE) $(CORE_SOURCES) include/uxn.h include/varvara.h include/rom.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c $(ROM_SOURCE) $(CORE_SOURCES) -o $@

$(EMU_BIN): src/emu.c $(ROM_SOURCE) $(CORE_SOURCES) include/uxn.h include/varvara.h include/rom.h | bin
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) src/emu.c $(ROM_SOURCE) $(CORE_SOURCES) $(SDL_LIBS) -o $@

$(CONSTELLATION_BIN): src/constellation_main.c src/constellation.c src/uxn.c include/constellation.h include/uxn.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) src/constellation_main.c src/constellation.c src/uxn.c -o $@

$(TEST_BIN): tests/test_uxn.c $(CORE_SOURCES) include/uxn.h include/varvara.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_uxn.c $(CORE_SOURCES) -o $@

$(CONSTELLATION_TEST_BIN): tests/test_constellation.c src/constellation.c src/uxn.c include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_constellation.c src/constellation.c src/uxn.c -o $@

$(REAL_ROM_TEST_BIN): tests/test_real_roms.c $(ROM_SOURCE) $(CORE_SOURCES) include/uxn.h include/varvara.h include/rom.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_real_roms.c $(ROM_SOURCE) $(CORE_SOURCES) -o $@

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

test: $(TEST_BIN) $(CONSTELLATION_TEST_BIN)
	./$(TEST_BIN)
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

GARDEN_SOURCES := src/garden.c src/constellation.c src/uxn.c
GARDEN_ROMS := build/garden-view.rom build/garden-world.rom

build/garden-%.rom: examples/garden-%.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@

bin/garden: src/garden_sdl.c $(GARDEN_SOURCES) include/garden.h include/constellation.h include/uxn.h | bin
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) src/garden_sdl.c $(GARDEN_SOURCES) $(SDL_LIBS) -o $@

.PHONY: garden
garden: bin/garden $(GARDEN_ROMS)
	./bin/garden

build/test_garden: tests/test_garden.c $(GARDEN_SOURCES) include/garden.h include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_garden.c $(GARDEN_SOURCES) -o $@

.PHONY: garden-check
garden-check: bin/garden build/test_garden $(GARDEN_ROMS)
	./build/test_garden
	SDL_VIDEODRIVER=dummy ./bin/garden --script EEEEESSSSG --screenshot build/garden.bmp

check: all test $(PIXEL_ROM)
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./$(EMU_BIN) \
		--frames 2 --screenshot build/offline-screen.bmp $(PIXEL_ROM)
	test -s build/offline-screen.bmp
	@printf '%s\n' 'Offline window check: pass'

compatibility: $(BIN) $(EMU_BIN)
	./tests/compatibility.sh

verify-online: compatibility real-roms

real-roms: $(BIN) $(EMU_BIN) $(ASSEMBLER_ROM) $(REAL_ROM_TEST_BIN)
	./tests/real-roms.sh

assembler: $(ASSEMBLER_ROM)

example: $(HELLO_ROM)
	./$(BIN) $(HELLO_ROM)

lesson-memory: $(LESSON_MEMORY_BIN)
	./$(LESSON_MEMORY_BIN)

clean:
	rm -rf bin build
