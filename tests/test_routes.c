#include "constellation_routes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while(0)

/* The prototype's cartridge declaration. Names are for humans; local route
 * selectors, not these names or node roles, are the guest interface. */
static const char *const roms[] = {
	"build/routes-burst.rom", "build/routes-relay.rom", "build/routes-collect.rom"
};
static const RoutedRoute routes[] = {{0, 1, 1}, {0, 2, 2}, {1, 7, 2}};

typedef struct { RoutedHost host; RoutedNode nodes[3]; RoutedLink links[4]; } Fixture;

static Fixture *
fresh(void)
{
	Fixture *f = calloc(1, sizeof(*f));
	CHECK(f != NULL);
	CHECK(routed_init(&f->host, f->nodes, 3, f->links, routes, 3, 10000));
	return f;
}

static void
load_file(Fixture *f, unsigned id, const char *path)
{
	uint8_t bytes[UXN_RAM_SIZE - UXN_ROM_START + 1];
	FILE *file = fopen(path, "rb");
	CHECK(file != NULL);
	size_t size = fread(bytes, 1, sizeof(bytes), file);
	CHECK(!ferror(file)); CHECK(fclose(file) == 0);
	CHECK(routed_load(&f->host, id, bytes, size));
}

/* Compare guest and scheduler state, deliberately excluding host pointers. */
static void
same(const Fixture *a, const Fixture *b)
{
	CHECK(a->host.fault == b->host.fault);
	CHECK(a->host.trace_exhausted == b->host.trace_exhausted);
	CHECK(a->host.booted == b->host.booted);
	CHECK(a->host.next_node == b->host.next_node);
	CHECK(a->host.trace_count == b->host.trace_count);
	CHECK(a->host.trace_sequence == b->host.trace_sequence);
	CHECK(memcmp(a->host.trace, b->host.trace, sizeof(a->host.trace)) == 0);
	CHECK(memcmp(a->links, b->links, sizeof(a->links)) == 0);
	for(unsigned i = 0; i < 3; i++) {
		const RoutedNode *x = &a->nodes[i], *y = &b->nodes[i];
		CHECK(memcmp(x->uxn.ram, y->uxn.ram, sizeof(x->uxn.ram)) == 0);
		CHECK(memcmp(x->uxn.devices, y->uxn.devices, sizeof(x->uxn.devices)) == 0);
		CHECK(memcmp(&x->uxn.working, &y->uxn.working, sizeof(UxnStack)) == 0);
		CHECK(memcmp(&x->uxn.return_stack, &y->uxn.return_stack, sizeof(UxnStack)) == 0);
		CHECK(x->uxn.instructions == y->uxn.instructions);
		CHECK(x->next_incoming == y->next_incoming && x->next_writable == y->next_writable);
		CHECK(x->prefer_receive == y->prefer_receive);
		CHECK(x->source == y->source && x->writable == y->writable && x->loaded == y->loaded);
	}
}

static void
test_three_roms(void)
{
	Fixture *a = fresh(), *b = fresh();
	for(unsigned i = 0; i < 3; i++) { load_file(a, i, roms[i]); load_file(b, i, roms[i]); }
	CHECK(routed_boot(&a->host)); CHECK(routed_boot(&b->host)); same(a, b);
	CHECK(a->links[0].queue.count == 4 && a->links[0].waiting);
	CHECK(a->links[1].queue.count == 1); /* independent send despite full route 01 */
	CHECK(a->nodes[0].uxn.devices[ROUTED_SEND_ROUTE] == 2);
	unsigned turns = 0;
	while(!routed_quiescent(&a->host) && turns++ < 64) {
		CHECK(routed_step(&a->host)); CHECK(routed_step(&b->host)); same(a, b);
		for(unsigned i = 0; i < 3; i++) {
			CHECK(a->nodes[i].uxn.working.pointer == 0);
			CHECK(a->nodes[i].uxn.return_stack.pointer == 0);
		}
	}
	CHECK(turns == 18); CHECK(routed_quiescent(&a->host)); CHECK(routed_quiescent(&b->host));
	CHECK(a->host.trace_count == 51);
	CHECK(memcmp(a->nodes[0].uxn.ram, "\x06\x01\x01\x01\x01", 5) == 0);
	CHECK(a->nodes[1].uxn.ram[0] == 5 && a->nodes[2].uxn.ram[0] == 6);
	CHECK(memcmp(a->nodes[1].uxn.ram + 0x400, "\x01\x02\x03\x04\x05", 5) == 0);
	CHECK(memcmp(a->nodes[1].uxn.ram + 0x500, "\0\0\0\0\0", 5) == 0);
	CHECK(memcmp(a->nodes[2].uxn.ram + 0x400, "\xaa\x01\x02\x03\x04\x05", 6) == 0);
	CHECK(memcmp(a->nodes[2].uxn.ram + 0x500, "\0\x01\x01\x01\x01\x01", 6) == 0);
	unsigned delivered[3] = {0}, full = 0, wake = 0;
	for(size_t i = 0; i < a->host.trace_count; i++) {
		const RoutedEvent *e = &a->host.trace[i];
		if(e->kind == CONSTELLATION_TRACE_SEND_FULL) {
			CHECK(e->from == 0 && e->to == 1 && e->selector == 1 && e->message.data[0] == 5); full++;
		}
		if(e->kind == CONSTELLATION_TRACE_WRITABLE) {
			CHECK(e->from == 0 && e->to == 1 && e->selector == 1); wake++;
		}
		if(e->kind == CONSTELLATION_TRACE_DELIVER) {
			CHECK(e->message.length == 1);
			if(e->from == 0 && e->to == 1) { CHECK(e->selector == 1); CHECK(e->message.data[0] == ++delivered[0]); }
			else if(e->from == 0 && e->to == 2) { CHECK(e->selector == 2 && e->message.data[0] == 0xaa); delivered[1]++; }
			else { CHECK(e->from == 1 && e->to == 2 && e->selector == 7); CHECK(e->message.data[0] == ++delivered[2]); }
		}
	}
	CHECK(full == 1 && wake == 1 && delivered[0] == 5 && delivered[1] == 1 && delivered[2] == 5);
	puts("Three ROMs: 51 trace events, 18 turns, 11 exact deliveries, route-local recovery; full state replay matches after every turn.");
	free(a); free(b);
}

/* Small bytecode fixtures exercise the host contract independently of the
 * application's TAL. DEO always goes through the real unchanged processor. */
static size_t
deo(uint8_t *rom, size_t at, uint8_t value, uint8_t port)
{
	rom[at++] = 0x80; rom[at++] = value; rom[at++] = 0x80; rom[at++] = port; rom[at++] = 0x17;
	return at;
}

static void
load_stubs(Fixture *f, const uint8_t *first, size_t length, bool receive)
{
	uint8_t rom[128] = {0};
	if(receive) { size_t n = deo(rom, 0, 1, 0xd0); deo(rom, n, 0x70, 0xd1); }
	CHECK(routed_load(&f->host, 0, first, length));
	CHECK(routed_load(&f->host, 1, rom, sizeof(rom)));
	CHECK(routed_load(&f->host, 2, rom, sizeof(rom)));
}

static void
terminal(Fixture *f, RoutedFault reason)
{
	CHECK(f->host.fault == reason);
	Fixture *copy = malloc(sizeof(*copy)); CHECK(copy != NULL); memcpy(copy, f, sizeof(*copy));
	CHECK(!routed_step(&f->host)); CHECK(!routed_boot(&f->host));
	CHECK(!routed_load(&f->host, 0, (const uint8_t *)"", 0));
	CHECK(memcmp(copy, f, sizeof(*f)) == 0);
	free(copy);
}

static void
test_faults_and_lifecycle(void)
{
	Fixture *f = fresh(); uint8_t rom[128] = {0};
	CHECK(!routed_step(&f->host)); CHECK(!routed_boot(&f->host));
	CHECK(!routed_load(&f->host, 3, rom, sizeof(rom)));
	CHECK(!routed_load(&f->host, 0, rom, UXN_RAM_SIZE));
	load_stubs(f, rom, sizeof(rom), true);
	CHECK(!routed_load(&f->host, 0, rom, sizeof(rom)));
	CHECK(routed_boot(&f->host)); CHECK(!routed_boot(&f->host));
	CHECK(!routed_load(&f->host, 0, rom, sizeof(rom)));
	for(unsigned i = 0; i < 256 && routed_step(&f->host); i++) { }
	terminal(f, ROUTED_TRACE_FULL); CHECK(f->host.trace_count == 128); free(f);

	f = fresh(); size_t n = deo(rom, 0, 99, 0xdc); n = deo(rom, n, 1, 0xd9); rom[n] = 0;
	load_stubs(f, rom, sizeof(rom), true); CHECK(!routed_boot(&f->host)); terminal(f, ROUTED_BAD_ROUTE); free(f);

	f = fresh(); memset(rom, 0, sizeof(rom)); n = deo(rom, 0, 1, 0xdc); n = deo(rom, n, 1, 0xd9); rom[n] = 0;
	load_stubs(f, rom, sizeof(rom), false); CHECK(routed_boot(&f->host));
	CHECK(routed_step(&f->host)); CHECK(!routed_step(&f->host)); terminal(f, ROUTED_RECEIVE_VECTOR_MISSING); free(f);

	f = fresh(); memset(rom, 0, sizeof(rom)); n = deo(rom, 0, 1, 0xdc);
	for(unsigned i = 0; i < 5; i++) n = deo(rom, n, 1, 0xd9);
	load_stubs(f, rom, sizeof(rom), true); CHECK(routed_boot(&f->host));
	for(unsigned i = 0; i < 3; i++) CHECK(routed_step(&f->host));
	CHECK(!routed_step(&f->host)); terminal(f, ROUTED_WRITABLE_VECTOR_MISSING); free(f);

	f = fresh(); const uint8_t loop[] = {0x80, 0x01, 0x80, 0x00, 0x2c};
	load_stubs(f, loop, sizeof(loop), true); CHECK(!routed_boot(&f->host)); terminal(f, ROUTED_LIMIT); free(f);

	f = fresh(); RoutedRoute bad[] = {{0, 1, 1}, {0, 1, 2}};
	CHECK(!routed_init(&f->host, f->nodes, 3, f->links, bad, 2, 100)); terminal(f, ROUTED_BAD_CONFIG);
	bad[1] = (RoutedRoute){2, 1, 3};
	CHECK(!routed_init(&f->host, f->nodes, 3, f->links, bad, 2, 100));
	bad[1] = (RoutedRoute){2, ROUTED_NONE, 1};
	CHECK(!routed_init(&f->host, f->nodes, 3, f->links, bad, 2, 100));
	CHECK(!routed_init(&f->host, f->nodes, 0, f->links, routes, 3, 100));
	CHECK(!routed_init(&f->host, f->nodes, 3, f->links, routes, 3, 0));
	CHECK(!routed_init(&f->host, f->nodes, 256, f->links, routes, 3, 100));
	CHECK(!routed_init(&f->host, f->nodes, 3, f->links, routes, 256, 100));
	free(f);
}

static void
test_fault_trace_boundaries(void)
{
	/* Boot adds one event. Exercise a normal fault, the last available slot,
	 * an omitted fault event, and trace exhaustion before a later bad send. */
	const unsigned sends[] = {0, 126, 127, 128};
	for(unsigned case_id = 0; case_id < sizeof(sends) / sizeof(sends[0]); case_id++) {
		unsigned count = sends[case_id];
		uint8_t rom[1024] = {0};
		size_t n = deo(rom, 0, 1, 0xdc);
		for(unsigned i = 0; i < count; i++) n = deo(rom, n, 1, 0xd9);
		n = deo(rom, n, 99, 0xdc); n = deo(rom, n, 1, 0xd9);
		/* Execution finishes at BRK after the fault. A changed selector must
		 * not erase the fault's context, and later sends must be suppressed. */
		n = deo(rom, n, 1, 0xdc); deo(rom, n, 1, 0xd9);
		Fixture *a = fresh(), *b = fresh();
		load_stubs(a, rom, sizeof(rom), true); load_stubs(b, rom, sizeof(rom), true);
		CHECK(!routed_boot(&a->host)); CHECK(!routed_boot(&b->host)); same(a, b);
		RoutedFault expected = count == 128 ? ROUTED_TRACE_FULL : ROUTED_BAD_ROUTE;
		terminal(a, expected); terminal(b, expected);
		CHECK(a->host.trace_exhausted == (count >= 127));
		CHECK(a->host.trace_count == (count ? 128 : 2));
		CHECK(a->nodes[0].uxn.devices[0xdc] == 1);
		CHECK(a->nodes[1].uxn.instructions == 0 && a->nodes[2].uxn.instructions == 0);
		CHECK(a->links[0].queue.count == (count ? 4 : 0));
		const RoutedEvent *last = &a->host.trace[a->host.trace_count - 1];
		if(count < 127) {
			CHECK(last->kind == CONSTELLATION_TRACE_FAULT && last->reason == ROUTED_BAD_ROUTE);
			CHECK(last->from == 0 && last->to == ROUTED_NONE && last->selector == 99);
		} else {
			CHECK(last->kind == CONSTELLATION_TRACE_SEND_FULL);
			for(size_t i = 0; i < a->host.trace_count; i++) CHECK(a->host.trace[i].kind != CONSTELLATION_TRACE_FAULT);
		}
		CHECK(routed_init(&a->host, a->nodes, 3, a->links, routes, 3, 10000));
		CHECK(!a->host.trace_exhausted && a->host.fault == ROUTED_OK && a->host.trace_count == 0);
		free(a); free(b);
	}
}

static void
test_rewire_and_single_node(void)
{
	Fixture *f = fresh();
	const RoutedRoute rewired[] = {{0, 1, 2}, {0, 2, 1}, {2, 7, 1}};
	CHECK(routed_init(&f->host, f->nodes, 3, f->links, rewired, 3, 10000));
	load_file(f, 0, roms[0]); load_file(f, 1, roms[2]); load_file(f, 2, roms[1]);
	CHECK(routed_boot(&f->host));
	unsigned turns = 0;
	while(!routed_quiescent(&f->host) && turns++ < 64) CHECK(routed_step(&f->host));
	CHECK(routed_quiescent(&f->host));
	CHECK(memcmp(f->nodes[1].uxn.ram + 0x400, "\xaa\x01\x02\x03\x04\x05", 6) == 0);
	CHECK(memcmp(f->nodes[1].uxn.ram + 0x500, "\0\x02\x02\x02\x02\x02", 6) == 0);
	CHECK(f->nodes[0].uxn.ram[1] == 1 && f->nodes[2].uxn.ram[0] == 5);
	/* No obligatory three-node layout, routes, presentation node, or game tick. */
	CHECK(routed_init(&f->host, f->nodes, 1, NULL, NULL, 0, 100));
	const uint8_t empty[] = {0};
	CHECK(routed_load(&f->host, 0, empty, sizeof(empty)));
	CHECK(routed_boot(&f->host)); CHECK(routed_quiescent(&f->host));
	CHECK(routed_step(&f->host)); CHECK(f->host.next_node == 0);
	const RoutedRoute self = {0, 1, 0}; uint8_t rom[128] = {0};
	CHECK(routed_init(&f->host, f->nodes, 1, f->links, &self, 1, 100));
	size_t n = deo(rom, 0, 1, 0xd0); n = deo(rom, n, 0x70, 0xd1);
	n = deo(rom, n, 1, 0xdc); deo(rom, n, 1, 0xd9);
	CHECK(routed_load(&f->host, 0, rom, sizeof(rom)));
	CHECK(routed_boot(&f->host)); CHECK(!routed_quiescent(&f->host));
	CHECK(routed_step(&f->host)); CHECK(routed_quiescent(&f->host));
	free(f);
}

static void
test_trace_batches(void)
{
	Fixture *f = fresh(); const uint8_t empty[] = {0};
	RoutedEvent batch[128], sentinel[128]; size_t count = 777;
	load_stubs(f, empty, sizeof(empty), true); CHECK(routed_boot(&f->host));
	RoutedHost before = f->host;
	memset(batch, 0xa5, sizeof(batch)); memcpy(sentinel, batch, sizeof(batch));
	CHECK(!routed_take_trace(&f->host, batch, 2, &count));
	CHECK(!routed_take_trace(&f->host, NULL, 128, &count));
	CHECK(!routed_take_trace(&f->host, batch, 128, NULL));
	CHECK(count == 777 && memcmp(batch, sentinel, sizeof(batch)) == 0);
	CHECK(memcmp(&f->host, &before, sizeof(before)) == 0);
	CHECK(routed_take_trace(&f->host, batch, 128, &count));
	CHECK(count == 3 && f->host.trace_count == 0 && f->host.trace_sequence == 3);
	CHECK(memcmp(batch, before.trace, count * sizeof(*batch)) == 0);
	CHECK(memcmp(batch + count, sentinel + count, (128 - count) * sizeof(*batch)) == 0);
	CHECK(routed_take_trace(&f->host, NULL, 0, &count)); CHECK(count == 0);
	CHECK(routed_step(&f->host));
	CHECK(routed_take_trace(&f->host, batch, 128, &count));
	CHECK(count == 2 && batch[0].sequence == 3 && batch[1].sequence == 4);
	for(unsigned i = 0; i < 128 && routed_step(&f->host); i++) { }
	CHECK(f->host.trace_exhausted && f->host.fault == ROUTED_TRACE_FULL);
	uint64_t sequence = f->host.trace_sequence;
	CHECK(routed_take_trace(&f->host, batch, 128, &count));
	CHECK(count == 128 && f->host.trace_count == 0 && f->host.trace_sequence == sequence);
	CHECK(f->host.trace_exhausted); terminal(f, ROUTED_TRACE_FULL);
	/* Taking an omitted-fault trace must also retain its original reason. */
	CHECK(routed_init(&f->host, f->nodes, 3, f->links, routes, 3, 10000));
	uint8_t rom[1024] = {0}; size_t n = deo(rom, 0, 1, 0xdc);
	for(unsigned i = 0; i < 127; i++) n = deo(rom, n, 1, 0xd9);
	n = deo(rom, n, 99, 0xdc); deo(rom, n, 1, 0xd9);
	load_stubs(f, rom, sizeof(rom), true); CHECK(!routed_boot(&f->host));
	CHECK(routed_take_trace(&f->host, batch, 128, &count));
	CHECK(count == 128 && f->host.trace_exhausted); terminal(f, ROUTED_BAD_ROUTE);
	/* The sequence number must never silently wrap, even when space remains. */
	CHECK(routed_init(&f->host, f->nodes, 3, f->links, routes, 3, 10000));
	CHECK(!f->host.trace_exhausted && f->host.trace_sequence == 0);
	load_stubs(f, empty, sizeof(empty), true); f->host.trace_sequence = UINT64_MAX - 1;
	CHECK(!routed_boot(&f->host));
	CHECK(f->host.trace_exhausted && f->host.trace_sequence == UINT64_MAX);
	CHECK(routed_take_trace(&f->host, batch, 128, &count));
	CHECK(count == 1 && batch[0].sequence == UINT64_MAX - 1); terminal(f, ROUTED_TRACE_FULL);
	free(f);
}

static void
test_multiple_wakeups(void)
{
	Fixture *f = fresh(); uint8_t rom[240] = {0};
	size_t n = deo(rom, 0, 1, 0xda); n = deo(rom, n, 0xc0, 0xdb);
	for(unsigned selector = 1; selector <= 2; selector++) {
		n = deo(rom, n, (uint8_t)selector, 0xdc);
		for(unsigned i = 0; i < 5; i++) n = deo(rom, n, 1, 0xd9);
	}
	/* Log each genuine notification selector, even after attempted spoofing. */
	n = deo(rom, 0xc0, 99, 0xde);
	const uint8_t log_wake[] = {
		0x80, 0xde, 0x16, /* de DEI */
		0x80, 0, 0x10, 0x80, 0, 0x04, /* 00 LDZ 00 SWP */
		0xa0, 0x06, 0, 0x38, 0x15, /* 0600 ADD2 STA */
		0x80, 0, 0x10, 0x01, 0x80, 0, 0x11, 0 /* increment count, BRK */
	};
	memcpy(rom + n, log_wake, sizeof(log_wake));
	load_stubs(f, rom, sizeof(rom), true); CHECK(routed_boot(&f->host));
	CHECK(f->links[0].waiting && f->links[1].waiting);
	unsigned turns = 0;
	while(!routed_quiescent(&f->host) && turns++ < 64) CHECK(routed_step(&f->host));
	CHECK(routed_quiescent(&f->host));
	CHECK(f->nodes[0].uxn.ram[0] == 2);
	CHECK(f->nodes[0].uxn.ram[0x600] == 1 && f->nodes[0].uxn.ram[0x601] == 2);
	CHECK(f->nodes[0].writable == ROUTED_NONE);
	CHECK(!f->links[0].waiting && !f->links[1].waiting);
	free(f);
}

static void
test_fairness_and_payloads(void)
{
	for(unsigned length = 0; length <= 255; length += 255) {
		Fixture *f = fresh();
		const RoutedRoute incoming[] = {{0, 1, 2}, {1, 1, 2}};
		CHECK(routed_init(&f->host, f->nodes, 3, f->links, incoming, 2, 10000));
		uint8_t send[128] = {0}, receive[128] = {0};
		size_t n = deo(send, 0, 1, 0xdc);
		n = deo(send, n, (uint8_t)length, 0xd8);
		n = deo(send, n, 0xff, 0xd6); n = deo(send, n, 0xff, 0xd7);
		for(unsigned i = 0; i < 4; i++) n = deo(send, n, 1, 0xd9);
		n = deo(receive, 0, 1, 0xd0); n = deo(receive, n, 0x70, 0xd1);
		n = deo(receive, n, 0xff, 0xd2); deo(receive, n, 0xff, 0xd3);
		/* Try to spoof dd, then read it through DEI and record the true sender. */
		n = deo(receive, 0x70, 99, 0xdd);
		const uint8_t record_source[] = {0x80, 0xdd, 0x16, 0xa0, 0x06, 0x00, 0x15, 0};
		memcpy(receive + n, record_source, sizeof(record_source));
		CHECK(routed_load(&f->host, 0, send, sizeof(send)));
		CHECK(routed_load(&f->host, 1, send, sizeof(send)));
		CHECK(routed_load(&f->host, 2, receive, sizeof(receive)));
		for(unsigned id = 0; id < 2; id++)
			for(unsigned i = 0; i < 255; i++) f->nodes[id].uxn.ram[(uint16_t)(0xffff + i)] = (uint8_t)(id * 128 + i);
		CHECK(routed_boot(&f->host));
		CHECK(f->links[0].queue.count == 4 && f->links[1].queue.count == 4);
		/* Send copies are immutable even if the original RAM changes later. */
		f->nodes[0].uxn.ram[0xffff] = f->nodes[1].uxn.ram[0xffff] = 99;
		unsigned turns = 0, delivered = 0;
		while(!routed_quiescent(&f->host) && turns++ < 64) CHECK(routed_step(&f->host));
		CHECK(routed_quiescent(&f->host));
		for(size_t i = 0; i < f->host.trace_count; i++) {
			const RoutedEvent *e = &f->host.trace[i];
			if(e->kind != CONSTELLATION_TRACE_DELIVER) continue;
			CHECK(e->from == delivered % 2 && e->to == 2 && e->message.length == length);
			for(unsigned j = 0; j < length; j++) CHECK(e->message.data[j] == (uint8_t)(e->from * 128 + j));
			delivered++;
		}
		CHECK(delivered == 8); CHECK(f->nodes[2].uxn.ram[0x600] == 1);
		for(unsigned j = 0; j < length; j++) CHECK(f->nodes[2].uxn.ram[(uint16_t)(0xffff + j)] == (uint8_t)(128 + j));
		CHECK(f->nodes[2].source == ROUTED_NONE);
		free(f);
	}
}

static void
test_recovery_does_not_starve_receive(void)
{
	Fixture *a = fresh(), *b = fresh();
	const RoutedRoute graph[] = {{0,1,1}, {1,1,0}, {2,1,0}};
	uint8_t sender[256] = {0}, receiver[256] = {0}, empty[] = {0};
	size_t n = deo(sender, 0, 1, 0xd0); n = deo(sender, n, 0x70, 0xd1);
	n = deo(sender, n, 1, 0xda); n = deo(sender, n, 0x90, 0xdb); n = deo(sender, n, 1, 0xdc);
	for(unsigned i = 0; i < 5; i++) n = deo(sender, n, 1, 0xd9);
	const uint8_t counter[] = {0x80,0,0x10,0x01,0x80,0,0x11,0};
	memcpy(sender + 0x70, counter, sizeof(counter));
	/* Every writable callback refills until full and rearms itself. */
	n = deo(sender, 0x90, 1, 0xd9);
	const uint8_t refill[] = {0x80,0xd9,0x16,0xa0,1,0x90,0x2d,0};
	memcpy(sender + n, refill, sizeof(refill));
	n = deo(receiver, 0, 1, 0xd0); n = deo(receiver, n, 0x70, 0xd1); deo(receiver, n, 1, 0xdc);
	/* The receiver sends one message back on its first delivery. */
	const uint8_t once[] = {0x80,0,0x10,0xa0,1,0x90,0x2d,0x80,1,0x80,0,0x11};
	memcpy(receiver + 0x70, once, sizeof(once)); deo(receiver, 0x70 + sizeof(once), 1, 0xd9);
	Fixture *runs[] = {a, b};
	for(unsigned i = 0; i < 2; i++) {
		Fixture *f = runs[i];
		CHECK(routed_init(&f->host, f->nodes, 3, f->links, graph, 3, 10000));
		CHECK(routed_load(&f->host, 0, sender, sizeof(sender)));
		CHECK(routed_load(&f->host, 1, receiver, sizeof(receiver)));
		CHECK(routed_load(&f->host, 2, empty, sizeof(empty)));
		CHECK(routed_boot(&f->host));
	}
	same(a, b);
	RoutedEvent events[128], replay[128]; size_t count, replay_count;
	unsigned wakes = 0, deliveries = 0;
	for(unsigned turn = 1; turn <= 300; turn++) {
		CHECK(routed_step(&a->host)); CHECK(routed_step(&b->host)); same(a, b);
		CHECK(routed_take_trace(&a->host, events, 128, &count));
		CHECK(routed_take_trace(&b->host, replay, 128, &replay_count));
		CHECK(count == replay_count && memcmp(events, replay, count * sizeof(*events)) == 0);
		for(size_t i = 0; i < count; i++) {
			wakes += events[i].kind == CONSTELLATION_TRACE_WRITABLE && events[i].from == 0;
			deliveries += events[i].kind == CONSTELLATION_TRACE_DELIVER && events[i].to == 1;
		}
		if(turn == 7) CHECK(a->nodes[0].uxn.ram[0] == 1 && a->links[1].queue.count == 0);
	}
	CHECK(wakes >= 90 && deliveries == 100);
	CHECK(a->nodes[0].uxn.ram[0] == 1 && a->links[1].queue.count == 0);
	CHECK(a->host.fault == ROUTED_OK && !routed_quiescent(&a->host));
	printf("Fair dispatch: incoming message handled by turn 7; %u recovery callbacks and %u downstream deliveries across 300 turns, replay identical.\n", wakes, deliveries);
	free(a); free(b);
}

static void
test_callback_class_and_route_fairness(void)
{
	Fixture *f = fresh();
	const RoutedRoute graph[] = {{0,1,1}, {0,2,2}, {1,7,0}, {2,8,0}};
	uint8_t rom[128] = {0};
	size_t n = deo(rom, 0, 1, 0xd0); n = deo(rom, n, 0x70, 0xd1);
	n = deo(rom, n, 1, 0xda); deo(rom, n, 0x70, 0xdb);
	CHECK(routed_init(&f->host, f->nodes, 3, f->links, graph, 4, 10000));
	for(unsigned i = 0; i < 3; i++) CHECK(routed_load(&f->host, i, rom, sizeof(rom)));
	CHECK(routed_boot(&f->host));
	CHECK(!f->nodes[0].prefer_receive);
	RoutedEvent events[128]; size_t count;
	CHECK(routed_take_trace(&f->host, events, 128, &count));
	/* Inject readiness to isolate scheduler policy from guest protocol choices.
	 * Sixteen mixed turns, then receive-only, writable-only twice, idle,
	 * receive-only: unavailable preferred classes must never waste a turn. */
	for(unsigned turn = 0; turn < 21; turn++) {
		bool outgoing = turn < 16 || turn == 17 || turn == 18;
		bool incoming = turn < 16 || turn == 16 || turn == 20;
		for(unsigned i = 0; i < 4; i++) {
			memset(&f->links[i].queue, 0, sizeof(f->links[i].queue));
			f->links[i].waiting = outgoing && i < 2;
			f->links[i].queue.count = incoming && i >= 2;
		}
		RoutedNode *node = &f->nodes[0];
		size_t old_incoming = node->next_incoming, old_writable = node->next_writable;
		bool old_preference = node->prefer_receive;
		CHECK(f->host.next_node == 0);
		CHECK(routed_step(&f->host));
		CHECK(routed_take_trace(&f->host, events, 128, &count));
		CHECK(count == 2 && events[0].kind == CONSTELLATION_TRACE_TURN && events[0].from == 0);
		if(turn == 19) {
			CHECK(events[1].kind == CONSTELLATION_TRACE_IDLE);
			CHECK(node->prefer_receive == old_preference);
			CHECK(node->next_incoming == old_incoming && node->next_writable == old_writable);
		} else if((turn < 16 && turn % 2 == 0) || turn == 17 || turn == 18) {
			unsigned route = turn < 16 ? (turn / 2) % 2 : turn - 17;
			CHECK(events[1].kind == CONSTELLATION_TRACE_WRITABLE && events[1].selector == route + 1);
			CHECK(node->prefer_receive && node->next_incoming == old_incoming);
			CHECK(node->next_writable == route + 1 && !f->links[route].waiting);
			CHECK(f->links[1 - route].waiting);
			CHECK(f->links[2].queue.count == incoming && f->links[3].queue.count == incoming);
		} else {
			unsigned route = turn < 16 ? 2 + (turn / 2) % 2 : (turn == 16 ? 2 : 3);
			CHECK(events[1].kind == CONSTELLATION_TRACE_DELIVER && events[1].from == route - 1);
			CHECK(!node->prefer_receive && node->next_writable == old_writable);
			CHECK(node->next_incoming == (route + 1) % 4 && f->links[route].queue.count == 0);
			CHECK(f->links[5 - route].queue.count == 1);
			CHECK(f->links[0].waiting == outgoing && f->links[1].waiting == outgoing);
		}
		/* The other nodes still receive exactly their normal scheduled turns. */
		CHECK(routed_step(&f->host)); CHECK(routed_step(&f->host));
		CHECK(routed_take_trace(&f->host, events, 128, &count));
		CHECK(count == 4 && events[0].from == 1 && events[2].from == 2);
	}
	f->nodes[0].prefer_receive = true;
	CHECK(routed_init(&f->host, f->nodes, 3, f->links, graph, 4, 10000));
	CHECK(!f->nodes[0].prefer_receive);
	free(f);
}

int main(void)
{
	test_three_roms(); test_faults_and_lifecycle();
	test_fault_trace_boundaries();
	test_trace_batches();
	test_rewire_and_single_node(); test_fairness_and_payloads(); test_multiple_wakeups();
	test_recovery_does_not_starve_receive();
	test_callback_class_and_route_fairness();
	printf("%u routed-host checks passed\n", checks);
	return 0;
}
