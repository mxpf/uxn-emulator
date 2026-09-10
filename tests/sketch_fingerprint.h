#include <inttypes.h>
#include <stdio.h>

static void fingerprint_byte(uint64_t *h, uint8_t b) { *h = (*h ^ b) * UINT64_C(1099511628211); }
static void fingerprint_number(uint64_t *h, uint64_t n)
{ for(unsigned i = 0; i < 8; i++, n >>= 8) fingerprint_byte(h, (uint8_t)n); }
static const char *fingerprint(const Sketchpad *s)
{
	static char text[240]; uint64_t state = UINT64_C(14695981039346656037), pixels = state;
	const RoutedHost *h = &s->runner.host;
	if(h->nodes) {
		const RoutedNode *n = h->nodes; const Uxn *u = &n->uxn;
		for(size_t i = 0; i < sizeof(u->ram); i++) fingerprint_byte(&state, u->ram[i]);
		for(size_t i = 0; i < sizeof(u->devices); i++) fingerprint_byte(&state, u->devices[i]);
		for(unsigned i = 0; i < UXN_STACK_SIZE; i++) { fingerprint_byte(&state, u->working.data[i]); fingerprint_byte(&state, u->return_stack.data[i]); }
		fingerprint_number(&state, u->working.pointer); fingerprint_number(&state, u->return_stack.pointer); fingerprint_number(&state, u->instructions);
		fingerprint_number(&state, n->next_incoming); fingerprint_number(&state, n->next_writable); fingerprint_number(&state, n->prefer_receive);
		fingerprint_number(&state, n->source); fingerprint_number(&state, n->writable); fingerprint_number(&state, n->loaded); fingerprint_number(&state, n->id);
		const RoutedLink *l = h->links;
		fingerprint_number(&state, l->route.from); fingerprint_number(&state, l->route.to); fingerprint_number(&state, l->route.selector);
		fingerprint_number(&state, l->waiting); fingerprint_number(&state, l->queue.head); fingerprint_number(&state, l->queue.count);
		for(unsigned i = 0; i < CONSTELLATION_QUEUE_CAPACITY; i++) {
			fingerprint_number(&state, l->queue.messages[i].length);
			for(unsigned j = 0; j < CONSTELLATION_MESSAGE_MAX; j++) fingerprint_byte(&state, l->queue.messages[i].data[j]);
		}
	}
	fingerprint_number(&state, h->node_count); fingerprint_number(&state, h->route_count); fingerprint_number(&state, h->next_node);
	fingerprint_number(&state, h->trace_sequence); fingerprint_number(&state, h->trace_count); fingerprint_number(&state, h->fault);
	fingerprint_number(&state, h->booted); fingerprint_number(&state, h->trace_exhausted); fingerprint_number(&state, h->ceiling);
	fingerprint_number(&state, s->failed); fingerprint_number(&state, s->mode);
	fingerprint_number(&state, s->importing); fingerprint_number(&state, s->incoming_offset);
	for(size_t i = 0; i < sizeof(s->paper); i++) fingerprint_byte(&state, s->paper[i]);
	for(size_t i = 0; i < sizeof(s->incoming); i++) fingerprint_byte(&state, s->incoming[i]);
	for(size_t i = 0; i < sizeof(s->error); i++) fingerprint_byte(&state, (uint8_t)s->error[i]);
	for(unsigned i = 0; i < SKETCH_WIDTH * SKETCH_HEIGHT; i++) fingerprint_number(&pixels, s->pixels[i]);
	snprintf(text, sizeof(text), "%u,%u %" PRIu64 " %" PRIu64 " %" PRIu64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64,
		s->x, s->y, s->commits, s->turns, s->events, s->trace_digest, state, pixels);
	return text;
}
