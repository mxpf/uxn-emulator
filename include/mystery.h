#ifndef MYSTERY_H
#define MYSTERY_H
#include "constellation_runner.h"
enum {
  MYSTERY_WIDTH = 256,
  MYSTERY_HEIGHT = 192,
  MYSTERY_INPUTS = 128,
  MYSTERY_EVENTS = 8192
};
typedef struct {
  ConstellationRunner runner;
  uint32_t pixels[MYSTERY_WIDTH * MYSTERY_HEIGHT],
      draft[MYSTERY_WIDTH * MYSTERY_HEIGHT];
  char text[1024], draft_text[1024];
  unsigned frames, turns;
  bool failed;
} MysteryMachine;
typedef enum { MYSTERY_COOPERATING, MYSTERY_SINGLE } MysteryVariant;
#ifndef MYSTERY_DEFAULT_SINGLE
#define MYSTERY_DEFAULT_SINGLE 0
#endif
typedef struct {
  MysteryVariant variant;
  MysteryMachine *live, *play;
  uint8_t inputs[MYSTERY_INPUTS][3];
  RoutedEvent events[MYSTERY_EVENTS];
  size_t count, event_count, play_input, play_event;
  bool failed, sealed, replaying, replay_done;
  char error[160];
} MysterySession;
/* Only zero/freed session storage. ROMs and display are cartridge-local. */
bool mystery_init(MysterySession *s);
bool mystery_init_variant(MysterySession *s, MysteryVariant variant);
void mystery_free(MysterySession *s);
bool mystery_input(MysterySession *s, const uint8_t input[3]);
bool mystery_replay_begin(MysterySession *s);
bool mystery_replay_step(MysterySession *s);
bool mystery_same(const MysteryMachine *a, const MysteryMachine *b);
#endif
