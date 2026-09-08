#include "garden.h"
#include <stdlib.h>
#include <string.h>

static void
fail(Garden *g, const char *message)
{
	g->failed = true;
	snprintf(g->error, sizeof(g->error), "%s", message);
}

static void
note(Garden *g, const char *message)
{
	if(g->history_count == GARDEN_HISTORY) {
		memmove(g->history, g->history + 1,
			(GARDEN_HISTORY - 1) * sizeof(g->history[0]));
		g->history_count--;
	}
	snprintf(g->history[g->history_count++], sizeof(g->history[0]), "%s", message);
}

static void
hash_byte(Garden *g, uint8_t byte)
{
	g->trace_hash = (g->trace_hash ^ byte) * UINT64_C(1099511628211);
}

static uint8_t
view_read(Uxn *u, uint8_t port, void *context)
{
	Garden *g = context;
	return g->port_read(u, port, g->port_context);
}

static void
view_write(Uxn *u, uint8_t port, uint8_t value, void *context)
{
	static const uint32_t palette[] = {
		0xff203f37, 0xff305347, 0xff789c70,
		0xffed987f, 0xfff0ce83, 0xff639eaa
	};
	Garden *g = context;
	if(g->failed) return;
	if(port == 0x25 && value) {
		unsigned tx = u->devices[0x20], ty = u->devices[0x21];
		unsigned color = u->devices[0x24] & 0x7f;
		uint16_t address = (uint16_t)((u->devices[0x22] << 8) | u->devices[0x23]);
		if(tx >= 16 || ty >= 12 || color >= sizeof(palette) / sizeof(palette[0])) {
			fail(g, "invalid sprite command");
			return;
		}
		for(unsigned y = 0; y < 8; y++) {
			uint8_t bits = u->ram[(uint16_t)(address + y)];
			for(unsigned x = 0; x < 8; x++)
				if((bits & (0x80 >> x)) || !(u->devices[0x24] & 0x80))
					g->pixels[(ty * 8 + y) * GARDEN_WIDTH + tx * 8 + x] =
						palette[(bits & (0x80 >> x)) ? color : 0];
		}
	} else {
		g->port_write(u, port, value, g->port_context);
	}
}

static bool
load_rom(Garden *g, unsigned id, const char *path)
{
	uint8_t bytes[UXN_RAM_SIZE - UXN_ROM_START + 1];
	FILE *f = fopen(path, "rb");
	size_t length;
	bool ok;
	if(!f) { fail(g, "cannot open garden ROM"); return false; }
	length = fread(bytes, 1, sizeof(bytes), f);
	ok = !ferror(f) && constellation_load(&g->pair, (ConstellationEndpointId)id,
		bytes, length);
	fclose(f);
	if(!ok) fail(g, "cannot load garden ROM");
	if(ok) {
		hash_byte(g, (uint8_t)id);
		hash_byte(g, (uint8_t)(length >> 8));
		hash_byte(g, (uint8_t)length);
		for(size_t i = 0; i < length; i++) hash_byte(g, bytes[i]);
	}
	return ok;
}

static bool
drain(Garden *g)
{
	unsigned turns = 0;
	while(!g->failed && !constellation_is_quiescent(&g->pair) && turns++ < 32)
		if(!constellation_step(&g->pair)) {
			fail(g, constellation_fault_name(g->pair.fault_reason));
			return false;
		}
	if(!constellation_is_quiescent(&g->pair))
		fail(g, "message exchange exceeded 32 turns");
	return !g->failed;
}

static void
collect_trace(Garden *g)
{
	char line[80];
	/* Hash canonical fields, never C struct padding. */
	for(size_t i = 0; i < g->pair.trace_count; i++) {
		const ConstellationTraceEvent *e = &g->pair.trace[i];
		hash_byte(g, (uint8_t)e->kind);
		hash_byte(g, (uint8_t)e->reason);
		hash_byte(g, (uint8_t)e->endpoint);
		hash_byte(g, (uint8_t)e->peer);
		hash_byte(g, e->message.length);
		for(unsigned j = 0; j < e->message.length; j++)
			hash_byte(g, e->message.data[j]);
		if(e->kind == CONSTELLATION_TRACE_SEND) {
			if(e->message.length == 200) {
				const uint8_t *s = e->message.data;
				snprintf(line, sizeof(line), "WORLD > VIEW P%u,%u C%u,%u %s",
					s[0], s[1], s[2], s[3],
					s[5] ? "FRIEND" : s[4] ? "CURIOUS" : "WANDER");
			} else
				snprintf(line, sizeof(line), "%s > %s  %s",
					e->endpoint ? "WORLD" : "VIEW", e->peer ? "WORLD" : "VIEW",
					e->message.length == 1 ? garden_action_name(e->message.data[0]) : "MESSAGE");
			note(g, line);
		}
	}
}

bool
garden_init(Garden *g, const char *view, const char *world)
{
	Uxn *u;
	memset(g, 0, sizeof(*g));
	g->trace_hash = UINT64_C(14695981039346656037);
	constellation_init(&g->pair, 100000);
	u = &g->pair.endpoints[0].uxn;
	g->port_read = u->device_read;
	g->port_write = u->device_write;
	g->port_context = u->device_context;
	uxn_connect_devices(u, view_read, view_write, g);
	if(!load_rom(g, 0, view) || !load_rom(g, 1, world)) return false;
	if(!constellation_boot(&g->pair)) {
		fail(g, constellation_fault_name(g->pair.fault_reason));
		return false;
	}
	if(!drain(g)) return false;
	collect_trace(g);
	return true;
}

bool
garden_step(Garden *g, uint8_t action)
{
	Uxn *u = &g->pair.endpoints[0].uxn;
	uint16_t vector = (uint16_t)((u->devices[0xe0] << 8) | u->devices[0xe1]);
	char line[80];
	if(g->failed) return false;
	if(action > 5 || !vector || !constellation_is_quiescent(&g->pair)) {
		fail(g, "invalid input event or unfinished exchange");
		return false;
	}
	/* Previous batch has been consumed. Keep the v0 bounded trace per event. */
	memset(g->pair.trace, 0, sizeof(g->pair.trace));
	g->pair.trace_count = 0;
	g->tick++;
	hash_byte(g, 0xff); /* external event, distinct from supervisor events */
	hash_byte(g, action);
	snprintf(line, sizeof(line), "TICK %llu  %s",
		(unsigned long long)g->tick, garden_action_name(action));
	note(g, line);
	u->devices[0xe2] = action;
	if(uxn_eval(u, vector, 100000) != UXN_STOP_BREAK) {
		fail(g, "input handler instruction limit");
		return false;
	}
	if(constellation_has_fault(&g->pair)) {
		fail(g, constellation_fault_name(g->pair.fault_reason));
		return false;
	}
	if(!drain(g)) return false;
	collect_trace(g);
	return true;
}

const uint8_t *
garden_state(const Garden *g)
{
	return &g->pair.endpoints[0].uxn.ram[0x400];
}

const char *
garden_action_name(uint8_t action)
{
	static const char *names[] = {"WAIT", "NORTH", "EAST", "SOUTH", "WEST", "GREET"};
	return action < 6 ? names[action] : "INVALID";
}
