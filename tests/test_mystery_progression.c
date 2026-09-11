#include "mystery.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks, checkpoints, scenarios;
#define CHECK(c)                                                               \
  do {                                                                         \
    checks++;                                                                  \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
typedef struct {
  unsigned msg, facts, who, how, answer, exit, q, key, compared, cora, receipt,
      phase, wait, sealed;
} Model;
typedef struct {
  MysterySession *multi, *single;
  Model model;
} Pair;
static MysterySession *fresh(MysteryVariant v) {
  MysterySession *s = calloc(1, sizeof(*s));
  CHECK(s);
  if (!mystery_init_variant(s, v))
    fprintf(stderr, "init: %s\n", s->error);
  CHECK(s->live && !s->failed);
  return s;
}
static void dispose(MysterySession *s) {
  mystery_free(s);
  free(s);
}
static uint8_t *desk(MysterySession *s) {
  return s->live->runner.host
      .nodes[s->variant == MYSTERY_PROGRESSION_MULTI ? 1 : 0]
      .uxn.ram;
}
static void output(Pair *p) {
  CHECK(!strcmp(p->multi->live->text, p->single->live->text));
  CHECK(!memcmp(p->multi->live->pixels, p->single->live->pixels,
                sizeof(p->multi->live->pixels)));
  CHECK(p->multi->live->frames == p->single->live->frames);
  checkpoints++;
}
static Pair pair(void) {
  Pair p = {
      fresh(MYSTERY_PROGRESSION_MULTI), fresh(MYSTERY_PROGRESSION_SINGLE), {0}};
  scenarios++;
  output(&p);
  return p;
}
/* Independent stable-cut behavioral oracle. Expected outcome comes from the
   player's pre-incident request, not TAL, JSON, messages or the other ROM. */
static void model(Model *m, unsigned a) {
  static const unsigned masks[] = {0, 1, 2, 4, 24, 32};
  if (m->exit)
    return;
  if (a == 10) {
    if (m->q != 1)
      return;
    m->q = 2;
    m->answer = m->compared = 0;
    m->cora = m->phase = 3;
    m->receipt = m->wait ? 2 : 1;
    m->sealed = 1;
    m->msg = 11;
  } else if (a <= 5) {
    m->facts |= masks[a];
    m->msg = a;
    if (a == 3)
      m->key = 1;
    if (!m->q && (m->facts & 24) == 24)
      m->q = 1;
    if (a == 2 && m->phase < 2)
      m->wait = 1;
    if (m->q == 2) {
      if (a == 2) {
        m->compared |= 1;
        m->msg = 12 + m->receipt;
      }
      if (a == 3) {
        m->compared |= 2;
        m->msg = 16 + m->receipt;
      }
      if (a == 4)
        m->msg = 15;
    }
  } else if (a < 9) {
    unsigned *v = a == 6 ? &m->who : a == 7 ? &m->how : &m->answer;
    *v = *v % 3 + 1;
    m->msg = 6;
  } else {
    m->msg = m->facts != 63                     ? 7
             : m->q == 2 && m->compared != 3    ? 16
             : !m->who || !m->how || !m->answer ? 8
             : m->who != 3 || m->how != 1 ||
                     m->answer != (m->q == 2 ? m->receipt : 1) || !m->key
                 ? 9
             : m->q == 2 ? 12
                         : 10;
    m->exit = m->msg == 12 || m->msg == 10;
  }
}
static void assert_model(Pair *p) {
  Model *m = &p->model;
  unsigned e[] = {m->msg,    m->facts, m->who, m->how,
                  m->answer, m->exit,  m->q,   m->key};
  for (unsigned j = 0; j < 2; j++) {
    MysterySession *s = j ? p->single : p->multi;
    uint8_t *r = s->live->runner.host.nodes[0].uxn.ram, *d = desk(s);
    for (unsigned i = 0; i < 8; i++) {
      if (r[i] != e[i])
        fprintf(stderr, "%s byte %u actual %u expected %u\n",
                j ? "single" : "multi", i, r[i], e[i]);
      CHECK(r[i] == e[i]);
    }
    CHECK(r[10] == m->compared && r[12] == m->cora && r[13] == m->receipt &&
          !r[11]);
    CHECK(d[16] == m->phase && d[17] == m->wait && d[18] == m->sealed &&
          d[19] == m->receipt);
    for (size_t i = 0; i < s->live->runner.host.node_count; i++) {
      Uxn *u = &s->live->runner.host.nodes[i].uxn;
      CHECK(!u->working.pointer && !u->return_stack.pointer && !u->ram[254]);
    }
  }
}
static void action(Pair *p, unsigned a, bool click) {
  static const uint8_t xy[][2] = {{30, 60},   {150, 100}, {100, 105}, {30, 105},
                                  {200, 105}, {20, 118},  {20, 128},  {20, 138},
                                  {200, 125}, {200, 12}};
  uint8_t data[] = {1, (uint8_t)a, 0};
  if (click) {
    data[0] = 0;
    data[1] = xy[a - 1][0];
    data[2] = xy[a - 1][1];
  }
  for (unsigned j = 0; j < 2; j++) {
    MysterySession *s = j ? p->single : p->multi;
    bool ok = mystery_input(s, data);
    if (!ok)
      fprintf(stderr, "input %u: %s\n", a, s->error);
    CHECK(ok);
  }
  model(&p->model, a);
  assert_model(p);
  output(p);
}
static void script(Pair *p, const char *s) {
  for (; *s; s++)
    action(p, *s == 'q' ? 10 : (unsigned)(*s - '0'), false);
}
static void explain(MysterySession *s);
static void finish(Pair *p) {
  explain(p->multi);
  for (unsigned i = 0; i < 2; i++) {
    MysterySession *s = i ? p->single : p->multi;
    CHECK(mystery_replay_begin(s));
    while (s->replaying)
      CHECK(mystery_replay_step(s));
    CHECK(s->replay_done && mystery_same(s->live, s->play));
  }
  dispose(p->multi);
  dispose(p->single);
}
/* Reconstruct both progressions from authenticated DELIVER events. This needs
   the cartridge transition rules, but not RAM snapshots or prose selection.
   SEND is intentionally not treated as receipt or observation. */
static void explain(MysterySession *s) {
  unsigned phase = 0, wait = 0, sealed = 0, location = 0, cora = 0, receipt = 0;
  const RoutedEvent *events = s->live->runner.host.trace_count
                                 ? s->live->runner.host.trace
                                 : s->events;
  size_t count = s->live->runner.host.trace_count
                     ? s->live->runner.host.trace_count
                     : s->event_count;
  for (size_t i = 0; i < count; i++) {
    const RoutedEvent *e = events + i;
    if (e->kind != CONSTELLATION_TRACE_DELIVER || e->message.length != 2)
      continue;
    unsigned op = e->message.data[0], v = e->message.data[1];
    if (e->from == 0 && e->to == 1 && !v) {
      if (op == 0x10 && phase < 2)
        wait = 1;
      if (op == 0x40 && !phase)
        phase = 1;
      if (op == 0x20 && phase && !location)
        location = sealed ? 1 : 2;
    }
    if (e->from == 1 && e->to == 1 && op == 0x60 && v == phase && phase >= 1 &&
        phase <= 2) {
      phase++;
      if (phase == 3 || !wait)
        sealed = 1;
    }
    if (e->from == 0 && e->to == 0 && op == 0x50) {
      if (!cora)
        cora = 1;
      CHECK(v == cora);
      cora++;
    }
    if (e->from == 1 && e->to == 0 && op == 0x31) {
      CHECK(v == location);
      receipt = v;
    }
  }
  uint8_t *d = desk(s), *r = s->live->runner.host.nodes[0].uxn.ram;
  CHECK(d[16] == phase && d[17] == wait && d[18] == sealed &&
        d[19] == location);
  CHECK(r[12] == cora && r[13] == receipt);
}
static void step(RoutedHost *h) {
  CHECK(routed_step(h));
  CHECK(h->fault == ROUTED_OK);
}
static void drain(RoutedHost *h) {
  for (unsigned i = 0; !routed_quiescent(h); i++) {
    CHECK(i < 32);
    step(h);
  }
}
/* A test-only guest sends through its actual attached device. No fabricated
   queue contents and no source spoofing by changing a trace or payload. */
static void packet(MysterySession *s, unsigned node, unsigned route,
                   unsigned op, unsigned value, unsigned length) {
  Uxn *u = &s->live->runner.host.nodes[node].uxn;
  u->ram[0x900] = (uint8_t)op;
  u->ram[0x901] = (uint8_t)value;
  u->devices[0xdc] = (uint8_t)route;
  u->devices[0xd8] = (uint8_t)length;
  const uint8_t send[] = {0x80, 1, 0x80, 0xd9, 0x17, 0};
  memcpy(u->ram + 0xa00, send, sizeof(send));
  CHECK(uxn_eval(u, 0xa00, 100) == UXN_STOP_BREAK);
  u->devices[0xd8] = 2;
}
static void raw_incident(MysterySession *s) {
  uint8_t desk_input[] = {1, 4, 0}, q[] = {1, 10, 0};
  CHECK(mystery_input(s, desk_input));
  CHECK(routed_input(&s->live->runner.host, 7, q, 3) == ROUTED_INPUT_ACCEPTED);
}
static void boundaries(void) {
  MysterySession *s = fresh(MYSTERY_PROGRESSION_MULTI);
  raw_incident(s);
  RoutedHost *h = &s->live->runner.host;
  uint8_t *c = h->nodes[0].uxn.ram, *d = desk(s);
  for (unsigned i = 0; !c[12]; i++) {
    CHECK(i < 3);
    step(h);
  }
  CHECK(c[12] == 1 && d[16] == 0); // Cora fetching, Bram has not begun.
  step(h);
  CHECK(c[12] == 1 && d[16] == 1);
  step(h);
  CHECK(c[12] == 2 && d[16] == 1); // Cora advances without Bram.
  step(h);
  CHECK(c[12] == 2 && d[16] == 2 && d[18] == 1);
  step(h);
  CHECK(c[12] == 3 && !c[13] && !d[19]); // Offer sent, not delivered.
  step(h);
  CHECK(d[19] == 1 && !c[13]); // Desk record changed, Cora not informed.
  drain(h);
  CHECK(c[13] == 1 && d[16] == 3);
  explain(s);
  dispose(s);

  // Bram completes his own round with no Cora progression or page message.
  s = fresh(MYSTERY_PROGRESSION_MULTI);
  packet(s, 0, 1, 0x40, 0, 2);
  drain(&s->live->runner.host);
  CHECK(desk(s)[16] == 3 && desk(s)[18] == 1 && !desk(s)[19]);
  CHECK(!s->live->runner.host.nodes[0].uxn.ram[12]);
  explain(s);
  dispose(s);

  // A request can be SENT at phase 1 and delivered only AFTER sealing because
  // a self-continuation is next in the existing incoming-route rotation.
  // Replay this exact boundary script twice; no elapsed-time sleeps.
  MysterySession *runs[2];
  for (unsigned run = 0; run < 2; run++) {
    runs[run] = s = fresh(MYSTERY_PROGRESSION_MULTI);
    raw_incident(s);
    h = &s->live->runner.host;
    for (unsigned i = 0; desk(s)[16] != 1; i++) {
      CHECK(i < 4);
      step(h);
    }
    CHECK(!desk(s)[18]);
    packet(s, 0, 1, 0x10, 0, 2);
    step(h);
    step(h);
    CHECK(desk(s)[16] == 2 && desk(s)[18] == 1 && !desk(s)[17]);
    drain(h);
    CHECK(!desk(s)[17] && desk(s)[19] == 1);
    explain(s);
  }
  CHECK(mystery_same(runs[0]->live, runs[1]->live));
  CHECK(runs[0]->live->runner.host.trace_count ==
        runs[1]->live->runner.host.trace_count);
  CHECK(!memcmp(runs[0]->live->runner.host.trace,
                runs[1]->live->runner.host.trace,
                runs[0]->live->runner.host.trace_count * sizeof(RoutedEvent)));
  dispose(runs[0]);
  dispose(runs[1]);

  // Physically isolated accidental wait-field write: corrupting main RAM at
  // Bram's address changes the single outcome, not the separate desk machine.
  // A deliberate mutation probe, not an observed real-world authoring error.
  for (unsigned single = 0; single < 2; single++) {
    s = fresh(single ? MYSTERY_PROGRESSION_SINGLE : MYSTERY_PROGRESSION_MULTI);
    Uxn *u = &s->live->runner.host.nodes[0].uxn;
    const uint8_t store[] = {0x80, 1, 0x80, 0x11, 0x11, 0};
    memcpy(u->ram + 0xa00, store, sizeof(store));
    CHECK(uxn_eval(u, 0xa00, 100) == UXN_STOP_BREAK);
    uint8_t x[] = {1, 4, 0}, q[] = {1, 10, 0};
    CHECK(mystery_input(s, x));
    CHECK(mystery_input(s, q));
    CHECK(u->ram[13] == 1 + single);
    dispose(s);
  }
}
static void faults(void) {
  MysterySession *s = fresh(MYSTERY_PROGRESSION_MULTI);
  s->live->runner.host.nodes[1].uxn.devices[0xd0] = 0;
  s->live->runner.host.nodes[1].uxn.devices[0xd1] = 0;
  uint8_t x[] = {1, 4, 0}, q[] = {1, 10, 0};
  CHECK(mystery_input(s, x));
  CHECK(!mystery_input(s, q));
  CHECK(s->live->runner.host.fault == ROUTED_RECEIVE_VECTOR_MISSING);
  CHECK(!mystery_replay_begin(s));
  dispose(s);
  s = fresh(MYSTERY_PROGRESSION_MULTI);
  CHECK(mystery_input(s, x));
  CHECK(mystery_input(s, q));
  bool changed = false;
  for (size_t i = 0; i < s->event_count; i++) {
    RoutedEvent *e = s->events + i;
    if (e->kind == CONSTELLATION_TRACE_DELIVER && e->from == 1 && e->to == 1) {
      e->message.data[1]++;
      changed = true;
      break;
    }
  }
  CHECK(changed && mystery_replay_begin(s));
  while (s->replaying && !s->failed)
    (void)mystery_replay_step(s);
  CHECK(s->failed && !strcmp(s->error, "Replay trace differs."));
  dispose(s);

  // Malformed, wrong-source self progress, unstarted offer and duplicate start.
  const unsigned bad[][4] = {{1, 0x40, 1, 2},
                             {1, 0x40, 0, 3},
                             {1, 0x60, 1, 2},
                             {1, 0x20, 0, 2},
                             {2, 0x50, 0, 2}};
  for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
    s = fresh(MYSTERY_PROGRESSION_MULTI);
    packet(s, 0, bad[i][0], bad[i][1], bad[i][2], bad[i][3]);
    drain(&s->live->runner.host);
    CHECK(!desk(s)[16] && !desk(s)[17] && !desk(s)[18] && !desk(s)[19]);
    CHECK(!s->live->runner.host.nodes[0].uxn.ram[12]);
    dispose(s);
  }
  s = fresh(MYSTERY_PROGRESSION_MULTI);
  packet(s, 0, 1, 0x40, 0, 2);
  drain(&s->live->runner.host);
  packet(s, 0, 1, 0x40, 0, 2);
  packet(s, 1, 2, 0x60, 2, 2);
  drain(&s->live->runner.host);
  CHECK(desk(s)[16] == 3 && desk(s)[18] == 1 && !desk(s)[19]);
  dispose(s);
}
static void behavior(void) {
  const char *paths[] = {"q24q2323qq",     "4q2323",          "4q3232",
                         "12345666789q23", "123458q23666789", "4q1235666789",
                         "12345q236667889"};
  for (unsigned i = 0; i < sizeof(paths) / sizeof(*paths); i++) {
    Pair p = pair();
    script(&p, paths[i]);
    finish(&p);
  }
  /* 2 timings x 4 post-return account subsets x 4 WHY values. */
  for (unsigned early = 0; early < 2; early++)
    for (unsigned seen = 0; seen < 4; seen++)
      for (unsigned why = 0; why < 4; why++) {
        Pair p = pair();
        script(&p, early ? "12345q" : "1345q");
        /* Late path must gather Bram evidence, so missing Bram comparison is
           intentionally also missing his baseline fact. Other subsets isolate
           the fresh-Cora and fresh-Bram guards on the early path. */
        if (seen & 1)
          script(&p, "2");
        if (seen & 2)
          script(&p, "3");
        script(&p, "6667");
        for (unsigned i = 0; i < why; i++)
          script(&p, "8");
        script(&p, "9");
        CHECK(p.model.exit == (seen == 3 && why == (early ? 2u : 1u)));
        finish(&p);
      }
  Pair p = pair();
  const char *s = "4q1235666789";
  for (; *s; s++)
    action(&p, *s == 'q' ? 10 : (unsigned)(*s - '0'), true);
  CHECK(p.model.exit);
  finish(&p);
}
static uint64_t instructions(MysterySession *s) {
  uint64_t n = 0;
  for (size_t i = 0; i < s->live->runner.host.node_count; i++)
    n += s->live->runner.host.nodes[i].uxn.instructions;
  return n;
}
static void measure(void) {
  for (unsigned early = 0; early < 2; early++) {
    Pair p = pair();
    uint64_t base[] = {instructions(p.multi), instructions(p.single)};
    size_t events[] = {p.multi->event_count, p.single->event_count};
    const char *path = early ? "12345q236667889" : "4q1235666789";
    script(&p, path);
    CHECK(p.model.exit);
    for (unsigned i = 0; i < 2; i++) {
      MysterySession *s = i ? p.single : p.multi;
      RoutedHost *h = &s->live->runner.host;
      printf("%s %s: %" PRIu64 " instructions, %u turns, %zu post-boot events, "
                               "%zu node/link bytes.\n",
             i ? "single" : "multi", path, instructions(s) - base[i],
             s->live->turns, s->event_count - events[i],
             h->node_count * sizeof(RoutedNode) +
                 h->route_count * sizeof(RoutedLink));
    }
    finish(&p);
  }
  for (unsigned early = 0; early < 2; early++) {
    Pair p = pair();
    script(&p, early ? "24" : "4");
    uint64_t ins[] = {instructions(p.multi), instructions(p.single)};
    unsigned turns[] = {p.multi->live->turns, p.single->live->turns};
    size_t events[] = {p.multi->event_count, p.single->event_count};
    script(&p, "q");
    for (unsigned i = 0; i < 2; i++) {
      MysterySession *s = i ? p.single : p.multi;
      printf("%s %s Q only: %" PRIu64 " instructions, %u turns, %zu events.\n",
             i ? "single" : "multi", early ? "early" : "late",
             instructions(s) - ins[i], s->live->turns - turns[i],
             s->event_count - events[i]);
    }
    if (!early)
      for (size_t i = events[0]; i < p.multi->event_count; i++) {
        const RoutedEvent *e = p.multi->events + i;
        if (e->kind == CONSTELLATION_TRACE_DELIVER && e->message.length == 2)
          printf("incident delivery %" PRIu64 ": %u->%u %02x %02x\n",
                 e->sequence, e->from, e->to, e->message.data[0],
                 e->message.data[1]);
      }
    finish(&p);
  }
}
int main(int argc, char **argv) {
  if (argc == 2 && !strcmp(argv[1], "--revision-receipts")) {
    for (unsigned i = 0; i < 2; i++) {
      MysterySession *s =
          fresh(i ? MYSTERY_PROGRESSION_SINGLE : MYSTERY_PROGRESSION_MULTI);
      const uint8_t d[] = {1, 4, 0}, q[] = {1, 10, 0};
      CHECK(mystery_input(s, d));
      CHECK(mystery_input(s, q));
      printf("%u\n", s->live->runner.host.nodes[0].uxn.ram[13]);
      dispose(s);
    }
    return 0;
  }
  behavior();
  boundaries();
  faults();
  measure();
  printf("%u assertions, %u player-visible checkpoints, %u paired scenarios "
         "passed.\n",
         checks, checkpoints, scenarios);
}
