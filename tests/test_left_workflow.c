#define main uxnemu_program_main
#include "../src/emu.c"
#undef main

static unsigned int checks;

#define CHECK(condition) do { \
	checks++; \
	if(!(condition)) { \
		fprintf(stderr, "Left workflow failed at %s:%d: %s\n", \
			__FILE__, __LINE__, #condition); \
		exit(1); \
	} \
} while(0)

typedef struct {
	Varvara *varvara;
	Display display;
} LeftSession;

static uint16_t
memory_short(const Uxn *uxn, uint16_t address)
{
	return (uint16_t)((uxn->ram[address] << 8) |
		uxn->ram[(uint16_t)(address + 1u)]);
}

static uint16_t
find_symbol(const char *path, const char *wanted)
{
	FILE *file = fopen(path, "rb");
	uint8_t address[2];
	char name[256];
	CHECK(file != NULL);
	while(fread(address, 1, 2, file) == 2) {
		size_t length = 0;
		int byte;
		while((byte = fgetc(file)) != EOF && byte != 0) {
			if(length + 1u < sizeof(name))
				name[length++] = (char)byte;
		}
		name[length] = '\0';
		if(strcmp(name, wanted) == 0) {
			uint16_t result = (uint16_t)((address[0] << 8) | address[1]);
			CHECK(fclose(file) == 0);
			return result;
		}
		if(byte == EOF) break;
	}
	CHECK(fclose(file) == 0);
	return 0;
}

static void
write_fixture(const char *path, const uint8_t *bytes, size_t length)
{
	FILE *file = fopen(path, "wb");
	CHECK(file != NULL);
	CHECK(fwrite(bytes, 1, length, file) == length);
	CHECK(fclose(file) == 0);
}

static void
check_file(const char *path, const uint8_t *expected, size_t length)
{
	FILE *file = fopen(path, "rb");
	uint8_t actual[64];
	CHECK(length <= sizeof(actual));
	CHECK(file != NULL);
	CHECK(fread(actual, 1, length, file) == length);
	CHECK(memcmp(actual, expected, length) == 0);
	CHECK(fgetc(file) == EOF);
	CHECK(fclose(file) == 0);
}

static void
run_frames(Varvara *varvara, unsigned int count)
{
	while(count-- && !varvara_is_halted(varvara))
		(void)varvara_screen_frame(varvara, 0);
}

static void
start_left(LeftSession *session, const char *rom_path, const char *text_path,
	FILE *output, FILE *errors)
{
	char error[512];
	char *arguments[3];
	memset(session, 0, sizeof(*session));
	session->varvara = calloc(1, sizeof(*session->varvara));
	CHECK(session->varvara != NULL);
	CHECK(varvara_init(session->varvara, output, errors));
	CHECK(rom_load_file(&session->varvara->uxn, rom_path, error,
		sizeof(error)));
	session->varvara->uxn.devices[0x17] = 1;
	CHECK(varvara_start(session->varvara, 0) == UXN_STOP_BREAK);
	CHECK(varvara_has_console_vector(session->varvara));
	arguments[0] = (char *)"uxnemu";
	arguments[1] = (char *)rom_path;
	arguments[2] = (char *)text_path;
	send_arguments(session->varvara, 3, arguments, 2);
	CHECK(!varvara_is_halted(session->varvara));
	run_frames(session->varvara, 5);
	CHECK(display_init(&session->display, &session->varvara->screen, 1,
		false));
}

static void
stop_left(LeftSession *session)
{
	display_destroy(&session->display);
	varvara_destroy(session->varvara);
	free(session->varvara);
}

static void
deliver_queued_event(LeftSession *session, SDL_Event *event)
{
	SDL_Event received;
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
	CHECK(SDL_PushEvent(event) == 1);
	CHECK(SDL_PollEvent(&received) == 1);
	CHECK(received.type == event->type);
	CHECK(handle_event(session->varvara, &session->display, &received));
}

static void
deliver_key(LeftSession *session, Uint32 type, SDL_Keycode key)
{
	SDL_Event event;
	SDL_zero(event);
	event.type = type;
	event.key.windowID = SDL_GetWindowID(session->display.window);
	event.key.repeat = 0;
	event.key.keysym.sym = key;
	deliver_queued_event(session, &event);
}

static void
deliver_text_directly(LeftSession *session, const char *text)
{
	SDL_Event event;
	SDL_zero(event);
	event.type = SDL_TEXTINPUT;
	event.text.windowID = SDL_GetWindowID(session->display.window);
	snprintf(event.text.text, sizeof(event.text.text), "%s", text);
	CHECK(handle_event(session->varvara, &session->display, &event));
}

int
main(int argc, char **argv)
{
	static const uint8_t original[] = "alpha\nbeta\n";
	static const uint8_t edited[] = "lpha\nbeta\n";
	const char *fixture_path = "left-workflow.txt";
	LeftSession session;
	FILE *output;
	FILE *errors;
	uint16_t text_buffer;
	uint16_t text_end;

	if(argc != 3) {
		fprintf(stderr, "usage: %s left.rom left.rom.sym\n", argv[0]);
		return 64;
	}
	output = tmpfile();
	errors = tmpfile();
	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) == 0);
	text_buffer = find_symbol(argv[2], "textarea/buf");
	text_end = find_symbol(argv[2], "textarea/eof");
	CHECK(text_buffer != 0);
	CHECK(text_end != 0);
	(void)remove(fixture_path);
	write_fixture(fixture_path, original, sizeof(original) - 1u);

	start_left(&session, argv[1], fixture_path, output, errors);
	CHECK(memcmp(&session.varvara->uxn.ram[text_buffer], original,
		sizeof(original) - 1u) == 0);
	CHECK(session.varvara->uxn.ram[text_buffer + sizeof(original) - 1u] == 0);
	CHECK(memory_short(&session.varvara->uxn, text_end) ==
		text_buffer + sizeof(original));

	deliver_key(&session, SDL_KEYDOWN, SDLK_DELETE);
	run_frames(session.varvara, 3);
	CHECK(memcmp(&session.varvara->uxn.ram[text_buffer], edited,
		sizeof(edited) - 1u) == 0);
	CHECK(session.varvara->uxn.ram[text_buffer + sizeof(edited) - 1u] == 0);
	CHECK(memory_short(&session.varvara->uxn, text_end) ==
		text_buffer + sizeof(edited));

	deliver_key(&session, SDL_KEYDOWN, SDLK_LCTRL);
	CHECK(session.varvara->uxn.devices[0x82] == 0x01);
	deliver_text_directly(&session, "s");
	CHECK(session.varvara->uxn.devices[0x83] == 0x00);
	deliver_key(&session, SDL_KEYUP, SDLK_LCTRL);
	CHECK(session.varvara->uxn.devices[0x82] == 0x00);
	run_frames(session.varvara, 3);
	check_file(fixture_path, edited, sizeof(edited) - 1u);
	stop_left(&session);

	start_left(&session, argv[1], fixture_path, output, errors);
	CHECK(memcmp(&session.varvara->uxn.ram[text_buffer], edited,
		sizeof(edited) - 1u) == 0);
	CHECK(session.varvara->uxn.ram[text_buffer + sizeof(edited) - 1u] == 0);
	CHECK(memory_short(&session.varvara->uxn, text_end) ==
		text_buffer + sizeof(edited));
	stop_left(&session);

	CHECK(remove(fixture_path) == 0);
	SDL_Quit();
	fclose(output);
	fclose(errors);
	printf("left: load, edit, save, and reopen passed (%u checks)\n", checks);
	return 0;
}
