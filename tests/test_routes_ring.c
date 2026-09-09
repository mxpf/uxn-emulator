#include "constellation_routes.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while(0)
enum { NODES = 3, TOKENS = 15, SLOTS = 16, TRACE = CONSTELLATION_TRACE_CAPACITY };
typedef struct { RoutedHost host; RoutedNode nodes[NODES]; RoutedLink links[NODES]; } Fixture;
typedef struct { unsigned location, remaining; bool queued, retired; } Token;
typedef struct {
	Token tokens[TOKENS];
	ConstellationQueue queues[NODES];
	unsigned sends[NODES], deliveries[NODES], full[NODES], wakes[NODES], retired[NODES];
	unsigned peak[NODES], private_peak[NODES], last_delivery[NODES], max_gap[NODES], last_progress;
	uint64_t events;
} Record;

static unsigned
short_at(const uint8_t *ram, unsigned address)
{
	return (unsigned)ram[address] * 256 + ram[address + 1];
}

static Fixture *
fresh(unsigned direction, unsigned hops)
{
	Fixture *f = calloc(1, sizeof(*f)); CHECK(f != NULL);
	RoutedRoute routes[NODES];
	for(unsigned i = 0; i < NODES; i++) routes[i] = (RoutedRoute){i, 1, (i + direction) % NODES};
	CHECK(routed_init(&f->host, f->nodes, NODES, f->links, routes, NODES, 10000));
	uint8_t rom[UXN_RAM_SIZE - UXN_ROM_START + 1];
	FILE *file = fopen("build/routes-ring.rom", "rb"); CHECK(file != NULL);
	size_t length = fread(rom, 1, sizeof(rom), file);
	CHECK(!ferror(file)); CHECK(fclose(file) == 0);
	for(unsigned i = 0; i < NODES; i++) {
		CHECK(routed_load(&f->host, i, rom, length));
		/* Explicit initial fixture data, not a new identity/clock host device. */
		f->nodes[i].uxn.ram[0] = (uint8_t)i;
		f->nodes[i].uxn.ram[1] = (uint8_t)hops;
	}
	return f;
}

static void
same(const Fixture *a, const Fixture *b, bool all_memory)
{
	CHECK(a->host.fault == ROUTED_OK && b->host.fault == ROUTED_OK);
	CHECK(!a->host.trace_exhausted && !b->host.trace_exhausted);
	CHECK(a->host.booted == b->host.booted && a->host.next_node == b->host.next_node);
	CHECK(a->host.trace_sequence == b->host.trace_sequence);
	CHECK(memcmp(a->links, b->links, sizeof(a->links)) == 0);
	for(unsigned i = 0; i < NODES; i++) {
		const RoutedNode *x = &a->nodes[i], *y = &b->nodes[i];
		CHECK(memcmp(x->uxn.ram, y->uxn.ram, all_memory ? sizeof(x->uxn.ram) : UXN_RAM_SIZE) == 0);
		CHECK(memcmp(x->uxn.devices, y->uxn.devices, sizeof(x->uxn.devices)) == 0);
		CHECK(memcmp(&x->uxn.working, &y->uxn.working, sizeof(UxnStack)) == 0);
		CHECK(memcmp(&x->uxn.return_stack, &y->uxn.return_stack, sizeof(UxnStack)) == 0);
		CHECK(x->uxn.working.pointer == 0 && x->uxn.return_stack.pointer == 0);
		CHECK(x->uxn.instructions == y->uxn.instructions);
		CHECK(x->next_incoming == y->next_incoming && x->next_writable == y->next_writable);
		CHECK(x->prefer_receive == y->prefer_receive && x->loaded == y->loaded);
		CHECK(x->source == ROUTED_NONE && y->source == ROUTED_NONE);
		CHECK(x->writable == ROUTED_NONE && y->writable == ROUTED_NONE);
	}
}

static unsigned
token_id(const uint8_t *data)
{
	CHECK(data[0] < NODES && data[1] < 5);
	return data[0] * 5 + data[1];
}

/* Independent protocol oracle: every successful send must eventually deliver
 * once, in route FIFO order, with one fewer hop on the next forwarding send. */
static void
observe(Record *r, const Fixture *f, const RoutedEvent *events, size_t count, unsigned turn)
{
	for(size_t i = 0; i < count; i++) {
		const RoutedEvent *e = &events[i];
		CHECK(e->sequence == r->events++ && e->reason == ROUTED_OK);
		if(e->kind == CONSTELLATION_TRACE_BOOT || e->kind == CONSTELLATION_TRACE_IDLE) continue;
		if(e->kind == CONSTELLATION_TRACE_TURN) {
			CHECK(turn > 0 && e->from == (turn - 1) % NODES); continue;
		}
		CHECK(e->from < NODES && e->to == f->links[e->from].route.to && e->selector == 1);
		unsigned route = e->from;
		if(e->kind == CONSTELLATION_TRACE_WRITABLE) { r->wakes[route]++; continue; }
		CHECK(e->message.length == 3);
		Token *t = &r->tokens[token_id(e->message.data)];
		CHECK(!t->retired && t->location == e->from && t->remaining == e->message.data[2]);
		ConstellationQueue *q = &r->queues[route];
		if(e->kind == CONSTELLATION_TRACE_SEND_FULL) {
			CHECK(!t->queued && q->count == CONSTELLATION_QUEUE_CAPACITY); r->full[route]++;
		} else if(e->kind == CONSTELLATION_TRACE_SEND) {
			CHECK(!t->queued && q->count < CONSTELLATION_QUEUE_CAPACITY);
			q->messages[(q->head + q->count) % CONSTELLATION_QUEUE_CAPACITY] = e->message;
			q->count++; t->queued = true; r->sends[route]++;
			if(q->count > r->peak[route]) r->peak[route] = q->count;
		} else {
			CHECK(e->kind == CONSTELLATION_TRACE_DELIVER && t->queued && q->count > 0);
			CHECK(memcmp(&q->messages[q->head], &e->message, sizeof(e->message)) == 0);
			q->head = (q->head + 1) % CONSTELLATION_QUEUE_CAPACITY; q->count--;
			t->queued = false; t->location = e->to; t->remaining--;
			if(!t->remaining) { t->retired = true; r->retired[e->to]++; }
			r->deliveries[route]++;
			unsigned gap = turn - r->last_delivery[route];
			if(gap > r->max_gap[route]) r->max_gap[route] = gap;
			r->last_delivery[route] = r->last_progress = turn;
		}
	}
}

static void
locate(Record *r, bool *seen, const uint8_t *data, unsigned location, bool queued)
{
	unsigned id = token_id(data); Token *t = &r->tokens[id];
	CHECK(!seen[id] && !t->retired && t->queued == queued);
	CHECK(t->location == location && t->remaining == data[2]); seen[id] = true;
}

/* At every callback boundary all fifteen tokens must exist exactly once in
 * a host queue, a guest's private FIFO, or the retired set. */
static void
conservation(Record *r, const Fixture *f)
{
	bool seen[TOKENS] = {false};
	for(unsigned i = 0; i < NODES; i++) {
		const ConstellationQueue *q = &f->links[i].queue;
		CHECK(q->count == r->queues[i].count && q->head == r->queues[i].head);
		for(unsigned j = 0; j < q->count; j++) {
			const ConstellationMessage *m = &q->messages[(q->head + j) % CONSTELLATION_QUEUE_CAPACITY];
			CHECK(m->length == 3); locate(r, seen, m->data, i, true);
		}
		const uint8_t *ram = f->nodes[i].uxn.ram;
		CHECK(ram[2] < SLOTS && ram[3] < SLOTS && ram[4] <= TOKENS && !ram[0x0e]);
		CHECK(ram[3] == (ram[2] + ram[4]) % SLOTS);
		if(ram[4] > r->private_peak[i]) r->private_peak[i] = ram[4];
		for(unsigned j = 0; j < ram[4]; j++) locate(r, seen, ram + 0x800 + ((ram[2] + j) % SLOTS) * 4, i, false);
		CHECK(short_at(ram, 6) == r->retired[i]);
		CHECK(short_at(ram, 0x0a) == r->wakes[i] && short_at(ram, 0x0c) == r->full[i]);
		unsigned incoming = 0;
		for(unsigned j = 0; j < NODES; j++) if(f->links[j].route.to == i) incoming += r->deliveries[j];
		CHECK(short_at(ram, 8) == incoming);
	}
	for(unsigned i = 0; i < TOKENS; i++) CHECK(seen[i] != r->tokens[i].retired);
}

static void
test_ring(unsigned direction, unsigned hops)
{
	Fixture *a = fresh(direction, hops), *b = fresh(direction, hops);
	Record record = {0};
	for(unsigned i = 0; i < TOKENS; i++) record.tokens[i] = (Token){.location = i / 5, .remaining = hops};
	CHECK(routed_boot(&a->host)); CHECK(routed_boot(&b->host)); same(a, b, true);
	RoutedEvent batch[TRACE], pending[TRACE], replay[TRACE]; size_t count, pending_count = 0, replay_count;
	CHECK(routed_take_trace(&a->host, batch, TRACE, &count)); observe(&record, a, batch, count, 0);
	CHECK(routed_take_trace(&b->host, replay, TRACE, &replay_count));
	CHECK(count == replay_count && memcmp(batch, replay, count * sizeof(*batch)) == 0);
	conservation(&record, a);
	for(unsigned i = 0; i < NODES; i++) {
		CHECK(a->links[i].queue.count == 4 && a->links[i].waiting);
		CHECK(a->nodes[i].uxn.ram[4] == 1 && record.full[i] == 1);
	}
	unsigned turn = 0;
	while(!routed_quiescent(&a->host) && turn < hops * 120 + 128) {
		turn++;
		CHECK(routed_step(&a->host)); CHECK(routed_step(&b->host)); same(a, b, false);
		CHECK(routed_take_trace(&a->host, batch, TRACE, &count)); observe(&record, a, batch, count, turn);
		CHECK(pending_count + count <= TRACE);
		memcpy(pending + pending_count, batch, count * sizeof(*batch)); pending_count += count;
		if(turn % 7 == 0 || routed_quiescent(&a->host)) {
			CHECK(routed_take_trace(&b->host, replay, TRACE, &replay_count));
			CHECK(pending_count == replay_count && memcmp(pending, replay, replay_count * sizeof(*replay)) == 0);
			pending_count = 0;
		}
		conservation(&record, a);
		CHECK(turn - record.last_progress <= 18);
	}
	CHECK(routed_quiescent(&a->host) && routed_quiescent(&b->host) && pending_count == 0);
	CHECK(record.events == a->host.trace_sequence); same(a, b, true);
	for(unsigned i = 0; i < TOKENS; i++) CHECK(record.tokens[i].retired);
	for(unsigned i = 0; i < NODES; i++) {
		CHECK(record.sends[i] == 5 * hops && record.deliveries[i] == 5 * hops);
		CHECK(record.retired[i] == 5 && record.peak[i] == 4);
		CHECK(record.full[i] >= hops && record.wakes[i] >= hops && a->nodes[i].uxn.ram[4] == 0);
		CHECK(record.max_gap[i] <= 6);
	}
	printf("Ring direction=%u hops=%u: %u turns, %" PRIu64 " exact events, %u deliveries; full=%u/%u/%u, wakes=%u/%u/%u, private peaks=%u/%u/%u, delivery gaps=%u/%u/%u.\n",
		direction, hops, turn, record.events, TOKENS * hops,
		record.full[0], record.full[1], record.full[2], record.wakes[0], record.wakes[1], record.wakes[2],
		record.private_peak[0], record.private_peak[1], record.private_peak[2], record.max_gap[0], record.max_gap[1], record.max_gap[2]);
	free(a); free(b);
}

int main(void)
{
	const unsigned hops[] = {1, 3, 12, 64};
	for(unsigned direction = 1; direction <= 2; direction++)
		for(unsigned i = 0; i < sizeof(hops) / sizeof(*hops); i++) test_ring(direction, hops[i]);
	printf("%u cyclic-pressure checks passed\n", checks);
	return 0;
}
