#include "sketchpad.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned action(SDL_Keycode key)
{
	switch(key) {
	case SDLK_UP: return 1; case SDLK_RIGHT: return 2; case SDLK_DOWN: return 3;
	case SDLK_LEFT: return 4; case SDLK_SPACE: return 5; case SDLK_x: return 6;
	default: return 0;
	}
}
int main(int argc, char **argv)
{
	const char *script = NULL;
	if(argc == 3 && !strcmp(argv[1], "--script")) script = argv[2];
	else if(argc != 1) { fprintf(stderr, "usage: sketchpad [--script 1..6]\n"); return 1; }
	Sketchpad *s = calloc(1, sizeof(*s)); bool ok = false, quit = false;
	SDL_Window *window = NULL; SDL_Renderer *renderer = NULL; SDL_Texture *texture = NULL;
	if(!s || !sketch_open(s) || SDL_Init(SDL_INIT_VIDEO) != 0) goto done;
	window = SDL_CreateWindow("Sketchpad | Space toggles draw | X toggles erase | Esc quit",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 768, 576,
		script ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if(!window) goto done;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if(!renderer || SDL_RenderSetLogicalSize(renderer, SKETCH_WIDTH, SKETCH_HEIGHT) != 0) goto done;
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SKETCH_WIDTH, SKETCH_HEIGHT);
	if(!texture) goto done;
	if(script) for(size_t i = 0; script[i]; i++) {
		if(script[i] < '1' || script[i] > '6' || !sketch_apply(s, (unsigned)(script[i] - '0'))) goto done;
	}
	do {
		SDL_Event e;
		while(!script && SDL_PollEvent(&e)) {
			if(e.type == SDL_QUIT) quit = true;
			if(e.type != SDL_KEYDOWN) continue;
			if(e.key.keysym.sym == SDLK_ESCAPE) quit = true;
			/* One operation per press; no OS-repeat backlog. */
			unsigned a = action(e.key.keysym.sym);
			if(a && !e.key.repeat && !sketch_apply(s, a)) goto done;
		}
		char title[160];
		snprintf(title, sizeof(title), "Sketchpad | %s | Arrows move | Space toggles draw | X toggles erase | Esc quit",
			s->mode == 1 ? "DRAW ON" : s->mode == 2 ? "ERASE ON" : "MOVE ONLY");
		SDL_SetWindowTitle(window, title);
		if(SDL_SetRenderDrawColor(renderer, 32, 53, 75, 255) != 0 || SDL_RenderClear(renderer) != 0 ||
			SDL_UpdateTexture(texture, NULL, s->pixels, SKETCH_WIDTH * sizeof(uint32_t)) != 0 ||
			SDL_RenderCopy(renderer, texture, NULL, NULL) != 0) goto done;
		SDL_RenderPresent(renderer); if(!script) SDL_Delay(10);
	} while(!script && !quit);
	printf("Sketchpad: cursor=%u,%u turns=%llu commits=%llu\n", s->x, s->y,
		(unsigned long long)s->turns, (unsigned long long)s->commits); ok = true;
done:
	if(!ok) fprintf(stderr, "Sketchpad: %s %s\n", s && s->error[0] ? s->error : "Could not start or draw.", SDL_GetError());
	SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
	sketch_free(s); free(s); return ok ? 0 : 1;
}
