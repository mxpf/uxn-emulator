#include "constellation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int tests_run;

enum {
	TEST_VECTOR = 0x0180,
	TEST_DATA = 0x01c0,
	TEST_VECTOR_OFFSET = TEST_VECTOR - UXN_ROM_START,
	TEST_DATA_OFFSET = TEST_DATA - UXN_ROM_START
};

#define CHECK(expression) do { \
	tests_run++; \
	if(!(expression)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
		exit(1); \
	} \
} while(0)

static void
put_short(uint8_t *rom, size_t *cursor, uint16_t value)
{
	rom[(*cursor)++] = 0xa0;
	rom[(*cursor)++] = (uint8_t)(value >> 8);
	rom[(*cursor)++] = (uint8_t)value;
}

static void
put_byte(uint8_t *rom, size_t *cursor, uint8_t value)
{
	rom[(*cursor)++] = 0x80;
	rom[(*cursor)++] = value;
}

static void
put_deo2(uint8_t *rom, size_t *cursor, uint16_t value, uint8_t port)
{
	put_short(rom, cursor, value);
	put_byte(rom, cursor, port);
	rom[(*cursor)++] = 0x37;
}

static void
put_deo(uint8_t *rom, size_t *cursor, uint8_t value, uint8_t port)
{
	put_byte(rom, cursor, value);
	put_byte(rom, cursor, port);
	rom[(*cursor)++] = 0x17;
}

static void
make_receiver(uint8_t *rom, size_t size, bool reply)
{
	size_t cursor = 0;
	memset(rom, 0, size);
	put_deo2(rom, &cursor, TEST_VECTOR, CONSTELLATION_PORT_VECTOR);
	put_deo2(rom, &cursor, 0x0300, CONSTELLATION_PORT_RECEIVE_ADDRESS);
	rom[cursor] = 0;
	if(reply) {
		cursor = TEST_VECTOR_OFFSET;
		put_deo2(rom, &cursor, TEST_DATA,
			CONSTELLATION_PORT_SEND_ADDRESS);
		put_deo(rom, &cursor, 4, CONSTELLATION_PORT_SEND_LENGTH);
		put_deo(rom, &cursor, 1, CONSTELLATION_PORT_SEND);
		rom[cursor] = 0;
		memcpy(&rom[TEST_DATA_OFFSET], "PONG", 4);
	} else {
		cursor = TEST_VECTOR_OFFSET;
		put_byte(rom, &cursor, 1);
		put_byte(rom, &cursor, 0);
		rom[cursor++] = 0x11;
		rom[cursor] = 0;
	}
}

static void
make_sender(uint8_t *rom, size_t size, unsigned int sends)
{
	size_t cursor = 0;
	unsigned int i;
	memset(rom, 0, size);
	put_deo2(rom, &cursor, TEST_VECTOR, CONSTELLATION_PORT_VECTOR);
	put_deo2(rom, &cursor, 0x0300, CONSTELLATION_PORT_RECEIVE_ADDRESS);
	put_deo2(rom, &cursor, TEST_DATA, CONSTELLATION_PORT_SEND_ADDRESS);
	put_deo(rom, &cursor, 4, CONSTELLATION_PORT_SEND_LENGTH);
	for(i = 0; i < sends; i++)
		put_deo(rom, &cursor, 1, CONSTELLATION_PORT_SEND);
	rom[cursor] = 0;
	memcpy(&rom[TEST_DATA_OFFSET], "PING", 4);
	cursor = TEST_VECTOR_OFFSET;
	put_byte(rom, &cursor, 1);
	put_byte(rom, &cursor, 0);
	rom[cursor++] = 0x11;
	rom[cursor] = 0;
}

static Constellation *
new_demo(void)
{
	Constellation *constellation = calloc(1, sizeof(*constellation));
	uint8_t sender[0xd0];
	uint8_t receiver[0xd0];
	CHECK(constellation != NULL);
	make_sender(sender, sizeof(sender), 1);
	make_receiver(receiver, sizeof(receiver), true);
	constellation_init(constellation, 10000);
	CHECK(constellation_load(constellation, CONSTELLATION_A,
		sender, sizeof(sender)));
	CHECK(constellation_load(constellation, CONSTELLATION_B,
		receiver, sizeof(receiver)));
	CHECK(constellation_boot(constellation));
	return constellation;
}

static void
test_ping_pong(void)
{
	Constellation *constellation = new_demo();
	const Uxn *a;
	const Uxn *b;
	CHECK(!constellation_is_quiescent(constellation));
	CHECK(constellation_step(constellation));
	CHECK(constellation_step(constellation));
	CHECK(constellation_is_quiescent(constellation));
	a = constellation_uxn(constellation, CONSTELLATION_A);
	b = constellation_uxn(constellation, CONSTELLATION_B);
	CHECK(memcmp(&a->ram[0x0300], "PONG", 4) == 0);
	CHECK(memcmp(&b->ram[0x0300], "PING", 4) == 0);
	CHECK(a->ram[0] == 1);
	CHECK(!constellation_has_fault(constellation));
	free(constellation);
}

static void
test_full_queue_fails(void)
{
	Constellation *constellation = calloc(1, sizeof(*constellation));
	uint8_t sender[0xd0];
	uint8_t receiver[0xd0];
	size_t i;
	size_t full_events = 0;
	CHECK(constellation != NULL);
	make_sender(sender, sizeof(sender), 5);
	make_receiver(receiver, sizeof(receiver), false);
	constellation_init(constellation, 10000);
	CHECK(constellation_load(constellation, CONSTELLATION_A,
		sender, sizeof(sender)));
	CHECK(constellation_load(constellation, CONSTELLATION_B,
		receiver, sizeof(receiver)));
	CHECK(constellation_boot(constellation));
	CHECK(constellation->queues[CONSTELLATION_A].count == 4);
	CHECK(constellation_uxn(constellation, CONSTELLATION_A)->
		devices[CONSTELLATION_PORT_SEND] == 0);
	for(i = 0; i < constellation_trace_count(constellation); i++)
		if(constellation_trace_event(constellation, i)->kind ==
			CONSTELLATION_TRACE_SEND_FULL)
			full_events++;
	CHECK(full_events == 1);
	free(constellation);
}

static void
check_same_state(const Constellation *first, const Constellation *second)
{
	CHECK(first->trace_count == second->trace_count);
	CHECK(memcmp(first->trace, second->trace,
		first->trace_count * sizeof(first->trace[0])) == 0);
	CHECK(memcmp(first->endpoints[CONSTELLATION_A].uxn.ram,
		second->endpoints[CONSTELLATION_A].uxn.ram, UXN_RAM_SIZE) == 0);
	CHECK(memcmp(first->endpoints[CONSTELLATION_B].uxn.ram,
		second->endpoints[CONSTELLATION_B].uxn.ram, UXN_RAM_SIZE) == 0);
	for(unsigned i = 0; i < 2; i++) {
		const Uxn *a = &first->endpoints[i].uxn;
		const Uxn *b = &second->endpoints[i].uxn;
		CHECK(memcmp(&a->working, &b->working, sizeof(a->working)) == 0);
		CHECK(memcmp(&a->return_stack, &b->return_stack,
			sizeof(a->return_stack)) == 0);
		CHECK(memcmp(a->devices, b->devices, sizeof(a->devices)) == 0);
		CHECK(a->instructions == b->instructions);
		CHECK(first->endpoints[i].faulted == second->endpoints[i].faulted);
		CHECK(first->endpoints[i].loaded == second->endpoints[i].loaded);
		CHECK(first->endpoints[i].waiting_for_space ==
			second->endpoints[i].waiting_for_space);
	}
	CHECK(memcmp(first->queues, second->queues, sizeof(first->queues)) == 0);
	CHECK(first->next_turn == second->next_turn);
	CHECK(first->trace_exhausted == second->trace_exhausted);
	CHECK(first->fault_reason == second->fault_reason);
	CHECK(first->booted == second->booted);
	CHECK(first->instruction_ceiling == second->instruction_ceiling);
}

static void
test_replay_is_identical(void)
{
	Constellation *first = new_demo();
	Constellation *second = new_demo();
	while(!constellation_is_quiescent(first))
		CHECK(constellation_step(first));
	while(!constellation_is_quiescent(second))
		CHECK(constellation_step(second));
	check_same_state(first, second);
	free(first);
	free(second);
}

/* Execute actual DEO instructions to exercise the public device boundary. */
static void
send_bytes(Constellation *c, unsigned id, uint16_t address, uint8_t length)
{
	uint8_t program[32] = {0};
	size_t cursor = 0;
	Uxn *u = &c->endpoints[id].uxn;
	put_deo2(program, &cursor, address, CONSTELLATION_PORT_SEND_ADDRESS);
	put_deo(program, &cursor, length, CONSTELLATION_PORT_SEND_LENGTH);
	put_deo(program, &cursor, 1, CONSTELLATION_PORT_SEND);
	memcpy(&u->ram[0x500], program, sizeof(program));
	CHECK(uxn_eval(u, 0x500, 100) == UXN_STOP_BREAK);
}

static Constellation *
empty_pair(void)
{
	Constellation *c = calloc(1, sizeof(*c));
	uint8_t rom[0xd0];
	CHECK(c != NULL);
	make_receiver(rom, sizeof(rom), false);
	constellation_init(c, 100);
	CHECK(constellation_load(c, CONSTELLATION_A, rom, sizeof(rom)));
	CHECK(constellation_load(c, CONSTELLATION_B, rom, sizeof(rom)));
	CHECK(constellation_boot(c));
	return c;
}

static void
test_boundaries_and_fifo(void)
{
	Constellation *c = empty_pair();
	Uxn *a = &c->endpoints[0].uxn;
	Uxn *b = &c->endpoints[1].uxn;
	for(unsigned i = 0; i < 255; i++)
		a->ram[(uint16_t)(0xff80 + i)] = (uint8_t)i;
	send_bytes(c, 0, 0xff80, 255);
	memset(&a->ram[0xff80], 0, 128); /* queued data is an independent copy */
	CHECK(constellation_step(c));
	for(unsigned i = 0; i < 255; i++)
		CHECK(b->ram[0x300 + i] == (uint8_t)i);
	CHECK(b->devices[CONSTELLATION_PORT_RECEIVE_LENGTH] == 255);
	CHECK(b->devices[CONSTELLATION_PORT_RECEIVE_READY] == 0);
	CHECK(constellation_step(c)); /* A idle */
	b->ram[0x300] = 0x7f;
	send_bytes(c, 0, 0, 0);
	CHECK(constellation_step(c));
	CHECK(b->devices[CONSTELLATION_PORT_RECEIVE_LENGTH] == 0);
	CHECK(b->ram[0x300] == 0x7f);
	CHECK(constellation_step(c));
	for(unsigned i = 0; i < 4; i++) {
		a->ram[0x400] = (uint8_t)(10 + i);
		send_bytes(c, 0, 0x400, 1);
	}
	for(unsigned i = 0; i < 4; i++) {
		CHECK(constellation_step(c));
		CHECK(b->ram[0x300] == 10 + i);
		CHECK(constellation_step(c));
	}
	/* Queue head has wrapped; ensure it can be reused. */
	a->ram[0x400] = 99;
	send_bytes(c, 0, 0x400, 1);
	CHECK(constellation_step(c));
	CHECK(b->ram[0x300] == 99);
	free(c);
}

static Constellation *
run_retry_without_reply(void)
{
	Constellation *c = empty_pair();
	Uxn *a = &c->endpoints[0].uxn;
	uint8_t retry[16] = {0};
	size_t cursor = 0;
	put_deo(retry, &cursor, 1, CONSTELLATION_PORT_SEND);
	memcpy(&a->ram[0x600], retry, sizeof(retry));
	a->devices[CONSTELLATION_PORT_WRITABLE_VECTOR] = 6;
	a->devices[CONSTELLATION_PORT_WRITABLE_VECTOR + 1] = 0;
	for(unsigned i = 0; i < 5; i++) {
		a->ram[0x400] = (uint8_t)i;
		send_bytes(c, 0, 0x400, 1);
	}
	CHECK(a->devices[CONSTELLATION_PORT_SEND] == 0);
	CHECK(c->endpoints[0].waiting_for_space);
	CHECK(constellation_step(c)); /* B consumes one, sends no reply */
	CHECK(constellation_step(c)); /* A is woken and retries fifth byte */
	CHECK(a->devices[CONSTELLATION_PORT_SEND] == 1);
	CHECK(!c->endpoints[0].waiting_for_space);
	for(unsigned i = 1; i < 5; i++) {
		CHECK(constellation_step(c));
		CHECK(c->endpoints[1].uxn.ram[0x300] == i);
		CHECK(constellation_step(c));
	}
	CHECK(constellation_is_quiescent(c));
	return c;
}

static void
test_retry_without_reply(void)
{
	Constellation *a = run_retry_without_reply();
	Constellation *b = run_retry_without_reply();
	check_same_state(a, b);
	free(a);
	free(b);
}

static void
test_faults_and_trace_limit(void)
{
	Constellation *c = calloc(1, sizeof(*c));
	const uint8_t loop[] = {0x40, 0xff, 0xfd};
	CHECK(c != NULL);
	constellation_init(c, 0);
	CHECK(constellation_has_fault(c));
	CHECK(!constellation_boot(c));
	constellation_init(c, 10);
	CHECK(constellation_load(c, CONSTELLATION_B, loop, sizeof(loop)));
	CHECK(!constellation_boot(c));
	CHECK(c->endpoints[1].uxn.instructions == 10);
	CHECK(!constellation_step(c));
	CHECK(!constellation_boot(c));
	CHECK(c->endpoints[1].uxn.instructions == 10);
	free(c);
	c = empty_pair();
	c->endpoints[1].uxn.devices[CONSTELLATION_PORT_VECTOR] = 0;
	c->endpoints[1].uxn.devices[CONSTELLATION_PORT_VECTOR + 1] = 0;
	send_bytes(c, 0, 0, 0);
	CHECK(!constellation_step(c));
	CHECK(constellation_has_fault(c));
	CHECK(!constellation_step(c));
	free(c);
	c = empty_pair();
	for(unsigned i = 0; i < 128 && !constellation_has_fault(c); i++)
		(void)constellation_step(c);
	CHECK(c->trace_exhausted);
	CHECK(c->trace_count == CONSTELLATION_TRACE_CAPACITY);
	CHECK(!constellation_step(c));
	free(c);
}

static void
test_assembled_demo(char **paths)
{
	Constellation *c = calloc(1, sizeof(*c));
	uint8_t bytes[1024];
	CHECK(c != NULL);
	constellation_init(c, 1000);
	for(unsigned id = 0; id < 2; id++) {
		FILE *f = fopen(paths[id], "rb");
		size_t length;
		CHECK(f != NULL);
		length = fread(bytes, 1, sizeof(bytes), f);
		CHECK(!ferror(f) && feof(f));
		fclose(f);
		CHECK(constellation_load(c, (ConstellationEndpointId)id, bytes, length));
	}
	CHECK(constellation_boot(c));
	CHECK(constellation_step(c));
	CHECK(constellation_step(c));
	CHECK(constellation_is_quiescent(c));
	CHECK(!constellation_has_fault(c));
	CHECK(memcmp(&c->endpoints[0].uxn.ram[0x300], "PONG", 4) == 0);
	CHECK(memcmp(&c->endpoints[1].uxn.ram[0x300], "PING", 4) == 0);
	CHECK(c->endpoints[0].uxn.ram[0] == 1);
	free(c);
}

static void
test_reload_rejected(void)
{
	Constellation *c = calloc(1, sizeof(*c));
	const uint8_t original[] = {0, 0, 0, 0x80, 0x42, 0x80, 0, 0x11, 0};
	const uint8_t replacement[] = {0x40, 0, 0};
	CHECK(c != NULL);
	constellation_init(c, 100);
	CHECK(constellation_load(c, CONSTELLATION_A, original, sizeof(original)));
	CHECK(!constellation_load(c, CONSTELLATION_A, replacement, sizeof(replacement)));
	CHECK(memcmp(&c->endpoints[0].uxn.ram[0x100], original,
		sizeof(original)) == 0);
	CHECK(constellation_boot(c));
	CHECK(c->endpoints[0].uxn.ram[0] == 0);
	constellation_init(c, 100);
	CHECK(constellation_load(c, CONSTELLATION_A, replacement, sizeof(replacement)));
	CHECK(constellation_boot(c));
	CHECK(c->endpoints[0].uxn.ram[0] == 0);
	free(c);
}

static Constellation *
run_fault_case(ConstellationFault reason, bool active_trace)
{
	Constellation *c = empty_pair();
	Uxn *b = &c->endpoints[1].uxn;
	if(reason == CONSTELLATION_FAULT_INVALID_CEILING) {
		constellation_init(c, 0);
	} else if(reason == CONSTELLATION_FAULT_WRITABLE_VECTOR) {
		for(unsigned i = 0; i < 5; i++) send_bytes(c, 0, 0x400, 0);
		CHECK(constellation_step(c));
		CHECK(!constellation_step(c));
	} else if(reason == CONSTELLATION_FAULT_TRACE_EXHAUSTED && !active_trace) {
		for(unsigned i = 0; i < 128 && !constellation_has_fault(c); i++)
			(void)constellation_step(c);
	} else {
		if(reason == CONSTELLATION_FAULT_RECEIVE_VECTOR) {
			b->devices[CONSTELLATION_PORT_VECTOR] = 0;
			b->devices[CONSTELLATION_PORT_VECTOR + 1] = 0;
		} else {
			b->devices[CONSTELLATION_PORT_VECTOR] = 8;
			b->devices[CONSTELLATION_PORT_VECTOR + 1] = 0;
			if(reason == CONSTELLATION_FAULT_INSTRUCTION_LIMIT) {
				const uint8_t loop[] = {0x40, 0xff, 0xfd};
				memcpy(&b->ram[0x800], loop, sizeof(loop));
			} else {
				uint8_t sends[1024] = {0};
				size_t cursor = 0;
				c->instruction_ceiling = 10000;
				for(unsigned i = 0; i < 130; i++)
					put_deo(sends, &cursor, 1, CONSTELLATION_PORT_SEND);
				memcpy(&b->ram[0x800], sends, sizeof(sends));
			}
		}
		send_bytes(c, 0, 0x400, 0);
		CHECK(!constellation_step(c));
	}
	CHECK(constellation_has_fault(c));
	CHECK(c->fault_reason == reason);
	if(reason != CONSTELLATION_FAULT_TRACE_EXHAUSTED) {
		CHECK(c->trace[c->trace_count - 1].kind == CONSTELLATION_TRACE_FAULT);
		CHECK(c->trace[c->trace_count - 1].reason == reason);
	} else {
		CHECK(c->trace_exhausted);
		CHECK(c->trace_count == CONSTELLATION_TRACE_CAPACITY);
	}
	{
		Constellation *snapshot = malloc(sizeof(*snapshot));
		CHECK(snapshot != NULL);
		memcpy(snapshot, c, sizeof(*snapshot));
		CHECK(!constellation_step(c));
		CHECK(!constellation_boot(c));
		check_same_state(snapshot, c);
		free(snapshot);
	}
	return c;
}

static void
test_fault_replays(void)
{
	for(ConstellationFault reason = CONSTELLATION_FAULT_INVALID_CEILING;
		reason <= CONSTELLATION_FAULT_TRACE_EXHAUSTED; reason++) {
		Constellation *a = run_fault_case(reason, false);
		Constellation *b = run_fault_case(reason, false);
		check_same_state(a, b);
		free(a);
		free(b);
	}
	{
		Constellation *a = run_fault_case(CONSTELLATION_FAULT_TRACE_EXHAUSTED, true);
		Constellation *b = run_fault_case(CONSTELLATION_FAULT_TRACE_EXHAUSTED, true);
		check_same_state(a, b);
		free(a);
		free(b);
	}
}

int
main(int argc, char **argv)
{
	test_ping_pong();
	test_full_queue_fails();
	test_replay_is_identical();
	test_boundaries_and_fifo();
	test_retry_without_reply();
	test_faults_and_trace_limit();
	test_reload_rejected();
	test_fault_replays();
	if(argc == 3) test_assembled_demo(argv + 1);
	printf("%u constellation checks passed\n", tests_run);
	return 0;
}
