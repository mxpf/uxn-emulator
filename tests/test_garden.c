#include "garden.h"
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while(0)

static Garden *
fresh(void)
{
	Garden *g = calloc(1, sizeof(*g));
	CHECK(g != NULL);
	CHECK(garden_init(g, "build/garden-view.rom", "build/garden-world.rom"));
	CHECK(garden_state(g)[0] == 2 && garden_state(g)[1] == 2);
	CHECK(garden_state(g)[2] == 8 && garden_state(g)[3] == 6);
	CHECK(g->pixels[0] != g->pixels[3]);
	return g;
}

static void
step(Garden *g, uint8_t action)
{
	CHECK(garden_step(g, action));
	CHECK(constellation_is_quiescent(&g->pair));
	CHECK(!constellation_has_fault(&g->pair));
	CHECK(memcmp(garden_state(g), &g->pair.endpoints[1].uxn.ram[0x400], 200) == 0);
	for(unsigned i = 0; i < 2; i++) {
		CHECK(g->pair.endpoints[i].uxn.working.pointer == 0);
		CHECK(g->pair.endpoints[i].uxn.return_stack.pointer == 0);
	}
}

static void
test_movement(void)
{
	Garden *g = fresh();
	step(g, 4); /* west to 1,2 */
	step(g, 4); /* perimeter tree blocks */
	CHECK(garden_state(g)[0] == 1 && garden_state(g)[1] == 2);
	step(g, 1);
	step(g, 1);
	CHECK(garden_state(g)[0] == 1 && garden_state(g)[1] == 1);
	step(g, 5); /* greeting from far away does not create a friend */
	CHECK(garden_state(g)[5] == 0);
	free(g);
	g = fresh();
	for(unsigned i = 0; i < 8; i++) step(g, 2); /* 10,2 */
	step(g, 3); /* 10,3 */
	step(g, 3); /* water at 10,4 */
	CHECK(garden_state(g)[0] == 10 && garden_state(g)[1] == 3);
	free(g);
}

static void
test_creature_and_greeting(void)
{
	Garden *g = fresh();
	step(g, 0); step(g, 0); step(g, 0);
	CHECK(garden_state(g)[2] == 9 && garden_state(g)[3] == 6);
	for(unsigned i = 0; i < 3; i++) step(g, 0);
	CHECK(garden_state(g)[2] == 9 && garden_state(g)[3] == 7);
	free(g);
	g = fresh();
	for(unsigned i = 0; i < 5; i++) step(g, 2);
	for(unsigned i = 0; i < 4; i++) step(g, 3);
	CHECK(garden_state(g)[0] == 7 && garden_state(g)[1] == 6);
	CHECK(garden_state(g)[4] == 1);
	uint8_t cx = garden_state(g)[2], cy = garden_state(g)[3];
	step(g, 5);
	CHECK(garden_state(g)[5] == 1 && garden_state(g)[4] == 2);
	for(unsigned i = 0; i < 12; i++) step(g, 0);
	CHECK(garden_state(g)[2] == cx && garden_state(g)[3] == cy);
	free(g);
}

static void
test_recordable_determinism(void)
{
	Garden *a = fresh(), *b = fresh();
	uint32_t seed = 42;
	for(unsigned i = 0; i < 1000; i++) {
		seed = seed * 1664525u + 1013904223u;
		uint8_t action = (uint8_t)((seed >> 16) % 6);
		step(a, action);
		step(b, action);
		CHECK(a->trace_hash == b->trace_hash);
		CHECK(memcmp(a->pixels, b->pixels, sizeof(a->pixels)) == 0);
		CHECK(a->pair.trace_count == b->pair.trace_count);
		CHECK(memcmp(a->pair.trace, b->pair.trace, sizeof(a->pair.trace)) == 0);
		for(unsigned j = 0; j < 2; j++) {
			CHECK(memcmp(a->pair.endpoints[j].uxn.ram, b->pair.endpoints[j].uxn.ram, UXN_RAM_SIZE) == 0);
			CHECK(memcmp(a->pair.endpoints[j].uxn.devices, b->pair.endpoints[j].uxn.devices, UXN_DEVICE_SIZE) == 0);
		}
	}
	CHECK(a->tick == 1000);
	CHECK(!garden_step(a, 6));
	CHECK(a->failed);
	CHECK(!garden_step(a, 0)); /* terminal game error */
	free(a); free(b);
}

int
main(void)
{
	test_movement();
	test_creature_and_greeting();
	test_recordable_determinism();
	printf("%u garden assertions passed\n", checks);
	return 0;
}
