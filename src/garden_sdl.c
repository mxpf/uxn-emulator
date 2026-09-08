#include "garden.h"
#include <SDL.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/* Five-by-seven lettering, drawn locally so the host needs only SDL2. */
static const uint8_t letters[][7] = {
	{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
	{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
	{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
	{7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
	{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
	{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
	{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
	{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
	{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
	{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
	{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
	{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
	{14,17,17,15,1,1,14}
};

static void
color(SDL_Renderer *r, uint32_t rgb)
{
	SDL_SetRenderDrawColor(r, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255);
}

static void
text(SDL_Renderer *r, int x, int y, int scale, uint32_t rgb, const char *s)
{
	color(r, rgb);
	for(; *s; s++, x += 6 * scale) {
		uint8_t extra[7] = {0};
		const uint8_t *glyph = extra;
		if(*s >= 'A' && *s <= 'Z') glyph = letters[*s - 'A'];
		else if(*s >= '0' && *s <= '9') glyph = letters[26 + *s - '0'];
		else if(*s == '>') { extra[1] = 8; extra[2] = 4; extra[3] = 2; extra[4] = 4; extra[5] = 8; }
		else if(*s == '-') extra[3] = 14;
		else if(*s == ':') { extra[2] = 4; extra[4] = 4; }
		else if(*s == '.') extra[6] = 4;
		else if(*s == '/') { extra[1]=1; extra[2]=2; extra[3]=4; extra[4]=8; extra[5]=16; }
		for(int row = 0; row < 7; row++)
			for(int col = 0; col < 5; col++)
				if(glyph[row] & (16 >> col)) {
					SDL_Rect dot = {x + col * scale, y + row * scale, scale, scale};
					SDL_RenderFillRect(r, &dot);
				}
	}
}

static void
draw(SDL_Renderer *r, SDL_Texture *t, const Garden *g, bool paused)
{
	const uint8_t *s = garden_state(g);
	char line[80];
	SDL_Rect map = {32, 120, 512, 384};
	SDL_Rect panel = {572, 32, 356, 492};
	color(r, 0xeee9d8); SDL_RenderClear(r);
	color(r, 0x203f37); SDL_RenderFillRect(r, &panel);
	text(r, 32, 32, 3, 0x203f37, "TINY NEIGHBORS");
	text(r, 32, 70, 2, 0x547263, "A GARDEN IN TWO ROMS");
	SDL_UpdateTexture(t, NULL, g->pixels, GARDEN_WIDTH * sizeof(uint32_t));
	SDL_RenderCopy(r, t, NULL, &map);
	text(r, 32, 526, 2, 0x203f37,
		s[5] ? "A NEW FRIEND. STAY A WHILE." :
		s[4] ? "IT SEES YOU. SPACE TO SAY HELLO." : "FIND THE LITTLE GOLDEN CREATURE.");
	text(r, 32, 562, 1, 0x547263, "ARROWS / WASD MOVE   SPACE GREET   P PAUSE   N STEP   ESC QUIT");
	text(r, 596, 52, 2, 0xf0ce83, "TWO LITTLE MACHINES");
	text(r, 596, 90, 1, 0xd6e2c8, "VIEW   DRAWING AND INPUT");
	text(r, 596, 108, 1, 0xd6e2c8, "WORLD  TERRAIN AND BEHAVIOR");
	snprintf(line, sizeof(line), "TICK %llu  %s",
		(unsigned long long)g->tick, paused ? "PAUSED" : "LIVE");
	text(r, 596, 146, 2, 0xeee9d8, line);
	text(r, 596, 184, 1, 0x98b79b, "MESSAGES - NEWEST AT BOTTOM");
	unsigned first = g->history_count > 14 ? g->history_count - 14 : 0;
	for(unsigned i = first; i < g->history_count; i++)
		text(r, 596, 210 + (int)(i - first) * 20, 1,
			strncmp(g->history[i], "TICK", 4) == 0 ? 0x98b79b : 0xeee9d8,
			g->history[i]);
	if(g->failed) text(r, 32, 100, 1, 0xa13f34, "EXECUTION STOPPED - SEE TERMINAL");
}

static uint8_t
key_action(SDL_Keycode key)
{
	switch(key) {
	case SDLK_UP: case SDLK_w: return 1;
	case SDLK_RIGHT: case SDLK_d: return 2;
	case SDLK_DOWN: case SDLK_s: return 3;
	case SDLK_LEFT: case SDLK_a: return 4;
	case SDLK_SPACE: return 5;
	default: return 0;
	}
}

static uint8_t
script_action(char ch)
{
	switch(ch) {
	case '.': return 0;
	case 'N': return 1;
	case 'E': return 2;
	case 'S': return 3;
	case 'W': return 4;
	case 'G': return 5;
	default: return 255;
	}
}

static bool
save_screen(SDL_Renderer *r, const char *path)
{
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 960, 600, 32, SDL_PIXELFORMAT_ARGB8888);
	bool ok = s && SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0
		&& SDL_SaveBMP(s, path) == 0;
	SDL_FreeSurface(s);
	return ok;
}

int
main(int argc, char **argv)
{
	const char *script = NULL, *screenshot = NULL, *record_path = NULL, *replay_path = NULL;
	FILE *record = NULL, *replay = NULL;
	Garden *g = NULL;
	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	SDL_Texture *texture = NULL;
	bool quit = false, paused = false, batch, ok = false, initialized = false;
	uint8_t pending = 0;
	uint32_t previous;
	size_t cursor = 0;
	int result = 1;
	for(int i = 1; i < argc; i++) {
		if(i + 1 < argc && strcmp(argv[i], "--script") == 0) script = argv[++i];
		else if(i + 1 < argc && strcmp(argv[i], "--screenshot") == 0) screenshot = argv[++i];
		else if(i + 1 < argc && strcmp(argv[i], "--record") == 0) record_path = argv[++i];
		else if(i + 1 < argc && strcmp(argv[i], "--replay") == 0) replay_path = argv[++i];
		else {
			fprintf(stderr, "usage: garden [--script NESW.G] [--record file] [--replay file] [--screenshot file.bmp]\n");
			return 1;
		}
	}
	if((script && replay_path) || (record_path && replay_path)) {
		fprintf(stderr, "choose script or replay; recording replay is not supported\n");
		return 1;
	}
	batch = script || replay_path;
	g = calloc(1, sizeof(*g));
	if(!g || !garden_init(g, "build/garden-view.rom", "build/garden-world.rom")) goto done;
	if(record_path) {
		record = fopen(record_path, "wx");
		if(!record || fprintf(record, "GARDEN1 %016" PRIx64 "\n", g->trace_hash) < 0) goto done;
	}
	if(replay_path) {
		uint64_t expected;
		char header[80], extra;
		replay = fopen(replay_path, "r");
		if(!replay || !fgets(header, sizeof(header), replay) ||
			sscanf(header, "GARDEN1 %" SCNx64 " %c", &expected, &extra) != 1 || expected != g->trace_hash) {
			fprintf(stderr, "replay header or initial state mismatch\n");
			goto done;
		}
	}
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) goto done;
	initialized = true;
	window = SDL_CreateWindow("Tiny Neighbors - Constellation", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, 960, 600, batch ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
	if(!window) goto done;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if(!renderer) goto done;
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING, GARDEN_WIDTH, GARDEN_HEIGHT);
	if(!texture) goto done;
	previous = SDL_GetTicks();
	while(!quit) {
		SDL_Event event;
		bool step = false;
		while(SDL_PollEvent(&event)) {
			if(event.type == SDL_QUIT) quit = true;
			if(event.type == SDL_KEYDOWN && !event.key.repeat) {
				SDL_Keycode key = event.key.keysym.sym;
				if(key == SDLK_ESCAPE) quit = true;
				else if(key == SDLK_p) paused = !paused;
				else if(key == SDLK_n && paused) step = true;
				else if(key_action(key)) pending = key_action(key);
			}
		}
		if(quit) break;
		if(batch) {
			uint64_t expected = 0;
			if(script) {
				if(!script[cursor]) break;
				pending = script_action(script[cursor++]);
			} else {
				unsigned action;
				char line[80], extra;
				if(!fgets(line, sizeof(line), replay)) {
					if(ferror(replay)) goto done;
					break;
				}
				if(sscanf(line, "%u %" SCNx64 " %c", &action, &expected, &extra) != 2 || action > 5) {
					fprintf(stderr, "invalid replay event\n"); goto done;
				}
				pending = (uint8_t)action;
			}
			if(!garden_step(g, pending)) goto done;
			if(replay && expected != g->trace_hash) {
				fprintf(stderr, "replay diverged at tick %" PRIu64 "\n", g->tick); goto done;
			}
			step = true;
		} else if(step || (!paused && SDL_GetTicks() - previous >= 250)) {
			if(!pending) {
				const uint8_t *keys = SDL_GetKeyboardState(NULL);
				if(keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) pending = 1;
				else if(keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) pending = 2;
				else if(keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) pending = 3;
				else if(keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) pending = 4;
			}
			if(!garden_step(g, pending)) goto done;
			previous = SDL_GetTicks();
			step = true;
		}
		if(step && record && (fprintf(record, "%u %016" PRIx64 "\n", pending, g->trace_hash) < 0 ||
			fflush(record) != 0)) goto done;
		pending = step ? 0 : pending;
		draw(renderer, texture, g, paused);
		SDL_RenderPresent(renderer);
		if(!batch) SDL_Delay(16);
	}
	draw(renderer, texture, g, paused);
	if(screenshot && !save_screen(renderer, screenshot)) goto done;
	printf("tick=%" PRIu64 " player=%u,%u creature=%u,%u friend=%u trace=%016" PRIx64 "\n",
		g->tick, garden_state(g)[0], garden_state(g)[1], garden_state(g)[2],
		garden_state(g)[3], garden_state(g)[5], g->trace_hash);
	ok = true;
done:
	if(!ok) fprintf(stderr, "garden: %s (%s)\n", g && g->failed ? g->error : "could not complete",
		initialized ? SDL_GetError() : "check ROM and file paths");
	if(record && fclose(record) != 0) ok = false;
	if(replay) fclose(replay);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	if(initialized) SDL_Quit();
	free(g);
	result = ok ? 0 : 1;
	return result;
}
