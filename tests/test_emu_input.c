#define main uxnemu_program_main
#include "../src/emu.c"
#undef main

static unsigned int tests_run;

#define CHECK(expression) do { \
	tests_run++; \
	if(!(expression)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			#expression); \
	exit(1); \
	} \
} while(0)

/*
 * The reset vector installs Controller and Mouse callbacks. The callbacks copy
 * the device values they see into zero page before transient values are reset.
 */
static const uint8_t input_rom[] = {
	0xa0, 0x01, 0x20, 0x80, 0x80, 0x37, /* Controller vector = 0120 */
	0xa0, 0x01, 0x50, 0x80, 0x90, 0x37, /* Mouse vector = 0150 */
	0x00,
	[0x20] =
	0x80, 0x82, 0x16, 0x80, 0x00, 0x11, /* buttons -> 00 */
	0x80, 0x83, 0x16, 0x80, 0x01, 0x11, /* key -> 01 */
	0x80, 0x02, 0x10, 0x01, 0x80, 0x02, 0x11, /* count 02 */
	0x00,
	[0x50] =
	0x80, 0x92, 0x36, 0xa0, 0x00, 0x10, 0x35, /* x -> 0010 */
	0x80, 0x94, 0x36, 0xa0, 0x00, 0x12, 0x35, /* y -> 0012 */
	0x80, 0x96, 0x16, 0x80, 0x14, 0x11, /* buttons -> 14 */
	0x80, 0x1a, 0x10, 0x01, 0x80, 0x1a, 0x11, /* count 1a */
	0x00
};

static uint16_t
memory_short(const Uxn *uxn, uint16_t address)
{
	return (uint16_t)((uxn->ram[address] << 8) |
		uxn->ram[(uint16_t)(address + 1u)]);
}

static uint16_t
test_device_short(const Uxn *uxn, uint8_t port)
{
	return (uint16_t)((uxn->devices[port] << 8) |
		uxn->devices[(uint8_t)(port + 1u)]);
}

static void
deliver_event(Varvara *varvara, Display *display, const SDL_Event *event)
{
	SDL_Event received;
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
	CHECK(SDL_PushEvent((SDL_Event *)event) == 1);
	CHECK(SDL_PollEvent(&received) == 1);
	CHECK(received.type == event->type);
	CHECK(handle_event(varvara, display, &received));
}

static void
test_sdl_controller_events(Varvara *varvara, Display *display)
{
	SDL_Event event;

	SDL_zero(event);
	event.type = SDL_KEYDOWN;
	event.key.windowID = SDL_GetWindowID(display->window);
	event.key.repeat = 0;
	event.key.keysym.sym = SDLK_RETURN;
	deliver_event(varvara, display, &event);
	CHECK(varvara->uxn.ram[0x00] == 0x00);
	CHECK(varvara->uxn.ram[0x01] == '\n');
	CHECK(varvara->uxn.ram[0x02] == 1);
	CHECK(varvara->uxn.devices[0x83] == 0);

	SDL_zero(event);
	event.type = SDL_KEYDOWN;
	event.key.windowID = SDL_GetWindowID(display->window);
	event.key.repeat = 0;
	event.key.keysym.sym = SDLK_LSHIFT;
	deliver_event(varvara, display, &event);
	CHECK(varvara->uxn.ram[0x00] == 0x04);
	CHECK(varvara->uxn.ram[0x01] == 0x00);
	CHECK(varvara->uxn.ram[0x02] == 2);
	CHECK(varvara->uxn.devices[0x82] == 0x04);

	SDL_zero(event);
	event.type = SDL_KEYUP;
	event.key.windowID = SDL_GetWindowID(display->window);
	event.key.keysym.sym = SDLK_LSHIFT;
	deliver_event(varvara, display, &event);
	CHECK(varvara->uxn.ram[0x00] == 0x00);
	CHECK(varvara->uxn.ram[0x02] == 3);
	CHECK(varvara->uxn.devices[0x82] == 0x00);
}

static void
test_sdl_mouse_events(Varvara *varvara, Display *display)
{
	SDL_Event event;

	SDL_zero(event);
	event.type = SDL_MOUSEMOTION;
	event.motion.windowID = SDL_GetWindowID(display->window);
	event.motion.x = 37;
	event.motion.y = 19;
	deliver_event(varvara, display, &event);
	CHECK(memory_short(&varvara->uxn, 0x10) == 37);
	CHECK(memory_short(&varvara->uxn, 0x12) == 19);
	CHECK(varvara->uxn.ram[0x14] == 0);
	CHECK(varvara->uxn.ram[0x1a] == 1);
	CHECK(varvara->uxn.devices[0x93] == 37);
	CHECK(varvara->uxn.devices[0x95] == 19);

	SDL_zero(event);
	event.type = SDL_MOUSEBUTTONDOWN;
	event.button.windowID = SDL_GetWindowID(display->window);
	event.button.button = SDL_BUTTON_LEFT;
	deliver_event(varvara, display, &event);
	CHECK(varvara->uxn.ram[0x14] == 0x01);
	CHECK(varvara->uxn.ram[0x1a] == 2);
	CHECK(varvara->uxn.devices[0x96] == 0x01);

	SDL_zero(event);
	event.type = SDL_MOUSEBUTTONUP;
	event.button.windowID = SDL_GetWindowID(display->window);
	event.button.button = SDL_BUTTON_LEFT;
	deliver_event(varvara, display, &event);
	CHECK(varvara->uxn.ram[0x14] == 0x00);
	CHECK(varvara->uxn.ram[0x1a] == 3);
	CHECK(varvara->uxn.devices[0x96] == 0x00);
}

int
main(void)
{
	Varvara varvara;
	Display display;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	CHECK(uxn_load(&varvara.uxn, input_rom, sizeof(input_rom)));
	CHECK(varvara_start(&varvara, 1000) == UXN_STOP_BREAK);
	CHECK(test_device_short(&varvara.uxn, 0x80) == 0x0120);
	CHECK(test_device_short(&varvara.uxn, 0x90) == 0x0150);
	CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) == 0);
	CHECK(display_init(&display, &varvara.screen, 1, false));

	test_sdl_controller_events(&varvara, &display);
	test_sdl_mouse_events(&varvara, &display);

	display_destroy(&display);
	SDL_Quit();
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
	printf("ok - %u SDL input checks\n", tests_run);
	return 0;
}
