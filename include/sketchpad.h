#ifndef SKETCHPAD_H
#define SKETCHPAD_H
#include "constellation_runner.h"

/* Application choices only, not a shared screen or input ABI. */
enum { SKETCH_COLS = 32, SKETCH_ROWS = 24, SKETCH_SCALE = 4,
	SKETCH_WIDTH = 128, SKETCH_HEIGHT = 96, SKETCH_PAPER = 0x1000,
	SKETCH_FILE_SIZE = 784 };
typedef struct {
	ConstellationRunner runner;
	uint32_t pixels[SKETCH_WIDTH * SKETCH_HEIGHT];
	unsigned x, y, mode; /* Snapshot from ROM: 0 move, 1 draw, 2 erase. */
	uint64_t commits, turns, events, trace_digest;
	bool failed;
	char error[160];
	uint8_t paper[SKETCH_COLS * SKETCH_ROWS]; /* Last validated display commit. */
	uint8_t incoming[771];
	size_t incoming_offset;
	bool importing;
} Sketchpad;

/* Start only zero/freed, stable-address storage; source bytes are copied. */
bool sketch_start(Sketchpad *s, RunnerRom rom);
bool sketch_open(Sketchpad *s); /* Local/embedded file, at the app boundary. */
void sketch_free(Sketchpad *s);
RoutedInputResult sketch_input(Sketchpad *s, unsigned action);
bool sketch_step(Sketchpad *s);
/* UI policy: admit one action and complete one turn synchronously. */
bool sketch_apply(Sketchpad *s, unsigned action);
/* App document, not a VM snapshot. Bad files do not mutate the drawing. */
bool sketch_save(const Sketchpad *s, uint8_t *bytes, size_t length);
bool sketch_load(Sketchpad *s, const uint8_t *bytes, size_t length);
bool sketch_read_file(Sketchpad *s, const char *path);
bool sketch_write_file(const Sketchpad *s, const char *path); /* Never overwrite. */
#endif
