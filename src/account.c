#include "account.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Bounded Console byte sink attached only to the record/presentation node.
 * It never reads either domain state or interprets a story string. */
static void output(Uxn *u, uint8_t port, uint8_t value, void *context) {
  (void)u;
  (void)port;
  Account *a = context;
  if (a->failed)
    return;
  if (a->used + 1 >= ACCOUNT_OUTPUT ||
      (value != 10 && (value < 32 || value > 126))) {
    a->failed = true;
    return;
  }
  a->output[a->used++] = (char)value;
}
void account_free(Account *a) {
  runner_free(&a->runner);
  memset(a, 0, sizeof(*a));
}
bool account_take(Account *a) {
  if (a->failed || a->runner.host.fault != ROUTED_OK)
    return false;
  for (size_t i = 0; i < a->runner.host.node_count; i++)
    if (a->runner.host.nodes[i].uxn.ram[0xfe]) {
      a->failed = true;
      return false;
    }
  size_t n = 0;
  if (a->event_count + CONSTELLATION_TRACE_CAPACITY > ACCOUNT_EVENTS ||
      !routed_take_trace(&a->runner.host, a->events + a->event_count,
                         ACCOUNT_EVENTS - a->event_count, &n)) {
    a->failed = true;
    return false;
  }
  a->event_count += n;
  return true;
}
bool account_start(Account *a, bool single, bool adversarial_route) {
  if (a->runner.host.nodes)
    return false;
  const char *paths[2] = {single ? "build/account-single.rom"
                                 : "build/account-record.rom",
                          "build/account-witness.rom"};
  uint8_t *bytes[2] = {NULL, NULL};
  RunnerRom roms[2] = {0};
  bool ok = false;
  a->single = single;
  for (unsigned i = 0; i < (single ? 1u : 2u); i++) {
    bytes[i] = malloc(UXN_RAM_SIZE - UXN_ROM_START + 1);
    if (!bytes[i])
      goto end;
    FILE *f = fopen(paths[i], "rb");
    if (!f)
      goto end;
    roms[i].length = fread(bytes[i], 1, UXN_RAM_SIZE - UXN_ROM_START + 1, f);
    bool bad = ferror(f);
    if (fclose(f))
      bad = true;
    if (bad)
      goto end;
    roms[i].bytes = bytes[i];
  }
  const RoutedRoute routes[] = {{255, 7, 0}, {0, 1, 1}, {1, 1, 0}, {255, 8, 1}};
  const RunnerDevice device = {0, 0x18, 0x18, NULL, output, a};
  const RunnerDefinition d = {roms,    single ? 1 : 2,
                              routes,  single ? 1 : (adversarial_route ? 4 : 3),
                              &device, 1,
                              10000};
  RunnerDiagnostic diagnostic;
  ok = runner_start(&a->runner, &d, &diagnostic) && account_take(a);
end:
  free(bytes[0]);
  free(bytes[1]);
  if (!ok) {
    runner_free(&a->runner);
    a->failed = true;
  }
  return ok;
}
bool account_step(Account *a) {
  if (a->failed || !routed_step(&a->runner.host))
    return false;
  a->turns++;
  return account_take(a);
}
bool account_action(Account *a, unsigned action) {
  if (a->failed || action < 1 || action > 4 || a->attempts >= ACCOUNT_INPUTS ||
      a->event_count + 128 > ACCOUNT_EVENTS ||
      a->used + 128 >= ACCOUNT_OUTPUT || !routed_quiescent(&a->runner.host))
    return false;
  uint8_t byte = (uint8_t)action;
  if (routed_input(&a->runner.host, 7, &byte, 1) != ROUTED_INPUT_ACCEPTED ||
      !account_take(a))
    return false;
  a->attempts++;
  for (unsigned n = 0; n < 16; n++) {
    if (routed_quiescent(&a->runner.host))
      return true;
    if (!account_step(a))
      return false;
  }
  a->failed = true;
  return false;
}
bool account_same(const Account *a, const Account *b) {
  const RoutedHost *x = &a->runner.host, *y = &b->runner.host;
  if (a->single != b->single || a->failed != b->failed || a->used != b->used ||
      a->attempts != b->attempts || a->turns != b->turns ||
      a->event_count != b->event_count ||
      memcmp(a->output, b->output, sizeof(a->output)) ||
      memcmp(a->events, b->events, sizeof(a->events)) ||
      x->node_count != y->node_count || x->route_count != y->route_count ||
      x->next_node != y->next_node || x->trace_sequence != y->trace_sequence ||
      x->trace_count != y->trace_count || x->fault != y->fault ||
      x->trace_exhausted != y->trace_exhausted || x->booted != y->booted ||
      memcmp(x->trace, y->trace, sizeof(x->trace)) ||
      memcmp(x->links, y->links, x->route_count * sizeof(*x->links)))
    return false;
  for (size_t i = 0; i < x->node_count; i++) {
    const RoutedNode *p = &x->nodes[i], *q = &y->nodes[i];
    if (memcmp(p->uxn.ram, q->uxn.ram, sizeof(p->uxn.ram)) ||
        memcmp(p->uxn.devices, q->uxn.devices, sizeof(p->uxn.devices)) ||
        memcmp(&p->uxn.working, &q->uxn.working, sizeof(UxnStack)) ||
        memcmp(&p->uxn.return_stack, &q->uxn.return_stack, sizeof(UxnStack)) ||
        p->uxn.instructions != q->uxn.instructions ||
        p->next_incoming != q->next_incoming ||
        p->next_writable != q->next_writable ||
        p->prefer_receive != q->prefer_receive || p->source != q->source ||
        p->writable != q->writable || p->loaded != q->loaded || p->id != q->id)
      return false;
  }
  return true;
}
