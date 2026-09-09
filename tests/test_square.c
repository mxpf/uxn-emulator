#include "square_demo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while(0)

static SquareSession *
fresh(void)
{
	SquareSession *s = calloc(1, sizeof(*s)); CHECK(s != NULL); CHECK(square_init(s)); return s;
}

static void
turns(SquareSession *s, unsigned count)
{
	for(unsigned i = 0; i < count; i++) CHECK(square_step(s));
}

static void
position(SquareSession *s, unsigned x, unsigned y)
{
	CHECK(s->live->nodes[1].uxn.ram[0] == x && s->live->nodes[1].uxn.ram[1] == y);
	for(unsigned row = 0; row < SQUARE_HEIGHT; row++) for(unsigned col = 0; col < SQUARE_WIDTH; col++) {
		bool inside = col >= x * 8 && col < x * 8 + 8 && row >= y * 8 && row < y * 8 + 8;
		CHECK(s->live->pixels[row * SQUARE_WIDTH + col] == (inside ? 0xffdc5c36 : 0xfff4f1e9));
	}
}

static void
replay(SquareSession *s)
{
	CHECK(square_replay_begin(s)); unsigned steps = 0;
	while(!s->replay_done && steps++ <= s->turn + 1) CHECK(square_replay_step(s));
	CHECK(s->replay_done && square_same(s->live, s->play));
}

int main(void)
{
	SquareSession *s = fresh(); turns(s, 3); position(s, 8, 6);
	const uint8_t actions[] = {1,2,3,4}; const unsigned positions[][2] = {{8,5},{9,5},{9,6},{8,6}};
	for(unsigned i = 0; i < 4; i++) { CHECK(square_input(s, actions[i])); turns(s, 3); position(s, positions[i][0], positions[i][1]); }
	for(unsigned i = 0; i < 24; i++) { CHECK(square_input(s, 1)); CHECK(square_input(s, 4)); turns(s, 6); }
	position(s, 0, 0);
	for(unsigned i = 0; i < 24; i++) { CHECK(square_input(s, 2)); CHECK(square_input(s, 3)); turns(s, 6); }
	position(s, 15, 11); turns(s, 780);
	CHECK(square_input(s, 4)); turns(s, 3); position(s, 14, 11);
	replay(s); replay(s); /* Replay never overwrites the captured session. */
	CHECK(s->event_count == s->play_event && s->input_count == s->play_input);
	square_free(s); free(s);

	s = fresh();
	for(unsigned i = 0; i < 5; i++) CHECK(square_input(s, 2));
	CHECK(s->input_count == 5 && s->rejected == 1);
	CHECK(s->inputs[4].result == ROUTED_INPUT_FULL); turns(s, 30); position(s, 12, 6); replay(s);
	square_free(s); free(s);
	/* Freeze/replay with input still queued, including at boundary zero. */
	s = fresh(); CHECK(square_input(s, 1)); replay(s); CHECK(s->live->links[0].queue.count == 1);
	square_free(s); free(s);
	/* A trace mutation must fail verification instead of claiming success. */
	s = fresh(); CHECK(square_input(s, 2)); turns(s, 6);
	s->events[0].selector ^= 1; CHECK(!square_replay_begin(s) && s->failed);
	square_free(s); free(s);
	s = fresh(); CHECK(square_input(s, 2)); turns(s, 3);
	s->inputs[0].result = ROUTED_INPUT_FULL;
	CHECK(square_replay_begin(s)); CHECK(!square_replay_step(s) && s->failed);
	square_free(s); free(s);
	s = fresh(); turns(s, 3); s->live->pixels[0] ^= 1;
	CHECK(square_replay_begin(s));
	while(!s->failed && !s->replay_done) square_replay_step(s);
	CHECK(s->failed && !s->replay_done); square_free(s); free(s);
	/* Reaching the bounded archive limit freezes before another operation. */
	s = fresh();
	while(!s->sealed) CHECK(square_step(s));
	unsigned frozen = s->turn; size_t events = s->event_count;
	CHECK(frozen <= SQUARE_TURNS); CHECK(square_input(s, 2)); CHECK(square_step(s));
	CHECK(s->turn == frozen && s->event_count == events && s->input_count == 0); replay(s);
	square_free(s); free(s);
	s = fresh();
	for(unsigned i = 0; i < SQUARE_INPUTS; i++) CHECK(square_input(s, 1));
	CHECK(square_step(s) && s->sealed && s->turn == 0 && s->input_count == SQUARE_INPUTS);
	CHECK(s->rejected == SQUARE_INPUTS - 4); replay(s);
	square_free(s); free(s);
	printf("%u square demo checks passed\n", checks); return 0;
}
