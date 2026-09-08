#define _POSIX_C_SOURCE 200809L

#include "rom.h"
#include "varvara.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

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

static void
send_standard_input(Varvara *varvara, bool accept_input)
{
	bool input_ended = !accept_input;
	while(!varvara_is_halted(varvara)) {
		struct timeval timeout;
		struct timeval *wait = NULL;
		fd_set input;
		int ready;
		int byte;

		varvara_exec_poll(varvara);
		if(input_ended && !varvara_exec_running(varvara)) break;
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;
		if(varvara_exec_running(varvara)) wait = &timeout;
		FD_ZERO(&input);
		if(!input_ended) FD_SET(STDIN_FILENO, &input);
		ready = select(input_ended ? 0 : STDIN_FILENO + 1,
			input_ended ? NULL : &input, NULL, NULL, wait);
		if(ready < 0) break;
		if(ready == 0 || input_ended) continue;
		byte = fgetc(stdin);
		if(byte == EOF)
			input_ended = true;
		else
			(void)varvara_console_input(varvara, (uint8_t)byte, 1, 0);
	}
	if(!varvara_is_halted(varvara))
		varvara_console_input(varvara, '\n', 4, 0);
}

int
main(int argc, char **argv)
{
	Varvara *varvara;
	char error[512];
	int rom_index = 1;
	bool sandbox_files = true;
	bool allow_exec = false;

	while(rom_index < argc && argv[rom_index][0] == '-') {
		if(strcmp(argv[rom_index], "--") == 0) {
			rom_index++;
			break;
		} else if(strcmp(argv[rom_index], "--allow-filesystem") == 0) {
			sandbox_files = false;
		} else if(strcmp(argv[rom_index], "--allow-exec") == 0) {
			allow_exec = true;
		} else if(strcmp(argv[rom_index], "-v") == 0 ||
			strcmp(argv[rom_index], "--version") == 0) {
			printf("%s - Varvara(79K) command-line emulator\n", argv[0]);
			return 0;
		} else if(strcmp(argv[rom_index], "-h") == 0 ||
			strcmp(argv[rom_index], "--help") == 0) {
			printf("usage: %s [--allow-filesystem] [--allow-exec] program.rom "
				"[arguments ...]\n", argv[0]);
			return 0;
		} else {
			fprintf(stderr, "uxncli: unknown option: %s\n", argv[rom_index]);
			return 64;
		}
		rom_index++;
	}
	if(rom_index >= argc) {
		fprintf(stderr,
			"usage: %s [--allow-filesystem] [--allow-exec] program.rom "
			"[arguments ...]\n",
			argv[0]);
		return 64;
	}
	varvara = calloc(1, sizeof(*varvara));
	if(!varvara) {
		fprintf(stderr, "uxncli: could not allocate the Uxn machine\n");
		return 70;
	}
	if(!varvara_init(varvara, stdout, stderr)) {
		fprintf(stderr, "uxncli: could not allocate the Varvara screen\n");
		free(varvara);
		return 70;
	}
	varvara_files_set_sandbox(varvara, sandbox_files);
	varvara_exec_set_allowed(varvara, allow_exec);
	if(!rom_load_file(&varvara->uxn, argv[rom_index], error, sizeof(error))) {
		fprintf(stderr, "uxncli: %s\n", error);
		varvara_destroy(varvara);
		free(varvara);
		return 66;
	}

	varvara->uxn.devices[0x17] = argc > rom_index + 1 ? 1 : 0;
	varvara_start(varvara, 0);
	if(!varvara_is_halted(varvara) &&
		(varvara_has_console_vector(varvara) || varvara_exec_running(varvara))) {
		if(varvara_has_console_vector(varvara) && argc > rom_index + 1)
			send_arguments(varvara, argc, argv, rom_index + 1);
		send_standard_input(varvara, varvara_has_console_vector(varvara));
	}
	argc = varvara_exit_code(varvara);
	varvara_destroy(varvara);
	free(varvara);
	return argc;
}
