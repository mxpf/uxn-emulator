#ifndef SQUARE_DEMO_H
#define SQUARE_DEMO_H
#include "constellation_routes.h"

enum { SQUARE_WIDTH = 128, SQUARE_HEIGHT = 96, SQUARE_INPUTS = 1024,
	SQUARE_EVENTS = 16384, SQUARE_TURNS = 8192 };
typedef struct {
	RoutedHost host;
	RoutedNode nodes[3];
	RoutedLink links[3];
	uint32_t pixels[SQUARE_WIDTH * SQUARE_HEIGHT];
	UxnDeviceWrite routed_write;
	void *routed_context;
	unsigned frames;
	bool failed;
} SquareMachine;
typedef struct { unsigned turn; uint8_t action; RoutedInputResult result; } SquareInput;
typedef struct {
	SquareMachine *live, *play;
	SquareInput inputs[SQUARE_INPUTS];
	RoutedEvent events[SQUARE_EVENTS];
	size_t input_count, event_count, play_input, play_event;
	unsigned turn, play_turn, rejected;
	bool sealed, replay_done, failed;
	char error[160];
} SquareSession;

/* Demo-only lifecycle: initialize fresh/freed storage; free before restarting.
 * Sealed recordings reject further live work without mutation and remain
 * replayable. A true return can therefore mean no-op; inspect sealed. */
bool square_init(SquareSession *s);
void square_free(SquareSession *s);
bool square_input(SquareSession *s, uint8_t action);
bool square_step(SquareSession *s);
bool square_replay_begin(SquareSession *s);
bool square_replay_step(SquareSession *s);
bool square_same(const SquareMachine *a, const SquareMachine *b);
#endif
