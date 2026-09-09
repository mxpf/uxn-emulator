#include "square_demo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
fail(SquareSession *s, const char *message)
{
	s->failed = true; snprintf(s->error, sizeof(s->error), "%s", message); return false;
}

/* Cartridge-local drawing, explicitly attached to one port of one node. */
static void
draw_write(Uxn *u, uint8_t port, uint8_t value, void *context)
{
	SquareMachine *m = context;
	static const uint32_t colors[] = {0xfff4f1e9, 0xffdc5c36};
	(void)port;
	if(!value || m->failed) return;
	unsigned color = u->devices[0x24];
	if(color >= sizeof(colors) / sizeof(*colors)) { m->failed = true; return; }
	if(value == 1) {
		for(unsigned i = 0; i < SQUARE_WIDTH * SQUARE_HEIGHT; i++) m->pixels[i] = colors[color];
	} else if(value == 2) {
		unsigned x = u->devices[0x20], y = u->devices[0x21], w = u->devices[0x22], h = u->devices[0x23];
		if(x + w > SQUARE_WIDTH || y + h > SQUARE_HEIGHT || !w || !h) { m->failed = true; return; }
		for(unsigned row = y; row < y + h; row++)
			for(unsigned col = x; col < x + w; col++) m->pixels[row * SQUARE_WIDTH + col] = colors[color];
		m->frames++;
	} else m->failed = true;
}

static SquareMachine *
machine(char *error, size_t capacity)
{
	const RoutedRoute routes[] = {{ROUTED_NONE,1,0}, {0,1,1}, {1,1,2}};
	const char *roms[] = {"build/routes-square-input.rom", "build/routes-square-world.rom", "build/routes-square-draw.rom"};
	SquareMachine *m = calloc(1, sizeof(*m));
	if(!m) { snprintf(error, capacity, "Not enough memory to create the game."); return NULL; }
	RunnerRom images[3] = {{0}};
	uint8_t *buffers[3] = {0};
	for(unsigned i = 0; i < 3; i++) {
		buffers[i] = malloc(UXN_RAM_SIZE - UXN_ROM_START + 1);
		if(!buffers[i]) { snprintf(error, capacity, "Not enough memory to load ROM %u.", i); goto bad; }
		FILE *file = fopen(roms[i], "rb");
		if(!file) { snprintf(error, capacity, "Cannot open %s. Run from the repository root.", roms[i]); goto bad; }
		size_t length = fread(buffers[i], 1, UXN_RAM_SIZE - UXN_ROM_START + 1, file);
		bool ok = !ferror(file); if(fclose(file) != 0) ok = false;
		if(!ok) { snprintf(error, capacity, "Cannot read %s.", roms[i]); goto bad; }
		images[i] = (RunnerRom){buffers[i], length};
	}
	const RunnerDevice drawing = {2, 0x25, 0x25, NULL, draw_write, m};
	const RunnerDefinition definition = {images, 3, routes, 3, &drawing, 1, 10000};
	for(unsigned i = 0; i < SQUARE_WIDTH * SQUARE_HEIGHT; i++) m->pixels[i] = 0xfff4f1e9;
	RunnerDiagnostic diagnostic;
	if(!runner_start(&m->runner, &definition, &diagnostic)) {
		if(diagnostic.node == ROUTED_NONE) snprintf(error, capacity, "%s", runner_diagnostic_text(&diagnostic));
		else snprintf(error, capacity, "Node %u: %s", diagnostic.node, runner_diagnostic_text(&diagnostic));
		goto bad;
	}
	for(unsigned i = 0; i < 3; i++) free(buffers[i]);
	return m;
bad:
	for(unsigned i = 0; i < 3; i++) free(buffers[i]);
	runner_free(&m->runner);
	free(m); return NULL;
}

static void
free_machine(SquareMachine *m)
{
	if(m) { runner_free(&m->runner); free(m); }
}

static bool
consume(SquareSession *s, bool replay)
{
	SquareMachine *m = replay ? s->play : s->live;
	RoutedEvent batch[CONSTELLATION_TRACE_CAPACITY]; size_t count;
	if(m->failed || m->runner.host.nodes[0].uxn.ram[0] || m->runner.host.nodes[1].uxn.ram[2] || m->runner.host.fault != ROUTED_OK)
		return fail(s, "Drawing or message execution failed; recording is incomplete.");
	if(!routed_take_trace(&m->runner.host, batch, CONSTELLATION_TRACE_CAPACITY, &count)) return fail(s, "Cannot consume trace.");
	if(replay) {
		if(s->play_event + count > s->event_count || memcmp(s->events + s->play_event, batch, count * sizeof(*batch)) != 0)
			return fail(s, "Replay trace differs from recording.");
		s->play_event += count;
	} else {
		if(s->event_count + count > SQUARE_EVENTS) return fail(s, "Recording trace capacity exceeded.");
		memcpy(s->events + s->event_count, batch, count * sizeof(*batch)); s->event_count += count;
	}
	return true;
}

bool
square_init(SquareSession *s)
{
	memset(s, 0, sizeof(*s));
	s->live = machine(s->error, sizeof(s->error));
	if(!s->live) { s->failed = true; return false; }
	return consume(s, false);
}

void
square_free(SquareSession *s)
{
	free_machine(s->live); free_machine(s->play); s->live = s->play = NULL;
}

/* Freeze a complete recording before its bounds are crossed. A full archive
 * can still be replayed; no partial operation is hidden or discarded. */
static bool
room(SquareSession *s)
{
	if(s->input_count == SQUARE_INPUTS || s->turn == SQUARE_TURNS ||
		s->event_count + CONSTELLATION_TRACE_CAPACITY > SQUARE_EVENTS) s->sealed = true;
	return !s->sealed;
}

bool
square_input(SquareSession *s, uint8_t action)
{
	if(s->failed) return false;
	if(action < 1 || action > 4) return fail(s, "Invalid arrow action.");
	if(!room(s)) return true;
	RoutedInputResult result = routed_input(&s->live->runner.host, 1, &action, 1);
	if(result != ROUTED_INPUT_ACCEPTED && result != ROUTED_INPUT_FULL) return fail(s, "Input submission failed.");
	s->inputs[s->input_count++] = (SquareInput){s->turn, action, result};
	if(result == ROUTED_INPUT_FULL) s->rejected++;
	return consume(s, false);
}

bool
square_step(SquareSession *s)
{
	if(s->failed) return false;
	if(!room(s)) return true;
	if(!routed_step(&s->live->runner.host)) return fail(s, "Routed execution stopped.");
	s->turn++; return consume(s, false);
}

bool
square_same(const SquareMachine *a, const SquareMachine *b)
{
	if(a->failed != b->failed || a->frames != b->frames || memcmp(a->pixels, b->pixels, sizeof(a->pixels)) ||
		a->runner.host.fault != b->runner.host.fault || a->runner.host.trace_exhausted != b->runner.host.trace_exhausted ||
		a->runner.host.booted != b->runner.host.booted || a->runner.host.next_node != b->runner.host.next_node ||
		a->runner.host.trace_sequence != b->runner.host.trace_sequence || a->runner.host.trace_count != b->runner.host.trace_count ||
		memcmp(a->runner.host.links, b->runner.host.links, 3 * sizeof(*a->runner.host.links))) return false;
	for(unsigned i = 0; i < 3; i++) {
		const RoutedNode *x = &a->runner.host.nodes[i], *y = &b->runner.host.nodes[i];
		if(memcmp(x->uxn.ram, y->uxn.ram, sizeof(x->uxn.ram)) ||
			memcmp(x->uxn.devices, y->uxn.devices, sizeof(x->uxn.devices)) ||
			memcmp(&x->uxn.working, &y->uxn.working, sizeof(UxnStack)) ||
			memcmp(&x->uxn.return_stack, &y->uxn.return_stack, sizeof(UxnStack)) ||
			x->uxn.instructions != y->uxn.instructions || x->loaded != y->loaded ||
			x->source != y->source || x->writable != y->writable ||
			x->next_incoming != y->next_incoming || x->next_writable != y->next_writable ||
			x->prefer_receive != y->prefer_receive) return false;
	}
	return true;
}

bool
square_replay_begin(SquareSession *s)
{
	if(s->failed) return false;
	s->sealed = true; s->replay_done = false;
	free_machine(s->play); s->play = machine(s->error, sizeof(s->error));
	s->play_turn = 0; s->play_input = s->play_event = 0;
	if(!s->play) { s->failed = true; return false; }
	return consume(s, true);
}

bool
square_replay_step(SquareSession *s)
{
	if(s->failed || !s->play) return false;
	if(s->replay_done) return true;
	while(s->play_input < s->input_count && s->inputs[s->play_input].turn == s->play_turn) {
		const SquareInput *input = &s->inputs[s->play_input++];
		if(routed_input(&s->play->runner.host, 1, &input->action, 1) != input->result) return fail(s, "Replay input result differs.");
		if(!consume(s, true)) return false;
	}
	if(s->play_turn == s->turn) {
		if(s->play_input != s->input_count || s->play_event != s->event_count || !square_same(s->live, s->play))
			return fail(s, "Replay final machine state or drawing differs.");
		s->replay_done = true; return true;
	}
	if(!routed_step(&s->play->runner.host)) return fail(s, "Replay execution stopped.");
	s->play_turn++; return consume(s, true);
}
