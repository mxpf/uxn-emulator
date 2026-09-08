#include "constellation.h"
#include <stdio.h>
#include <stdlib.h>

static bool
load(Constellation *c, ConstellationEndpointId id, const char *path)
{
	uint8_t bytes[UXN_RAM_SIZE - UXN_ROM_START + 1];
	FILE *file = fopen(path, "rb");
	size_t length;
	bool ok;
	if(!file) return false;
	length = fread(bytes, 1, sizeof(bytes), file);
	ok = !ferror(file) && constellation_load(c, id, bytes, length);
	fclose(file);
	return ok;
}

int
main(int argc, char **argv)
{
	Constellation *c = calloc(1, sizeof(*c));
	size_t i, turns = 0;
	bool ok;
	if(argc != 3 || !c) {
		fprintf(stderr, "usage: constellation-v0 A.rom B.rom\n");
		free(c);
		return 1;
	}
	constellation_init(c, 100000);
	ok = load(c, CONSTELLATION_A, argv[1]) &&
		load(c, CONSTELLATION_B, argv[2]) && constellation_boot(c);
	while(ok && !constellation_is_quiescent(c) && turns++ < 256)
		ok = constellation_step(c);
	for(i = 0; i < c->trace_count; i++) {
		const ConstellationTraceEvent *e = &c->trace[i];
		unsigned j;
		printf("%03zu %-9s %s", i + 1,
			constellation_trace_kind_name(e->kind),
			constellation_endpoint_name(e->endpoint));
		if(e->kind == CONSTELLATION_TRACE_SEND ||
			e->kind == CONSTELLATION_TRACE_DELIVER ||
			e->kind == CONSTELLATION_TRACE_SEND_FULL) {
			printf(" -> %s [%u bytes]", constellation_endpoint_name(e->peer),
				e->message.length);
			for(j = 0; j < e->message.length; j++)
				printf(" %02x", e->message.data[j]);
		}
		if(e->kind == CONSTELLATION_TRACE_FAULT)
			printf(" reason=%s", constellation_fault_name(e->reason));
		putchar('\n');
	}
	ok = ok && !constellation_has_fault(c) && constellation_is_quiescent(c);
	if(c->trace_exhausted)
		fprintf(stderr, "trace capacity exhausted: record is incomplete\n");
	if(c->fault_reason != CONSTELLATION_FAULT_NONE)
		fprintf(stderr, "fault: %s\n", constellation_fault_name(c->fault_reason));
	puts(ok ? "success: quiescent" : "failed: load, fault, or turn limit");
	free(c);
	return ok ? 0 : 1;
}
