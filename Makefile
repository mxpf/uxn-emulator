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

ROUTED_ROMS := build/routes-burst.rom build/routes-relay.rom build/routes-collect.rom
build/routes-%.rom: examples/routes-%.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@

build/test_routes: tests/test_routes.c src/constellation_routes.c src/uxn.c include/constellation_routes.h include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_routes.c src/constellation_routes.c src/uxn.c -o $@

.PHONY: constellation-routes
constellation-routes: build/test_routes $(ROUTED_ROMS)
	./build/test_routes

build/test_routes_sustained: tests/test_routes_sustained.c src/constellation_routes.c src/uxn.c include/constellation_routes.h include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_routes_sustained.c src/constellation_routes.c src/uxn.c -o $@

.PHONY: constellation-sustained
constellation-sustained: build/test_routes_sustained build/routes-cycle-burst.rom build/routes-relay.rom build/routes-cycle-collect.rom
	./build/test_routes_sustained

build/test_routes_ring: tests/test_routes_ring.c src/constellation_routes.c src/uxn.c include/constellation_routes.h include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_routes_ring.c src/constellation_routes.c src/uxn.c -o $@

.PHONY: constellation-ring
constellation-ring: build/test_routes_ring build/routes-ring.rom
	./build/test_routes_ring

build/test_routes_input: tests/test_routes_input.c src/constellation_routes.c src/uxn.c include/constellation_routes.h include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_routes_input.c src/constellation_routes.c src/uxn.c -o $@

.PHONY: constellation-input
constellation-input: build/test_routes_input build/routes-input.rom
	./build/test_routes_input

SQUARE_SOURCES := src/square_demo.c src/constellation_runner.c src/constellation_routes.c src/uxn.c
SQUARE_ROMS := build/routes-square-input.rom build/routes-square-world.rom build/routes-square-draw.rom

RUNNER_SOURCES := src/constellation_runner.c src/constellation_routes.c src/uxn.c
build/test_runner: tests/test_runner.c $(RUNNER_SOURCES) include/constellation_runner.h include/constellation_routes.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_runner.c $(RUNNER_SOURCES) -o $@

.PHONY: runner-check runner-web-check
build/test_runner_alloc: tests/test_runner_alloc.c $(RUNNER_SOURCES) include/constellation_runner.h include/constellation_routes.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_runner_alloc.c src/constellation_routes.c src/uxn.c -o $@

runner-check: build/test_runner build/test_runner_alloc $(ROUTED_ROMS)
	./build/test_runner
	./build/test_runner_alloc

runner-web-check: runner-check
	mkdir -p build/runner-web
	$(EMCC) $(CPPFLAGS) $(CFLAGS) tests/test_runner.c $(RUNNER_SOURCES) \
		-sSTACK_SIZE=262144 -sSTACK_OVERFLOW_CHECK=2 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		--embed-file build/routes-burst.rom --embed-file build/routes-relay.rom --embed-file build/routes-collect.rom \
		-o build/runner-web/check.js
	node build/runner-web/check.js
	cp tests/runner-browser.html build/runner-web/index.html

bin/square build/test_square build/test_square_ui build/square_native_digest: include/constellation_runner.h

bin/square: src/square_sdl.c $(SQUARE_SOURCES) include/square_demo.h include/constellation_routes.h include/constellation.h include/uxn.h | bin
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) src/square_sdl.c $(SQUARE_SOURCES) $(SDL_LIBS) -o $@

build/test_square: tests/test_square.c $(SQUARE_SOURCES) include/square_demo.h include/constellation_routes.h include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_square.c $(SQUARE_SOURCES) -o $@

build/test_square_ui: tests/test_square_ui.c src/square_sdl.c $(SQUARE_SOURCES) include/square_demo.h include/constellation_routes.h include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) tests/test_square_ui.c $(SQUARE_SOURCES) $(SDL_LIBS) -o $@

.PHONY: square square-check
square: bin/square $(SQUARE_ROMS)
	./bin/square

build/test_square_startup: tests/test_square_startup.c $(SQUARE_SOURCES) include/square_demo.h include/constellation_runner.h include/constellation_routes.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_square_startup.c $(RUNNER_SOURCES) -o $@

square-check: bin/square build/test_square build/test_square_ui build/test_square_startup $(SQUARE_ROMS)
	./build/test_square
	./build/test_square_ui
	./build/test_square_startup
	SDL_VIDEODRIVER=dummy ./bin/square --script NNEE.SW --verify-replay --screenshot build/square.bmp

GARDEN_SOURCES := src/garden.c src/constellation.c src/uxn.c
SKETCH_SOURCES := src/sketchpad.c $(RUNNER_SOURCES)
SKETCH_HEADERS := include/sketchpad.h include/constellation_runner.h include/constellation_routes.h include/constellation.h include/uxn.h

build/sketchpad.rom: examples/sketchpad.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@

bin/sketchpad: src/sketch_sdl.c $(SKETCH_SOURCES) $(SKETCH_HEADERS) | bin
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) src/sketch_sdl.c $(SKETCH_SOURCES) $(SDL_LIBS) -o $@

build/test_sketch: tests/test_sketch.c $(SKETCH_SOURCES) $(SKETCH_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_sketch.c $(SKETCH_SOURCES) -o $@

build/sketch_probe: tests/sketch_probe.c tests/sketch_fingerprint.h src/sketch_web.c $(SKETCH_SOURCES) $(SKETCH_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/sketch_probe.c $(SKETCH_SOURCES) -o $@

.PHONY: sketchpad sketch-web sketch-check sketch-web-check
sketchpad: bin/sketchpad build/sketchpad.rom
	./bin/sketchpad

sketch-check: build/test_sketch bin/sketchpad build/sketchpad.rom
	./build/test_sketch
	SDL_VIDEODRIVER=dummy ./bin/sketchpad --script 525364

# Build the same application at its public relative path.
sketch-web: build/sketchpad.rom
	mkdir -p build/web/sketchpad
	$(EMCC) $(CPPFLAGS) $(CFLAGS) src/sketch_web.c $(SKETCH_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createSketch -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/sketchpad.rom -o build/web/sketchpad/sketch.js
	cp web/sketchpad/index.html web/sketchpad/style.css web/sketchpad/app.js build/web/sketchpad/

sketch-web-check: sketch-web build/sketch_probe
	mkdir -p build/sketch-check
	$(EMCC) $(CPPFLAGS) $(CFLAGS) tests/sketch_probe.c $(SKETCH_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createSketch -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/sketchpad.rom -o build/sketch-check/sketch.js
	node tests/sketch_web_parity.cjs

GARDEN_ROMS := build/garden-view.rom build/garden-world.rom
EMCC ?= emcc

.PHONY: square-web playing-web
square-web: $(SQUARE_ROMS)
	mkdir -p build/web/no-escape
	$(EMCC) $(CPPFLAGS) $(CFLAGS) src/square_web.c $(SQUARE_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createNoEscape -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU8,HEAPU32 \
		--embed-file build/routes-square-input.rom --embed-file build/routes-square-world.rom --embed-file build/routes-square-draw.rom \
		-o build/web/no-escape/no-escape.js
	cp web/no-escape/index.html web/no-escape/app.js build/web/no-escape/
	cp web/style.css build/web/

playing-web: garden-web square-web sketch-web

build/square_native_digest: tests/square_native_digest.c src/square_web.c $(SQUARE_SOURCES) include/square_demo.h include/constellation_routes.h include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/square_native_digest.c $(SQUARE_SOURCES) -o $@

.PHONY: square-web-check
square-web-check: square-web build/square_native_digest
	node tests/square_web_parity.cjs
	node tests/square_controls.cjs

.PHONY: garden-web garden-web-check garden-measure
garden-web: $(GARDEN_ROMS)
	mkdir -p build/web
	$(EMCC) $(CPPFLAGS) $(CFLAGS) src/garden_web.c $(GARDEN_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createGarden -sSTACK_SIZE=262144 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU8,HEAPU32 \
		--embed-file build/garden-view.rom --embed-file build/garden-world.rom \
		-o build/web/garden.js
	cp web/index.html web/home.css web/tiny-neighbors-cartridge.png web/no-escape-cartridge.png web/sketchpad-cartridge.png web/playinghaus-chrome.png web/style.css web/app.js $(GARDEN_ROMS) build/web/
	mkdir -p build/web/tiny-neighbors
	cp web/tiny-neighbors/index.html build/web/tiny-neighbors/
	cp web/.nojekyll build/web/

build/garden_native_digest: tests/garden_native_digest.c src/garden_web.c $(GARDEN_SOURCES) include/garden.h include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/garden_native_digest.c src/garden_web.c $(GARDEN_SOURCES) -o $@

build/garden_probe: tests/garden_probe.c $(GARDEN_SOURCES) include/garden.h include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/garden_probe.c $(GARDEN_SOURCES) -o $@

garden-web-check: garden-web build/garden_native_digest build/garden_probe
	node tests/garden_web_parity.cjs

garden-measure: garden-web build/garden_probe
	node tests/garden_measure.cjs

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
	sh tests/garden-replay.sh

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
