#include "mystery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(c)                                                               \
  do {                                                                         \
    checks++;                                                                  \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static MysterySession *fresh(void) {
  MysterySession *s = calloc(1, sizeof(*s));
  CHECK(s);
  CHECK(mystery_init(s));
  CHECK(s->live->frames == 1);
  return s;
}
static void key(MysterySession *s, unsigned a) {
  uint8_t in[] = {1, (uint8_t)a, 0};
  CHECK(mystery_input(s, in));
}
static void replay(MysterySession *s) {
  CHECK(mystery_replay_begin(s));
  while (s->replaying)
    CHECK(mystery_replay_step(s));
  CHECK(s->replay_done);
  CHECK(mystery_same(s->live, s->play));
}
static void dispose(MysterySession *s) {
  mystery_free(s);
  mystery_free(s);
  free(s);
}

/* Independent model: authored facts/solution intentionally not read from the
 * ROM. */
typedef struct {
  unsigned facts, choice[3], message, question;
  bool key, ended;
} Model;
static void model_action(Model *m, unsigned a) {
  static const unsigned masks[] = {1, 2, 4, 24, 32};
  if (m->ended)
    return;
  if (a == 10) {
    if (m->question == 1) {
      m->question = 2;
      m->message = 11;
    }
    return;
  }
  if (a <= 5) {
    m->facts |= masks[a - 1];
    if (a == 4 && !m->question)
      m->question = 1;
    m->message = a;
    if (a == 3)
      m->key = true;
  } else if (a <= 8) {
    unsigned *c = &m->choice[a - 6];
    *c = *c % 3 + 1;
    m->message = 6;
  } else if (m->facts != 63)
    m->message = 7;
  else if (!m->choice[0] || !m->choice[1] || !m->choice[2])
    m->message = 8;
  else if (m->choice[0] != 3 || m->choice[1] != 1 ||
           m->choice[2] != (m->question == 2 ? 3u : 1u))
    m->message = 9;
  else {
    m->message = m->question == 2 ? 12 : 10;
    m->ended = true;
  }
}
static void step(MysterySession *s, Model *m, unsigned a) {
  model_action(m, a);
  key(s, a);
  const uint8_t *p = s->live->runner.host.nodes[0].uxn.ram;
  const uint8_t *d =
      MYSTERY_DEFAULT_SINGLE ? p : s->live->runner.host.nodes[1].uxn.ram;
  const uint8_t *k =
      MYSTERY_DEFAULT_SINGLE ? p : s->live->runner.host.nodes[2].uxn.ram;
  CHECK(p[0] == m->message && p[1] == m->facts && p[5] == m->ended);
  if (MYSTERY_DEFAULT_SINGLE) {
    CHECK(p[7] == m->key && p[6] == m->question);
    for (unsigned i = 0; i < 3; i++)
      CHECK(p[i + 2] == m->choice[i]);
    CHECK(!s->live->runner.host.nodes[0].uxn.working.pointer);
    CHECK(!s->live->runner.host.nodes[0].uxn.return_stack.pointer);
    return;
  }
  CHECK(d[0] == m->key && d[1] == m->ended && d[2] == 0);
  CHECK(k[0] == m->facts);
  CHECK(p[6] == m->question && d[3] == m->question);
  CHECK(k[5] == (m->question == 2));
  for (unsigned i = 0; i < 3; i++) {
    CHECK(p[i + 2] == m->choice[i] && k[i + 1] == m->choice[i]);
    CHECK(!s->live->runner.host.nodes[i].uxn.working.pointer);
    CHECK(!s->live->runner.host.nodes[i].uxn.return_stack.pointer);
  }
}
static void exhaustive(void) {
  /* Every discoverable evidence subset x every unanswered/answered hypothesis.
     Includes all 27 complete combinations; exactly one can win with all facts.
   */
  unsigned wins = 0;
  for (unsigned followup = 0; followup < 2; followup++)
    for (unsigned subset = 0; subset < 32; subset++)
      for (unsigned combination = 0; combination < 64; combination++) {
        MysterySession *s = fresh();
        Model m = {0};
        for (unsigned a = 1; a <= 5; a++)
          if (subset & (1u << (a - 1)))
            step(s, &m, a);
        if (followup)
          step(s, &m, 10);
        unsigned code = combination;
        for (unsigned field = 0; field < 3; field++, code >>= 2)
          for (unsigned n = 0; n < (code & 3); n++)
            step(s, &m, field + 6);
        step(s, &m, 9);
        wins += m.ended;
        replay(s);
        dispose(s);
      }
  CHECK(wins == 2);
}
static void orders(unsigned *order, unsigned depth, unsigned used) {
  if (depth < 5) {
    for (unsigned a = 1; a <= 5; a++)
      if (!(used & (1u << a))) {
        order[depth] = a;
        orders(order, depth + 1, used | (1u << a));
      }
    return;
  }
  MysterySession *s = fresh();
  Model m = {0};
  // A complete correct guess cannot bypass undiscovered evidence.
  step(s, &m, 6);
  step(s, &m, 6);
  step(s, &m, 6);
  step(s, &m, 7);
  step(s, &m, 8);
  for (unsigned i = 0; i < 5; i++) {
    step(s, &m, 9);
    CHECK(!m.ended);
    step(s, &m, order[i]);
    step(s, &m, order[i]);
  }
  step(s, &m, 9);
  CHECK(m.ended);
  CHECK(strstr(s->live->text, "HER DENIAL WAS FALSE"));
  CHECK(s->live->pixels[40 * 256 + 200] == 0xffede0bd);
  replay(s);
  replay(s);
  // Completed state stays completed for all actions.
  for (unsigned a = 1; a <= 10; a++)
    step(s, &m, a);
  replay(s);
  dispose(s);
  // Same evidence orders, but the desk enables a result that revises the
  // answer.
  s = fresh();
  m = (Model){0};
  step(s, &m, 10);
  for (unsigned i = 0; i < 5; i++) {
    step(s, &m, order[i]);
    if (order[i] == 4) {
      step(s, &m, 10);
      CHECK(m.question == 2);
      unsigned frames = s->live->frames;
      step(s, &m, 10);
      CHECK(s->live->frames == frames);
    }
  }
  step(s, &m, 6);
  step(s, &m, 6);
  step(s, &m, 6);
  step(s, &m, 7);
  step(s, &m, 8);
  step(s, &m, 9);
  CHECK(!m.ended && m.message == 9);
  step(s, &m, 8);
  step(s, &m, 8);
  step(s, &m, 9);
  CHECK(m.ended && m.message == 12);
  CHECK(strstr(s->live->text, "SHE KEPT NOTHING"));
  for (unsigned a = 1; a <= 10; a++)
    step(s, &m, a);
  replay(s);
  dispose(s);
}
static void boundaries(void) {
  /* Left/top inclusive, right/bottom exclusive; the labels are not guessed by
   * JS. */
  const unsigned points[][3] = {
      {14, 36, 1},   {62, 75, 1},   {63, 75, 0},   {13, 36, 0},   {136, 55, 2},
      {173, 113, 2}, {174, 113, 5}, {80, 30, 3},   {135, 113, 3}, {136, 113, 2},
      {8, 76, 4},    {79, 113, 4},  {80, 113, 3},  {174, 26, 5},  {239, 113, 5},
      {240, 113, 0}, {8, 114, 6},   {175, 123, 6}, {176, 123, 0}, {8, 124, 6},
      {8, 134, 6},   {182, 114, 7}, {247, 143, 7}, {248, 143, 0}, {174, 4, 0},
      {247, 21, 0},  {248, 21, 0},  {173, 4, 0},   {174, 22, 0},  {255, 191, 0},
      {153, 103, 2}, {100, 108, 3}, {195, 112, 5}};
  for (size_t i = 0; i < sizeof(points) / sizeof(*points); i++) {
    MysterySession *s = fresh();
    uint8_t in[] = {0, (uint8_t)points[i][0], (uint8_t)points[i][1]};
    CHECK(mystery_input(s, in));
    unsigned expected = points[i][2];
    CHECK(s->live->runner.host.nodes[0].uxn.ram[0] == expected);
    replay(s);
    dispose(s);
  }
  /* Bad external messages are ignored by the ROM. */
  for (unsigned fault = 0; fault < 5; fault++) {
    MysterySession *s = fresh();
    uint8_t data[] = {1, 1, 0};
    unsigned length = 3;
    if (fault == 0)
      length = 0;
    if (fault == 1)
      length = 2;
    if (fault == 2)
      data[0] = 2;
    if (fault == 3)
      data[1] = 11;
    if (fault == 4)
      data[2] = 1;
    CHECK(routed_input(&s->live->runner.host, 7, data, length) ==
          ROUTED_INPUT_ACCEPTED);
    unsigned turns = 0;
    while (!routed_quiescent(&s->live->runner.host)) {
      CHECK(turns++ < 32);
      CHECK(routed_step(&s->live->runner.host));
    }
    CHECK(s->live->frames == 1 &&
          (MYSTERY_DEFAULT_SINGLE
               ? s->live->runner.host.nodes[0].uxn.ram[7]
               : s->live->runner.host.nodes[1].uxn.ram[0]) == 0);
    dispose(s);
  }
  /* Malformed drawing commands cannot replace the committed display. */
  for (unsigned fault = 0; fault < 5; fault++) {
    MysterySession *s = fresh();
    MysteryMachine *m = s->live;
    Uxn *u = &m->runner.host.nodes[0].uxn;
    uint32_t before[256 * 192];
    memcpy(before, m->pixels, sizeof(before));
    u->devices[0x20] = 250;
    u->devices[0x21] = 10;
    u->devices[0x22] = 10;
    u->devices[0x23] = 10;
    if (fault == 1)
      u->devices[0x24] = 7;
    if (fault == 2)
      u->devices[0x28] = 161;
    if (fault == 3) {
      u->devices[0x26] = 255;
      u->devices[0x27] = 255;
      u->devices[0x28] = 2;
    }
    u->device_write(u, 0x25,
                    fault < 2    ? 2
                    : fault == 4 ? 255
                                 : 3,
                    u->device_context);
    CHECK(m->failed);
    CHECK(!memcmp(before, m->pixels, sizeof(before)));
    dispose(s);
  }
  MysterySession *s = fresh();
  key(s, 1);
  s->events[0].kind = CONSTELLATION_TRACE_IDLE;
  CHECK(!mystery_replay_begin(s));
  CHECK(s->failed);
  dispose(s);
}

static void protocol(void) {
  if (MYSTERY_DEFAULT_SINGLE)
    return; // No inter-machine protocol in this variant.
  // Invalid source, length, range and unsolicited verdicts are ignored.
  for (unsigned node = 0; node < 3; node++)
    for (unsigned fault = 0; fault < 7; fault++) {
      MysterySession *s = fresh();
      RoutedNode *n = &s->live->runner.host.nodes[node];
      Uxn *u = &n->uxn;
      n->source = node == 0 ? 1 : node == 1 ? 2 : 1;
      u->devices[0xd4] = node == 2 ? 1 : node == 0 ? 7 : 6;
      memset(u->ram + 0x800, 0, 7);
      if (node == 2)
        u->ram[0x800] = 1;
      if (fault == 0)
        n->source = 77;
      if (fault == 1)
        u->devices[0xd4] = 0;
      if (fault == 2)
        u->ram[0x800] = 255;
      if (fault == 3) {
        if (node == 2)
          u->ram[0x800] = 0;
        else
          u->ram[0x802] = 4;
      }
      if (fault == 4) {
        if (node == 2)
          u->devices[0xd4] = 6;
        else
          u->ram[0x805] = 2;
      }
      if (fault == 5) {
        if (node == 0)
          u->ram[0x801] = 64;
        else if (node == 2)
          n->source = 0;
      }
      if (fault == 6) {
        if (node == 0)
          u->ram[0x806] = 3;
        else if (node == 1) {
          n->source = 0;
          u->devices[0xd4] = 1;
          u->ram[0x800] = 10;
        } else {
          u->devices[0xd4] = 2;
          u->ram[0x800] = 10;
          u->ram[0x801] = 1;
        }
      }
      uint8_t before[256];
      memcpy(before, u->ram, 256);
      unsigned frames = s->live->frames;
      CHECK(uxn_eval(u, ((unsigned)u->devices[0xd0] << 8) | u->devices[0xd1],
                     10000) == UXN_STOP_BREAK);
      CHECK(!memcmp(before, u->ram, 256));
      CHECK(s->live->frames == frames);
      CHECK(routed_quiescent(&s->live->runner.host));
      dispose(s);
    }
}
static void rejected_message(MysterySession *s, unsigned node, unsigned source,
                             const uint8_t *message, unsigned length) {
  RoutedNode *n = &s->live->runner.host.nodes[node];
  Uxn *u = &n->uxn;
  uint8_t before[256];
  memcpy(before, u->ram, sizeof(before));
  unsigned frames = s->live->frames;
  n->source = source;
  u->devices[0xd4] = (uint8_t)length;
  memcpy(u->ram + 0x800, message, length);
  CHECK(uxn_eval(u, ((unsigned)u->devices[0xd0] << 8) | u->devices[0xd1],
                 10000) == UXN_STOP_BREAK);
  CHECK(!memcmp(before, u->ram, sizeof(before)));
  CHECK(frames == s->live->frames);
  CHECK(!u->working.pointer && !u->return_stack.pointer);
  CHECK(routed_quiescent(&s->live->runner.host));
}
static void conditional_protocol(void) {
  const uint8_t request[] = {10}, result[] = {10, 1};
  const uint8_t wrong_tag[] = {11, 1}, wrong_value[] = {10, 2};
  MysterySession *s = fresh();
  Model m = {0};
  if (!MYSTERY_DEFAULT_SINGLE) {
    rejected_message(s, 1, 0, request, 1);
    rejected_message(s, 2, 1, result,
                     2); // Correct shape but prerequisite missing.
    step(s, &m, 4);
    rejected_message(s, 2, 0, result, 2);
    rejected_message(s, 2, 1, result, 1);
    rejected_message(s, 2, 1, wrong_tag, 2);
    rejected_message(s, 2, 1, wrong_value, 2);
    step(s, &m, 10);
    rejected_message(s, 1, 0, request, 1);
    rejected_message(s, 2, 1, result, 2); // Already applied: no second result.
  }
  dispose(s); // Direct injections are deliberately not part of session replay.

  // Actual canvas input follows the identical gate and replays exactly.
  s = fresh();
  uint8_t click[] = {0, 200, 12};
  unsigned frames = s->live->frames;
  CHECK(mystery_input(s, click));
  CHECK(s->live->frames == frames);
  key(s, 4);
  CHECK(mystery_input(s, click));
  CHECK(s->live->runner.host.nodes[0].uxn.ram[6] == 2);
  frames = s->live->frames;
  CHECK(mystery_input(s, click));
  CHECK(s->live->frames == frames);
  replay(s);
  dispose(s);

  // Measure the accepted follow-up against an ordinary discovery.
  s = fresh();
  unsigned turns = s->live->turns;
  size_t events = s->event_count;
  key(s, 4);
  printf("Desk discovery: %u turns, %zu trace events. ", s->live->turns - turns,
         s->event_count - events);
  turns = s->live->turns;
  events = s->event_count;
  key(s, 10);
  printf("Follow-up: %u turns, %zu trace events. ", s->live->turns - turns,
         s->event_count - events);
  turns = s->live->turns;
  events = s->event_count;
  key(s, 10);
  printf("Duplicate: %u turns, %zu trace events.\n", s->live->turns - turns,
         s->event_count - events);
  replay(s);
  dispose(s);
}
int main(void) {
  MysterySession *s = fresh();
  Model m = {0};
  CHECK(strstr(s->live->text, "ONE FALSE ACCOUNT"));
  replay(s);
  // Wrong conclusion can be corrected without resetting or losing evidence.
  const unsigned sequence[] = {1, 2, 3, 4, 5, 9, 6, 7, 8, 9, 6, 6, 9};
  for (unsigned i = 0; i < sizeof(sequence) / sizeof(*sequence); i++)
    step(s, &m, sequence[i]);
  CHECK(m.ended);
  replay(s);
  uint8_t invalid[] = {2, 0, 0};
  size_t count = s->count;
  CHECK(!mystery_input(s, invalid));
  CHECK(s->count == count);
  unsigned frames = s->live->frames;
  uint8_t miss[] = {0, 0, 191};
  CHECK(mystery_input(s, miss));
  CHECK(s->live->frames == frames);
  CHECK(!mystery_init(s));
  dispose(s);
  // Mixed exploration/selection/revision, including wrap-around, against model.
  for (unsigned trial = 0; trial < 16; trial++) {
    s = fresh();
    m = (Model){0};
    unsigned seed = 17 + trial;
    for (unsigned i = 0; i < 100; i++) {
      seed = seed * 1664525u + 1013904223u;
      step(s, &m, (seed >> 16) % 10 + 1);
    }
    replay(s);
    dispose(s);
  }
  s = fresh();
  for (unsigned i = 0; i < MYSTERY_INPUTS; i++)
    key(s, 4);
  count = s->event_count;
  uint8_t action[] = {1, 4, 0};
  CHECK(!mystery_input(s, action));
  CHECK(s->sealed && !s->failed && s->event_count == count);
  replay(s);
  dispose(s);
  exhaustive();
  unsigned order[5];
  orders(order, 0, 0);
  boundaries();
  protocol();
  conditional_protocol();
  printf("%u mystery checks passed: 4096 evidence/hypothesis branches, 120 "
         "discovery orders, revisions, invalid protocols, recording bounds and "
         "exact replay.\n",
         checks);
  return 0;
}
