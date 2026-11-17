#include "account.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks, checkpoints;
#define CHECK(c)                                                               \
  do {                                                                         \
    checks++;                                                                  \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static const char *const record_text[] = {
    "DESK RECORD: CORA KEPT THE LEDGER PAGE.\n",
    "DESK RECORD: THE PAGE IS NOW BACK IN THE DRAWER.\n"};
static const char *const belief_text[] = {
    "BRAM: CORA STILL HAS THE PAGE.\n",
    "BRAM: THE PAGE IS BACK IN THE DRAWER.\n"};
static const char *shared = "YOU SHOW BRAM THE CURRENT DESK ENTRY.\n";
static Account *fresh(bool single, bool attack) {
  Account *a = calloc(1, sizeof(*a));
  CHECK(a && account_start(a, single, attack));
  return a;
}
static void dispose(Account *a) {
  account_free(a);
  free(a);
}
static unsigned belief(const Account *a) {
  return a->runner.host.nodes[a->single ? 0 : 1].uxn.ram[0x11];
}
static void drain(Account *a) {
  for (unsigned i = 0; !routed_quiescent(&a->runner.host); i++) {
    CHECK(i < 16);
    CHECK(account_step(a));
  }
}
static void external(Account *a, unsigned selector, const uint8_t *p,
                     size_t n) {
  CHECK(routed_input(&a->runner.host, selector, p, n) == ROUTED_INPUT_ACCEPTED);
  CHECK(account_take(a));
  drain(a);
}
/* Reconstruct belief solely from authenticated DELIVER events. No guest RAM
 * supplies the reconstruction. Numeric protocol semantics are fixture-local. */
static void explain(const Account *a) {
  unsigned record = 0, known = 0, answers = 0, learns = 0;
  for (size_t i = 0; i < a->event_count; i++) {
    const RoutedEvent *e = a->events + i;
    CHECK(e->sequence == i);
    if (e->kind != CONSTELLATION_TRACE_DELIVER)
      continue;
    const uint8_t *p = e->message.data;
    if (e->from == 255 && e->to == 0 && e->message.length == 1) {
      if (p[0] == 3)
        record = 1;
      if (a->single && p[0] == 4)
        known = record;
    }
    if (!a->single && e->from == 0 && e->to == 1 && e->message.length == 2) {
      CHECK(p[0] == 0x20 && p[1] == record);
      known = p[1];
      learns++;
    }
    if (!a->single && e->from == 1 && e->to == 0) {
      CHECK(e->message.length == 2 && (p[0] == 0x30 || p[0] == 0x31));
      CHECK(p[1] == known);
      if (p[0] == 0x30)
        answers++;
    }
  }
  CHECK(known == belief(a) && record == a->runner.host.nodes[0].uxn.ram[0x10]);
  (void)answers;
  (void)learns;
}
/* Oracle is independent of guest code/data: record changes on 3; belief only
 * on 4. */
static void behavior(void) {
  for (unsigned word = 0; word < 1024; word++) {
    Account *multi = fresh(false, false), *single = fresh(true, false);
    char expected[1024];
    snprintf(expected, sizeof(expected), "%s", record_text[0]);
    unsigned record = 0, known = 0, code = word;
    uint8_t log[5];
    for (unsigned step = 0; step < 5; step++, code >>= 2) {
      unsigned action = code % 4 + 1;
      log[step] = (uint8_t)action;
      if (action == 3)
        record = 1;
      if (action == 4)
        known = record;
      const char *line = action == 2   ? belief_text[known]
                         : action == 4 ? shared
                                       : record_text[record];
      strcat(expected, line);
      CHECK(account_action(multi, action));
      CHECK(account_action(single, action));
      CHECK(!strcmp(multi->output, expected));
      CHECK(!strcmp(single->output, expected));
      CHECK(multi->runner.host.nodes[0].uxn.ram[0x10] == record);
      CHECK(single->runner.host.nodes[0].uxn.ram[0x10] == record);
      CHECK(belief(multi) == known && belief(single) == known);
      CHECK(multi->runner.host.nodes[0].uxn.ram[0x12] == 0);
      for (unsigned i = 0; i < 2; i++)
        CHECK(!multi->runner.host.nodes[i].uxn.working.pointer &&
              !multi->runner.host.nodes[i].uxn.return_stack.pointer);
      CHECK(!single->runner.host.nodes[0].uxn.working.pointer &&
            !single->runner.host.nodes[0].uxn.return_stack.pointer);
      checkpoints++;
    }
    explain(multi);
    explain(single);
    /* Playback starts only after capture finished. No oracle drives its timing.
     */
    Account *mr = fresh(false, false), *sr = fresh(true, false);
    for (unsigned i = 0; i < 5; i++) {
      CHECK(account_action(mr, log[i]));
      CHECK(account_action(sr, log[i]));
    }
    CHECK(account_same(multi, mr));
    CHECK(account_same(single, sr));
    dispose(multi);
    dispose(single);
    dispose(mr);
    dispose(sr);
  }
}
static void boundaries(void) {
  Account *a = fresh(false, true);
  uint8_t spoof[] = {0x20, 1};
  size_t start = a->event_count, used = a->used;
  external(a, 8, spoof, 2);
  CHECK(belief(a) == 0 && a->used == used);
  bool seen = false;
  for (size_t i = start; i < a->event_count; i++) {
    const RoutedEvent *e = a->events + i;
    if (e->kind == CONSTELLATION_TRACE_DELIVER && e->to == 1) {
      CHECK(e->from == 255 && e->message.length == 2 &&
            !memcmp(e->message.data, spoof, 2));
      seen = true;
    }
  }
  CHECK(seen);
  CHECK(account_action(a, 3));
  /* A return event cannot write ANY byte of the other guest's memory. */
  Uxn *w = &a->runner.host.nodes[1].uxn;
  uint8_t *before = malloc(sizeof(w->ram));
  CHECK(before);
  memcpy(before, w->ram, sizeof(w->ram));
  CHECK(account_action(a, 1));
  CHECK(account_action(a, 3));
  CHECK(!memcmp(before, w->ram, sizeof(w->ram)));
  free(before);
  /* Admit Share, run only record node; message is queued, not learned. */
  uint8_t share = 4;
  CHECK(routed_input(&a->runner.host, 7, &share, 1) == ROUTED_INPUT_ACCEPTED);
  CHECK(account_take(a));
  for (unsigned n = 0; !a->runner.host.links[1].queue.count; n++) {
    CHECK(n < 3);
    CHECK(account_step(a));
  }
  CHECK(belief(a) == 0 && a->runner.host.nodes[0].uxn.ram[0x12] == 4);
  CHECK(a->runner.host.links[1]
            .queue.messages[a->runner.host.links[1].queue.head]
            .data[1] == 1);
  CHECK(account_step(a));
  CHECK(belief(a) == 1); // Delivery, not send, changed the belief.
  drain(a);
  CHECK(a->runner.host.nodes[0].uxn.ram[0x12] == 0);
  CHECK(account_action(a, 2));
  CHECK(strstr(a->output, belief_text[1]));
  dispose(a);

  /* Same actions, different logical order: sharing BEFORE return stays stale.
   */
  a = fresh(false, false);
  Account *b = fresh(false, false);
  const unsigned early[] = {4, 3, 2}, late[] = {3, 4, 2};
  for (unsigned i = 0; i < 3; i++) {
    CHECK(account_action(a, early[i]));
    CHECK(account_action(b, late[i]));
  }
  CHECK(belief(a) == 0 && belief(b) == 1);
  CHECK(strcmp(a->output, b->output));
  CHECK(!account_same(a, b));
  dispose(a);
  dispose(b);

  /* Real Uxn store/read instructions, executed only in the record machine.
     In the single control the same address IS Bram's state; in multi it is not.
     This is explicit test-only cartridge corruption, not a production shortcut.
   */
  for (unsigned single = 0; single < 2; single++) {
    a = fresh(single, false);
    Uxn *u = &a->runner.host.nodes[0].uxn;
    const uint8_t probe[] = {0x80, 1,    0x80, 0x11, 0x11, 0x80,
                             0x11, 0x10, 0x80, 0x13, 0x11, 0};
    memcpy(u->ram + 0xa00, probe, sizeof(probe));
    CHECK(uxn_eval(u, 0xa00, 100) == UXN_STOP_BREAK);
    CHECK(u->ram[0x13] == 1);
    CHECK(belief(a) == single);
    CHECK(account_action(a, 2));
    CHECK(strstr(a->output, belief_text[single]));
    dispose(a);
  }
}
static void protocols(void) {
  /* Execute test-only ordinary send bytecode in authenticated record node.
     Test malformed tag/length/value independently of its normal encoder. */
  const uint8_t cases[][3] = {
      {0x20, 2, 0}, {0x20, 255, 0}, {0x10, 0, 0}, {0x21, 1, 0}, {0x20, 1, 0}};
  const unsigned lengths[] = {2, 2, 2, 2, 3};
  for (unsigned k = 0; k < 5; k++) {
    Account *a = fresh(false, false);
    Uxn *u = &a->runner.host.nodes[0].uxn;
    uint8_t code[] = {0xa0, 0x0b, 0,    0x80, 0xd6, 0x37, 0x80, 0, 0x80,
                      0xd8, 0x17, 0x80, 1,    0x80, 0xd9, 0x17, 0};
    code[7] = (uint8_t)lengths[k];
    memcpy(u->ram + 0xa00, code, sizeof(code));
    memcpy(u->ram + 0xb00, cases[k], 3);
    CHECK(uxn_eval(u, 0xa00, 100) == UXN_STOP_BREAK);
    CHECK(account_take(a));
    drain(a);
    CHECK(belief(a) == 0 && a->used == strlen(record_text[0]));
    dispose(a);
  }
  for (unsigned single = 0; single < 2; single++) {
    Account *a = fresh(single, false);
    const uint8_t invalid[] = {0, 5, 255};
    for (unsigned i = 0; i < 3; i++) {
      external(a, 7, invalid + i, 1);
      CHECK(a->used == strlen(record_text[0]));
    }
    external(a, 7, NULL, 0);
    external(a, 7, invalid, 3);
    CHECK(a->used == strlen(record_text[0]));
    for (unsigned i = 0; i < 64; i++)
      CHECK(account_action(a, 2));
    size_t n = a->event_count;
    CHECK(!account_action(a, 2));
    CHECK(a->event_count == n && !a->failed);
    CHECK(!account_action(a, 0) && !account_action(a, 5));
    dispose(a);
  }
}
static void in_flight_replay(void) {
  Account *a = fresh(false, false);
  CHECK(account_action(a, 3));
  uint8_t share = 4;
  CHECK(routed_input(&a->runner.host, 7, &share, 1) == ROUTED_INPUT_ACCEPTED);
  CHECK(account_take(a));
  unsigned steps = 0;
  while (!a->runner.host.links[1].queue.count) {
    CHECK(account_step(a));
    CHECK(++steps <= 2);
  }
  CHECK(!routed_quiescent(&a->runner.host) && belief(a) == 0);
  /* Capture ends here. Preserve the input and exact ending turn, not a drain.
   */
  Account *b = fresh(false, false);
  CHECK(account_action(b, 3));
  CHECK(routed_input(&b->runner.host, 7, &share, 1) == ROUTED_INPUT_ACCEPTED);
  CHECK(account_take(b));
  for (unsigned i = 0; i < steps; i++)
    CHECK(account_step(b));
  CHECK(account_same(a, b));
  drain(a);
  drain(b);
  CHECK(account_same(a, b));
  CHECK(belief(a) == 1);
  /* Altered evidence is detected, rather than calling any replay a success. */
  b->runner.host.nodes[1].uxn.ram[0x11] = 0;
  CHECK(!account_same(a, b));
  b->runner.host.nodes[1].uxn.ram[0x11] = 1;
  b->events[0].sequence++;
  CHECK(!account_same(a, b));
  dispose(a);
  dispose(b);
}
static void limit_and_fault(void) {
  for (unsigned single = 0; single < 2; single++) {
    Account *a = fresh(single, false);
    for (unsigned i = 0; i < 64; i++)
      CHECK(account_action(a, i % 4 + 1));
    CHECK(!account_action(a, 2));
    Account *b = fresh(single, false);
    for (unsigned i = 0; i < 64; i++)
      CHECK(account_action(b, i % 4 + 1));
    CHECK(account_same(a, b));
    dispose(a);
    dispose(b);
  }
  Account *a = fresh(false, false);
  CHECK(account_action(a, 3));
  /* Missing witness handler is terminal for the host, not graceful isolation. */
  a->runner.host.nodes[1].uxn.devices[0xd0] = 0;
  a->runner.host.nodes[1].uxn.devices[0xd1] = 0;
  CHECK(!account_action(a, 2));
  CHECK(a->runner.host.fault == ROUTED_RECEIVE_VECTOR_MISSING);
  CHECK(!account_action(a, 1));
  dispose(a);
}
static uint64_t instructions(Account *a) {
  uint64_t n = 0;
  for (size_t i = 0; i < a->runner.host.node_count; i++)
    n += a->runner.host.nodes[i].uxn.instructions;
  return n;
}
static void measure(void) {
  for (unsigned single = 0; single < 2; single++) {
    Account *a = fresh(single, false);
    uint64_t boot = instructions(a);
    size_t events = a->event_count;
    unsigned actions[] = {1, 3, 2, 4, 2};
    for (unsigned i = 0; i < 5; i++)
      CHECK(account_action(a, actions[i]));
    printf(
        "%s: %" PRIu64
        " post-boot instructions, %u turns, %zu events, %zu node/link bytes.\n",
        single ? "single" : "multi", instructions(a) - boot, a->turns,
        a->event_count - events,
        a->runner.host.node_count * sizeof(RoutedNode) +
            a->runner.host.route_count * sizeof(RoutedLink));
    dispose(a);
  }
}
int main(void) {
  behavior();
  boundaries();
  protocols();
  in_flight_replay();
  limit_and_fault();
  measure();
  printf("%u assertions; %u exact observable/model checkpoints across all 1024 "
         "five-action words; independent full-state replay, origin, delivery "
         "timing and memory-boundary probes passed.\n",
         checks, checkpoints);
}
