/* Test-only canonical fingerprints: never linked into the playable page. */
#include "../src/mystery_web.c"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static void byte(uint64_t *h, uint8_t b) {
  *h = (*h ^ b) * UINT64_C(1099511628211);
}
static void number(uint64_t *h, uint64_t n) {
  for (unsigned i = 0; i < 8; i++, n >>= 8)
    byte(h, (uint8_t)n);
}
static void bytes(uint64_t *h, const void *p, size_t n) {
  const uint8_t *b = p;
  for (size_t i = 0; i < n; i++)
    byte(h, b[i]);
}
static void machine_hash(uint64_t *h, const MysteryMachine *m) {
  number(h, m != NULL);
  if (!m)
    return;
  const RoutedHost *r = &m->runner.host;
  number(h, m->frames);
  number(h, m->turns);
  number(h, m->failed);
  for (unsigned i = 0; i < 256 * 192; i++) {
    number(h, m->pixels[i]);
    number(h, m->draft[i]);
  }
  bytes(h, m->text, sizeof(m->text));
  bytes(h, m->draft_text, sizeof(m->draft_text));
  number(h, r->node_count);
  number(h, r->route_count);
  number(h, r->next_node);
  number(h, r->trace_sequence);
  number(h, r->trace_count);
  number(h, r->fault);
  number(h, r->booted);
  number(h, r->trace_exhausted);
  number(h, r->ceiling);
  for (size_t i = 0; i < r->node_count; i++) {
    const RoutedNode *n = r->nodes + i;
    const Uxn *u = &n->uxn;
    bytes(h, u->ram, sizeof(u->ram));
    bytes(h, u->devices, sizeof(u->devices));
    bytes(h, u->working.data, sizeof(u->working.data));
    bytes(h, u->return_stack.data, sizeof(u->return_stack.data));
    number(h, u->working.pointer);
    number(h, u->return_stack.pointer);
    number(h, u->instructions);
    number(h, n->id);
    number(h, n->loaded);
    number(h, n->source);
    number(h, n->writable);
    number(h, n->next_incoming);
    number(h, n->next_writable);
    number(h, n->prefer_receive);
  }
  for (size_t i = 0; i < r->route_count; i++) {
    const RoutedLink *l = r->links + i;
    number(h, l->route.from);
    number(h, l->route.to);
    number(h, l->route.selector);
    number(h, l->waiting);
    number(h, l->queue.head);
    number(h, l->queue.count);
    for (unsigned j = 0; j < CONSTELLATION_QUEUE_CAPACITY; j++) {
      number(h, l->queue.messages[j].length);
      bytes(h, l->queue.messages[j].data, CONSTELLATION_MESSAGE_MAX);
    }
  }
}
EXPORT const char *mystery_digest(void) {
  static char out[32];
  uint64_t h = UINT64_C(14695981039346656037);
  machine_hash(&h, session.live);
  machine_hash(&h, session.play);
  number(&h, session.count);
  number(&h, session.event_count);
  number(&h, session.play_input);
  number(&h, session.play_event);
  number(&h, session.failed);
  number(&h, session.sealed);
  number(&h, session.replaying);
  number(&h, session.replay_done);
  bytes(&h, session.inputs, session.count * 3);
  bytes(&h, session.error, sizeof(session.error));
  for (size_t i = 0; i < session.event_count; i++) {
    const RoutedEvent *e = session.events + i;
    number(&h, e->sequence);
    number(&h, e->kind);
    number(&h, e->reason);
    number(&h, e->from);
    number(&h, e->to);
    number(&h, e->selector);
    number(&h, e->message.length);
    bytes(&h, e->message.data, e->message.length);
  }
  snprintf(out, sizeof(out), "%016" PRIx64, h);
  return out;
}
EXPORT int mystery_test_op(int op) {
  if (op == '0')
    return mystery_reset();
  if (op >= '1' && op <= '9')
    return mystery_action((unsigned)(op - '0'));
  if (op == 'q')
    return mystery_action(10);
  if (op == 'f')
    return mystery_click(200, 12);
  if (op == 'r')
    return mystery_play();
  if (op == '.')
    return mystery_tick();
  if (op == 'a')
    return mystery_click(100, 60);
  if (op == 'b')
    return mystery_click(30, 90);
  if (op == 'c')
    return mystery_click(200, 50);
  if (op == 'd')
    return mystery_click(200, 124);
  if (op == 'm')
    return mystery_click(0, 191);
  if (op == 'x')
    return mystery_click(256, 0);
  return -1;
}
#ifndef __EMSCRIPTEN__
int main(int argc, char **argv) {
  if (argc == 3 && !strcmp(argv[1], "--pixels")) {
    if (!mystery_reset())
      return 1;
    for (size_t i = 0; argv[2][i]; i++)
      if (!mystery_test_op(argv[2][i]))
        return 1;
    for (size_t i = 0; i < 256 * 192; i++) {
      uint32_t p = mystery_pixels()[i];
      putchar(p >> 16 & 255);
      putchar(p >> 8 & 255);
      putchar(p & 255);
      putchar(255);
    }
  } else {
    int c;
    while ((c = getchar()) != EOF) {
      int result = mystery_test_op(c);
      printf("%d %s\n", result, mystery_digest());
    }
  }
  mystery_free(&session);
  return 0;
}
#endif
