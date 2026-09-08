/* Thin host boundary: no game rules or alternate VM implementation. */
#include "garden.h"
#include <inttypes.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

static Garden garden;

EXPORT int garden_web_reset(void)
{
	return garden_init(&garden, "build/garden-view.rom", "build/garden-world.rom");
}

EXPORT int garden_web_step(int action)
{
	/* Validate before narrowing to the guest's byte-sized action. */
	return garden_step(&garden, action >= 0 && action <= 5 ? (uint8_t)action : 255);
}

EXPORT const uint32_t *garden_web_pixels(void) { return garden.pixels; }
EXPORT const uint8_t *garden_web_state(void) { return garden_state(&garden); }
EXPORT const char *garden_web_error(void) { return garden.error; }
EXPORT unsigned garden_web_history_count(void) { return garden.history_count; }
EXPORT const char *garden_web_history(unsigned index)
{
	return index < garden.history_count ? garden.history[index] : "";
}

/* Canonical numeric pixels, independent of native byte order. Diagnostic only. */
EXPORT const char *garden_web_digest(void)
{
	static char digest[96];
	uint64_t pixels = UINT64_C(14695981039346656037);
	for(unsigned i = 0; i < GARDEN_WIDTH * GARDEN_HEIGHT; i++)
		for(unsigned shift = 0; shift < 32; shift += 8)
			pixels = (pixels ^ ((garden.pixels[i] >> shift) & 255)) * UINT64_C(1099511628211);

	snprintf(digest, sizeof(digest), "%" PRIu64 " %016" PRIx64 " %016" PRIx64,
		garden.tick, garden.trace_hash, pixels);
	return digest;
}
