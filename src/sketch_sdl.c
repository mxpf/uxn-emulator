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
	const char *script = NULL, *open_path = NULL, *save_path = NULL;
	for(int i = 1; i < argc; i++) {
		if(i + 1 < argc && !strcmp(argv[i], "--script")) script = argv[++i];
		else if(i + 1 < argc && !strcmp(argv[i], "--open")) open_path = argv[++i];
		else if(i + 1 < argc && !strcmp(argv[i], "--save")) save_path = argv[++i];
		else { fprintf(stderr, "usage: sketchpad [--open drawing.sketch] [--save new.sketch] [--script 1..6]\n"); return 1; }
	}
	Sketchpad *s = calloc(1, sizeof(*s)); bool ok = false, quit = false;
	SDL_Window *window = NULL; SDL_Renderer *renderer = NULL; SDL_Texture *texture = NULL;
	if(!s || !sketch_open(s) || SDL_Init(SDL_INIT_VIDEO) != 0) goto done;
	if(open_path && !sketch_read_file(s, open_path)) { fprintf(stderr, "Cannot open drawing: %s\n", open_path); goto done; }
	window = SDL_CreateWindow("Sketchpad | Space toggles draw | X toggles erase | Esc quit",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 768, 576,
		script ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if(!window) goto done;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if(!renderer || SDL_RenderSetLogicalSize(renderer, SKETCH_WIDTH, SKETCH_HEIGHT) != 0) goto done;
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SKETCH_WIDTH, SKETCH_HEIGHT);
	if(!texture) goto done;
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	const char *file_status = "S saves new sketchpad.sketch | Drop file to open";
	if(script) for(size_t i = 0; script[i]; i++) {
		if(script[i] < '1' || script[i] > '6' || !sketch_apply(s, (unsigned)(script[i] - '0'))) goto done;
	}
	do {
		SDL_Event e;
		while(!script && SDL_PollEvent(&e)) {
			if(e.type == SDL_QUIT) quit = true;
			if(e.type == SDL_DROPFILE) {
				file_status = sketch_read_file(s, e.drop.file) ? "Drawing opened" : "Could not open drawing; check file";
				SDL_free(e.drop.file); if(s->failed) goto done;
			}
			if(e.type != SDL_KEYDOWN) continue;
			if(e.key.keysym.sym == SDLK_ESCAPE) quit = true;
			if(e.key.keysym.sym == SDLK_s && !e.key.repeat) {
				file_status = sketch_write_file(s, save_path ? save_path : "sketchpad.sketch")
					? "Drawing saved" : "Save failed: destination must be new and writable";
			}
			/* One operation per press; no OS-repeat backlog. */
			unsigned a = action(e.key.keysym.sym);
			if(a && !e.key.repeat && !sketch_apply(s, a)) goto done;
		}
		char title[256];
		snprintf(title, sizeof(title), "Sketchpad | %s | Space: draw | X: erase | %s",
			s->mode == 1 ? "DRAW ON" : s->mode == 2 ? "ERASE ON" : "MOVE ONLY", file_status);
		SDL_SetWindowTitle(window, title);
		if(SDL_SetRenderDrawColor(renderer, 32, 53, 75, 255) != 0 || SDL_RenderClear(renderer) != 0 ||
			SDL_UpdateTexture(texture, NULL, s->pixels, SKETCH_WIDTH * sizeof(uint32_t)) != 0 ||
			SDL_RenderCopy(renderer, texture, NULL, NULL) != 0) goto done;
		SDL_RenderPresent(renderer); if(!script) SDL_Delay(10);
	} while(!script && !quit);
	if(script && save_path && !sketch_write_file(s, save_path)) { fprintf(stderr, "Could not save new drawing: %s\n", save_path); goto done; }
	printf("Sketchpad: cursor=%u,%u turns=%llu commits=%llu\n", s->x, s->y,
		(unsigned long long)s->turns, (unsigned long long)s->commits); ok = true;
done:
	if(!ok) fprintf(stderr, "Sketchpad: %s %s\n", s && s->error[0] ? s->error : "Could not start or draw.", SDL_GetError());
	SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
	sketch_free(s); free(s); return ok ? 0 : 1;
}
