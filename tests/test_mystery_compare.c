#include "mystery.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checkpoints;
#define REQUIRE(c)                                                             \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static MysterySession *new_session(MysteryVariant variant) {
  MysterySession *s = calloc(1, sizeof(*s));
  REQUIRE(s && mystery_init_variant(s, variant));
  REQUIRE(s->variant == variant);
  REQUIRE(s->live->runner.host.node_count == (variant == MYSTERY_SINGLE ? 1u : 3u));
  REQUIRE(s->live->runner.host.route_count == (variant == MYSTERY_SINGLE ? 1u : 5u));
  REQUIRE(!mystery_init_variant(s, variant));
  return s;
}
static void same_output(const MysteryMachine *a, const MysteryMachine *b) {
  REQUIRE(a->frames == b->frames);
  REQUIRE(!memcmp(a->pixels, b->pixels, sizeof(a->pixels)));
  REQUIRE(!strcmp(a->text, b->text));
  REQUIRE(!memcmp(a->runner.host.nodes[0].uxn.ram,
                  b->runner.host.nodes[0].uxn.ram, 7));
  REQUIRE(a->runner.host.nodes[1].uxn.ram[0] ==
          b->runner.host.nodes[0].uxn.ram[7]);
  REQUIRE(!a->failed && !b->failed);
  checkpoints++;
}
static void input(MysterySession *a, MysterySession *b, const uint8_t data[3]) {
  REQUIRE(mystery_input(a, data) == mystery_input(b, data));
  REQUIRE(a->count == b->count && a->sealed == b->sealed &&
          a->failed == b->failed);
  same_output(a->live, b->live);
}
static void key(MysterySession *a, MysterySession *b, unsigned action) {
  const uint8_t data[] = {1, (uint8_t)action, 0};
  input(a, b, data);
}
static void exact_replays(MysterySession *a, MysterySession *b) {
  REQUIRE(mystery_replay_begin(a) && mystery_replay_begin(b));
  same_output(a->play, b->play);
  while (a->replaying || b->replaying) {
    REQUIRE(a->replaying == b->replaying);
    REQUIRE(mystery_replay_step(a) && mystery_replay_step(b));
    same_output(a->play, b->play);
  }
  REQUIRE(mystery_same(a->live, a->play) && mystery_same(b->live, b->play));
}
static void dispose(MysterySession *s) {
  mystery_free(s);
  free(s);
}
static uint64_t instructions(const MysterySession *s) {
  uint64_t n = 0;
  for (size_t i = 0; i < s->live->runner.host.node_count; i++)
    n += s->live->runner.host.nodes[i].uxn.instructions;
  return n;
}
int main(void) {
  MysterySession *invalid = calloc(1, sizeof(*invalid));
  REQUIRE(invalid && !mystery_init_variant(invalid, (MysteryVariant)99));
  REQUIRE(!invalid->live && !invalid->play);
  dispose(invalid);
  // All evidence subsets and answer combinations on both branches.
  for (unsigned followup = 0; followup < 2; followup++)
    for (unsigned subset = 0; subset < 32; subset++)
      for (unsigned combination = 0; combination < 64; combination++) {
        MysterySession *a = new_session(MYSTERY_COOPERATING),
                       *b = new_session(MYSTERY_SINGLE);
        same_output(a->live, b->live);
        key(a, b, 10); // Locked question.
        for (unsigned i = 1; i <= 5; i++)
          if (subset & (1u << (i - 1)))
            key(a, b, i);
        if (followup) {
          key(a, b, 10);
          key(a, b, 10);
        }
        unsigned code = combination;
        for (unsigned field = 0; field < 3; field++, code >>= 2)
          for (unsigned n = 0; n < (code & 3); n++)
            key(a, b, field + 6);
        key(a, b, 9);
        // Each architecture compares its own complete RAM/trace during replay;
        // cross-architecture comparison checks domain state and output, not RAM
        // layout.
        exact_replays(a, b);
        dispose(a);
        dispose(b);
      }
  for (unsigned trial = 0; trial < 16; trial++) {
    MysterySession *a = new_session(MYSTERY_COOPERATING),
                   *b = new_session(MYSTERY_SINGLE);
    unsigned seed = trial + 17;
    for (unsigned i = 0; i < MYSTERY_INPUTS; i++) {
      seed = seed * 1664525u + 1013904223u;
      if (i % 3)
        key(a, b, (seed >> 16) % 10 + 1);
      else {
        uint8_t data[] = {0, (uint8_t)(seed >> 16),
                          (uint8_t)((seed >> 24) % 192)};
        input(a, b, data);
      }
    }
    key(a, b, 4); // Both recordings seal at the same input bound.
    exact_replays(a, b);
    dispose(a);
    dispose(b);
  }
  MysterySession *a = new_session(MYSTERY_COOPERATING),
                 *b = new_session(MYSTERY_SINGLE);
  const char *script = "q12345qq666789889q";
  uint64_t ia = instructions(a), ib = instructions(b);
  unsigned ta = a->live->turns, tb = b->live->turns;
  size_t ea = a->event_count, eb = b->event_count;
  for (const char *p = script; *p; p++)
    key(a, b, *p == 'q' ? 10 : (unsigned)(*p - '0'));
  printf("Scenario %s, post-boot:\n", script);
  printf("Three ROMs: %" PRIu64
         " instructions, %u turns, %zu events, %zu node/link bytes.\n",
         instructions(a) - ia, a->live->turns - ta, a->event_count - ea,
         3 * sizeof(RoutedNode) + 5 * sizeof(RoutedLink));
  printf("One ROM: %" PRIu64
         " instructions, %u turns, %zu events, %zu node/link bytes.\n",
         instructions(b) - ib, b->live->turns - tb, b->event_count - eb,
         sizeof(RoutedNode) + sizeof(RoutedLink));
  exact_replays(a, b);
  dispose(a);
  dispose(b);
  printf("%u exact cross-architecture state/text/pixel checkpoints passed.\n",
         checkpoints);
  return 0;
}
