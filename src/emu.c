#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "rom.h"
#include "varvara.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_GameController *gamepad;
	uint32_t *pixels;
	size_t pixel_count;
	int16_t *audio_samples;
	size_t audio_capacity;
	unsigned int scale;
	SDL_AudioDeviceID audio;
	uint32_t audio_rate;
	uint8_t gamepad_directions;
	char *rom_path;
	bool fullscreen;
	bool borderless;
} Display;

static Uint32 standard_input_event;

static uint8_t
controller_button(SDL_Keycode key)
{
	switch(key) {
	case SDLK_LCTRL:
	case SDLK_RCTRL: return 0x01;
	case SDLK_LALT:
	case SDLK_RALT:
	case SDLK_LGUI:
	case SDLK_RGUI: return 0x02;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT: return 0x04;
	case SDLK_HOME: return 0x08;
	case SDLK_UP: return 0x10;
	case SDLK_DOWN: return 0x20;
	case SDLK_LEFT: return 0x40;
	case SDLK_RIGHT: return 0x80;
	default: return 0;
	}
}

static uint8_t
mouse_button(uint8_t button)
{
	if(button == SDL_BUTTON_LEFT) return 0x01;
	if(button == SDL_BUTTON_RIGHT) return 0x02;
	if(button == SDL_BUTTON_MIDDLE) return 0x04;
	return 0;
}

static uint8_t
special_key(SDL_Keycode key)
{
	switch(key) {
	case SDLK_BACKSPACE: return '\b';
	case SDLK_TAB: return '\t';
	case SDLK_RETURN:
	case SDLK_KP_ENTER: return '\n';
	case SDLK_ESCAPE: return 0x1b;
	case SDLK_DELETE: return 0x7f;
	default: return 0;
	}
}

static uint8_t
gamepad_button(SDL_GameControllerButton button)
{
	switch(button) {
	case SDL_CONTROLLER_BUTTON_A: return 0x01;
	case SDL_CONTROLLER_BUTTON_B: return 0x02;
	case SDL_CONTROLLER_BUTTON_X: return 0x04;
	case SDL_CONTROLLER_BUTTON_Y: return 0x08;
	case SDL_CONTROLLER_BUTTON_DPAD_UP: return 0x10;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return 0x20;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return 0x40;
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return 0x80;
	default: return 0;
	}
}

static bool
display_resize(Display *display, const VarvaraScreen *screen)
{
	int width = screen->width ? screen->width : 1;
	int height = screen->height ? screen->height : 1;
	size_t count = (size_t)width * (size_t)height;
	uint32_t *pixels = realloc(display->pixels, count * sizeof(*pixels));

	if(!pixels)
		return false;
	display->pixels = pixels;
	display->pixel_count = count;
	if(display->texture)
		SDL_DestroyTexture(display->texture);
	display->texture = SDL_CreateTexture(display->renderer,
		SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
	if(!display->texture)
		return false;
	SDL_SetTextureBlendMode(display->texture, SDL_BLENDMODE_NONE);
	SDL_RenderSetLogicalSize(display->renderer, width, height);
	SDL_SetWindowSize(display->window, width * (int)display->scale,
		height * (int)display->scale);
	return true;
}

static bool
display_init(Display *display, const VarvaraScreen *screen, unsigned int scale,
	bool fullscreen)
{
	int width = screen->width ? screen->width : 1;
	int height = screen->height ? screen->height : 1;

	memset(display, 0, sizeof(*display));
	display->scale = scale;
	display->fullscreen = fullscreen;
	{
		SDL_AudioSpec wanted;
		SDL_AudioSpec obtained;
		SDL_zero(wanted);
		wanted.freq = 44100;
		wanted.format = AUDIO_S16SYS;
		wanted.channels = 2;
		wanted.samples = 512;
		display->audio = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
		if(display->audio) {
			display->audio_rate = (uint32_t)obtained.freq;
			SDL_PauseAudioDevice(display->audio, 0);
		} else {
			display->audio_rate = 44100;
			fprintf(stderr, "uxnemu: audio is unavailable: %s\n", SDL_GetError());
		}
	}
	display->window = SDL_CreateWindow("Our Uxn emulator",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		width * (int)scale, height * (int)scale,
		SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE |
		(fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	if(!display->window)
		return false;
	display->renderer = SDL_CreateRenderer(display->window, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if(!display->renderer)
		display->renderer = SDL_CreateRenderer(display->window, -1,
			SDL_RENDERER_SOFTWARE);
	if(!display->renderer)
		return false;
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 255);
	if(SDL_NumJoysticks() > 0 && SDL_IsGameController(0))
		display->gamepad = SDL_GameControllerOpen(0);
	return display_resize(display, screen);
}

static void
display_destroy(Display *display)
{
	if(display->gamepad)
		SDL_GameControllerClose(display->gamepad);
	if(display->audio)
		SDL_CloseAudioDevice(display->audio);
	free(display->audio_samples);
	free(display->pixels);
	free(display->rom_path);
	if(display->texture)
		SDL_DestroyTexture(display->texture);
	if(display->renderer)
		SDL_DestroyRenderer(display->renderer);
	if(display->window)
		SDL_DestroyWindow(display->window);
}

static char *
copy_string(const char *text)
{
	size_t size = strlen(text) + 1u;
	char *copy = malloc(size);
	if(copy) memcpy(copy, text, size);
	return copy;
}

static const char *
path_name(const char *path)
{
	const char *slash = strrchr(path, '/');
	const char *backslash = strrchr(path, '\\');
	if(!slash || (backslash && backslash > slash)) slash = backslash;
	return slash ? slash + 1 : path;
}

static void
display_update_title(Display *display, const Varvara *varvara,
	const char *rom_path)
{
	char title[160];
	char name[96];
	size_t length = 0;
	uint16_t address = (uint16_t)((varvara->uxn.devices[0x06] << 8) |
		varvara->uxn.devices[0x07]);

	if(address && varvara->uxn.ram[address] == 0) {
		address++;
		while(length + 1u < sizeof(name) && address &&
			varvara->uxn.ram[address] && varvara->uxn.ram[address] != '\n') {
			uint8_t byte = varvara->uxn.ram[address++];
			if(byte < 0x20 || byte > 0x7e) break;
			name[length++] = (char)byte;
		}
	}
	name[length] = '\0';
	if(!length)
		snprintf(name, sizeof(name), "%s", path_name(rom_path));
	snprintf(title, sizeof(title), "%s - Uxn", name);
	SDL_SetWindowTitle(display->window, title);
}

static void
play_audio_frame(Display *display, Varvara *varvara)
{
	size_t frame_count;
	uint8_t finished;
	unsigned int voice;
	if(!display->audio || !varvara_audio_active(varvara)) return;
	if(SDL_GetQueuedAudioSize(display->audio) > display->audio_rate / 10u * 4u)
		return;
	frame_count = display->audio_rate / 60u;
	if(display->audio_capacity < frame_count * 2u) {
		int16_t *samples = realloc(display->audio_samples,
			frame_count * 2u * sizeof(*samples));
		if(!samples) return;
		display->audio_samples = samples;
		display->audio_capacity = frame_count * 2u;
	}
	finished = varvara_audio_render(varvara, display->audio_samples, frame_count);
	if(SDL_QueueAudio(display->audio, display->audio_samples,
		(uint32_t)(frame_count * 2u * sizeof(*display->audio_samples))) != 0)
		fprintf(stderr, "uxnemu: could not queue audio: %s\n", SDL_GetError());
	for(voice = 0; voice < 4; voice++)
		if(finished & (1u << voice))
			varvara_audio_finished(varvara, voice, 0);
}

static bool
display_present(Display *display, Varvara *varvara)
{
	int width = varvara->screen.width ? varvara->screen.width : 1;
	if(varvara->screen.resized && !display_resize(display, &varvara->screen))
		return false;
	if(!varvara_screen_compose(varvara, display->pixels,
		display->pixel_count))
		return false;
	if(SDL_UpdateTexture(display->texture, NULL, display->pixels,
		width * (int)sizeof(*display->pixels)) != 0)
		return false;
	SDL_RenderClear(display->renderer);
	if(SDL_RenderCopy(display->renderer, display->texture, NULL, NULL) != 0)
		return false;
	SDL_RenderPresent(display->renderer);
	varvara_screen_presented(varvara);
	return true;
}

static bool
display_save_bmp(Display *display, Varvara *varvara, const char *path)
{
	SDL_Surface *surface;
	int width = varvara->screen.width ? varvara->screen.width : 1;
	int height = varvara->screen.height ? varvara->screen.height : 1;
	bool saved;
	if(varvara->screen.dirty && !display_present(display, varvara))
		return false;
	surface = SDL_CreateRGBSurfaceWithFormatFrom(display->pixels, width, height,
		32, width * (int)sizeof(*display->pixels), SDL_PIXELFORMAT_ARGB8888);
	if(!surface)
		return false;
	saved = SDL_SaveBMP(surface, path) == 0;
	SDL_FreeSurface(surface);
	return saved;
}

static bool
restart_program(Varvara *varvara, Display *display, const char *rom_path,
	bool soft)
{
	char error[512];
	if(!varvara_reboot(varvara, soft)) {
		fprintf(stderr, "uxnemu: could not reset the screen\n");
		return false;
	}
	if(!rom_load_file(&varvara->uxn, rom_path, error, sizeof(error))) {
		fprintf(stderr, "uxnemu: %s\n", error);
		return false;
	}
	if(display->audio)
		SDL_ClearQueuedAudio(display->audio);
	varvara_audio_set_sample_rate(varvara, display->audio_rate);
	varvara_start(varvara, 0);
	display_update_title(display, varvara, rom_path);
	return true;
}

static bool
open_dropped_rom(Varvara *varvara, Display *display, const char *path)
{
	char *copy = copy_string(path);
	if(!copy)
		return false;
	if(!restart_program(varvara, display, path, false)) {
		free(copy);
		if(display->rom_path)
			(void)restart_program(varvara, display, display->rom_path, false);
		return false;
	}
	free(display->rom_path);
	display->rom_path = copy;
	return true;
}

static void
set_fullscreen(Display *display, bool fullscreen)
{
	display->fullscreen = fullscreen;
	if(SDL_SetWindowFullscreen(display->window,
		fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0)
		fprintf(stderr, "uxnemu: could not change fullscreen mode: %s\n",
			SDL_GetError());
}

static void
set_borderless(Display *display, bool borderless)
{
	if(display->fullscreen) return;
	display->borderless = borderless;
	SDL_SetWindowBordered(display->window, borderless ? SDL_FALSE : SDL_TRUE);
}

static void
change_gamepad_directions(Varvara *varvara, Display *display,
	SDL_GameControllerAxis axis, int16_t value)
{
	uint8_t old = display->gamepad_directions;
	uint8_t next = old;
	const int dead_zone = 8000;
	if(axis == SDL_CONTROLLER_AXIS_LEFTX) {
		next &= (uint8_t)~0xc0;
		if(value < -dead_zone) next |= 0x40;
		else if(value > dead_zone) next |= 0x80;
	} else if(axis == SDL_CONTROLLER_AXIS_LEFTY) {
		next &= (uint8_t)~0x30;
		if(value < -dead_zone) next |= 0x10;
		else if(value > dead_zone) next |= 0x20;
	} else {
		return;
	}
	if(old & (uint8_t)~next)
		varvara_controller_up(varvara, old & (uint8_t)~next, 0);
	if(next & (uint8_t)~old)
		varvara_controller_down(varvara, next & (uint8_t)~old, 0);
	display->gamepad_directions = next;
}

static int
read_standard_input(void *unused)
{
	int byte;
	(void)unused;
	while((byte = fgetc(stdin)) != EOF) {
		SDL_Event event;
		SDL_zero(event);
		event.type = standard_input_event;
		event.user.code = byte | (1 << 8);
		while(SDL_PushEvent(&event) < 0)
			SDL_Delay(25);
	}
	{
		SDL_Event event;
		SDL_zero(event);
		event.type = standard_input_event;
		event.user.code = '\n' | (4 << 8);
		while(SDL_PushEvent(&event) < 0)
			SDL_Delay(25);
	}
	return 0;
}

static void
send_arguments(Varvara *varvara, int argc, char **argv, int first)
{
	int i;
	for(i = first; i < argc && !varvara_is_halted(varvara); i++) {
		const unsigned char *cursor = (const unsigned char *)argv[i];
		while(*cursor && !varvara_is_halted(varvara))
			varvara_console_input(varvara, *cursor++, 2, 0);
		varvara_console_input(varvara, '\n',
			(uint8_t)(i == argc - 1 ? 4 : 3), 0);
	}
}

static bool
handle_event(Varvara *varvara, Display *display, const SDL_Event *event)
{
	uint8_t button;
	if(event->type == SDL_QUIT)
		return false;
	if(event->type == SDL_DROPFILE) {
		if(!open_dropped_rom(varvara, display, event->drop.file))
			fprintf(stderr, "uxnemu: could not open dropped ROM\n");
		SDL_free(event->drop.file);
		return true;
	}
	if(event->type == standard_input_event) {
		varvara_console_input(varvara, (uint8_t)(event->user.code & 0xff),
			(uint8_t)((event->user.code >> 8) & 0xff), 0);
	} else if(event->type == SDL_WINDOWEVENT &&
		event->window.event == SDL_WINDOWEVENT_EXPOSED)
		varvara->screen.dirty = true;
	else if(event->type == SDL_KEYDOWN && !event->key.repeat) {
		button = controller_button(event->key.keysym.sym);
		if(button)
			varvara_controller_down(varvara, button, 0);
		else if(event->key.keysym.sym == SDLK_F1) {
			display->scale = display->scale % 3u + 1u;
			SDL_SetWindowSize(display->window,
				(int)varvara->screen.width * (int)display->scale,
				(int)varvara->screen.height * (int)display->scale);
		} else if(event->key.keysym.sym == SDLK_F2)
			varvara->uxn.device_write(&varvara->uxn, 0x0e, 1, varvara);
		else if(event->key.keysym.sym == SDLK_F3)
			varvara->uxn.devices[0x0f] = 0xff;
		else if(event->key.keysym.sym == SDLK_F4)
			return restart_program(varvara, display, display->rom_path, false);
		else if(event->key.keysym.sym == SDLK_F5)
			return restart_program(varvara, display, display->rom_path, true);
		else if(event->key.keysym.sym == SDLK_F11)
			set_fullscreen(display, !display->fullscreen);
		else if(event->key.keysym.sym == SDLK_F12)
			set_borderless(display, !display->borderless);
		else {
			uint8_t key = special_key(event->key.keysym.sym);
			if(key) varvara_controller_key(varvara, key, 0);
		}
	} else if(event->type == SDL_KEYUP) {
		button = controller_button(event->key.keysym.sym);
		if(button)
			varvara_controller_up(varvara, button, 0);
	} else if(event->type == SDL_TEXTINPUT) {
		const unsigned char *cursor = (const unsigned char *)event->text.text;
		while(*cursor)
			varvara_controller_key(varvara, *cursor++, 0);
	} else if(event->type == SDL_MOUSEMOTION) {
		float x;
		float y;
		SDL_RenderWindowToLogical(display->renderer, event->motion.x,
			event->motion.y, &x, &y);
		if(x < 0) x = 0;
		if(y < 0) y = 0;
		varvara_mouse_move(varvara, (uint16_t)x, (uint16_t)y, 0);
	} else if(event->type == SDL_MOUSEBUTTONDOWN) {
		button = mouse_button(event->button.button);
		if(button) varvara_mouse_down(varvara, button, 0);
	} else if(event->type == SDL_MOUSEBUTTONUP) {
		button = mouse_button(event->button.button);
		if(button) varvara_mouse_up(varvara, button, 0);
	} else if(event->type == SDL_MOUSEWHEEL) {
		varvara_mouse_scroll(varvara, (int16_t)event->wheel.x,
			(int16_t)-event->wheel.y, 0);
	} else if(event->type == SDL_CONTROLLERBUTTONDOWN ||
		event->type == SDL_CONTROLLERBUTTONUP) {
		button = gamepad_button((SDL_GameControllerButton)event->cbutton.button);
		if(button) {
			if(event->type == SDL_CONTROLLERBUTTONDOWN)
				varvara_controller_down(varvara, button, 0);
			else
				varvara_controller_up(varvara, button, 0);
		}
	} else if(event->type == SDL_CONTROLLERAXISMOTION) {
		change_gamepad_directions(varvara, display,
			(SDL_GameControllerAxis)event->caxis.axis, event->caxis.value);
	} else if(event->type == SDL_CONTROLLERDEVICEADDED && !display->gamepad) {
		display->gamepad = SDL_GameControllerOpen(event->cdevice.which);
	} else if(event->type == SDL_CONTROLLERDEVICEREMOVED && display->gamepad &&
		event->cdevice.which == SDL_JoystickInstanceID(
			SDL_GameControllerGetJoystick(display->gamepad))) {
		SDL_GameControllerClose(display->gamepad);
		display->gamepad = NULL;
		if(display->gamepad_directions)
			varvara_controller_up(varvara, display->gamepad_directions, 0);
		display->gamepad_directions = 0;
	}
	return true;
}

static void
usage(const char *program)
{
	fprintf(stderr,
		"usage: %s [-f|-2] [--scale 1|2|3] [--frames count] "
		"[--screenshot image.bmp] [--allow-filesystem] [--allow-exec] "
		"program.rom [arguments ...]\n",
		program);
}

int
main(int argc, char **argv)
{
	Varvara *varvara;
	Display display;
	char error[512];
	unsigned int scale = 1;
	unsigned long frame_limit = 0;
	unsigned long frames = 0;
	int rom_index = 1;
	bool running = true;
	bool fullscreen = false;
	bool sandbox_files = true;
	bool allow_exec = false;
	const char *screenshot_path = NULL;
	uint64_t next_frame;
	uint64_t frame_ticks;

	while(rom_index < argc && argv[rom_index][0] == '-') {
		if(strcmp(argv[rom_index], "--") == 0) {
			rom_index++;
			break;
		} else if(strcmp(argv[rom_index], "-v") == 0 ||
			strcmp(argv[rom_index], "--version") == 0) {
			printf("%s - Varvara(79K) emulator\n", argv[0]);
			return 0;
		} else if(strcmp(argv[rom_index], "-h") == 0 ||
			strcmp(argv[rom_index], "--help") == 0) {
			usage(argv[0]);
			return 0;
		} else if(strcmp(argv[rom_index], "-f") == 0 ||
			strcmp(argv[rom_index], "--fullscreen") == 0) {
			fullscreen = true;
		} else if(strcmp(argv[rom_index], "-2") == 0) {
			scale = 2;
		} else if(strcmp(argv[rom_index], "--allow-filesystem") == 0) {
			sandbox_files = false;
		} else if(strcmp(argv[rom_index], "--allow-exec") == 0) {
			allow_exec = true;
		} else if(strcmp(argv[rom_index], "--scale") == 0 && rom_index + 1 < argc) {
			scale = (unsigned int)strtoul(argv[++rom_index], NULL, 10);
			if(scale < 1 || scale > 3) return usage(argv[0]), 64;
		} else if(strcmp(argv[rom_index], "--frames") == 0 &&
			rom_index + 1 < argc) {
			frame_limit = strtoul(argv[++rom_index], NULL, 10);
		} else if(strcmp(argv[rom_index], "--screenshot") == 0 &&
			rom_index + 1 < argc) {
			screenshot_path = argv[++rom_index];
		} else {
			return usage(argv[0]), 64;
		}
		rom_index++;
	}
	if(rom_index >= argc)
		return usage(argv[0]), 64;
	varvara = calloc(1, sizeof(*varvara));
	if(!varvara) {
		fprintf(stderr, "uxnemu: could not allocate the Uxn machine\n");
		return 70;
	}
	if(!varvara_init(varvara, stdout, stderr)) {
		fprintf(stderr, "uxnemu: could not allocate the Varvara screen\n");
		free(varvara);
		return 70;
	}
	varvara_files_set_sandbox(varvara, sandbox_files);
	varvara_exec_set_allowed(varvara, allow_exec);
	if(!rom_load_file(&varvara->uxn, argv[rom_index], error, sizeof(error))) {
		fprintf(stderr, "uxnemu: %s\n", error);
		varvara_destroy(varvara);
		free(varvara);
		return 66;
	}
	varvara->uxn.devices[0x17] = rom_index + 1 < argc ? 1 : 0;
	varvara_start(varvara, 0);
	if(varvara_has_console_vector(varvara) && rom_index + 1 < argc)
		send_arguments(varvara, argc, argv, rom_index + 1);
	if(varvara_is_halted(varvara)) {
		int exit_code = varvara_exit_code(varvara);
		varvara_destroy(varvara);
		free(varvara);
		return exit_code;
	}
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER |
		SDL_INIT_AUDIO) != 0) {
		fprintf(stderr, "uxnemu: SDL could not start: %s\n", SDL_GetError());
		varvara_destroy(varvara);
		free(varvara);
		return 70;
	}
	if(!display_init(&display, &varvara->screen, scale, fullscreen)) {
		fprintf(stderr, "uxnemu: window could not start: %s\n", SDL_GetError());
		display_destroy(&display);
		SDL_Quit();
		varvara_destroy(varvara);
		free(varvara);
		return 70;
	}
	display.rom_path = copy_string(argv[rom_index]);
	if(!display.rom_path) {
		fprintf(stderr, "uxnemu: could not remember the ROM path\n");
		display_destroy(&display);
		SDL_Quit();
		varvara_destroy(varvara);
		free(varvara);
		return 70;
	}
	display_update_title(&display, varvara, display.rom_path);
	varvara_audio_set_sample_rate(varvara, display.audio_rate);
	standard_input_event = SDL_RegisterEvents(1);
	if(standard_input_event == (Uint32)-1) {
		fprintf(stderr, "uxnemu: standard input is unavailable: %s\n",
			SDL_GetError());
	} else {
		SDL_Thread *thread = SDL_CreateThread(read_standard_input,
			"uxnemu standard input", NULL);
		if(thread)
			SDL_DetachThread(thread);
		else
			fprintf(stderr, "uxnemu: standard input is unavailable: %s\n",
				SDL_GetError());
	}
	SDL_StartTextInput();
	frame_ticks = SDL_GetPerformanceFrequency() / 60u;
	next_frame = SDL_GetPerformanceCounter();
	while(running && !varvara_is_halted(varvara)) {
		SDL_Event event;
		uint64_t now;
		varvara_exec_poll(varvara);
		while(SDL_PollEvent(&event))
			if(!handle_event(varvara, &display, &event))
				running = false;
		now = SDL_GetPerformanceCounter();
		if(now >= next_frame) {
			varvara_screen_frame(varvara, 0);
			play_audio_frame(&display, varvara);
			next_frame = now + frame_ticks;
			frames++;
			if(frame_limit && frames >= frame_limit)
				running = false;
		}
		if(varvara->screen.dirty && !display_present(&display, varvara)) {
			fprintf(stderr, "uxnemu: drawing failed: %s\n", SDL_GetError());
			running = false;
		}
		SDL_Delay(1);
	}
	if(screenshot_path &&
		!display_save_bmp(&display, varvara, screenshot_path))
		fprintf(stderr, "uxnemu: could not save %s: %s\n", screenshot_path,
			SDL_GetError());
	SDL_StopTextInput();
	display_destroy(&display);
	SDL_Quit();
	argc = varvara_exit_code(varvara);
	varvara_destroy(varvara);
	free(varvara);
	return argc;
}
