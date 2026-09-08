#ifndef GARDEN_H
#define GARDEN_H
#include "constellation.h"
#include <stdio.h>

enum { GARDEN_WIDTH = 128, GARDEN_HEIGHT = 96, GARDEN_HISTORY = 24 };
typedef struct {
	Constellation pair;
	uint32_t pixels[GARDEN_WIDTH * GARDEN_HEIGHT];
	UxnDeviceRead port_read;
	UxnDeviceWrite port_write;
	void *port_context;
	uint64_t tick;
	uint64_t trace_hash;
	char history[GARDEN_HISTORY][80];
	unsigned history_count;
	bool failed;
	char error[128];
} Garden;

bool garden_init(Garden *g, const char *view, const char *world);
bool garden_step(Garden *g, uint8_t action);
const uint8_t *garden_state(const Garden *g);
const char *garden_action_name(uint8_t action);
#endif
