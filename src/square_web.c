/* Browser boundary only; the three ROMs, routed host and recorder are shared. */
#include "square_demo.h"
#include <inttypes.h>
#include <stdio.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif
static SquareSession session;
EXPORT int no_escape_reset(void) { square_free(&session); return square_init(&session); }
EXPORT int no_escape_input(int action)
{
	return action >= 1 && action <= 4 && square_input(&session, (uint8_t)action);
}
/* Read-only browser admission check; queue bounds and raw input stay unchanged. */
EXPORT int no_escape_input_ready(void)
{
	return session.live && !session.play && !session.sealed && !session.failed &&
		session.live->runner.host.links[0].queue.count < CONSTELLATION_QUEUE_CAPACITY;
}
EXPORT int no_escape_step(void)
{
	return session.play ? square_replay_step(&session) : square_step(&session);
}
EXPORT int no_escape_replay(void) { return square_replay_begin(&session); }
EXPORT unsigned no_escape_flags(void)
{
	return session.sealed | (session.play != NULL) << 1 | session.replay_done << 2 | session.failed << 3;
}
EXPORT unsigned no_escape_turn(void) { return session.play ? session.play_turn : session.turn; }
EXPORT unsigned no_escape_rejected(void) { return session.rejected; }
EXPORT const char *no_escape_error(void) { return session.error; }
EXPORT const uint32_t *no_escape_pixels(void) { return (session.play ? session.play : session.live)->pixels; }

static void hash_byte(uint64_t *hash, uint8_t byte) { *hash = (*hash ^ byte) * UINT64_C(1099511628211); }
static void hash_number(uint64_t *hash, uint64_t value)
{
	for(unsigned shift = 0; shift < 64; shift += 8) hash_byte(hash, (uint8_t)(value >> shift));
}
EXPORT const char *no_escape_digest(void)
{
	static char text[160];
	const SquareMachine *m = session.play ? session.play : session.live;
	uint64_t trace = UINT64_C(14695981039346656037), state = trace, pixels = trace;
	for(size_t i = 0; i < session.event_count; i++) {
		const RoutedEvent *e = &session.events[i];
		hash_number(&trace, e->sequence); hash_number(&trace, e->kind); hash_number(&trace, e->reason);
		hash_byte(&trace, e->from); hash_byte(&trace, e->to); hash_byte(&trace, e->selector); hash_byte(&trace, e->message.length);
		for(unsigned j = 0; j < e->message.length; j++) hash_byte(&trace, e->message.data[j]);
	}
	for(unsigned i = 0; i < 3; i++) {
		const RoutedNode *n = &m->runner.host.nodes[i];
		for(unsigned j = 0; j < sizeof(n->uxn.ram); j++) hash_byte(&state, n->uxn.ram[j]);
		for(unsigned j = 0; j < sizeof(n->uxn.devices); j++) hash_byte(&state, n->uxn.devices[j]);
		for(unsigned j = 0; j < UXN_STACK_SIZE; j++) { hash_byte(&state, n->uxn.working.data[j]); hash_byte(&state, n->uxn.return_stack.data[j]); }
		hash_number(&state, n->uxn.working.pointer); hash_number(&state, n->uxn.return_stack.pointer);
		hash_number(&state, n->uxn.instructions); hash_number(&state, n->next_incoming); hash_number(&state, n->next_writable);
		hash_number(&state, n->prefer_receive); hash_number(&state, n->source); hash_number(&state, n->writable);
		const RoutedLink *l = &m->runner.host.links[i];
		hash_number(&state, l->waiting); hash_number(&state, l->queue.head); hash_number(&state, l->queue.count);
		for(unsigned j = 0; j < CONSTELLATION_QUEUE_CAPACITY; j++) {
			const ConstellationMessage *message = &l->queue.messages[j]; hash_byte(&state, message->length);
			for(unsigned k = 0; k < CONSTELLATION_MESSAGE_MAX; k++) hash_byte(&state, message->data[k]);
		}
	}
	hash_number(&state, m->runner.host.next_node); hash_number(&state, m->runner.host.trace_sequence); hash_number(&state, m->frames);
	for(unsigned i = 0; i < SQUARE_WIDTH * SQUARE_HEIGHT; i++) hash_number(&pixels, m->pixels[i]);
	snprintf(text, sizeof(text), "%u %zu %zu %u %016" PRIx64 " %016" PRIx64 " %016" PRIx64,
		no_escape_turn(), session.input_count, session.event_count, no_escape_flags(), trace, state, pixels);
	return text;
}
