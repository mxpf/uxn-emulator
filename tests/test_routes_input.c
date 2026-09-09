#include "constellation_routes.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while(0)
enum { TRACE = CONSTELLATION_TRACE_CAPACITY, INPUT_ROUTE = 3, LOG_CAPACITY = 32 };
typedef struct { RoutedHost host; RoutedNode nodes[3]; RoutedLink links[5]; } Fixture;
static const RoutedRoute graph[] = {{0,7,1}, {1,1,2}, {2,1,0}, {ROUTED_NONE,7,0}, {ROUTED_NONE,8,1}};

static Fixture *
fresh(bool interactive)
{
	Fixture *f = calloc(1, sizeof(*f)); CHECK(f != NULL);
	RoutedRoute routes[5]; memcpy(routes, graph, sizeof(routes));
	if(interactive) routes[0].selector = 1;
	CHECK(routed_init(&f->host, f->nodes, 3, f->links, routes, 5, 10000));
	uint8_t rom[UXN_RAM_SIZE - UXN_ROM_START + 1] = {0}; size_t length = 128;
	if(interactive) {
		FILE *file = fopen("build/routes-input.rom", "rb"); CHECK(file != NULL);
		length = fread(rom, 1, sizeof(rom), file); CHECK(!ferror(file)); CHECK(fclose(file) == 0);
	} else {
		/* Install a receive callback and wrapping buffer. Try to spoof source,
		 * then record actual DEI source and callback count outside that buffer. */
		const uint8_t boot[] = {0xa0,1,0x70,0x80,0xd0,0x37,0xa0,0xff,0xff,0x80,0xd2,0x37,0};
		const uint8_t receive[] = {0x80,99,0x80,0xdd,0x17,0x80,0xdd,0x16,0xa0,6,0,0x15,
			0xa0,6,1,0x14,0x01,0xa0,6,1,0x15,0};
		memcpy(rom, boot, sizeof(boot)); memcpy(rom + 0x70, receive, sizeof(receive)); length = 160;
	}
	for(unsigned i = 0; i < 3; i++) CHECK(routed_load(&f->host, i, rom, length));
	return f;
}

static void
invalid_unchanged(Fixture *f, unsigned selector, const uint8_t *data, size_t length, RoutedInputResult result)
{
	Fixture *before = malloc(sizeof(*before)); CHECK(before != NULL); memcpy(before, f, sizeof(*before));
	CHECK(routed_input(&f->host, selector, data, length) == result);
	CHECK(memcmp(before, f, sizeof(*before)) == 0); free(before);
}

static void
test_boundary(void)
{
	Fixture *f = fresh(false);
	invalid_unchanged(f, 7, NULL, 0, ROUTED_INPUT_INVALID);
	CHECK(routed_boot(&f->host));
	invalid_unchanged(f, 6, NULL, 0, ROUTED_INPUT_INVALID);
	invalid_unchanged(f, 255, NULL, 0, ROUTED_INPUT_INVALID);
	invalid_unchanged(f, 256, NULL, 0, ROUTED_INPUT_INVALID);
	invalid_unchanged(f, 7, NULL, 1, ROUTED_INPUT_INVALID);
	uint8_t payload[256]; for(unsigned i = 0; i < sizeof(payload); i++) payload[i] = (uint8_t)i;
	invalid_unchanged(f, 7, payload, 256, ROUTED_INPUT_INVALID);
	uint64_t instructions = f->nodes[0].uxn.instructions;
	CHECK(routed_input(&f->host, 7, payload, 255) == ROUTED_INPUT_ACCEPTED);
	CHECK(routed_input(&f->host, 7, NULL, 0) == ROUTED_INPUT_ACCEPTED);
	CHECK(routed_input(&f->host, 7, payload, 1) == ROUTED_INPUT_ACCEPTED);
	CHECK(routed_input(&f->host, 7, payload + 1, 1) == ROUTED_INPUT_ACCEPTED);
	CHECK(f->nodes[0].uxn.instructions == instructions && f->host.next_node == 0);
	CHECK(!routed_quiescent(&f->host) && f->links[0].queue.count == 0);
	ConstellationQueue before = f->links[3].queue;
	CHECK(routed_input(&f->host, 7, payload + 2, 1) == ROUTED_INPUT_FULL);
	CHECK(memcmp(&before, &f->links[3].queue, sizeof(before)) == 0 && !f->links[3].waiting);
	CHECK(routed_input(&f->host, 8, NULL, 0) == ROUTED_INPUT_ACCEPTED); /* Independent route. */
	memset(payload, 99, sizeof(payload));
	CHECK(routed_step(&f->host));
	for(unsigned i = 0; i < 255; i++) CHECK(f->nodes[0].uxn.ram[(uint16_t)(0xffff + i)] == i);
	CHECK(f->nodes[0].uxn.ram[0x600] == ROUTED_NONE && f->nodes[0].uxn.ram[0x601] == 1);
	CHECK(f->nodes[0].source == ROUTED_NONE && f->nodes[0].uxn.devices[0xd5] == 0);
	payload[0] = 2;
	CHECK(routed_input(&f->host, 7, payload, 1) == ROUTED_INPUT_ACCEPTED);
	for(unsigned turn = 0; !routed_quiescent(&f->host) && turn < 40; turn++) CHECK(routed_step(&f->host));
	CHECK(routed_quiescent(&f->host) && f->nodes[0].uxn.ram[0x601] == 5);
	CHECK(f->nodes[1].uxn.ram[0x601] == 1);
	unsigned deliveries = 0, full = 0;
	for(size_t i = 0; i < f->host.trace_count; i++) {
		const RoutedEvent *e = &f->host.trace[i];
		CHECK(e->kind != CONSTELLATION_TRACE_WRITABLE);
		if(e->kind == CONSTELLATION_TRACE_SEND_FULL) { CHECK(e->from == ROUTED_NONE); full++; }
		if(e->kind != CONSTELLATION_TRACE_DELIVER || e->selector != 7) continue;
		CHECK(e->from == ROUTED_NONE && e->to == 0);
		if(deliveries == 0) { CHECK(e->message.length == 255); for(unsigned j = 0; j < 255; j++) CHECK(e->message.data[j] == j); }
		else if(deliveries == 1) CHECK(e->message.length == 0);
		else CHECK(e->message.length == 1 && e->message.data[0] == deliveries - 2);
		deliveries++;
	}
	CHECK(deliveries == 5 && full == 1);
	CHECK(routed_input(&f->host, 7, NULL, 0) == ROUTED_INPUT_ACCEPTED);
	CHECK(!routed_quiescent(&f->host));
	for(unsigned turn = 0; !routed_quiescent(&f->host) && turn < 3; turn++) CHECK(routed_step(&f->host));
	CHECK(routed_quiescent(&f->host) && f->nodes[0].uxn.ram[0x601] == 6); free(f);

	/* Only explicitly declared external sources are legal; they have their own
	 * selector namespace, and cannot be destinations or impersonated by ROMs. */
	f = fresh(false); RoutedRoute bad[] = {{ROUTED_NONE,7,0}, {ROUTED_NONE,7,1}};
	CHECK(!routed_init(&f->host, f->nodes, 3, f->links, bad, 2, 10000));
	bad[1] = (RoutedRoute){0,8,ROUTED_NONE};
	CHECK(!routed_init(&f->host, f->nodes, 3, f->links, bad, 2, 10000)); free(f);
	f = fresh(false);
	const uint8_t spoof[] = {0x80,8,0x80,0xdc,0x17,0x80,1,0x80,0xd9,0x17,0};
	memcpy(f->nodes[0].uxn.ram + UXN_ROM_START, spoof, sizeof(spoof));
	CHECK(!routed_boot(&f->host) && f->host.fault == ROUTED_BAD_ROUTE);
	CHECK(f->links[4].queue.count == 0);
	invalid_unchanged(f, 7, NULL, 0, ROUTED_INPUT_FAULT); free(f);
	f = fresh(false); CHECK(routed_boot(&f->host));
	f->nodes[0].uxn.devices[0xd0] = f->nodes[0].uxn.devices[0xd1] = 0;
	CHECK(routed_input(&f->host, 7, NULL, 0) == ROUTED_INPUT_ACCEPTED);
	CHECK(!routed_step(&f->host) && f->host.fault == ROUTED_RECEIVE_VECTOR_MISSING); free(f);
}

static void
test_trace_exhaustion(void)
{
	for(unsigned sequence_limit = 0; sequence_limit < 2; sequence_limit++) {
	for(unsigned full = 0; full < 2; full++) {
		Fixture *f = fresh(false); CHECK(routed_boot(&f->host));
		if(full) for(unsigned i = 0; i < 4; i++) CHECK(routed_input(&f->host, 7, NULL, 0) == ROUTED_INPUT_ACCEPTED);
		if(sequence_limit) f->host.trace_sequence = UINT64_MAX - 1;
		else f->host.trace_count = TRACE - 1;
		CHECK(routed_input(&f->host, 7, NULL, 0) == (full ? ROUTED_INPUT_FULL : ROUTED_INPUT_ACCEPTED));
		CHECK(f->links[3].queue.count == (full ? 4 : 1) && !f->host.trace_exhausted);
		CHECK(routed_input(&f->host, 7, NULL, 0) == ROUTED_INPUT_FAULT);
		CHECK(f->host.fault == ROUTED_TRACE_FULL && f->host.trace_exhausted);
		CHECK(f->links[3].queue.count == (full ? 4 : 1)); /* Unrecordable input was not admitted. */
		RoutedEvent events[TRACE]; size_t count;
		CHECK(routed_take_trace(&f->host, events, TRACE, &count));
		invalid_unchanged(f, 7, NULL, 0, ROUTED_INPUT_FAULT);
		CHECK(!routed_step(&f->host)); free(f);
	}
	}
}

typedef struct { unsigned after_turn, selector; uint8_t value; RoutedInputResult result; } Input;
typedef struct {
	ConstellationQueue queues[5];
	ConstellationMessage next_send[3];
	bool send_pending[3];
	unsigned count[3], internal[3], retired[3], full, accepted, delivered;
	uint8_t state[3], hash[3], final_xor[3];
	uint64_t events;
} Model;

static void
same(const Fixture *a, const Fixture *b, bool all_memory)
{
	CHECK(a->host.fault == ROUTED_OK && b->host.fault == ROUTED_OK);
	CHECK(!a->host.trace_exhausted && !b->host.trace_exhausted);
	CHECK(a->host.next_node == b->host.next_node && a->host.trace_sequence == b->host.trace_sequence);
	CHECK(memcmp(a->links, b->links, sizeof(a->links)) == 0);
	for(unsigned i = 0; i < 3; i++) {
		const RoutedNode *x = &a->nodes[i], *y = &b->nodes[i];
		CHECK(memcmp(x->uxn.ram, y->uxn.ram, all_memory ? sizeof(x->uxn.ram) : UXN_RAM_SIZE) == 0);
		CHECK(memcmp(x->uxn.devices, y->uxn.devices, sizeof(x->uxn.devices)) == 0);
		CHECK(memcmp(&x->uxn.working, &y->uxn.working, sizeof(UxnStack)) == 0);
		CHECK(memcmp(&x->uxn.return_stack, &y->uxn.return_stack, sizeof(UxnStack)) == 0);
		CHECK(x->uxn.working.pointer == 0 && x->uxn.return_stack.pointer == 0);
		CHECK(x->uxn.instructions == y->uxn.instructions && x->loaded == y->loaded);
		CHECK(x->next_incoming == y->next_incoming && x->next_writable == y->next_writable);
		CHECK(x->prefer_receive == y->prefer_receive && x->source == y->source && x->writable == y->writable);
	}
}

static void
observe(Model *m, const Fixture *f, const RoutedEvent *events, size_t count)
{
	for(size_t i = 0; i < count; i++) {
		const RoutedEvent *e = &events[i]; CHECK(e->sequence == m->events++ && e->reason == ROUTED_OK);
		if(e->kind == CONSTELLATION_TRACE_BOOT || e->kind == CONSTELLATION_TRACE_TURN || e->kind == CONSTELLATION_TRACE_IDLE) continue;
		unsigned route = 0;
		while(route < 5 && (f->links[route].route.from != e->from || f->links[route].route.to != e->to || f->links[route].route.selector != e->selector)) route++;
		CHECK(route < 5); ConstellationQueue *q = &m->queues[route];
		if(e->kind == CONSTELLATION_TRACE_SEND_FULL) { CHECK(e->from == ROUTED_NONE && q->count == 4); m->full++; continue; }
		if(e->kind == CONSTELLATION_TRACE_SEND) {
			CHECK(q->count < 4);
			if(e->from == ROUTED_NONE) { CHECK(e->message.length == 1); m->accepted++; }
			else {
				CHECK(m->send_pending[e->from]);
				CHECK(memcmp(&m->next_send[e->from], &e->message, sizeof(e->message)) == 0);
				m->send_pending[e->from] = false;
			}
			q->messages[(q->head + q->count) % 4] = e->message; q->count++;
		} else {
			CHECK(e->kind == CONSTELLATION_TRACE_DELIVER && q->count > 0);
			CHECK(memcmp(&q->messages[q->head], &e->message, sizeof(e->message)) == 0);
			q->head = (q->head + 1) % 4; q->count--;
			unsigned to = e->to;
			if(e->from == ROUTED_NONE) {
				m->state[to] = e->message.data[0]; m->hash[to] = (uint8_t)(2 * m->hash[to] + m->state[to]);
				m->count[to]++; m->delivered++;
			} else {
				CHECK(e->message.length == 2 && e->message.data[0] > 0);
				m->internal[to]++;
				ConstellationMessage next = e->message; next.data[0]--; next.data[1] ^= m->state[to];
				if(next.data[0]) { CHECK(!m->send_pending[to]); m->next_send[to] = next; m->send_pending[to] = true; }
				else { m->retired[to]++; m->final_xor[to] ^= next.data[1]; }
			}
		}
	}
	for(unsigned i = 0; i < 5; i++) CHECK(memcmp(&m->queues[i], &f->links[i].queue, sizeof(m->queues[i])) == 0);
	for(unsigned i = 0; i < 3; i++) {
		const uint8_t *ram = f->nodes[i].uxn.ram;
		CHECK(!m->send_pending[i]);
		CHECK((unsigned)ram[0] * 256 + ram[1] == m->count[i] && ram[2] == m->hash[i] && ram[3] == m->state[i]);
		CHECK((unsigned)ram[6] * 256 + ram[7] == m->internal[i]);
		CHECK(ram[8] == m->retired[i] && ram[9] == m->final_xor[i]);
	}
}

static uint8_t
test_recorded_input(bool changed)
{
	/* A synthetic live source of button-state bytes. Replay reads the captured
	 * records, including rejected attempts, not this source schedule. */
	const Input source[] = {{0,7,1,0}, {0,7,0,0}, {0,7,2,0}, {0,7,0,0}, {0,7,4,ROUTED_INPUT_FULL},
		{1,7,4,ROUTED_INPUT_FULL}, {4,7,4,0}, {10,7,0,0}, {16,7,8,0}, {32,7,0,0}, {64,7,1,0}, {100,7,0,0}};
	Input log[LOG_CAPACITY]; unsigned logged = 0, replayed = 0, turn = 0;
	Fixture *a = fresh(true), *b = fresh(true); Model model = {0};
	for(unsigned i = 0; i < 3; i++) { model.send_pending[i] = true; model.next_send[i].length = 2; model.next_send[i].data[0] = 64; }
	CHECK(routed_boot(&a->host)); CHECK(routed_boot(&b->host));
	RoutedEvent events[TRACE], pending[TRACE], replay[TRACE]; size_t count, pending_count = 0, replay_count;
	CHECK(routed_take_trace(&a->host, events, TRACE, &count)); observe(&model, a, events, count);
	CHECK(routed_take_trace(&b->host, replay, TRACE, &replay_count));
	CHECK(count == replay_count && memcmp(events, replay, count * sizeof(*events)) == 0); same(a, b, true);
	for(;;) {
		while(logged < sizeof(source) / sizeof(*source) && source[logged].after_turn == turn) {
			CHECK(logged < LOG_CAPACITY && !routed_quiescent(&a->host));
			log[logged] = source[logged];
			if(changed && logged == 0) log[logged].value = 3;
			Input *input = &log[logged++];
			input->result = routed_input(&a->host, input->selector, &input->value, 1);
			CHECK(input->result == source[logged - 1].result);
		}
		while(replayed < logged) {
			const Input *input = &log[replayed++]; CHECK(input->after_turn == turn);
			CHECK(routed_input(&b->host, input->selector, &input->value, 1) == input->result);
		}
		same(a, b, false);
		CHECK(routed_take_trace(&a->host, events, TRACE, &count)); observe(&model, a, events, count);
		CHECK(pending_count + count <= TRACE); memcpy(pending + pending_count, events, count * sizeof(*events)); pending_count += count;
		bool done = logged == sizeof(source) / sizeof(*source) && routed_quiescent(&a->host);
		if(turn % 7 == 0 || done) {
			CHECK(routed_take_trace(&b->host, replay, TRACE, &replay_count));
			CHECK(pending_count == replay_count && memcmp(pending, replay, replay_count * sizeof(*replay)) == 0); pending_count = 0;
		}
		if(done) break;
		CHECK(turn < 400); CHECK(routed_step(&a->host)); CHECK(routed_step(&b->host)); turn++;
	}
	same(a, b, true);
	CHECK(routed_quiescent(&b->host) && logged == replayed && pending_count == 0);
	CHECK(model.full == 2 && model.accepted == 10 && model.delivered == 10);
	CHECK(log[4].result == ROUTED_INPUT_FULL && log[5].result == ROUTED_INPUT_FULL && log[6].result == ROUTED_INPUT_ACCEPTED);
	CHECK(model.count[0] == 10 && a->nodes[0].uxn.ram[4] == ROUTED_NONE);
	unsigned internal = 0, retired = 0;
	for(unsigned i = 0; i < 3; i++) { internal += model.internal[i]; retired += model.retired[i]; }
	CHECK(internal == 192 && retired == 3 && model.events == a->host.trace_sequence);
	uint8_t result = model.final_xor[0] ^ model.final_xor[1] ^ model.final_xor[2];
	printf("Recorded input%s: %u turns, %" PRIu64 " exact events, %u attempts, 10 admitted/2 full, 192 internal deliveries; final xor=%u.\n", changed ? " (changed first byte)" : "", turn, model.events, logged, result);
	free(a); free(b); return result;
}

int main(void)
{
	test_boundary(); test_trace_exhaustion();
	uint8_t original = test_recorded_input(false), changed = test_recorded_input(true);
	CHECK(original != changed);
	printf("%u external-input checks passed\n", checks);
	return 0;
}
