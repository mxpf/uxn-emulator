#include "constellation_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while(0)
static const uint8_t idle[] = {0};

static RunnerRom
read_rom(const char *path)
{
	FILE *file = fopen(path, "rb"); CHECK(file);
	uint8_t *bytes = malloc(UXN_RAM_SIZE); CHECK(bytes);
	size_t length = fread(bytes, 1, UXN_RAM_SIZE, file);
	CHECK(!ferror(file)); CHECK(fclose(file) == 0);
	return (RunnerRom){bytes, length};
}

static void
same(const RoutedHost *a, const RoutedHost *b)
{
	CHECK(a->node_count == b->node_count && a->route_count == b->route_count);
	CHECK(a->next_node == b->next_node && a->ceiling == b->ceiling);
	CHECK(a->fault == b->fault && a->booted == b->booted && a->trace_exhausted == b->trace_exhausted);
	CHECK(a->trace_count == b->trace_count && a->trace_sequence == b->trace_sequence);
	CHECK(memcmp(a->trace, b->trace, a->trace_count * sizeof(*a->trace)) == 0);
	if(a->route_count) CHECK(memcmp(a->links, b->links, a->route_count * sizeof(*a->links)) == 0);
	for(size_t i = 0; i < a->node_count; i++) {
		const RoutedNode *x = &a->nodes[i], *y = &b->nodes[i];
		CHECK(memcmp(x->uxn.ram, y->uxn.ram, sizeof(x->uxn.ram)) == 0);
		CHECK(memcmp(x->uxn.devices, y->uxn.devices, sizeof(x->uxn.devices)) == 0);
		CHECK(memcmp(&x->uxn.working, &y->uxn.working, sizeof(UxnStack)) == 0);
		CHECK(memcmp(&x->uxn.return_stack, &y->uxn.return_stack, sizeof(UxnStack)) == 0);
		CHECK(x->uxn.instructions == y->uxn.instructions && x->loaded == y->loaded && x->id == y->id);
		CHECK(x->source == y->source && x->writable == y->writable && x->prefer_receive == y->prefer_receive);
		CHECK(x->next_incoming == y->next_incoming && x->next_writable == y->next_writable);
	}
}

static void
branch(bool rearranged)
{
	RunnerRom files[] = {read_rom("build/routes-burst.rom"), read_rom("build/routes-relay.rom"), read_rom("build/routes-collect.rom")};
	RunnerRom roms[4] = {files[0], files[1], files[2], {idle, sizeof(idle)}};
	RoutedRoute routes[] = {{0,1,1}, {0,2,2}, {1,7,2}};
	unsigned count = rearranged ? 4 : 3, collector = rearranged ? 0 : 2;
	if(rearranged) {
		roms[0] = files[2]; roms[1] = (RunnerRom){idle, sizeof(idle)}; roms[2] = files[0]; roms[3] = files[1];
		routes[0] = (RoutedRoute){2,1,3}; routes[1] = (RoutedRoute){2,2,0}; routes[2] = (RoutedRoute){3,7,0};
	}
	RunnerDefinition d = {roms, count, routes, 3, NULL, 0, 10000};
	ConstellationRunner a = {0}, replay = {0};
	CHECK(runner_start(&a, &d, NULL)); CHECK(runner_start(&replay, &d, NULL));
	/* Independently built old-style host is the behavior oracle. */
	RoutedHost raw; RoutedNode *nodes = calloc(count, sizeof(*nodes)); RoutedLink links[3]; CHECK(nodes);
	CHECK(routed_init(&raw, nodes, count, links, routes, 3, 10000));
	for(unsigned i = 0; i < count; i++) CHECK(routed_load(&raw, i, roms[i].bytes, roms[i].length));
	CHECK(routed_boot(&raw)); same(&a.host, &raw); same(&a.host, &replay.host);
	for(unsigned i = 0; i < 3; i++) free((void *)files[i].bytes);
	memset(roms, 0, sizeof(roms)); memset(routes, 0, sizeof(routes)); /* Definition storage is no longer needed. */
	unsigned turns = 0;
	while(!routed_quiescent(&a.host) && turns++ < 100) {
		CHECK(routed_step(&a.host)); CHECK(routed_step(&raw)); CHECK(routed_step(&replay.host));
		same(&a.host, &raw); same(&a.host, &replay.host);
	}
	CHECK(routed_quiescent(&a.host)); CHECK(turns < 100);
	CHECK(a.host.nodes[collector].uxn.ram[0] == 6);
	CHECK(memcmp(a.host.nodes[collector].uxn.ram + 0x400, "\xaa\x01\x02\x03\x04\x05", 6) == 0);
	printf("Branch %u nodes: %u turns, %zu exact events; unchanged ROMs, independent raw host and fresh replay match.\n", count, turns, a.host.trace_count);
	runner_free(&a); runner_free(&replay); free(nodes);
}

static uint8_t
device_read(Uxn *u, uint8_t port, void *context)
{
	(void)u; CHECK(port == 0x21); return *(uint8_t *)context;
}
static void
device_write(Uxn *u, uint8_t port, uint8_t value, void *context)
{
	(void)u; CHECK(port == 0x20 && value == 9); (*(unsigned *)context)++;
}

static void
boundaries(void)
{
	ConstellationRunner r = {0}; RunnerRom rom = {idle, 1};
	RunnerDefinition d = {&rom, 1, NULL, 0, NULL, 0, 100};
	CHECK(runner_start(&r, &d, NULL)); CHECK(r.host.node_count == 1 && r.host.route_count == 0);
	RoutedNode *saved = r.host.nodes; CHECK(!runner_start(&r, &d, NULL)); CHECK(r.host.nodes == saved);
	runner_free(&r); runner_free(&r); CHECK(!r.host.nodes && !r.devices && !r.bindings);
	CHECK(!runner_start(NULL, &d, NULL)); CHECK(!runner_start(&r, NULL, NULL));
	d.node_count = 0; CHECK(!runner_start(&r, &d, NULL));
	d.node_count = ROUTED_MAX_NODES + 1; CHECK(!runner_start(&r, &d, NULL)); d.node_count = 1;
	d.ceiling = 0; CHECK(!runner_start(&r, &d, NULL)); d.ceiling = 100;
	rom.length = UXN_RAM_SIZE; CHECK(!runner_start(&r, &d, NULL)); rom.length = 1;
	rom.bytes = NULL; CHECK(!runner_start(&r, &d, NULL)); rom.bytes = idle;
	d.route_count = ROUTED_MAX_ROUTES + 1; CHECK(!runner_start(&r, &d, NULL));
	d.route_count = 1; CHECK(!runner_start(&r, &d, NULL));
	RoutedRoute bad = {0,1,1}; d.routes = &bad; CHECK(!runner_start(&r, &d, NULL));
	d.route_count = 0; d.routes = NULL;
	unsigned writes = 0; uint8_t value = 42;
	RunnerDevice devices[] = {{0,0x20,0x20,NULL,device_write,&writes},{0,0x21,0x21,device_read,NULL,&value}};
	d.devices = devices; d.device_count = 2;
	d.devices = NULL; CHECK(!runner_start(&r, &d, NULL)); d.devices = devices;
	d.device_count = UXN_DEVICE_SIZE + 1; CHECK(!runner_start(&r, &d, NULL)); d.device_count = 2;
	const uint8_t code[] = {0x80,9,0x80,0x20,0x17,0x80,0x21,0x16,0x80,0,0x11,0};
	rom = (RunnerRom){code,sizeof(code)};
	RunnerDevice valid = devices[0];
	devices[0].first = 0xd0; devices[0].last = 0xdf; CHECK(!runner_start(&r, &d, NULL)); devices[0] = valid;
	devices[0].last = 256; CHECK(!runner_start(&r, &d, NULL)); devices[0] = valid;
	devices[0].node = 1; CHECK(!runner_start(&r, &d, NULL)); devices[0] = valid;
	devices[0].first = 0x22; CHECK(!runner_start(&r, &d, NULL)); devices[0] = valid;
	devices[0].last = 0x21; CHECK(!runner_start(&r, &d, NULL)); devices[0] = valid;
	devices[0].write = NULL; CHECK(!runner_start(&r, &d, NULL)); devices[0] = valid;
	CHECK(writes == 0); CHECK(runner_start(&r, &d, NULL)); CHECK(writes == 1);
	CHECK(r.host.nodes[0].uxn.ram[0] == 42);
	memset(devices, 0, sizeof(devices));
	CHECK(uxn_eval(&r.host.nodes[0].uxn, UXN_ROM_START, 100) == UXN_STOP_BREAK); CHECK(writes == 2);
	Uxn *u = &r.host.nodes[0].uxn; u->devices[ROUTED_RECEIVE_SOURCE] = 3;
	CHECK(u->device_read(u, ROUTED_RECEIVE_SOURCE, u->device_context) == ROUTED_NONE);
	CHECK(u->device_read(u, 0x20, u->device_context) == 9); /* Missing read callback falls through. */
	runner_free(&r);
	const uint8_t loop[] = {0x40,0xff,0xfd}; rom = (RunnerRom){loop,sizeof(loop)};
	d.devices = NULL; d.device_count = 0; CHECK(!runner_start(&r, &d, NULL)); CHECK(!r.host.nodes && !r.bindings);
	/* Same port ranges, independent node contexts; failed boot releases storage
	 * but cannot roll back effects already delivered by a prior boot. */
	unsigned other_writes = 0; uint8_t other_value = 99;
	RunnerRom pair[] = {{code,sizeof(code)}, {loop,sizeof(loop)}};
	RunnerDevice pair_devices[] = {
		{0,0x20,0x20,NULL,device_write,&writes}, {0,0x21,0x21,device_read,NULL,&value},
		{1,0x20,0x20,NULL,device_write,&other_writes}, {1,0x21,0x21,device_read,NULL,&other_value}
	};
	RunnerDefinition two = {pair,2,NULL,0,pair_devices,4,100};
	CHECK(!runner_start(&r, &two, NULL)); CHECK(writes == 3 && other_writes == 0);
	CHECK(!r.host.nodes && !r.devices && !r.bindings);
	pair[1] = pair[0]; CHECK(runner_start(&r, &two, NULL));
	CHECK(r.host.nodes[0].uxn.ram[0] == 42 && r.host.nodes[1].uxn.ram[0] == 99);
	CHECK(writes == 4 && other_writes == 1); runner_free(&r);
	rom = (RunnerRom){idle,1}; CHECK(runner_start(&r, &d, NULL)); runner_free(&r);
}

static void
diagnostics(void)
{
	ConstellationRunner r = {0}, empty = {0}; RunnerDiagnostic error;
	RunnerRom roms[] = {{idle,1}, {idle,1}};
	RunnerDefinition d = {roms,2,NULL,0,NULL,0,10000};
	CHECK(!runner_start(NULL, &d, &error));
	CHECK(error.stage == RUNNER_VALIDATE && error.reason == RUNNER_INVALID_ARGUMENT && error.node == ROUTED_NONE);
	CHECK(!runner_start(&r, NULL, &error)); CHECK(error.reason == RUNNER_INVALID_ARGUMENT);
	d.ceiling = 0; CHECK(!runner_start(&r, &d, &error)); CHECK(error.reason == RUNNER_BAD_CONFIG); d.ceiling = 10000;
	roms[1].bytes = NULL; CHECK(!runner_start(&r, &d, &error));
	CHECK(error.stage == RUNNER_VALIDATE && error.reason == RUNNER_BAD_ROM && error.node == 1);
	roms[1].bytes = idle;
	RoutedRoute bad = {0,1,2}; d.routes = &bad; d.route_count = 1;
	CHECK(!runner_start(&r, &d, &error));
	CHECK(error.reason == RUNNER_BAD_ROUTES && error.fault == ROUTED_BAD_CONFIG && error.node == ROUTED_NONE);
	d.routes = NULL; d.route_count = 0;
	unsigned writes = 0;
	RunnerDevice devices[] = {{1,0x20,0x20,NULL,device_write,&writes},{1,0x20,0x20,NULL,device_write,&writes}};
	d.devices = devices; d.device_count = 2;
	CHECK(!runner_start(&r, &d, &error)); CHECK(error.reason == RUNNER_DEVICE_OVERLAP && error.node == 1);
	devices[0].node = 2; CHECK(!runner_start(&r, &d, &error));
	CHECK(error.reason == RUNNER_BAD_DEVICE && error.node == ROUTED_NONE);
	d.devices = NULL; d.device_count = 0;
	const uint8_t loop[] = {0x40,0xff,0xfd};
	roms[1] = (RunnerRom){loop,sizeof(loop)};
	CHECK(!runner_start(&r, &d, &error));
	CHECK(error.stage == RUNNER_BOOT && error.reason == RUNNER_HOST_FAULT && error.node == 1 && error.fault == ROUTED_LIMIT);
	CHECK(strstr(runner_diagnostic_text(&error), "instruction limit"));
	runner_free(&r); CHECK(error.fault == ROUTED_LIMIT && error.node == 1);
	CHECK(memcmp(&r, &empty, sizeof(r)) == 0);
	const uint8_t send[] = {0x80,1,0x80,0xdc,0x17,0x80,1,0x80,0xd9,0x17,0x40,0xff,0xf8};
	roms[1] = (RunnerRom){send,sizeof(send)};
	CHECK(!runner_start(&r, &d, &error));
	CHECK(error.stage == RUNNER_BOOT && error.fault == ROUTED_BAD_ROUTE && error.node == 1);
	CHECK(strstr(runner_diagnostic_text(&error), "undeclared route"));
	RoutedRoute self = {1,1,1}; d.routes = &self; d.route_count = 1;
	CHECK(!runner_start(&r, &d, &error));
	CHECK(error.stage == RUNNER_BOOT && error.fault == ROUTED_TRACE_FULL && error.node == ROUTED_NONE);
	CHECK(strstr(runner_diagnostic_text(&error), "trace capacity"));
	CHECK(memcmp(&r, &empty, sizeof(r)) == 0);
	roms[1] = (RunnerRom){idle,1};
	CHECK(runner_start(&r, &d, &error));
	CHECK(error.stage == RUNNER_STAGE_NONE && error.reason == RUNNER_SUCCESS && error.node == ROUTED_NONE && error.fault == ROUTED_OK);
	ConstellationRunner saved; memcpy(&saved, &r, sizeof(r));
	CHECK(!runner_start(&r, &d, &error)); CHECK(error.reason == RUNNER_ALREADY_LIVE);
	CHECK(memcmp(&saved, &r, sizeof(r)) == 0);
	runner_free(&r);
}

int main(void)
{
	boundaries(); diagnostics(); branch(false); branch(true);
	printf("%u runner checks passed.\n", checks); return 0;
}
