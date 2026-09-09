#include "constellation_routes.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while(0)
enum { BATCHES = 1024, TURN_LIMIT = 24000, LINK_COUNT = 4 };
static const RoutedRoute routes[] = {{0,1,1}, {0,2,2}, {1,7,2}, {2,9,0}};
static const char *const roms[] = {
	"build/routes-cycle-burst.rom", "build/routes-relay.rom", "build/routes-cycle-collect.rom"
};
typedef struct { RoutedHost host; RoutedNode nodes[3]; RoutedLink links[LINK_COUNT]; } Fixture;
typedef struct {
	uint64_t hash, events;
	unsigned sends[LINK_COUNT], delivered[LINK_COUNT], full[LINK_COUNT], wakes[LINK_COUNT];
	unsigned last_delivery[LINK_COUNT], max_gap[LINK_COUNT], peak[LINK_COUNT], queued[LINK_COUNT];
	unsigned batches, largest_batch;
} Record;

static Fixture *
fresh(void)
{
	Fixture *f = calloc(1, sizeof(*f)); CHECK(f != NULL);
	CHECK(routed_init(&f->host, f->nodes, 3, f->links, routes, LINK_COUNT, 10000));
	for(unsigned id = 0; id < 3; id++) {
		uint8_t rom[UXN_RAM_SIZE - UXN_ROM_START + 1];
		FILE *file = fopen(roms[id], "rb"); CHECK(file != NULL);
		size_t length = fread(rom, 1, sizeof(rom), file);
		CHECK(!ferror(file)); CHECK(fclose(file) == 0);
		CHECK(routed_load(&f->host, id, rom, length));
	}
	return f;
}

static void
same_execution(const Fixture *a, const Fixture *b, bool all_memory)
{
	CHECK(a->host.fault == b->host.fault && !a->host.trace_exhausted && !b->host.trace_exhausted);
	CHECK(a->host.booted == b->host.booted && a->host.next_node == b->host.next_node);
	CHECK(a->host.trace_sequence == b->host.trace_sequence);
	CHECK(memcmp(a->links, b->links, sizeof(a->links)) == 0);
	for(unsigned i = 0; i < 3; i++) {
		const RoutedNode *x = &a->nodes[i], *y = &b->nodes[i];
		CHECK(memcmp(x->uxn.ram, y->uxn.ram, all_memory ? sizeof(x->uxn.ram) : UXN_RAM_SIZE) == 0);
		CHECK(memcmp(x->uxn.devices, y->uxn.devices, sizeof(x->uxn.devices)) == 0);
		CHECK(memcmp(&x->uxn.working, &y->uxn.working, sizeof(UxnStack)) == 0);
		CHECK(memcmp(&x->uxn.return_stack, &y->uxn.return_stack, sizeof(UxnStack)) == 0);
		CHECK(x->uxn.instructions == y->uxn.instructions);
		CHECK(x->uxn.working.pointer == 0 && x->uxn.return_stack.pointer == 0);
		CHECK(x->next_incoming == y->next_incoming && x->next_writable == y->next_writable);
		CHECK(x->prefer_receive == y->prefer_receive);
		CHECK(x->source == y->source && x->writable == y->writable && x->loaded == y->loaded);
	}
}

static void
hash_byte(Record *r, uint8_t byte)
{
	r->hash = (r->hash ^ byte) * UINT64_C(1099511628211);
}

static void
observe(Record *r, const RoutedEvent *events, size_t count, unsigned turn, bool traffic)
{
	r->batches++;
	if(count > r->largest_batch) r->largest_batch = (unsigned)count;
	for(size_t i = 0; i < count; i++) {
		const RoutedEvent *e = &events[i];
		CHECK(e->sequence == r->events++); /* No omissions, duplicates, or reordered chunks. */
		for(unsigned shift = 0; shift < 64; shift += 8) hash_byte(r, (uint8_t)(e->sequence >> shift));
		hash_byte(r, (uint8_t)e->kind); hash_byte(r, (uint8_t)e->reason);
		hash_byte(r, e->from); hash_byte(r, e->to); hash_byte(r, e->selector);
		hash_byte(r, e->message.length);
		for(unsigned j = 0; j < e->message.length; j++) hash_byte(r, e->message.data[j]);
		if(!traffic || (e->kind != CONSTELLATION_TRACE_SEND && e->kind != CONSTELLATION_TRACE_SEND_FULL &&
			e->kind != CONSTELLATION_TRACE_DELIVER && e->kind != CONSTELLATION_TRACE_WRITABLE)) continue;
		unsigned link = 0;
		while(link < LINK_COUNT && (routes[link].from != e->from || routes[link].to != e->to || routes[link].selector != e->selector)) link++;
		CHECK(link < LINK_COUNT);
		if(e->kind == CONSTELLATION_TRACE_SEND_FULL) { CHECK(link == 0 && e->message.data[0] == 5); r->full[link]++; }
		else if(e->kind == CONSTELLATION_TRACE_WRITABLE) { CHECK(link == 0); r->wakes[link]++; }
		else {
			CHECK(e->message.length == 1);
			unsigned index = e->kind == CONSTELLATION_TRACE_SEND ? r->sends[link] : r->delivered[link];
			uint8_t expected = link == 1 ? 0xaa : link == 3 ? 0xcc : (uint8_t)(index % 5 + 1);
			CHECK(e->message.data[0] == expected);
			if(e->kind == CONSTELLATION_TRACE_SEND) {
				r->sends[link]++; r->queued[link]++;
				CHECK(r->queued[link] <= CONSTELLATION_QUEUE_CAPACITY);
				if(r->queued[link] > r->peak[link]) r->peak[link] = r->queued[link];
			} else {
				CHECK(r->queued[link] > 0); r->queued[link]--; r->delivered[link]++;
				unsigned gap = turn - r->last_delivery[link];
				if(gap > r->max_gap[link]) r->max_gap[link] = gap;
				r->last_delivery[link] = turn;
			}
		}
	}
}

int main(void)
{
	Fixture *a = fresh(), *b = fresh();
	RoutedEvent pending[128], batch[128]; size_t pending_count = 0, count;
	Record ra = {.hash = UINT64_C(14695981039346656037)}, rb = {.hash = UINT64_C(14695981039346656037)};
	CHECK(routed_boot(&a->host)); CHECK(routed_boot(&b->host)); same_execution(a, b, true);
	CHECK(routed_take_trace(&a->host, pending, 128, &pending_count));
	observe(&ra, pending, pending_count, 0, true);
	CHECK(routed_take_trace(&b->host, batch, 128, &count));
	CHECK(count == pending_count && memcmp(pending, batch, count * sizeof(*batch)) == 0);
	observe(&rb, batch, count, 0, false); pending_count = 0;
	unsigned turn = 0;
	while(!routed_quiescent(&a->host) && turn < TURN_LIMIT) {
		turn++;
		CHECK(routed_step(&a->host)); CHECK(routed_step(&b->host)); same_execution(a, b, false);
		/* The first run consumes every turn; the replay consumes every seven.
		 * Only a bounded pending batch is retained for exact stream comparison. */
		CHECK(routed_take_trace(&a->host, batch, 128, &count));
		CHECK(pending_count + count <= 128);
		memcpy(pending + pending_count, batch, count * sizeof(*batch)); pending_count += count;
		observe(&ra, batch, count, turn, true);
		if(turn % 7 == 0 || routed_quiescent(&a->host)) {
			CHECK(routed_take_trace(&b->host, batch, 128, &count));
			CHECK(count == pending_count && memcmp(pending, batch, count * sizeof(*batch)) == 0);
			observe(&rb, batch, count, turn, false); pending_count = 0;
			CHECK(ra.hash == rb.hash && ra.events == rb.events);
		}
		for(unsigned i = 0; i < LINK_COUNT; i++) {
			CHECK(ra.queued[i] == a->links[i].queue.count);
			CHECK(turn - ra.last_delivery[i] <= 24); /* Bound progress, not just final totals. */
		}
	}
	CHECK(routed_quiescent(&a->host) && routed_quiescent(&b->host));
	CHECK(turn == BATCHES * 18 + 1);
	CHECK(a->nodes[0].uxn.ram[6] == 4 && a->nodes[0].uxn.ram[7] == 0 && a->nodes[0].uxn.ram[8] == 1);
	CHECK(memcmp(a->nodes[2].uxn.ram + 0x400, "\xaa\x01\x02\x03\x04\x05", 6) == 0);
	CHECK(memcmp(a->nodes[2].uxn.ram + 0x500, "\0\x01\x01\x01\x01\x01", 6) == 0);
	for(unsigned i = 0; i < LINK_COUNT; i++) {
		unsigned expected = BATCHES * (i == 0 || i == 2 ? 5 : 1);
		CHECK(ra.sends[i] == expected && ra.delivered[i] == expected && ra.queued[i] == 0);
		CHECK(ra.full[i] == (i == 0 ? BATCHES : 0));
		CHECK(ra.wakes[i] == (i == 0 ? BATCHES : 0));
	}
	CHECK(ra.peak[0] == 4 && ra.peak[1] == 1 && ra.peak[2] == 2 && ra.peak[3] == 1);
	CHECK(ra.events == BATCHES * 49 + 5 && ra.events == a->host.trace_sequence);
	CHECK(ra.events == rb.events && ra.hash == rb.hash && pending_count == 0);
	same_execution(a, b, true);
	printf("Sustained: %u turns, %u cycles, %" PRIu64 " exact events, %u full/retry recoveries.\n", turn, BATCHES, ra.events, ra.full[0]);
	printf("Deliveries per route: %u %u %u %u; max gaps: %u %u %u %u turns.\n", ra.delivered[0], ra.delivered[1], ra.delivered[2], ra.delivered[3], ra.max_gap[0], ra.max_gap[1], ra.max_gap[2], ra.max_gap[3]);
	printf("Trace batches: %u / %u; largest: %u / %u events; matching digest %016" PRIx64 ".\n", ra.batches, rb.batches, ra.largest_batch, rb.largest_batch, ra.hash);
	printf("%u sustained checks passed\n", checks);
	free(a); free(b);
	return 0;
}
