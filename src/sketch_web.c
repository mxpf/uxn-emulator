#include "sketchpad.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif
static Sketchpad sketch;
EXPORT int sketch_reset(void) { sketch_free(&sketch); return sketch_open(&sketch); }
EXPORT int sketch_action(unsigned action) { return sketch_apply(&sketch, action); }
EXPORT unsigned sketch_mode(void) { return sketch.mode; }
EXPORT const uint32_t *sketch_pixels(void) { return sketch.pixels; }
EXPORT const char *sketch_error(void) { return sketch.error; }
