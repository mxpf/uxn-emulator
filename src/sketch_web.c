#include "sketchpad.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif
static Sketchpad sketch;
static uint8_t document[SKETCH_FILE_SIZE];
EXPORT uint8_t *sketch_document(void) { return document; }
EXPORT unsigned sketch_document_size(void) { return SKETCH_FILE_SIZE; }
EXPORT int sketch_export(void) { return sketch_save(&sketch, document, sizeof(document)); }
EXPORT int sketch_import(unsigned length) { return sketch_load(&sketch, document, length); }
EXPORT int sketch_reset(void) { sketch_free(&sketch); return sketch_open(&sketch); }
EXPORT int sketch_action(unsigned action) { return sketch_apply(&sketch, action); }
EXPORT unsigned sketch_mode(void) { return sketch.mode; }
EXPORT const uint32_t *sketch_pixels(void) { return sketch.pixels; }
EXPORT const char *sketch_error(void) { return sketch.error; }
