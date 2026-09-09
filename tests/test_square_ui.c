/* Exercise the same SDL event dispatcher used by the visible window. */
#define main square_window_main
#include "../src/square_sdl.c"
#undef main

static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while(0)

static void
key(SquareSession *s, SDL_Keycode code, bool repeat, bool *quit)
{
	SDL_Event event = {0}; event.type = SDL_KEYDOWN; event.key.keysym.sym = code; event.key.repeat = repeat;
	CHECK(SDL_PushEvent(&event) == 1);
	while(SDL_PollEvent(&event)) CHECK(handle_event(s, &event, quit));
}

int main(void)
{
	CHECK(SDL_Init(SDL_INIT_EVENTS) == 0);
	SquareSession *s = calloc(1, sizeof(*s)); CHECK(s != NULL); CHECK(square_init(s));
	bool quit = false;
	key(s, SDLK_UP, false, &quit); key(s, SDLK_RIGHT, true, &quit);
	CHECK(s->input_count == 2 && s->inputs[0].action == 1 && s->inputs[1].action == 2);
	for(unsigned i = 0; i < 6; i++) CHECK(square_step(s));
	CHECK(s->live->nodes[1].uxn.ram[0] == 9 && s->live->nodes[1].uxn.ram[1] == 5);
	key(s, SDLK_r, false, &quit); CHECK(s->play && s->sealed);
	key(s, SDLK_DOWN, false, &quit); CHECK(s->input_count == 2);
	while(!s->replay_done) CHECK(square_replay_step(s));
	key(s, SDLK_r, true, &quit); CHECK(s->replay_done);
	key(s, SDLK_n, false, &quit); CHECK(!s->sealed && !s->play && !s->input_count);
	key(s, SDLK_LEFT, false, &quit); key(s, SDLK_DOWN, false, &quit);
	CHECK(s->inputs[0].action == 4 && s->inputs[1].action == 3);
	key(s, SDLK_ESCAPE, false, &quit); CHECK(quit);
	quit = false; SDL_Event close = {0}; close.type = SDL_QUIT;
	CHECK(handle_event(s, &close, &quit) && quit);
	square_free(s); free(s); SDL_Quit();
	printf("%u square window-control checks passed\n", checks); return 0;
}
