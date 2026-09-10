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
  unsigned msg, facts, who, how, answer, exit, q, key, known, compared;
} Model;
typedef struct {
  MysterySession *multi, *single;
  Model model;
} Pair;
static MysterySession *fresh(MysteryVariant v) {
  MysterySession *s = calloc(1, sizeof(*s));
  CHECK(s && mystery_init_variant(s, v));
  return s;
}
static unsigned known(MysterySession *s) {
  return s->live->runner.host
      .nodes[s->variant == MYSTERY_KNOWLEDGE_MULTI ? 1 : 0]
      .uxn.ram[9];
}
static void output(const MysteryMachine *a, const MysteryMachine *b) {
  CHECK(a->frames == b->frames);
  CHECK(!memcmp(a->pixels, b->pixels, sizeof(a->pixels)));
  CHECK(!strcmp(a->text, b->text));
  CHECK(!memcmp(a->runner.host.nodes[0].uxn.ram,
                b->runner.host.nodes[0].uxn.ram, 8));
  CHECK(a->runner.host.nodes[0].uxn.ram[10] ==
        b->runner.host.nodes[0].uxn.ram[10]);
  checkpoints++;
}
static Pair pair(void) {
  Pair p = {
      fresh(MYSTERY_KNOWLEDGE_MULTI), fresh(MYSTERY_KNOWLEDGE_SINGLE), {0}};
  output(p.multi->live, p.single->live);
  scenarios++;
  return p;
}
/* Independent behavioral oracle, deliberately not generated from TAL or JSON.
 */
static void model_action(Model *m, unsigned a) {
  static const unsigned masks[] = {0, 1, 2, 4, 24, 32};
  if (m->exit)
    return;
  if (a == 10) {
    if (m->q == 1) {
      m->q = 2;
      m->answer = 0;
      m->compared = 0;
      m->msg = 11;
    } else if (m->q == 2) {
      m->q = 3;
      m->known = 1;
      m->msg = 15;
    }
  } else if (a <= 5) {
    m->facts |= masks[a];
    if (a == 3)
      m->key = 1;
    if (!m->q && (m->facts & 24) == 24)
      m->q = 1;
    m->msg = a;
    if (m->q >= 2) {
      if (a == 2) {
        m->compared = 1;
        m->msg = 13 + m->known;
      }
      if (a == 3)
        m->msg = 18;
      if (a == 4)
        m->msg = 17;
    }
  } else if (a < 9) {
    unsigned *field = a == 6 ? &m->who : a == 7 ? &m->how : &m->answer;
    *field = *field % 3 + 1;
    m->msg = 6;
  } else {
    m->msg = m->facts != 63                     ? 7
             : m->q >= 2 && !m->compared        ? 16
             : !m->who || !m->how || !m->answer ? 8
             : m->who != 3 || m->how != 1 ||
                     m->answer != (m->q >= 2 ? 3u : 1u) || !m->key
                 ? 9
             : m->q >= 2 ? 12
                         : 10;
    m->exit = m->msg == 10 || m->msg == 12;
  }
}
static void assert_model(Pair *p) {
  Model *m = &p->model;
  unsigned expected[] = {m->msg,    m->facts, m->who, m->how,
                         m->answer, m->exit,  m->q,   m->key};
  for (unsigned j = 0; j < 2; j++) {
    MysterySession *s = j ? p->single : p->multi;
    for (unsigned i = 0; i < 8; i++)
      CHECK(s->live->runner.host.nodes[0].uxn.ram[i] == expected[i]);
    CHECK(known(s) == m->known);
    CHECK(s->live->runner.host.nodes[0].uxn.ram[10] == m->compared);
    CHECK(s->live->runner.host.nodes[0].uxn.ram[11] == 0);
    for (size_t i = 0; i < s->live->runner.host.node_count; i++) {
      Uxn *u = &s->live->runner.host.nodes[i].uxn;
      CHECK(!u->working.pointer && !u->return_stack.pointer && !u->ram[254]);
    }
    if (m->msg == 13)
      CHECK(strstr(s->live->text, "I HAVE HEARD NOTHING SINCE."));
    if (m->msg == 14)
      CHECK(strstr(s->live->text, "BEFORE THAT I THOUGHT CORA HAD IT."));
    if (m->msg == 12)
      CHECK(strstr(s->live->text, "HIS OLD ACCOUNT WAS NOT A NEW LIE."));
    if (m->q >= 2)
      CHECK(strstr(s->live->text, "8 WHY:"));
  }
}
static void action(Pair *p, unsigned a, bool click) {
  static const uint8_t xy[10][2] = {
      {30, 60},  {150, 100}, {100, 105}, {30, 105},  {200, 105},
      {20, 118}, {20, 128},  {20, 138},  {200, 125}, {200, 12}};
  uint8_t data[3] = {1, (uint8_t)a, 0};
  if (click) {
    data[0] = 0;
    data[1] = xy[a - 1][0];
    data[2] = xy[a - 1][1];
  }
  CHECK(mystery_input(p->multi, data));
  CHECK(mystery_input(p->single, data));
  model_action(&p->model, a);
  assert_model(p);
  output(p->multi->live, p->single->live);
}
static void script(Pair *p, const char *s) {
  for (; *s; s++)
    action(p, *s == 'q' ? 10 : (unsigned)(*s - '0'), false);
}
/* Derive witness answers from authenticated deliveries, not from guest RAM.
   The initial observation at six is a documented cartridge initial condition.
 */
static void explain(MysterySession *s) {
  unsigned learned = 0, q = 0;
  bool desk = false;
  for (size_t i = 0; i < s->event_count; i++) {
    const RoutedEvent *e = s->events + i;
    CHECK(e->sequence == i);
    if (e->kind != CONSTELLATION_TRACE_DELIVER)
      continue;
    const uint8_t *d = e->message.data;
    if (e->from == 255 && e->to == 0 && e->message.length == 3) {
      unsigned a = d[0] == 1                   ? d[1]
                   : d[1] == 200 && d[2] == 12 ? 10
                   : d[1] == 30 && d[2] == 105 ? 4
                                               : 0;
      if (a == 4)
        desk = true;
      if (a == 10 && desk && q < 2)
        q++;
      if (s->variant == MYSTERY_KNOWLEDGE_SINGLE && q == 2)
        learned = 1;
    }
    if (e->from == 0 && e->to == 1 && e->message.length == 2 && d[0] == 0x20) {
      CHECK(q == 2 && d[1] == 1);
      learned = 1;
    }
    if (e->from == 1 && e->to == 0) {
      CHECK(e->message.length == 2 && (d[0] == 0x30 || d[0] == 0x31));
      CHECK(d[1] == learned);
    }
  }
  CHECK(learned == known(s));
}
static void finish(Pair *p) {
  for (unsigned i = 0; i < 2; i++) {
    MysterySession *s = i ? p->single : p->multi;
    /* Trace reconstruction is for nonterminal knowledge paths; terminal Q is
       intentionally ignored and is tested separately by the behavioral model.
     */
    if (!p->model.exit)
      explain(s);
    CHECK(mystery_replay_begin(s));
    while (s->replaying)
      CHECK(mystery_replay_step(s));
    CHECK(s->replay_done && mystery_same(s->live, s->play));
  }
  output(p->multi->play, p->single->play);
  mystery_free(p->multi);
  free(p->multi);
  mystery_free(p->single);
  free(p->single);
}
static void behavior(void) {
  const char *paths[] = {"q2q4q22",
                         "4q2q2",
                         "4qq2",
                         "24q342",
                         "4q3434342",
                         "4qqqq22",
                         "12345666789q28",
                         "123458q66678889",
                         "12345q6667888929",
                         "12345qq6667888929",
                         "12345q2q266678889q28",
                         "123458q2666789q288",
                         "123458q2666789q2889"};
  for (unsigned i = 0; i < sizeof(paths) / sizeof(*paths); i++) {
    Pair p = pair();
    script(&p, paths[i]);
    finish(&p);
  }
  /* The small decision matrix: both information states, fresh comparison or
     its absence, and all four WHY values. Old Bram evidence never substitutes.
   */
  for (unsigned learn = 0; learn < 2; learn++)
    for (unsigned compare = 0; compare < 2; compare++)
      for (unsigned why = 0; why < 4; why++) {
        Pair p = pair();
        script(&p, "123458q");
        CHECK(!p.model.answer);
        if (learn)
          script(&p, "q");
        if (compare)
          script(&p, "2");
        script(&p, "6667");
        for (unsigned i = 0; i < why; i++)
          script(&p, "8");
        script(&p, "9");
        CHECK(p.model.exit == (compare && why == 3));
        finish(&p);
      }
  /* Wrong identity, wrong means, absent desk and absent Ada remain meaningful.
   */
  const char *wrong[] = {"12345q2678889", "12345q2666778889", "1235q66678889",
                         "2345q266678889"};
  for (unsigned i = 0; i < 4; i++) {
    Pair p = pair();
    script(&p, wrong[i]);
    CHECK(!p.model.exit);
    finish(&p);
  }
  Pair p = pair();
  const char *clicks = "12345q2q266678889";
  for (const char *s = clicks; *s; s++)
    action(&p, *s == 'q' ? 10 : (unsigned)(*s - '0'), true);
  CHECK(p.model.exit);
  finish(&p);
  p = pair();
  for (unsigned i = 0; i < 128; i++)
    action(&p, i == 0 ? 4 : i % 3 ? 2 : 10, false);
  uint8_t input[] = {1, 2, 0};
  CHECK(!mystery_input(p.multi, input) && !mystery_input(p.single, input));
  CHECK(p.multi->sealed && p.single->sealed);
  finish(&p);
}
static void drain(RoutedHost *h) {
  for (unsigned i = 0; !routed_quiescent(h); i++) {
    CHECK(i < 32);
    CHECK(routed_step(h));
  }
  CHECK(h->fault == ROUTED_OK);
}
static void boundaries(void) {
  Pair p = pair();
  script(&p, "4");
  Uxn *w = &p.multi->live->runner.host.nodes[1].uxn;
  uint8_t *before = malloc(sizeof(w->ram));
  CHECK(before);
  memcpy(before, w->ram, sizeof(w->ram));
  script(&p, "q3434");
  CHECK(!memcmp(before, w->ram, sizeof(w->ram)));
  free(before);
  finish(&p);
  MysterySession *s = fresh(MYSTERY_KNOWLEDGE_MULTI);
  uint8_t desk[] = {1, 4, 0}, q[] = {1, 10, 0};
  CHECK(mystery_input(s, desk));
  CHECK(mystery_input(s, q));
  RoutedHost *h = &s->live->runner.host;
  CHECK(routed_input(h, 7, q, 3) == ROUTED_INPUT_ACCEPTED);
  for (unsigned i = 0; !h->links[1].queue.count; i++) {
    CHECK(i < 3);
    CHECK(routed_step(h));
  }
  CHECK(known(s) == 0 && h->nodes[0].uxn.ram[6] == 2 &&
        h->nodes[0].uxn.ram[11] == 10);
  CHECK(routed_step(h));
  CHECK(known(s) == 1);
  CHECK(h->nodes[0].uxn.ram[6] == 2);
  drain(h);
  CHECK(h->nodes[0].uxn.ram[6] == 3 && !h->nodes[0].uxn.ram[11]);
  mystery_free(s);
  free(s);
  /* Test-only real guest STZ at the belief's address. Main cannot alias the
     witness RAM, whereas single can. This is not a hostile-host guarantee. */
  for (unsigned single = 0; single < 2; single++) {
    s = fresh(single ? MYSTERY_KNOWLEDGE_SINGLE : MYSTERY_KNOWLEDGE_MULTI);
    CHECK(mystery_input(s, desk));
    CHECK(mystery_input(s, q));
    Uxn *u = &s->live->runner.host.nodes[0].uxn;
    const uint8_t store[] = {0x80, 1, 0x80, 9, 0x11, 0};
    memcpy(u->ram + 0xa00, store, sizeof(store));
    CHECK(uxn_eval(u, 0xa00, 100) == UXN_STOP_BREAK);
    CHECK(known(s) == single);
    const uint8_t ask_after_store[] = {1, 2, 0};
    CHECK(mystery_input(s, ask_after_store));
    CHECK(u->ram[0] == 13 + single);
    CHECK(strstr(s->live->text, single ? "YOU SHOWED ME THE RETURN."
                                     : "I HAVE HEARD NOTHING SINCE."));
    mystery_free(s);
    free(s);
  }
  s = fresh(MYSTERY_KNOWLEDGE_MULTI);
  s->live->runner.host.nodes[1].uxn.devices[0xd0] = 0;
  s->live->runner.host.nodes[1].uxn.devices[0xd1] = 0;
  uint8_t ask[] = {1, 2, 0};
  CHECK(!mystery_input(s, ask));
  CHECK(s->live->runner.host.fault == ROUTED_RECEIVE_VECTOR_MISSING);
  mystery_free(s);
  free(s);
  s = fresh(MYSTERY_KNOWLEDGE_MULTI);
  CHECK(mystery_input(s, desk));
  CHECK(mystery_input(s, q));
  CHECK(mystery_input(s, q));
  bool altered = false;
  for (size_t i = 0; i < s->event_count; i++) {
    RoutedEvent *e = s->events + i;
    if (e->kind == CONSTELLATION_TRACE_DELIVER && e->from == 0 &&
        e->to == 1 && e->message.data[0] == 0x20) {
      e->message.data[1] = 0;
      altered = true;
      break;
    }
  }
  CHECK(altered && mystery_replay_begin(s));
  while (s->replaying && !s->failed)
    (void)mystery_replay_step(s);
  CHECK(s->failed && !strcmp(s->error, "Replay trace differs."));
  mystery_free(s);
  free(s);
}
static void protocols(void) {
  /* Extra external route exists only in this test fixture, not in the page. */
  uint8_t bytes[256];
  FILE *f = fopen("build/mystery-knowledge-witness.rom", "rb");
  CHECK(f);
  size_t n = fread(bytes, 1, sizeof(bytes), f);
  CHECK(!ferror(f) && fclose(f) == 0 && n > 0 && n < sizeof(bytes));
  const uint8_t idle[] = {0};
  const RunnerRom roms[] = {{idle, 1}, {bytes, n}};
  const RoutedRoute routes[] = {{0, 1, 1}, {1, 1, 0}, {255, 8, 1}};
  ConstellationRunner *r = calloc(1, sizeof(*r));
  CHECK(r);
  RunnerDefinition def = {roms, 2, routes, 3, NULL, 0, 10000};
  RunnerDiagnostic diagnostic;
  CHECK(runner_start(r, &def, &diagnostic));
  uint8_t spoof[] = {0x20, 1};
  CHECK(routed_input(&r->host, 8, spoof, 2) == ROUTED_INPUT_ACCEPTED);
  drain(&r->host);
  CHECK(r->host.nodes[1].uxn.ram[9] == 0);
  bool delivered = false;
  for (size_t i = 0; i < r->host.trace_count; i++) {
    RoutedEvent *e = r->host.trace + i;
    if (e->kind == CONSTELLATION_TRACE_DELIVER) {
      CHECK(e->from == 255);
      delivered = true;
    }
  }
  CHECK(delivered);
  runner_free(r);
  free(r);
  const uint8_t bad[][3] = {
      {0x20, 2, 0}, {0x20, 1, 0}, {0x21, 1, 0}, {0x10, 1, 0}};
  const unsigned lengths[] = {2, 3, 2, 2};
  for (unsigned i = 0; i < 4; i++) {
    MysterySession *s = fresh(MYSTERY_KNOWLEDGE_MULTI);
    Uxn *u = &s->live->runner.host.nodes[0].uxn;
    memcpy(u->ram + 0x900, bad[i], 3);
    u->devices[0xd8] = (uint8_t)lengths[i];
    const uint8_t send[] = {0x80, 1, 0x80, 0xd9, 0x17, 0};
    memcpy(u->ram + 0xa00, send, sizeof(send));
    CHECK(uxn_eval(u, 0xa00, 100) == UXN_STOP_BREAK);
    drain(&s->live->runner.host);
    CHECK(!known(s));
    mystery_free(s);
    free(s);
  }
}
static uint64_t instructions(MysterySession *s) {
  uint64_t n = 0;
  for (size_t i = 0; i < s->live->runner.host.node_count; i++)
    n += s->live->runner.host.nodes[i].uxn.instructions;
  return n;
}
static void measure(void) {
  Pair p = pair();
  uint64_t ins[] = {instructions(p.multi), instructions(p.single)};
  unsigned turns[] = {p.multi->live->turns, p.single->live->turns};
  size_t events[] = {p.multi->event_count, p.single->event_count};
  script(&p, "12345q2q266678889");
  CHECK(p.model.exit);
  for (unsigned i = 0; i < 2; i++) {
    MysterySession *s = i ? p.single : p.multi;
    RoutedHost *h = &s->live->runner.host;
    printf("%s, script 12345q2q266678889: %" PRIu64
           " instructions, %u turns, %zu events, %zu node/link bytes "
           "(post-boot).\n",
           i ? "single" : "multi", instructions(s) - ins[i],
           s->live->turns - turns[i], s->event_count - events[i],
           h->node_count * sizeof(RoutedNode) +
               h->route_count * sizeof(RoutedLink));
  }
  for (size_t i = 0; i < p.multi->event_count; i++) {
    const RoutedEvent *e = p.multi->events + i;
    if ((e->kind == CONSTELLATION_TRACE_SEND ||
         e->kind == CONSTELLATION_TRACE_DELIVER) &&
        e->message.length == 2)
      printf("trace %" PRIu64 " %s %u->%u %02x %02x\n", e->sequence,
             e->kind == CONSTELLATION_TRACE_SEND ? "SEND" : "DELIVER",
             e->from, e->to, e->message.data[0], e->message.data[1]);
  }
  finish(&p);
}
int main(void) {
  behavior();
  boundaries();
  protocols();
  measure();
  printf("%u assertions, %u exact cross-control checkpoints, %u scenarios; "
         "model, replay, privacy, provenance and delivery checks passed.\n",
         checks, checkpoints, scenarios);
}
