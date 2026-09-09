#include "sketchpad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, x = 16, y = 12, mode;
static uint8_t paper[SKETCH_COLS * SKETCH_ROWS];
#define CHECK(c) do { checks++; if(!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while(0)
static Sketchpad *fresh(void)
{ Sketchpad *s = calloc(1, sizeof(*s)); CHECK(s); CHECK(sketch_open(s)); return s; }
static void dispose(Sketchpad *s) { sketch_free(s); sketch_free(s); free(s); }

static void model(unsigned action)
{
	switch(action) {
	case 1: if(y) y--; break; case 2: if(x < 31) x++; break;
	case 3: if(y < 23) y++; break; case 4: if(x) x--; break;
	case 5: mode = mode == 1 ? 0 : 1; break; case 6: mode = mode == 2 ? 0 : 2; break;
	}
	if(action && mode) paper[y * 32 + x] = mode == 1;
}
static void surface(const Sketchpad *s)
{
	CHECK(!s->failed); CHECK(s->x == x && s->y == y);
	CHECK(s->mode == mode && s->runner.host.nodes[0].uxn.ram[2] == mode);
	CHECK(memcmp(s->runner.host.nodes[0].uxn.ram + SKETCH_PAPER, paper, sizeof(paper)) == 0);
	CHECK(s->runner.host.nodes[0].uxn.ram[0] == x && s->runner.host.nodes[0].uxn.ram[1] == y);
	for(unsigned row = 0; row < 96; row++) for(unsigned col = 0; col < 128; col++) {
		uint32_t expected = paper[row / 4 * 32 + col / 4] ? 0xff20354b : 0xfff4f1e9;
		if(col / 4 == x && row / 4 == y && (col % 4 == 0 || col % 4 == 3 || row % 4 == 0 || row % 4 == 3)) expected = 0xffd58b36;
		CHECK(s->pixels[row * 128 + col] == expected);
	}
}
static void same(const Sketchpad *a, const Sketchpad *b)
{
	CHECK(a->trace_digest == b->trace_digest && a->events == b->events && a->turns == b->turns && a->commits == b->commits);
	CHECK(a->x == b->x && a->y == b->y && a->mode == b->mode && a->failed == b->failed);
	CHECK(memcmp(a->pixels, b->pixels, sizeof(a->pixels)) == 0);
	const RoutedHost *h = &a->runner.host, *k = &b->runner.host;
	CHECK(h->node_count == 1 && k->node_count == 1 && h->route_count == 1 && k->route_count == 1);
	CHECK(h->next_node == k->next_node && h->trace_sequence == k->trace_sequence && h->trace_count == 0 && k->trace_count == 0);
	CHECK(h->fault == k->fault && h->booted == k->booted && h->trace_exhausted == k->trace_exhausted);
	CHECK(memcmp(h->links, k->links, sizeof(*h->links)) == 0);
	const RoutedNode *n = h->nodes, *m = k->nodes;
	CHECK(n->source == m->source && n->writable == m->writable && n->next_incoming == m->next_incoming && n->next_writable == m->next_writable && n->prefer_receive == m->prefer_receive);
	CHECK(memcmp(n->uxn.ram, m->uxn.ram, sizeof(n->uxn.ram)) == 0);
	CHECK(memcmp(n->uxn.devices, m->uxn.devices, sizeof(n->uxn.devices)) == 0);
	CHECK(memcmp(&n->uxn.working, &m->uxn.working, sizeof(UxnStack)) == 0);
	CHECK(memcmp(&n->uxn.return_stack, &m->uxn.return_stack, sizeof(UxnStack)) == 0);
	CHECK(n->uxn.instructions == m->uxn.instructions);
}

static void drawing_and_replay(void)
{
	Sketchpad *live = fresh(), *replay = fresh(); same(live, replay); surface(live);
	/* This recorded test script includes idle gaps; the UI has no recorder. */
	unsigned script[512], count = 0;
	unsigned marks[] = {5,2,2,2,5,3,6,1,4,4,6,4,5,6,5,5,6,6};
	for(size_t i = 0; i < sizeof(marks) / sizeof(*marks); i++) script[count++] = marks[i];
	for(unsigned direction = 1; direction <= 4; direction++) {
		for(unsigned i = 0; i < 40; i++) script[count++] = direction;
		script[count++] = 5;
	}
	uint32_t seed = 47;
	for(unsigned i = 0; i < 200; i++) { seed = seed * 1664525u + 1013904223u; script[count++] = seed % 7; }
	for(unsigned i = 0; i < count; i++) {
		if(script[i]) { CHECK(sketch_input(live, script[i]) == ROUTED_INPUT_ACCEPTED); CHECK(sketch_input(replay, script[i]) == ROUTED_INPUT_ACCEPTED); same(live, replay); }
		CHECK(sketch_step(live)); CHECK(sketch_step(replay)); model(script[i]); same(live, replay); surface(live);
	}
	CHECK(live->runner.host.nodes[0].uxn.working.pointer == 0 && live->runner.host.nodes[0].uxn.return_stack.pointer == 0);
	CHECK(!sketch_open(live)); same(live, replay); /* Refuse live replacement. */
	printf("Sketch model and fresh replay: %u recorded turns, full guest memory and every displayed pixel match.\n", count);
	dispose(live); dispose(replay);
}

static void boundaries(void)
{
	Sketchpad *s = fresh(), *copy = fresh();
	CHECK(sketch_input(s, 0) == ROUTED_INPUT_INVALID); CHECK(sketch_input(s, 7) == ROUTED_INPUT_INVALID);
	CHECK(sketch_input(s, 256) == ROUTED_INPUT_INVALID); same(s, copy);
	/* Raw input remains bounded; rejection doesn't secretly apply or retry. */
	for(unsigned i = 0; i < CONSTELLATION_QUEUE_CAPACITY; i++) CHECK(sketch_input(s, 2) == ROUTED_INPUT_ACCEPTED);
	CHECK(sketch_input(s, 5) == ROUTED_INPUT_FULL); CHECK(s->x == 16 && s->commits == 1 && s->mode == 0);
	for(unsigned i = 0; i < CONSTELLATION_QUEUE_CAPACITY; i++) CHECK(sketch_step(s));
	CHECK(s->x == 20 && s->runner.host.nodes[0].uxn.ram[SKETCH_PAPER + 12 * 32 + 20] == 0);
	for(unsigned i = 0; i < 3000; i++) CHECK(sketch_apply(s, i % 2 ? 2 : 4));
	CHECK(!s->failed && !s->runner.host.links[0].queue.count && !s->runner.host.trace_count);
	/* Malformed messages are ignored by the ROM, not given app meaning by the runner. */
	uint8_t invalid[] = {5,6}; uint64_t commits = s->commits;
	CHECK(routed_input(&s->runner.host, 7, invalid, 2) == ROUTED_INPUT_ACCEPTED); CHECK(sketch_step(s)); CHECK(s->commits == commits);
	invalid[0] = 7; CHECK(routed_input(&s->runner.host, 7, invalid, 1) == ROUTED_INPUT_ACCEPTED); CHECK(sketch_step(s)); CHECK(s->commits == commits);
	dispose(s); dispose(copy);
	/* Invalid display snapshots fail without partially painting. */
	for(unsigned fault = 0; fault < 6; fault++) {
		s = fresh(); uint32_t before[SKETCH_WIDTH * SKETCH_HEIGHT]; memcpy(before, s->pixels, sizeof(before));
		Uxn *u = &s->runner.host.nodes[0].uxn;
		if(fault == 0) { u->devices[0x40] = 0xfd; u->devices[0x41] = 1; }
		if(fault == 1) u->devices[0x42] = 32;
		if(fault == 2) u->devices[0x43] = 24;
		if(fault == 3) u->ram[SKETCH_PAPER + 767] = 2;
		if(fault == 5) u->devices[0x45] = 3;
		u->device_write(u, 0x44, fault == 4 ? 2 : 1, u->device_context);
		CHECK(s->failed && s->error[0]); CHECK(memcmp(before, s->pixels, sizeof(before)) == 0);
		CHECK(!sketch_step(s)); CHECK(sketch_input(s, 1) == ROUTED_INPUT_FAULT); dispose(s);
	}
	Sketchpad empty = {0}; const uint8_t idle[] = {0}, loop[] = {0x40,0xff,0xfd};
	CHECK(!sketch_start(&empty, (RunnerRom){idle,1})); CHECK(!empty.runner.host.nodes && strstr(empty.error, "did not present"));
	CHECK(!sketch_start(&empty, (RunnerRom){loop,sizeof(loop)})); CHECK(!empty.runner.host.nodes && strstr(empty.error, "instruction limit"));
	sketch_free(&empty); CHECK(!sketch_step(&empty)); CHECK(sketch_input(&empty, 1) == ROUTED_INPUT_FAULT);
}
int main(void)
{ drawing_and_replay(); boundaries(); printf("%u sketch checks passed.\n", checks); return 0; }
