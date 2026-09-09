#ifndef SKETCHPAD_H
#define SKETCHPAD_H
#include "constellation_runner.h"

/* Application choices only, not a shared screen or input ABI. */
enum { SKETCH_COLS = 32, SKETCH_ROWS = 24, SKETCH_SCALE = 4,
	SKETCH_WIDTH = 128, SKETCH_HEIGHT = 96, SKETCH_PAPER = 0x1000 };
typedef struct {
	ConstellationRunner runner;
	uint32_t pixels[SKETCH_WIDTH * SKETCH_HEIGHT];
	unsigned x, y, mode; /* Snapshot from ROM: 0 move, 1 draw, 2 erase. */
	uint64_t commits, turns, events, trace_digest;
	bool failed;
	char error[160];
} Sketchpad;

/* Start only zero/freed, stable-address storage; source bytes are copied. */
bool sketch_start(Sketchpad *s, RunnerRom rom);
bool sketch_open(Sketchpad *s); /* Local/embedded file, at the app boundary. */
void sketch_free(Sketchpad *s);
RoutedInputResult sketch_input(Sketchpad *s, unsigned action);
bool sketch_step(Sketchpad *s);
/* UI policy: admit one action and complete one turn synchronously. */
bool sketch_apply(Sketchpad *s, unsigned action);
#endif
