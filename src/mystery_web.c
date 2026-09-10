#include "mystery.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif
static MysterySession session;
static MysteryMachine *visible(void) {
  return session.replaying ? session.play : session.live;
}
EXPORT int mystery_reset(void) {
  mystery_free(&session);
  return mystery_init(&session);
}
EXPORT int mystery_click(unsigned x, unsigned y) {
  if (x >= 256 || y >= 192)
    return 0;
  uint8_t in[] = {0, (uint8_t)x, (uint8_t)y};
  return mystery_input(&session, in);
}
EXPORT int mystery_action(unsigned action) {
  if (action < 1 || action > 10)
    return 0;
  uint8_t in[] = {1, (uint8_t)action, 0};
  return mystery_input(&session, in);
}
EXPORT const uint32_t *mystery_pixels(void) {
  return visible() ? visible()->pixels : NULL;
}
EXPORT const char *mystery_text(void) {
  return visible() ? visible()->text : "";
}
EXPORT const char *mystery_error(void) { return session.error; }
EXPORT int mystery_failed(void) { return session.failed; }
EXPORT int mystery_sealed(void) { return session.sealed; }
EXPORT int mystery_play(void) { return mystery_replay_begin(&session); }
EXPORT int mystery_tick(void) { return mystery_replay_step(&session); }
EXPORT int mystery_playing(void) { return session.replaying; }
