#include "sketchpad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool fail(Sketchpad *s, const char *message)
{ s->failed = true; snprintf(s->error, sizeof(s->error), "%s", message); return false; }

static void
present(Uxn *u, uint8_t port, uint8_t value, void *context)
{
	Sketchpad *s = context; (void)port;
	if(!value || s->failed) return;
	unsigned address = ((unsigned)u->devices[0x40] << 8) | u->devices[0x41];
	unsigned x = u->devices[0x42], y = u->devices[0x43], mode = u->devices[0x45];
	if(value != 1 || address > UXN_RAM_SIZE - SKETCH_COLS * SKETCH_ROWS || x >= SKETCH_COLS || y >= SKETCH_ROWS || mode > 2) {
		fail(s, "Invalid sketch display commit."); return;
	}
	for(unsigned i = 0; i < SKETCH_COLS * SKETCH_ROWS; i++)
		if(u->ram[address + i] > 1) { fail(s, "Invalid paper pixel."); return; }
	/* Validate the complete snapshot before replacing the visible surface. */
	for(unsigned row = 0; row < SKETCH_HEIGHT; row++) for(unsigned col = 0; col < SKETCH_WIDTH; col++) {
		bool ink = u->ram[address + (row / SKETCH_SCALE) * SKETCH_COLS + col / SKETCH_SCALE];
		bool cursor = col / SKETCH_SCALE == x && row / SKETCH_SCALE == y &&
			(col % SKETCH_SCALE == 0 || row % SKETCH_SCALE == 0 || col % SKETCH_SCALE == 3 || row % SKETCH_SCALE == 3);
		s->pixels[row * SKETCH_WIDTH + col] = cursor ? 0xffd58b36 : ink ? 0xff20354b : 0xfff4f1e9;
	}
	memcpy(s->paper, u->ram + address, sizeof(s->paper));
	s->x = x; s->y = y; s->mode = mode; s->commits++;
}

/* A bounded, read-only document stream, attached only to this app. */
static uint8_t document_read(Uxn *u, uint8_t port, void *context)
{
	Sketchpad *s = context; (void)u;
	if(port == 0x51) return s->importing;
	return s->importing && s->incoming_offset < sizeof(s->incoming)
		? s->incoming[s->incoming_offset++] : 0;
}

static void hash_number(uint64_t *hash, uint64_t n)
{
	for(unsigned i = 0; i < 8; i++, n >>= 8) *hash = (*hash ^ (uint8_t)n) * UINT64_C(1099511628211);
}
static bool
consume(Sketchpad *s)
{
	RoutedEvent batch[CONSTELLATION_TRACE_CAPACITY]; size_t count;
	if(!routed_take_trace(&s->runner.host, batch, CONSTELLATION_TRACE_CAPACITY, &count)) return fail(s, "Cannot consume sketch trace.");
	/* Bounded diagnostic fingerprint, not an archive or a universal recorder. */
	for(size_t i = 0; i < count; i++) {
		const RoutedEvent *e = &batch[i];
		hash_number(&s->trace_digest, e->sequence); hash_number(&s->trace_digest, e->kind);
		hash_number(&s->trace_digest, e->reason); hash_number(&s->trace_digest, e->from);
		hash_number(&s->trace_digest, e->to); hash_number(&s->trace_digest, e->selector);
		hash_number(&s->trace_digest, e->message.length);
		for(unsigned j = 0; j < e->message.length; j++) hash_number(&s->trace_digest, e->message.data[j]);
	}
	s->events += count;
	if(s->failed) return false;
	if(s->runner.host.fault != ROUTED_OK) return fail(s, "Sketch ROM execution failed.");
	return true;
}

void sketch_free(Sketchpad *s)
{ if(s) { runner_free(&s->runner); memset(s, 0, sizeof(*s)); } }

bool
sketch_start(Sketchpad *s, RunnerRom rom)
{
	if(!s || s->runner.host.nodes) return false;
	memset(s, 0, sizeof(*s)); s->trace_digest = UINT64_C(14695981039346656037);
	const RoutedRoute route = {ROUTED_NONE,7,0};
	const RunnerDevice devices[] = {{0,0x44,0x44,NULL,present,s}, {0,0x50,0x51,document_read,NULL,s}};
	const RunnerDefinition definition = {&rom,1,&route,1,devices,2,10000};
	RunnerDiagnostic diagnostic;
	if(!runner_start(&s->runner, &definition, &diagnostic)) return fail(s, runner_diagnostic_text(&diagnostic));
	if(!consume(s) || !s->commits) {
		if(!s->failed) fail(s, "Sketch ROM did not present a surface.");
		runner_free(&s->runner); return false;
	}
	return true;
}

bool
sketch_open(Sketchpad *s)
{
	if(!s || s->runner.host.nodes) return false;
	uint8_t *bytes = malloc(UXN_RAM_SIZE - UXN_ROM_START + 1);
	if(!bytes) return fail(s, "Not enough memory to load the sketch ROM.");
	FILE *file = fopen("build/sketchpad.rom", "rb");
	if(!file) { free(bytes); return fail(s, "Cannot open build/sketchpad.rom. Run from the repository root."); }
	size_t length = fread(bytes, 1, UXN_RAM_SIZE - UXN_ROM_START + 1, file);
	bool ok = !ferror(file); if(fclose(file) != 0) ok = false;
	if(ok) ok = sketch_start(s, (RunnerRom){bytes,length});
	else fail(s, "Cannot read the sketch ROM.");
	free(bytes); return ok;
}

RoutedInputResult
sketch_input(Sketchpad *s, unsigned action)
{
	if(!s || s->failed || !s->runner.host.nodes) return ROUTED_INPUT_FAULT;
	if(action < 1 || action > 6) return ROUTED_INPUT_INVALID;
	uint8_t byte = (uint8_t)action;
	RoutedInputResult result = routed_input(&s->runner.host, 7, &byte, 1);
	return consume(s) ? result : ROUTED_INPUT_FAULT;
}

bool
sketch_step(Sketchpad *s)
{
	if(!s || s->failed || !s->runner.host.nodes) return false;
	bool ok = routed_step(&s->runner.host); s->turns++;
	return consume(s) && ok;
}

bool
sketch_apply(Sketchpad *s, unsigned action)
{ return sketch_input(s, action) == ROUTED_INPUT_ACCEPTED && sketch_step(s); }

bool sketch_save(const Sketchpad *s, uint8_t *bytes, size_t length)
{
	if(!s || s->failed || !s->runner.host.nodes || !bytes || length != SKETCH_FILE_SIZE ||
		s->runner.host.links[0].queue.count || s->importing) return false;
	memset(bytes, 0, length); memcpy(bytes, "SKETCH01", 8);
	bytes[8] = SKETCH_COLS; bytes[9] = SKETCH_ROWS;
	bytes[10] = s->x; bytes[11] = s->y; bytes[12] = s->mode;
	memcpy(bytes + 16, s->paper, sizeof(s->paper)); return true;
}

bool sketch_load(Sketchpad *s, const uint8_t *bytes, size_t length)
{
	if(!s || s->failed || !s->runner.host.nodes || !bytes || length != SKETCH_FILE_SIZE ||
		s->runner.host.links[0].queue.count || s->importing) return false;
	if(memcmp(bytes, "SKETCH01", 8) || bytes[8] != SKETCH_COLS || bytes[9] != SKETCH_ROWS ||
		bytes[10] >= SKETCH_COLS || bytes[11] >= SKETCH_ROWS || bytes[12] > 2 || bytes[13] || bytes[14] || bytes[15]) return false;
	for(size_t i = 16; i < length; i++) if(bytes[i] > 1) return false;
	memcpy(s->incoming, bytes + 10, 3);
	memcpy(s->incoming + 3, bytes + 16, sizeof(s->paper));
	s->incoming_offset = 0; s->importing = true;
	uint8_t action = 7; uint64_t commits = s->commits;
	bool ok = routed_input(&s->runner.host, 7, &action, 1) == ROUTED_INPUT_ACCEPTED && sketch_step(s);
	ok = ok && s->incoming_offset == sizeof(s->incoming) && s->commits == commits + 1;
	s->importing = false; s->incoming_offset = 0; memset(s->incoming, 0, sizeof(s->incoming));
	return ok ? true : fail(s, "Sketch ROM could not open the document.");
}

bool sketch_read_file(Sketchpad *s, const char *path)
{
	if(!path) return false;
	FILE *f = fopen(path, "rb"); if(!f) return false;
	uint8_t bytes[SKETCH_FILE_SIZE + 1]; size_t n = fread(bytes, 1, sizeof(bytes), f);
	bool ok = !ferror(f); if(fclose(f)) ok = false;
	return ok && sketch_load(s, bytes, n);
}

bool sketch_write_file(const Sketchpad *s, const char *path)
{
	uint8_t bytes[SKETCH_FILE_SIZE];
	if(!path || !sketch_save(s, bytes, sizeof(bytes))) return false;
	FILE *f = fopen(path, "wbx"); if(!f) return false;
	bool ok = fwrite(bytes, 1, sizeof(bytes), f) == sizeof(bytes);
	if(fclose(f)) ok = false;
	if(!ok) remove(path); /* Only the new, exclusively-created incomplete file. */
	return ok;
}
