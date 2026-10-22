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
MYSTERY_SOURCES := src/mystery.c $(RUNNER_SOURCES)
MYSTERY_ROMS := build/mystery-presentation.rom build/mystery-director.rom build/mystery-deduction.rom

# Finite independent-progression comparison. Local only; not a public target.
PROGRESSION_ROMS := build/mystery-progression-multi.rom build/mystery-progression-single.rom build/mystery-progression-desk.rom
PROGRESSION_INPUTS := examples/mystery-progression.json examples/mystery-progression.tal examples/mystery-progression-local.tal examples/mystery-progression-remote.tal examples/mystery-room.json examples/mystery-presentation.tal tools/build_mystery.cjs
build/mystery-progression-multi.tal build/mystery-progression-single.tal: $(PROGRESSION_INPUTS) | build
	node tools/build_mystery.cjs $@
build/mystery-progression-multi.rom: build/mystery-progression-multi.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@
build/mystery-progression-single.rom: build/mystery-progression-single.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@
build/test_mystery_progression: tests/test_mystery_progression.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(MYSTERY_SOURCES) -o $@
.PHONY: mystery-progression-check mystery-progression-sanitize
mystery-progression-check: $(PROGRESSION_ROMS) build/test_mystery_progression
	./build/test_mystery_progression
	node tests/mystery_progression_authoring_check.cjs
mystery-progression-sanitize: $(PROGRESSION_ROMS)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_mystery_progression.c $(MYSTERY_SOURCES) -o build/test_mystery_progression_sanitize
	./build/test_mystery_progression_sanitize

build/mystery_progression_multi_probe: tests/mystery_probe.c src/mystery_web.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_VARIANT=MYSTERY_PROGRESSION_MULTI $< $(MYSTERY_SOURCES) -o $@
build/mystery_progression_single_probe: tests/mystery_probe.c src/mystery_web.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_VARIANT=MYSTERY_PROGRESSION_SINGLE $< $(MYSTERY_SOURCES) -o $@
.PHONY: mystery-progression-web mystery-progression-web-check
mystery-progression-web: $(PROGRESSION_ROMS)
	mkdir -p build/mystery-progression-multi-web build/mystery-progression-single-web
	$(EMCC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_VARIANT=MYSTERY_PROGRESSION_MULTI src/mystery_web.c $(MYSTERY_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createMystery -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/mystery-progression-multi.rom --embed-file build/mystery-progression-desk.rom -o build/mystery-progression-multi-web/mystery.js
	$(EMCC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_VARIANT=MYSTERY_PROGRESSION_SINGLE src/mystery_web.c $(MYSTERY_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createMystery -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/mystery-progression-single.rom -o build/mystery-progression-single-web/mystery.js
	node tools/build_mystery_progression_web.cjs build/mystery-progression-multi-web
	node tools/build_mystery_progression_web.cjs build/mystery-progression-single-web
mystery-progression-web-check: mystery-progression-web build/mystery_progression_multi_probe build/mystery_progression_single_probe
	mkdir -p build/mystery-progression-check
	$(EMCC) $(CPPFLAGS) $(CFLAGS) tests/test_mystery_progression.c $(MYSTERY_SOURCES) \
		-sSTACK_SIZE=262144 -sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		--embed-file build/mystery-progression-multi.rom --embed-file build/mystery-progression-desk.rom --embed-file build/mystery-progression-single.rom -o build/mystery-progression-check/check.js
	node build/mystery-progression-check/check.js

# Local, opt-in Last Visitor deduction comparison; excluded from public builds.
KNOWLEDGE_ROMS := build/mystery-knowledge-multi.rom build/mystery-knowledge-single.rom build/mystery-knowledge-witness.rom
KNOWLEDGE_INPUTS := examples/mystery-knowledge.json examples/mystery-knowledge.tal examples/mystery-knowledge-local.tal examples/mystery-knowledge-remote.tal examples/mystery-room.json examples/mystery-presentation.tal tools/build_mystery.cjs
build/mystery-knowledge-multi.tal build/mystery-knowledge-single.tal: $(KNOWLEDGE_INPUTS) | build
	node tools/build_mystery.cjs $@
build/mystery-knowledge-multi.rom: build/mystery-knowledge-multi.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@
build/mystery-knowledge-single.rom: build/mystery-knowledge-single.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@
build/test_mystery_knowledge: tests/test_mystery_knowledge.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(MYSTERY_SOURCES) -o $@
.PHONY: mystery-knowledge-check mystery-knowledge-sanitize
mystery-knowledge-check: $(KNOWLEDGE_ROMS) build/test_mystery_knowledge
	./build/test_mystery_knowledge
	node tests/mystery_knowledge_authoring_check.cjs
mystery-knowledge-sanitize: $(KNOWLEDGE_ROMS)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_mystery_knowledge.c $(MYSTERY_SOURCES) -o build/test_mystery_knowledge_sanitize
	./build/test_mystery_knowledge_sanitize

build/mystery_knowledge_multi_probe: tests/mystery_probe.c src/mystery_web.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_VARIANT=MYSTERY_KNOWLEDGE_MULTI $< $(MYSTERY_SOURCES) -o $@
build/mystery_knowledge_single_probe: tests/mystery_probe.c src/mystery_web.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_VARIANT=MYSTERY_KNOWLEDGE_SINGLE $< $(MYSTERY_SOURCES) -o $@
.PHONY: mystery-knowledge-web mystery-knowledge-web-check
mystery-knowledge-web: $(KNOWLEDGE_ROMS)
	mkdir -p build/mystery-knowledge-multi-web build/mystery-knowledge-single-web
	$(EMCC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_VARIANT=MYSTERY_KNOWLEDGE_MULTI src/mystery_web.c $(MYSTERY_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createMystery -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/mystery-knowledge-multi.rom --embed-file build/mystery-knowledge-witness.rom -o build/mystery-knowledge-multi-web/mystery.js
	$(EMCC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_VARIANT=MYSTERY_KNOWLEDGE_SINGLE src/mystery_web.c $(MYSTERY_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createMystery -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/mystery-knowledge-single.rom -o build/mystery-knowledge-single-web/mystery.js
	node tools/build_mystery_knowledge_web.cjs build/mystery-knowledge-multi-web
	node tools/build_mystery_knowledge_web.cjs build/mystery-knowledge-single-web
mystery-knowledge-web-check: mystery-knowledge-web build/mystery_knowledge_multi_probe build/mystery_knowledge_single_probe
	mkdir -p build/mystery-knowledge-check
	$(EMCC) $(CPPFLAGS) $(CFLAGS) tests/test_mystery_knowledge.c $(MYSTERY_SOURCES) \
		-sSTACK_SIZE=262144 -sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		--embed-file build/mystery-knowledge-multi.rom --embed-file build/mystery-knowledge-witness.rom --embed-file build/mystery-knowledge-single.rom -o build/mystery-knowledge-check/check.js
	node build/mystery-knowledge-check/check.js

build/mystery-presentation.tal build/mystery-deduction.tal: examples/mystery-presentation.tal examples/mystery-deduction.tal examples/mystery-room.json tools/build_mystery.cjs | build
	node tools/build_mystery.cjs $@
build/mystery-deduction.rom: build/mystery-deduction.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@
build/mystery-presentation.rom: build/mystery-presentation.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@
build/mystery-%.rom: examples/mystery-%.tal $(ASSEMBLER_ROM) $(BIN) | build
	./$(BIN) $(ASSEMBLER_ROM) $< $@
bin/mystery: src/mystery_sdl.c $(MYSTERY_SOURCES) include/mystery.h | bin
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) src/mystery_sdl.c $(MYSTERY_SOURCES) $(SDL_LIBS) -o $@
build/test_mystery: tests/test_mystery.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_mystery.c $(MYSTERY_SOURCES) -o $@
.PHONY: mystery mystery-check mystery-web
mystery: bin/mystery $(MYSTERY_ROMS)
	./bin/mystery
mystery-check: bin/mystery build/test_mystery $(MYSTERY_ROMS) build/mystery-single.rom
	./build/test_mystery
	node tests/mystery_authoring_check.cjs
	SDL_VIDEODRIVER=dummy ./bin/mystery --script 12345666789
	SDL_VIDEODRIVER=dummy ./bin/mystery --script q12345qq666789889q
build/mystery_probe: tests/mystery_probe.c src/mystery_web.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/mystery_probe.c $(MYSTERY_SOURCES) -o $@
.PHONY: mystery-web-check
mystery-web-check: mystery-web build/mystery_probe
	mkdir -p build/mystery-check
	$(EMCC) $(CPPFLAGS) $(CFLAGS) tests/mystery_probe.c $(MYSTERY_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createMystery -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/mystery-presentation.rom --embed-file build/mystery-director.rom --embed-file build/mystery-deduction.rom \
		-o build/mystery-check/mystery.js
	node tests/mystery_web_parity.cjs
# Deliberately outside build/web: not part of a Playinghaus release yet.
mystery-web: $(MYSTERY_ROMS)
	mkdir -p build/mystery-web
	$(EMCC) $(CPPFLAGS) $(CFLAGS) src/mystery_web.c $(MYSTERY_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createMystery -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/mystery-presentation.rom --embed-file build/mystery-director.rom --embed-file build/mystery-deduction.rom \
		-o build/mystery-web/mystery.js
	cp web/mystery/index.html web/mystery/style.css web/mystery/app.js build/mystery-web/
	cp web/fonts.css build/mystery-web/
	mkdir -p build/mystery-web/fonts
	cp web/fonts/orbitron-latin.woff2 web/fonts/OFL-Orbitron.txt build/mystery-web/fonts/

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
sketch-web: build/sketchpad.rom web-fonts
	mkdir -p build/web/sketchpad
	$(EMCC) $(CPPFLAGS) $(CFLAGS) src/sketch_web.c $(SKETCH_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createSketch -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32,HEAPU8 \
		--embed-file build/sketchpad.rom -o build/web/sketchpad/sketch.js
	cp web/sketchpad/index.html web/sketchpad/style.css web/sketchpad/app.js build/web/sketchpad/

sketch-web-check: sketch-web build/sketch_probe
	mkdir -p build/sketch-check
	$(EMCC) $(CPPFLAGS) $(CFLAGS) tests/sketch_probe.c $(SKETCH_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createSketch -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32,HEAPU8 \
		--embed-file build/sketchpad.rom -o build/sketch-check/sketch.js
	node tests/sketch_web_parity.cjs

GARDEN_ROMS := build/garden-view.rom build/garden-world.rom
EMCC ?= emcc

.PHONY: square-web playing-web
square-web: $(SQUARE_ROMS) web-fonts
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

.PHONY: web-fonts
web-fonts:
	mkdir -p build/web/fonts
	cp web/fonts.css build/web/
	cp web/fonts/orbitron-latin.woff2 web/fonts/OFL-Orbitron.txt build/web/fonts/

build/square_native_digest: tests/square_native_digest.c src/square_web.c $(SQUARE_SOURCES) include/square_demo.h include/constellation_routes.h include/constellation.h include/uxn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/square_native_digest.c $(SQUARE_SOURCES) -o $@

.PHONY: square-web-check
square-web-check: square-web build/square_native_digest
	node tests/square_web_parity.cjs
	node tests/square_controls.cjs

.PHONY: garden-web garden-web-check garden-measure
garden-web: $(GARDEN_ROMS) web-fonts
	mkdir -p build/web
	$(EMCC) $(CPPFLAGS) $(CFLAGS) src/garden_web.c $(GARDEN_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createGarden -sSTACK_SIZE=262144 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU8,HEAPU32 \
		--embed-file build/garden-view.rom --embed-file build/garden-world.rom \
		-o build/web/garden.js
	cp web/index.html web/home.css web/home.js web/tiny-neighbors-cartridge.png web/no-escape-cartridge.png web/sketchpad-cartridge.png web/playinghaus-chrome.png web/playinghaus-chrome-arc.png web/style.css web/app.js $(GARDEN_ROMS) build/web/
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

# Behavior-equivalent one-machine control. Existing three-ROM targets stay intact.
build/mystery-single.tal: examples/mystery-single.tal examples/mystery-presentation.tal examples/mystery-room.json examples/mystery-question-returned.json tools/build_mystery.cjs | build
	node tools/build_mystery.cjs $@
build/mystery-single.rom: build/mystery-single.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@
bin/mystery-single: src/mystery_sdl.c $(MYSTERY_SOURCES) include/mystery.h | bin
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_SINGLE=1 src/mystery_sdl.c $(MYSTERY_SOURCES) $(SDL_LIBS) -o $@
build/test_mystery_single: tests/test_mystery.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_SINGLE=1 tests/test_mystery.c $(MYSTERY_SOURCES) -o $@
build/mystery_single_probe: tests/mystery_probe.c src/mystery_web.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_SINGLE=1 tests/mystery_probe.c $(MYSTERY_SOURCES) -o $@
.PHONY: mystery-single mystery-single-check mystery-single-web mystery-single-web-check
mystery-single: bin/mystery-single build/mystery-single.rom
	./bin/mystery-single
mystery-single-check: bin/mystery-single build/test_mystery_single build/mystery-single.rom
	./build/test_mystery_single
	SDL_VIDEODRIVER=dummy ./bin/mystery-single --script 12345666789
	SDL_VIDEODRIVER=dummy ./bin/mystery-single --script q12345qq666789889q
mystery-single-web: build/mystery-single.rom
	mkdir -p build/mystery-single-web
	$(EMCC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_SINGLE=1 src/mystery_web.c $(MYSTERY_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createMystery -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/mystery-single.rom -o build/mystery-single-web/mystery.js
	cp web/mystery/index.html web/mystery/style.css web/mystery/app.js build/mystery-single-web/
	cp web/fonts.css build/mystery-single-web/
	mkdir -p build/mystery-single-web/fonts
	cp web/fonts/orbitron-latin.woff2 web/fonts/OFL-Orbitron.txt build/mystery-single-web/fonts/
mystery-single-web-check: mystery-single-web build/mystery_single_probe
	mkdir -p build/mystery-single-check
	$(EMCC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_SINGLE=1 tests/mystery_probe.c $(MYSTERY_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createMystery -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/mystery-single.rom -o build/mystery-single-check/mystery.js
	node tests/mystery_web_parity.cjs ../build/mystery-single-check/mystery.js ./build/mystery_single_probe

build/test_mystery_compare: tests/test_mystery_compare.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_mystery_compare.c $(MYSTERY_SOURCES) -o $@
.PHONY: mystery-compare-check
mystery-compare-check: build/test_mystery_compare $(MYSTERY_ROMS) build/mystery-single.rom
	./build/test_mystery_compare

# Same executable code, a second authored question rule and answer.
build/mystery-single-retained.tal: examples/mystery-single.tal examples/mystery-presentation.tal examples/mystery-room.json examples/mystery-question-retained.json tools/build_mystery.cjs | build
	node tools/build_mystery.cjs $@ examples/mystery-question-retained.json
build/mystery-single-retained.rom: build/mystery-single-retained.tal $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) $< $@

build/test_mystery_rules: tests/test_mystery_rules.c $(MYSTERY_SOURCES) include/mystery.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_SINGLE=1 tests/test_mystery_rules.c $(MYSTERY_SOURCES) -o $@
.PHONY: mystery-rules-check
mystery-rules-check: build/test_mystery_rules build/mystery-single.rom build/mystery-single-retained.rom
	node tests/mystery_rules_check.cjs

.PHONY: mystery-rules-sanitize
mystery-rules-sanitize: build/mystery-single.rom build/mystery-single-retained.rom
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g -fsanitize=address,undefined -DMYSTERY_DEFAULT_SINGLE=1 tests/test_mystery_rules.c $(MYSTERY_SOURCES) -o build/test_mystery_rules_sanitize
	MYSTERY_RULE_TEST=./build/test_mystery_rules_sanitize node tests/mystery_rules_check.cjs

.PHONY: mystery-rules-web mystery-rules-web-check
mystery-rules-web: mystery-single-web build/mystery-single-retained.rom
	mkdir -p build/mystery-retained-web build/mystery-single-check build/mystery-retained-check
	$(EMCC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_SINGLE=1 src/mystery_web.c $(MYSTERY_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createMystery -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/mystery-single-retained.rom@build/mystery-single.rom -o build/mystery-retained-web/mystery.js
	cp web/mystery/index.html web/mystery/style.css web/mystery/app.js web/fonts.css build/mystery-retained-web/
	mkdir -p build/mystery-retained-web/fonts
	cp web/fonts/orbitron-latin.woff2 web/fonts/OFL-Orbitron.txt build/mystery-retained-web/fonts/
	$(EMCC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_SINGLE=1 tests/mystery_probe.c $(MYSTERY_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createMystery -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/mystery-single.rom -o build/mystery-single-check/mystery.js
	$(EMCC) $(CPPFLAGS) $(CFLAGS) -DMYSTERY_DEFAULT_SINGLE=1 tests/mystery_probe.c $(MYSTERY_SOURCES) --no-entry \
		-sMODULARIZE=1 -sEXPORT_NAME=createMystery -sSTACK_SIZE=262144 \
		-sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU32 \
		--embed-file build/mystery-single-retained.rom@build/mystery-single.rom -o build/mystery-retained-check/mystery.js
mystery-rules-web-check: mystery-rules-web build/mystery_single_probe
	node tests/mystery_rules_web_check.cjs

# Isolated account/record experiment, not part of playing-web or Last Visitor.
ACCOUNT_SOURCES := src/account.c $(RUNNER_SOURCES)
ACCOUNT_ROMS := build/account-record.rom build/account-witness.rom build/account-single.rom
build/account-data.stamp: tools/build_account.cjs examples/account.json examples/account-record.tal examples/account-witness.tal examples/account-single.tal | build
	node tools/build_account.cjs
	touch $@
$(ACCOUNT_ROMS): build/account-%.rom: build/account-data.stamp $(ASSEMBLER_ROM) $(BIN)
	./$(BIN) $(ASSEMBLER_ROM) build/account-$*.tal $@
build/test_account: tests/test_account.c $(ACCOUNT_SOURCES) include/account.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_account.c $(ACCOUNT_SOURCES) -o $@
bin/account-study: tools/account_play.c $(ACCOUNT_SOURCES) include/account.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/account_play.c $(ACCOUNT_SOURCES) -o $@
.PHONY: account-check account-sanitize
account-check: $(ACCOUNT_ROMS) build/test_account bin/account-study
	./build/test_account
	./bin/account-study multi 13242
	./bin/account-study single 13242
	node tests/account_authoring_check.cjs
account-sanitize: $(ACCOUNT_ROMS)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g -fsanitize=address,undefined tests/test_account.c $(ACCOUNT_SOURCES) -o build/test_account_sanitize
	./build/test_account_sanitize

.PHONY: account-web account-web-check
account-web: $(ACCOUNT_ROMS)
	mkdir -p build/account-web
	$(EMCC) $(CPPFLAGS) $(CFLAGS) tools/account_play.c $(ACCOUNT_SOURCES) --no-entry -sMODULARIZE=1 -sEXPORT_NAME=createAccount -sSTACK_SIZE=262144 -sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=UTF8ToString --embed-file build/account-record.rom --embed-file build/account-witness.rom --embed-file build/account-single.rom -o build/account-web/account.js
	cp web/account/index.html build/account-web/
account-web-check: account-web
	mkdir -p build/account-check
	$(EMCC) $(CPPFLAGS) $(CFLAGS) tests/test_account.c $(ACCOUNT_SOURCES) -sSTACK_SIZE=262144 -sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=1 --embed-file build/account-record.rom --embed-file build/account-witness.rom --embed-file build/account-single.rom -o build/account-check/check.js
	node build/account-check/check.js
