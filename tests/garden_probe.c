/* Test-only measurement host. No changes to guest devices or scheduling. */
#define _POSIX_C_SOURCE 200809L
#include "garden.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double
now_us(void)
{
	struct timespec t;
	if(clock_gettime(CLOCK_MONOTONIC, &t)) { perror("clock_gettime"); exit(1); }
	return (double)t.tv_sec * 1000000.0 + (double)t.tv_nsec / 1000.0;
}

static void
row(const Garden *g, const uint64_t before[2], double elapsed)
{
	uint64_t pixels = UINT64_C(14695981039346656037);
	unsigned sent = 0, bytes = 0, turns = 0, full = 0;
	unsigned queued[2] = {0, 0}, peak[2] = {0, 0};
	if(memcmp(garden_state(g), &g->pair.endpoints[1].uxn.ram[0x400], 200)) {
		fputs("view/world snapshot mismatch\n", stderr); exit(1);
	}
	for(unsigned i = 0; i < GARDEN_WIDTH * GARDEN_HEIGHT; i++)
		for(unsigned shift = 0; shift < 32; shift += 8)
			pixels = (pixels ^ ((g->pixels[i] >> shift) & 255)) * UINT64_C(1099511628211);
	/* Each measured batch starts with empty queues. Delivery events name
	 * the sender in endpoint, matching constellation.c's trace contract. */
	for(size_t i = 0; i < g->pair.trace_count; i++) {
		const ConstellationTraceEvent *e = &g->pair.trace[i];
		unsigned id = (unsigned)e->endpoint;
		if(id >= 2) { fputs("invalid trace endpoint\n", stderr); exit(1); }
		if(e->kind == CONSTELLATION_TRACE_SEND) {
			sent++; bytes += e->message.length;
			if(++queued[id] > peak[id]) peak[id] = queued[id];
		} else if(e->kind == CONSTELLATION_TRACE_DELIVER) {
			if(!queued[id]) { fputs("unmatched delivery\n", stderr); exit(1); }
			queued[id]--;
		} else if(e->kind == CONSTELLATION_TRACE_TURN) turns++;
		else if(e->kind == CONSTELLATION_TRACE_SEND_FULL) full++;
	}
	for(unsigned i = 0; i < 2; i++)
		if(queued[i] != g->pair.queues[i].count || peak[i] > CONSTELLATION_QUEUE_CAPACITY) {
			fputs("queue accounting mismatch\n", stderr); exit(1);
		}
	printf("{\"digest\":\"%" PRIu64 " %016" PRIx64 " %016" PRIx64 "\",\"state\":\"",
		g->tick, g->trace_hash, pixels);
	for(unsigned i = 0; i < 200; i++) printf("%02x", garden_state(g)[i]);
	printf("\",\"instructions\":[%" PRIu64 ",%" PRIu64 "],\"messages\":%u,\"bytes\":%u,"
		"\"turns\":%u,\"traceEvents\":%zu,\"queuePeak\":[%u,%u],\"sendFull\":%u,\"us\":%.3f}\n",
		g->pair.endpoints[0].uxn.instructions - before[0],
		g->pair.endpoints[1].uxn.instructions - before[1], sent, bytes, turns,
		g->pair.trace_count, peak[0], peak[1], full, elapsed);
}

int
main(int argc, char **argv)
{
	if(argc == 2 && !strcmp(argv[1], "--sizes")) {
		printf("{\"gardenStructBytes\":%zu,\"uxnStructBytes\":%zu,"
			"\"guestAddressSpaceBytesPerNode\":%u,\"allocatedRamBytesPerNode\":%u}\n",
			sizeof(Garden), sizeof(Uxn), UXN_RAM_SIZE, UXN_MEMORY_SIZE);
		return 0;
	}
	if(argc != 1) return 2;
	Garden *g = calloc(1, sizeof(*g));
	uint64_t before[2] = {0, 0};
	int action, scanned;
	double start;
	if(!g) return 1;
	start = now_us();
	if(!garden_init(g, "build/garden-view.rom", "build/garden-world.rom")) {
		fprintf(stderr, "%s\n", g->error); free(g); return 1;
	}
	row(g, before, now_us() - start);
	while((scanned = scanf("%d", &action)) == 1) {
		if(action < 0 || action > 5) { free(g); return 2; }
		for(unsigned i = 0; i < 2; i++) before[i] = g->pair.endpoints[i].uxn.instructions;
		start = now_us();
		if(!garden_step(g, (uint8_t)action)) {
			fprintf(stderr, "%s\n", g->error); free(g); return 1;
		}
		double elapsed = now_us() - start;
		if(!constellation_is_quiescent(&g->pair) ||
			g->pair.endpoints[0].uxn.working.pointer || g->pair.endpoints[0].uxn.return_stack.pointer ||
			g->pair.endpoints[1].uxn.working.pointer || g->pair.endpoints[1].uxn.return_stack.pointer) {
			fputs("unfinished exchange or stack leak\n", stderr); free(g); return 1;
		}
		row(g, before, elapsed);
	}
	free(g);
	return scanned == EOF && !ferror(stdin) ? 0 : 2;
}
