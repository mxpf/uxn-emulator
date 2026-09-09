#include "square_demo.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t
action(SDL_Keycode key)
{
	switch(key) {
	case SDLK_UP: return 1;
	case SDLK_RIGHT: return 2;
	case SDLK_DOWN: return 3;
	case SDLK_LEFT: return 4;
	default: return 0;
	}
}

static bool
handle_event(SquareSession *s, const SDL_Event *event, bool *quit)
{
	if(event->type == SDL_QUIT) *quit = true;
	if(event->type != SDL_KEYDOWN) return true;
	SDL_Keycode key = event->key.keysym.sym;
	if(key == SDLK_ESCAPE) *quit = true;
	else if(key == SDLK_r && !event->key.repeat) return square_replay_begin(s);
	else if(key == SDLK_n && !event->key.repeat) { square_free(s); return square_init(s); }
	else if(action(key) && !s->sealed) return square_input(s, action(key));
	return true;
}

static bool
draw(SDL_Renderer *renderer, SDL_Texture *texture, const SquareSession *s)
{
	const SquareMachine *m = s->play ? s->play : s->live;
	SDL_Rect board = {64, 48, 512, 384};
	return SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255) == 0 && SDL_RenderClear(renderer) == 0 &&
		SDL_UpdateTexture(texture, NULL, m->pixels, SQUARE_WIDTH * sizeof(uint32_t)) == 0 &&
		SDL_RenderCopy(renderer, texture, NULL, &board) == 0;
}

static bool
screenshot(SDL_Renderer *renderer, const char *path)
{
	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_ARGB8888);
	bool ok = surface && SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) == 0 && SDL_SaveBMP(surface, path) == 0;
	SDL_FreeSurface(surface); return ok;
}

int
main(int argc, char **argv)
{
	const char *script = NULL, *image_path = NULL; bool verify = false, batch, quit = false, ok = false;
	for(int i = 1; i < argc; i++) {
		if(!strcmp(argv[i], "--script") && i + 1 < argc) script = argv[++i];
		else if(!strcmp(argv[i], "--screenshot") && i + 1 < argc) image_path = argv[++i];
		else if(!strcmp(argv[i], "--verify-replay")) verify = true;
		else { fprintf(stderr, "usage: square [--script NESW.] [--verify-replay] [--screenshot image.bmp]\n"); return 1; }
	}
	if(verify && !script) { fprintf(stderr, "--verify-replay needs --script; use R in the live window.\n"); return 1; }
	batch = script != NULL;
	SquareSession *s = calloc(1, sizeof(*s));
	SDL_Window *window = NULL; SDL_Renderer *renderer = NULL; SDL_Texture *texture = NULL;
	if(!s || !square_init(s)) goto done;
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) goto done;
	window = SDL_CreateWindow("Square | Arrows move | R replay | N new | Esc quit", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		640, 480, batch ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
	if(!window) goto done;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if(!renderer || SDL_RenderSetLogicalSize(renderer, 640, 480) != 0) goto done;
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SQUARE_WIDTH, SQUARE_HEIGHT);
	if(!texture) goto done;
	if(batch) {
		for(size_t i = 0; script[i]; i++) {
			SDL_Keycode key;
			switch(script[i]) {
			case 'N': key = SDLK_UP; break; case 'E': key = SDLK_RIGHT; break;
			case 'S': key = SDLK_DOWN; break; case 'W': key = SDLK_LEFT; break;
			case '.': key = 0; break;
			default: snprintf(s->error, sizeof(s->error), "Invalid script character."); goto done;
			}
			if(s->sealed) { snprintf(s->error, sizeof(s->error), "Script exceeds recording bounds."); goto done; }
			if(key && !square_input(s, action(key))) goto done;
			for(unsigned turn = 0; turn < 3; turn++) {
				if(!square_step(s)) goto done;
				if(s->sealed) { snprintf(s->error, sizeof(s->error), "Script exceeds recording bounds."); goto done; }
			}
		}
		if(verify) {
			if(!square_replay_begin(s)) goto done;
			while(!s->replay_done) if(!square_replay_step(s)) goto done;
		}
		if(!draw(renderer, texture, s)) goto done;
		if(image_path && !screenshot(renderer, image_path)) goto done;
		SDL_RenderPresent(renderer);
	} else {
		uint32_t previous = SDL_GetTicks();
		while(!quit) {
			SDL_Event event;
			while(SDL_PollEvent(&event)) {
				if(!handle_event(s, &event, &quit)) goto done;
			}
			if(quit) break;
			uint32_t now = SDL_GetTicks();
			if(now - previous >= 16) {
				previous = now;
				for(unsigned turn = 0; turn < 3; turn++) {
					if(s->play) { if(!square_replay_step(s)) goto done; }
					else if(!square_step(s)) goto done;
				}
			}
			char title[200];
			snprintf(title, sizeof(title), "Square | %s | Arrows move | R replay | N new | rejected inputs: %u",
				s->replay_done ? "REPLAY VERIFIED" : s->play ? "REPLAYING" : s->sealed ? "RECORDING LIMIT" : "RECORDING", s->rejected);
			SDL_SetWindowTitle(window, title);
			if(!draw(renderer, texture, s)) goto done;
			SDL_RenderPresent(renderer); SDL_Delay(5);
		}
		if(image_path && (!draw(renderer, texture, s) || !screenshot(renderer, image_path))) goto done;
	}
	printf("Square: turn=%u inputs=%zu rejected=%u events=%zu position=%u,%u replay=%s\n", s->turn, s->input_count, s->rejected,
		s->event_count, s->live->runner.host.nodes[1].uxn.ram[0], s->live->runner.host.nodes[1].uxn.ram[1], s->replay_done ? "verified" : "not requested");
	ok = true;
done:
	if(!ok) fprintf(stderr, "Square: %s %s\n", s && s->error[0] ? s->error : "Could not start or draw.", SDL_GetError());
	SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
	if(s) square_free(s); free(s); return ok ? 0 : 1;
}
