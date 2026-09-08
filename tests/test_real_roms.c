#include "rom.h"
#include "varvara.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition) do { \
	checks++; \
	if(!(condition)) { \
		fprintf(stderr, "real ROM check failed at %s:%d: %s\n", \
			__FILE__, __LINE__, #condition); \
		exit(1); \
	} \
} while(0)

static void
run_frames(Varvara *varvara, unsigned int count)
{
	while(count-- && !varvara_is_halted(varvara))
		(void)varvara_screen_frame(varvara, 0);
}

static uint64_t
screen_hash(const Varvara *varvara)
{
	uint64_t hash = UINT64_C(1469598103934665603);
	size_t count = (size_t)varvara->screen.width * varvara->screen.height;
	size_t i;
	for(i = 0; i < count; i++) {
		hash ^= varvara->screen.background[i];
		hash *= UINT64_C(1099511628211);
		hash ^= varvara->screen.foreground[i];
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

static void
load_rom(Varvara *varvara, FILE *output, FILE *errors, const char *path)
{
	char error[512];
	CHECK(varvara_init(varvara, output, errors));
	CHECK(rom_load_file(&varvara->uxn, path, error, sizeof(error)));
	CHECK(varvara_start(varvara, 0) == UXN_STOP_BREAK);
	run_frames(varvara, 3);
}

static void
control_key(Varvara *varvara, uint8_t key)
{
	(void)varvara_controller_down(varvara, 0x01, 0);
	(void)varvara_controller_key(varvara, key, 0);
	(void)varvara_controller_up(varvara, 0x01, 0);
	run_frames(varvara, 3);
}

static bool
file_has_bytes(const char *path)
{
	FILE *file = fopen(path, "rb");
	long size;
	if(!file) return false;
	if(fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return false;
	}
	size = ftell(file);
	fclose(file);
	return size > 0;
}

static void
test_left(const char *path, FILE *output, FILE *errors)
{
	Varvara *varvara = calloc(1, sizeof(*varvara));
	uint64_t before;
	CHECK(varvara != NULL);
	(void)remove("untitled.txt");
	load_rom(varvara, output, errors, path);
	before = screen_hash(varvara);
	(void)varvara_controller_key(varvara, 'H', 0);
	run_frames(varvara, 3);
	CHECK(screen_hash(varvara) != before);
	control_key(varvara, 's');
	CHECK(file_has_bytes("untitled.txt"));
	CHECK(remove("untitled.txt") == 0);
	varvara_destroy(varvara);
	free(varvara);
	printf("left: keyboard input and save passed\n");
}

static void
test_noodle(const char *path, FILE *output, FILE *errors)
{
	Varvara *varvara = calloc(1, sizeof(*varvara));
	uint64_t before;
	CHECK(varvara != NULL);
	(void)remove("untitled55x2e.icn");
	load_rom(varvara, output, errors, path);
	before = screen_hash(varvara);
	(void)varvara_mouse_move(varvara, 100, 100, 0);
	(void)varvara_mouse_down(varvara, 0x01, 0);
	(void)varvara_mouse_move(varvara, 108, 108, 0);
	(void)varvara_mouse_move(varvara, 116, 104, 0);
	(void)varvara_mouse_up(varvara, 0x01, 0);
	run_frames(varvara, 3);
	CHECK(screen_hash(varvara) != before);
	control_key(varvara, 's');
	CHECK(file_has_bytes("untitled55x2e.icn"));
	CHECK(remove("untitled55x2e.icn") == 0);
	varvara_destroy(varvara);
	free(varvara);
	printf("noodle: mouse drawing and save passed\n");
}

static void
test_cccc(const char *path, FILE *output, FILE *errors)
{
	Varvara *varvara = calloc(1, sizeof(*varvara));
	int16_t samples[735 * 2];
	bool heard = false;
	size_t i;
	CHECK(varvara != NULL);
	load_rom(varvara, output, errors, path);
	(void)varvara_controller_key(varvara, '1', 0);
	CHECK(varvara_audio_active(varvara));
	(void)varvara_audio_render(varvara, samples, 735);
	for(i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
		if(samples[i]) heard = true;
	CHECK(heard);
	varvara_destroy(varvara);
	free(varvara);
	printf("cccc: keyboard input produced non-silent audio\n");
}

int
main(int argc, char **argv)
{
	FILE *output;
	FILE *errors;
	if(argc != 4) {
		fprintf(stderr, "usage: %s left.rom noodle.rom cccc.rom\n", argv[0]);
		return 64;
	}
	output = tmpfile();
	errors = tmpfile();
	CHECK(output != NULL);
	CHECK(errors != NULL);
	test_left(argv[1], output, errors);
	test_noodle(argv[2], output, errors);
	test_cccc(argv[3], output, errors);
	fclose(output);
	fclose(errors);
	printf("real ROM interaction checks: %u passed\n", checks);
	return 0;
}
