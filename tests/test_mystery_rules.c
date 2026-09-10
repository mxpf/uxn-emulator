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
typedef struct {
  unsigned facts, choices[3], question, message;
  bool key, ended;
} Model;
static unsigned gate, outcome;
static MysterySession *fresh(void) {
  MysterySession *s = calloc(1, sizeof(*s));
  CHECK(s && mystery_init(s));
  CHECK(s->live->runner.host.node_count == 1);
  CHECK(s->live->frames == 1);
  return s;
}
static void dispose(MysterySession *s) {
  mystery_free(s);
  free(s);
}
static void step(MysterySession *s, Model *m, unsigned action) {
  static const unsigned masks[] = {1, 2, 4, 24, 32};
  if (!m->ended) {
    if (action == 10) {
      if (m->question == 1) {
        m->question = 2;
        m->message = 11;
      }
    } else if (action <= 5) {
      m->facts |= masks[action - 1];
      m->message = action;
      if (action == 3)
        m->key = true;
      if (!m->question && (m->facts & gate) == gate)
        m->question = 1;
    } else if (action <= 8) {
      unsigned *v = &m->choices[action - 6];
      *v = *v % 3 + 1;
      m->message = 6;
    } else if (m->facts != 63)
      m->message = 7;
    else if (!m->choices[0] || !m->choices[1] || !m->choices[2])
      m->message = 8;
    else if (m->choices[0] != 3 || m->choices[1] != 1 ||
             m->choices[2] != (m->question == 2 ? outcome : 1))
      m->message = 9;
    else {
      m->ended = true;
      m->message = (m->question == 2 && outcome == 3) ? 12 : 10;
    }
  }
  uint8_t data[] = {1, (uint8_t)action, 0};
  CHECK(mystery_input(s, data));
  const Uxn *u = &s->live->runner.host.nodes[0].uxn;
  CHECK(u->ram[0] == m->message && u->ram[1] == m->facts);
  CHECK(u->ram[5] == m->ended && u->ram[6] == m->question &&
        u->ram[7] == m->key);
  for (unsigned i = 0; i < 3; i++)
    CHECK(u->ram[i + 2] == m->choices[i]);
  CHECK(!u->working.pointer && !u->return_stack.pointer);
  if (m->message == 11)
    CHECK(strstr(s->live->text, outcome == 3 ? "THE MISSING PAGE IS THERE"
                                             : "I KEPT THE LEDGER PAGE"));
}
static void replay(MysterySession *s) {
  CHECK(mystery_replay_begin(s));
  while (s->replaying)
    CHECK(mystery_replay_step(s));
  CHECK(s->replay_done && mystery_same(s->live, s->play));
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
  step(s, &m, 10);
  for (unsigned i = 0; i < 5; i++) {
    step(s, &m, order[i]);
    step(s, &m, 10);
    unsigned frames = s->live->frames;
    step(s, &m, 10);
    CHECK(frames == s->live->frames);
    replay(s); // Resume across locked, available/answered and partial evidence.
  }
  CHECK(m.question == 2);
  step(s, &m, 6);
  step(s, &m, 6);
  step(s, &m, 6);
  step(s, &m, 7);
  for (unsigned n = 0; n < outcome; n++)
    step(s, &m, 8);
  step(s, &m, 9);
  CHECK(m.ended);
  for (unsigned a = 1; a <= 10; a++)
    step(s, &m, a);
  replay(s);
  dispose(s);
}
int main(int argc, char **argv) {
  CHECK(argc == 2);
  if (!strcmp(argv[1], "reject")) {
    MysterySession *s = calloc(1, sizeof(*s));
    CHECK(s);
    CHECK(!mystery_init(s));
    CHECK(s->failed);
    CHECK(!strcmp(s->error, "Mystery cartridge rule data is invalid."));
    CHECK(!s->live || s->live->frames == 0);
    dispose(s);
    puts("Malformed packed interaction rejected before presentation.");
    return 0;
  }
  CHECK(!strcmp(argv[1], "returned") || !strcmp(argv[1], "retained"));
  gate = !strcmp(argv[1], "returned") ? 24 : 56;
  outcome = !strcmp(argv[1], "returned") ? 3 : 1;
  unsigned wins = 0;
  for (unsigned ask = 0; ask < 2; ask++)
    for (unsigned subset = 0; subset < 32; subset++)
      for (unsigned choices = 0; choices < 64; choices++) {
        MysterySession *s = fresh();
        Model m = {0};
        step(s, &m, 10);
        for (unsigned a = 1; a <= 5; a++)
          if (subset & (1u << (a - 1)))
            step(s, &m, a);
        if (ask) {
          step(s, &m, 10);
          step(s, &m, 10);
        }
        unsigned code = choices;
        for (unsigned f = 0; f < 3; f++, code >>= 2)
          for (unsigned n = 0; n < (code & 3); n++)
            step(s, &m, f + 6);
        step(s, &m, 9);
        wins += m.ended;
        replay(s);
        dispose(s);
      }
  CHECK(wins == 2);
  unsigned order[5];
  orders(order, 0, 0);
  printf("%s: %u checks; 4096 branches, 120 discovery orders and exact replay "
         "passed.\n",
         argv[1], checks);
  return 0;
}
