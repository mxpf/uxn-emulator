#define _POSIX_C_SOURCE 200809L

#include "rom.h"
#include "uxn.h"
#include "varvara.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static unsigned int tests_run;

#define CHECK(expression) do { \
	tests_run++; \
	if(!(expression)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
		exit(1); \
	} \
} while(0)

static void
run(Uxn *uxn, const uint8_t *program, size_t length)
{
	uxn_init(uxn);
	CHECK(uxn_load(uxn, program, length));
	CHECK(uxn_eval(uxn, UXN_ROM_START, 1000) == UXN_STOP_BREAK);
}

static uint16_t
top_short(const UxnStack *stack)
{
	uint8_t low = stack->data[(uint8_t)(stack->pointer - 1u)];
	uint8_t high = stack->data[(uint8_t)(stack->pointer - 2u)];
	return (uint16_t)((high << 8) | low);
}

static void
host_write_byte(Varvara *varvara, uint8_t port, uint8_t value)
{
	varvara->uxn.devices[port] = value;
	varvara->uxn.device_write(&varvara->uxn, port, value, varvara);
}

static void
host_write_short(Varvara *varvara, uint8_t port, uint16_t value)
{
	varvara->uxn.devices[port] = (uint8_t)(value >> 8);
	host_write_byte(varvara, (uint8_t)(port + 1u), (uint8_t)value);
}

static void
write_rom_file(const char *path, const uint8_t *bytes, size_t length)
{
	FILE *file = fopen(path, "wb");
	CHECK(file != NULL);
	CHECK(fwrite(bytes, 1, length, file) == length);
	CHECK(fclose(file) == 0);
}

static void
test_literals_and_math(void)
{
	Uxn uxn;
	const uint8_t byte_math[] = {0x80, 0x07, 0x80, 0x05, 0x18, 0x00};
	const uint8_t short_math[] = {
		0xa0, 0x12, 0x34, 0xa0, 0x00, 0x02, 0x3a, 0x00
	};
	const uint8_t division_by_zero[] = {
		0x80, 0x2a, 0x80, 0x00, 0x1b, 0x00
	};

	run(&uxn, byte_math, sizeof(byte_math));
	CHECK(uxn.working.pointer == 1);
	CHECK(uxn.working.data[0] == 12);

	run(&uxn, short_math, sizeof(short_math));
	CHECK(top_short(&uxn.working) == 0x2468);

	run(&uxn, division_by_zero, sizeof(division_by_zero));
	CHECK(uxn.working.data[0] == 0);
}

static void
test_stack_modes(void)
{
	Uxn uxn;
	const uint8_t program[] = {
		0x80, 0x11, 0x80, 0x22, 0x80, 0x33,
		0x05,             /* ROT: 11 22 33 -> 22 33 11 */
		0x86,             /* DUPk keeps 11, then appends 11 11 */
		0x0f,             /* STH moves one byte to the return stack */
		0x4f,             /* STHr moves it back to working */
		0x00
	};

	run(&uxn, program, sizeof(program));
	CHECK(uxn.working.pointer == 5);
	CHECK(uxn.working.data[0] == 0x22);
	CHECK(uxn.working.data[1] == 0x33);
	CHECK(uxn.working.data[2] == 0x11);
	CHECK(uxn.working.data[3] == 0x11);
	CHECK(uxn.working.data[4] == 0x11);
	CHECK(uxn.return_stack.pointer == 0);
}

static void
test_memory_addressing(void)
{
	Uxn uxn;
	const uint8_t zero_page[] = {
		0xa0, 0xab, 0xcd, 0x80, 0xff, 0x31,
		0x80, 0xff, 0x30, 0x00
	};
	const uint8_t absolute[] = {
		0x80, 0x5a, 0xa0, 0x20, 0x00, 0x15,
		0xa0, 0x20, 0x00, 0x14, 0x00
	};

	run(&uxn, zero_page, sizeof(zero_page));
	CHECK(top_short(&uxn.working) == 0xabcd);
	CHECK(uxn.ram[0xff] == 0xab);
	CHECK(uxn.ram[0x00] == 0xcd);

	run(&uxn, absolute, sizeof(absolute));
	CHECK(uxn.working.pointer == 1);
	CHECK(uxn.working.data[0] == 0x5a);
	CHECK(uxn.ram[0x2000] == 0x5a);
}

typedef struct {
	uint8_t last_port;
	uint8_t last_value;
	unsigned int writes;
} DeviceSpy;

static uint8_t
spy_read(Uxn *uxn, uint8_t port, void *context)
{
	(void)context;
	return (uint8_t)(uxn->devices[port] + 1u);
}

static void
spy_write(Uxn *uxn, uint8_t port, uint8_t value, void *context)
{
	DeviceSpy *spy = context;
	(void)uxn;
	spy->last_port = port;
	spy->last_value = value;
	spy->writes++;
}

static void
test_devices(void)
{
	Uxn uxn;
	DeviceSpy spy = {0};
	const uint8_t program[] = {
		0xa0, 0xbe, 0xef, 0x80, 0x30, 0x37,
		0x80, 0x30, 0x36, 0x00
	};

	uxn_init(&uxn);
	uxn_connect_devices(&uxn, spy_read, spy_write, &spy);
	CHECK(uxn_load(&uxn, program, sizeof(program)));
	CHECK(uxn_eval(&uxn, UXN_ROM_START, 100) == UXN_STOP_BREAK);
	CHECK(uxn.devices[0x30] == 0xbe);
	CHECK(uxn.devices[0x31] == 0xef);
	CHECK(spy.writes == 1);
	CHECK(spy.last_port == 0x31);
	CHECK(spy.last_value == 0xef);
	CHECK(top_short(&uxn.working) == 0xbfef);
}

static void
test_control_flow(void)
{
	Uxn uxn;
	const uint8_t conditional[] = {
		0x80, 0x01,       /* condition */
		0x20, 0x00, 0x03, /* JCI over the next literal */
		0x80, 0xee,
		0x00,
		0x80, 0x2a,
		0x00
	};
	const uint8_t subroutine[] = {
		0x60, 0x00, 0x01, /* JSI to subroutine */
		0x00,
		0x80, 0x2a,
		0x6c             /* JMP2r */
	};

	run(&uxn, conditional, sizeof(conditional));
	CHECK(uxn.working.pointer == 1);
	CHECK(uxn.working.data[0] == 0x2a);

	run(&uxn, subroutine, sizeof(subroutine));
	CHECK(uxn.working.data[0] == 0x2a);
	CHECK(uxn.return_stack.pointer == 0);
}

static void
test_instruction_limit(void)
{
	Uxn uxn;
	const uint8_t loop[] = {0x40, 0xff, 0xfd};
	uxn_init(&uxn);
	CHECK(uxn_load(&uxn, loop, sizeof(loop)));
	CHECK(uxn_eval(&uxn, UXN_ROM_START, 10) == UXN_STOP_LIMIT);
	CHECK(uxn.instructions == 10);
}

static void
test_varvara_console_and_state(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	int character;
	const uint8_t program[] = {
		0xa0, 0x01, 0x07, 0x80, 0x10, 0x37, 0x00,
		0x80, 0x12, 0x16, 0x80, 0x18, 0x17, 0x00
	};
	const uint8_t exit_program[] = {
		0x80, 0x82, 0x80, 0x0f, 0x17, 0x00
	};

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	CHECK(uxn_load(&varvara.uxn, program, sizeof(program)));
	CHECK(varvara_start(&varvara, 100) == UXN_STOP_BREAK);
	CHECK(varvara_has_console_vector(&varvara));
	CHECK(varvara_console_input(&varvara, 'Z', 1, 100) == UXN_STOP_BREAK);
	rewind(output);
	character = fgetc(output);
	CHECK(character == 'Z');
	CHECK(varvara.uxn.devices[0x17] == 1);
	varvara_destroy(&varvara);

	CHECK(varvara_init(&varvara, output, errors));
	CHECK(uxn_load(&varvara.uxn, exit_program, sizeof(exit_program)));
	CHECK(varvara_start(&varvara, 100) == UXN_STOP_BREAK);
	CHECK(varvara_is_halted(&varvara));
	CHECK(varvara_exit_code(&varvara) == 2);
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_console_exec(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	const char command[] = "printf x";
	struct timespec pause = {0, 1000000};
	unsigned int tries;

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	memcpy(&varvara.uxn.ram[0x0300], command, sizeof(command));
	host_write_short(&varvara, 0x1c, 0x0300);
	host_write_byte(&varvara, 0x1e, 0x02);
	host_write_byte(&varvara, 0x1f, 1);
	CHECK(!varvara_exec_running(&varvara));
	CHECK(varvara.uxn.devices[0x15] == 0xff);
	CHECK(varvara.uxn.devices[0x16] == 126);

	varvara_exec_set_allowed(&varvara, true);
	host_write_byte(&varvara, 0x1f, 1);
	CHECK(varvara_exec_running(&varvara));
	for(tries = 0; tries < 1000 && varvara_exec_running(&varvara); tries++) {
		varvara_exec_poll(&varvara);
		(void)nanosleep(&pause, NULL);
	}
	varvara_exec_poll(&varvara);
	CHECK(!varvara_exec_running(&varvara));
	CHECK(varvara.uxn.devices[0x12] == 'x');
	CHECK(varvara.uxn.devices[0x15] == 0xff);
	CHECK(varvara.uxn.devices[0x16] == 0);
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_expansion(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	const uint8_t trigger[] = {
		0xa0, 0x02, 0x00, 0x80, 0x02, 0x37, 0x00
	};

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	CHECK(uxn_load(&varvara.uxn, trigger, sizeof(trigger)));
	varvara.uxn.ram[0x0200] = 0x00; /* fill */
	varvara.uxn.ram[0x0201] = 0x00;
	varvara.uxn.ram[0x0202] = 0x03; /* length */
	varvara.uxn.ram[0x0203] = 0x00;
	varvara.uxn.ram[0x0204] = 0x01; /* bank one */
	varvara.uxn.ram[0x0205] = 0x12;
	varvara.uxn.ram[0x0206] = 0x34; /* address */
	varvara.uxn.ram[0x0207] = 0xa5; /* value */
	CHECK(varvara_start(&varvara, 100) == UXN_STOP_BREAK);
	CHECK(varvara.uxn.ram[UXN_RAM_SIZE + 0x1234] == 0xa5);
	CHECK(varvara.uxn.ram[UXN_RAM_SIZE + 0x1235] == 0xa5);
	CHECK(varvara.uxn.ram[UXN_RAM_SIZE + 0x1236] == 0xa5);
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
run_expansion_command(Varvara *varvara, uint16_t address,
	const uint8_t *command, size_t length)
{
	memcpy(&varvara->uxn.ram[address], command, length);
	host_write_short(varvara, 0x02, address);
}

static void
test_varvara_expansion_modes(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	const uint8_t copy_left[] = {
		0x01, 0x00, 0x04, 0x00, 0x00, 0x05, 0x01,
		0x00, 0x00, 0x05, 0x00
	};
	const uint8_t copy_right[] = {
		0x02, 0x00, 0x04, 0x00, 0x00, 0x06, 0x00,
		0x00, 0x00, 0x06, 0x01
	};
	const uint8_t copy_to_bank_edge[] = {
		0x01, 0x00, 0x04, 0x00, 0x00, 0x07, 0x00,
		0x00, 0x01, 0xff, 0xfe
	};
	const uint8_t copy_from_bank_edge[] = {
		0x02, 0x00, 0x04, 0x00, 0x01, 0xff, 0xfe,
		0x00, 0x00, 0x07, 0x10
	};
	const uint8_t fill_last_bank[] = {
		0x00, 0x00, 0x04, 0x00, 0x0f, 0xff, 0xfe, 0x5a
	};
	const uint8_t invalid_source_bank[] = {
		0x01, 0x00, 0x02, 0x00, 0x10, 0x00, 0x00,
		0x00, 0x00, 0x07, 0x20
	};
	const uint8_t unknown_command[] = {
		0x7f, 0x00, 0x01, 0x00, 0x00, 0x07, 0x00
	};
	const uint8_t left_before[] = {1, 2, 3, 4, 5};
	const uint8_t left_after[] = {2, 3, 4, 5, 5};
	const uint8_t right_before[] = {1, 2, 3, 4, 5};
	const uint8_t right_after[] = {1, 1, 2, 3, 4};
	const uint8_t bank_source[] = {0xa1, 0xa2, 0xa3, 0xa4};
	const char diagnostic[] =
		"Varvara: unknown System/expansion command 7f\n";
	char actual[sizeof(diagnostic)];

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));

	memcpy(&varvara.uxn.ram[0x0500], left_before, sizeof(left_before));
	run_expansion_command(&varvara, 0x0200, copy_left,
		sizeof(copy_left));
	CHECK(memcmp(&varvara.uxn.ram[0x0500], left_after,
		sizeof(left_after)) == 0);

	memcpy(&varvara.uxn.ram[0x0600], right_before, sizeof(right_before));
	run_expansion_command(&varvara, 0x0200, copy_right,
		sizeof(copy_right));
	CHECK(memcmp(&varvara.uxn.ram[0x0600], right_after,
		sizeof(right_after)) == 0);

	memcpy(&varvara.uxn.ram[0x0700], bank_source, sizeof(bank_source));
	varvara.uxn.ram[UXN_RAM_SIZE + 0xfffd] = 0x19;
	varvara.uxn.ram[2u * UXN_RAM_SIZE] = 0x29;
	run_expansion_command(&varvara, 0x0200, copy_to_bank_edge,
		sizeof(copy_to_bank_edge));
	CHECK(varvara.uxn.ram[UXN_RAM_SIZE + 0xfffd] == 0x19);
	CHECK(varvara.uxn.ram[UXN_RAM_SIZE + 0xfffe] == 0xa1);
	CHECK(varvara.uxn.ram[UXN_RAM_SIZE + 0xffff] == 0xa2);
	CHECK(varvara.uxn.ram[2u * UXN_RAM_SIZE] == 0x29);

	varvara.uxn.ram[UXN_RAM_SIZE + 0xfffe] = 0xb1;
	varvara.uxn.ram[UXN_RAM_SIZE + 0xffff] = 0xb2;
	memset(&varvara.uxn.ram[0x0710], 0xee, 4);
	run_expansion_command(&varvara, 0x0200, copy_from_bank_edge,
		sizeof(copy_from_bank_edge));
	CHECK(varvara.uxn.ram[0x0710] == 0xb1);
	CHECK(varvara.uxn.ram[0x0711] == 0xb2);
	CHECK(varvara.uxn.ram[0x0712] == 0xee);
	CHECK(varvara.uxn.ram[0x0713] == 0xee);

	varvara.uxn.ram[15u * UXN_RAM_SIZE + 0xfffd] = 0x39;
	varvara.uxn.ram[0] = 0x49;
	run_expansion_command(&varvara, 0x0200, fill_last_bank,
		sizeof(fill_last_bank));
	CHECK(varvara.uxn.ram[15u * UXN_RAM_SIZE + 0xfffd] == 0x39);
	CHECK(varvara.uxn.ram[15u * UXN_RAM_SIZE + 0xfffe] == 0x5a);
	CHECK(varvara.uxn.ram[15u * UXN_RAM_SIZE + 0xffff] == 0x5a);
	CHECK(varvara.uxn.ram[0] == 0x49);

	varvara.uxn.ram[0x0720] = 0xc1;
	varvara.uxn.ram[0x0721] = 0xc2;
	run_expansion_command(&varvara, 0x0200, invalid_source_bank,
		sizeof(invalid_source_bank));
	CHECK(varvara.uxn.ram[0x0720] == 0xc1);
	CHECK(varvara.uxn.ram[0x0721] == 0xc2);

	run_expansion_command(&varvara, 0x0200, unknown_command,
		sizeof(unknown_command));
	rewind(errors);
	CHECK(fread(actual, 1, sizeof(diagnostic) - 1u, errors) ==
		sizeof(diagnostic) - 1u);
	CHECK(memcmp(actual, diagnostic, sizeof(diagnostic) - 1u) == 0);
	CHECK(fgetc(errors) == EOF);

	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_system_registers(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	const char diagnostic[] =
		"WST 00 00 00 00 00 00|aa bb  <02\n"
		"RST 00 00 00 00 00 00 00|cc  <01\n";
	char actual[sizeof(diagnostic)];

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	host_write_byte(&varvara, 0x04, 0x02);
	host_write_byte(&varvara, 0x05, 0x01);
	CHECK(varvara.uxn.working.pointer == 0x02);
	CHECK(varvara.uxn.return_stack.pointer == 0x01);
	varvara.uxn.devices[0x04] = 0xf4;
	varvara.uxn.devices[0x05] = 0xf5;
	CHECK(varvara.uxn.device_read(&varvara.uxn, 0x04, &varvara) == 0x02);
	CHECK(varvara.uxn.device_read(&varvara.uxn, 0x05, &varvara) == 0x01);
	varvara.uxn.working.data[0] = 0xaa;
	varvara.uxn.working.data[1] = 0xbb;
	varvara.uxn.return_stack.data[0] = 0xcc;
	host_write_byte(&varvara, 0x0e, 0x01);
	rewind(errors);
	CHECK(fread(actual, 1, sizeof(diagnostic) - 1u, errors) ==
		sizeof(diagnostic) - 1u);
	CHECK(memcmp(actual, diagnostic, sizeof(diagnostic) - 1u) == 0);
	CHECK(fgetc(errors) == EOF);

	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_screen(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	uint32_t pixels[12];
	const uint8_t program[] = {
		0xa0, 0x00, 0x04, 0x80, 0x22, 0x37, /* width = 4 */
		0xa0, 0x00, 0x03, 0x80, 0x24, 0x37, /* height = 3 */
		0xa0, 0x0f, 0x00, 0x80, 0x08, 0x37, /* color 1 = red */
		0xa0, 0x00, 0x01, 0x80, 0x28, 0x37, /* x = 1 */
		0xa0, 0x00, 0x01, 0x80, 0x2a, 0x37, /* y = 1 */
		0x80, 0x01, 0x80, 0x2e, 0x17,       /* draw color 1 */
		0x00
	};

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	CHECK(uxn_load(&varvara.uxn, program, sizeof(program)));
	CHECK(varvara_start(&varvara, 100) == UXN_STOP_BREAK);
	CHECK(varvara.screen.width == 4);
	CHECK(varvara.screen.height == 3);
	CHECK(varvara.screen.background[5] == 1);
	CHECK(varvara.screen.palette[1] == 0xffff0000u);
	CHECK(varvara_screen_compose(&varvara, pixels, 12));
	CHECK(pixels[5] == 0xffff0000u);
	CHECK(varvara.screen.dirty);
	CHECK(varvara.screen.resized);
	varvara_screen_presented(&varvara);
	CHECK(!varvara.screen.dirty);
	CHECK(!varvara.screen.resized);
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_sprite(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	const uint8_t program[] = {
		0xa0, 0x00, 0x08, 0x80, 0x22, 0x37, /* width = 8 */
		0xa0, 0x00, 0x08, 0x80, 0x24, 0x37, /* height = 8 */
		0xa0, 0x02, 0x00, 0x80, 0x2c, 0x37, /* sprite address */
		0x80, 0x41, 0x80, 0x2f, 0x17,       /* foreground sprite */
		0x00
	};

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	varvara.uxn.ram[0x0200] = 0x80;
	CHECK(uxn_load(&varvara.uxn, program, sizeof(program)));
	CHECK(varvara_start(&varvara, 100) == UXN_STOP_BREAK);
	CHECK(varvara.screen.foreground[0] == 1);
	CHECK(varvara.screen.foreground[1] == 0);
	CHECK(varvara.screen.foreground[8] == 0);
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_screen_auto(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	host_write_short(&varvara, 0x28, 4);
	host_write_short(&varvara, 0x2a, 5);
	host_write_byte(&varvara, 0x26, 0x03);
	host_write_byte(&varvara, 0x2e, 0x81);
	CHECK(varvara.screen.x == 4);
	CHECK(varvara.screen.y == 5);
	host_write_byte(&varvara, 0x2e, 0x01);
	CHECK(varvara.screen.x == 5);
	CHECK(varvara.screen.y == 6);
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_input_state(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	CHECK(varvara_controller_down(&varvara, 0x50, 100) == UXN_STOP_BREAK);
	CHECK(varvara.uxn.devices[0x82] == 0x50);
	CHECK(varvara_controller_up(&varvara, 0x10, 100) == UXN_STOP_BREAK);
	CHECK(varvara.uxn.devices[0x82] == 0x40);
	CHECK(varvara_controller_key(&varvara, 'a', 100) == UXN_STOP_BREAK);
	CHECK(varvara.uxn.devices[0x83] == 0);
	CHECK(varvara_mouse_move(&varvara, 123, 456, 100) == UXN_STOP_BREAK);
	CHECK(varvara.uxn.devices[0x92] == 0x00);
	CHECK(varvara.uxn.devices[0x93] == 123);
	CHECK(varvara.uxn.devices[0x94] == 0x01);
	CHECK(varvara.uxn.devices[0x95] == 0xc8);
	CHECK(varvara_mouse_down(&varvara, 0x01, 100) == UXN_STOP_BREAK);
	CHECK(varvara.uxn.devices[0x96] == 0x01);
	CHECK(varvara_mouse_up(&varvara, 0x01, 100) == UXN_STOP_BREAK);
	CHECK(varvara.uxn.devices[0x96] == 0x00);
	CHECK(varvara_mouse_scroll(&varvara, -1, 2, 100) == UXN_STOP_BREAK);
	CHECK(varvara.uxn.devices[0x9a] == 0);
	CHECK(varvara.uxn.devices[0x9b] == 0);
	CHECK(varvara.uxn.devices[0x9c] == 0);
	CHECK(varvara.uxn.devices[0x9d] == 0);
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_file(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	FILE *written;
	char contents[6] = {0};
	const char *path = "build/varvara-file-test.txt";
	const char *directory = "build/varvara-file-directory/";
	const char *absolute = "/tmp/uxn-emulator-unrestricted-file-test.txt";
	struct stat directory_info;

	CHECK(output != NULL);
	CHECK(errors != NULL);
	(void)remove(path);
	(void)rmdir(directory);
	CHECK(varvara_init(&varvara, output, errors));
	memcpy(&varvara.uxn.ram[0x0300], path, strlen(path) + 1u);
	memcpy(&varvara.uxn.ram[0x0400], "hello", 5);
	host_write_short(&varvara, 0xa8, 0x0300);
	host_write_short(&varvara, 0xaa, 5);
	host_write_short(&varvara, 0xae, 0x0400);
	CHECK(varvara.uxn.devices[0xa2] == 0);
	CHECK(varvara.uxn.devices[0xa3] == 5);
	written = fopen(path, "rb");
	CHECK(written != NULL);
	CHECK(fread(contents, 1, 5, written) == 5);
	CHECK(memcmp(contents, "hello", 5) == 0);
	fclose(written);

	host_write_short(&varvara, 0xa8, 0x0300);
	host_write_short(&varvara, 0xaa, 5);
	host_write_short(&varvara, 0xac, 0x0500);
	CHECK(varvara.uxn.devices[0xa2] == 0);
	CHECK(varvara.uxn.devices[0xa3] == 5);
	CHECK(memcmp(&varvara.uxn.ram[0x0500], "hello", 5) == 0);

	host_write_short(&varvara, 0xaa, 4);
	host_write_short(&varvara, 0xa4, 0x0600);
	CHECK(varvara.uxn.devices[0xa3] == 4);
	CHECK(memcmp(&varvara.uxn.ram[0x0600], "0005", 4) == 0);
	host_write_byte(&varvara, 0xa6, 1);
	CHECK(varvara.uxn.devices[0xa3] == 1);
	CHECK(fopen(path, "rb") == NULL);

	memcpy(&varvara.uxn.ram[0x0300], "../outside.txt", 15);
	host_write_short(&varvara, 0xa8, 0x0300);
	host_write_short(&varvara, 0xaa, 5);
	host_write_short(&varvara, 0xae, 0x0400);
	CHECK(varvara.uxn.devices[0xa2] == 0);
	CHECK(varvara.uxn.devices[0xa3] == 0);

	memcpy(&varvara.uxn.ram[0x0300], directory, strlen(directory) + 1u);
	host_write_short(&varvara, 0xa8, 0x0300);
	host_write_short(&varvara, 0xaa, 0);
	host_write_short(&varvara, 0xae, 0x0400);
	CHECK(varvara.uxn.devices[0xa2] == 0);
	CHECK(varvara.uxn.devices[0xa3] == 1);
	CHECK(stat(directory, &directory_info) == 0);
	CHECK(S_ISDIR(directory_info.st_mode));
	CHECK(rmdir(directory) == 0);

	(void)remove(absolute);
	varvara_files_set_sandbox(&varvara, false);
	memcpy(&varvara.uxn.ram[0x0300], absolute, strlen(absolute) + 1u);
	host_write_short(&varvara, 0xa8, 0x0300);
	host_write_short(&varvara, 0xaa, 5);
	host_write_short(&varvara, 0xae, 0x0400);
	CHECK(varvara.uxn.devices[0xa2] == 0);
	CHECK(varvara.uxn.devices[0xa3] == 5);
	CHECK(remove(absolute) == 0);
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_file1_independence(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	FILE *file;
	char contents[7] = {0};
	const char *path0 = "build/varvara-file0-independent.txt";
	const char *path1 = "build/varvara-file1-independent.txt";

	CHECK(output != NULL);
	CHECK(errors != NULL);
	(void)remove(path0);
	(void)remove(path1);
	CHECK(varvara_init(&varvara, output, errors));
	memcpy(&varvara.uxn.ram[0x0300], path0, strlen(path0) + 1u);
	memcpy(&varvara.uxn.ram[0x0340], path1, strlen(path1) + 1u);
	memcpy(&varvara.uxn.ram[0x0400], "abcdef", 6);
	memcpy(&varvara.uxn.ram[0x0500], "XYZ123", 6);

	/* Keep both write streams open while alternating between them. */
	host_write_short(&varvara, 0xa8, 0x0300);
	host_write_short(&varvara, 0xb8, 0x0340);
	host_write_short(&varvara, 0xaa, 3);
	host_write_short(&varvara, 0xba, 3);
	host_write_short(&varvara, 0xae, 0x0400);
	CHECK(varvara.uxn.devices[0xa2] == 0);
	CHECK(varvara.uxn.devices[0xa3] == 3);
	host_write_short(&varvara, 0xbe, 0x0500);
	CHECK(varvara.uxn.devices[0xb2] == 0);
	CHECK(varvara.uxn.devices[0xb3] == 3);
	host_write_short(&varvara, 0xae, 0x0403);
	CHECK(varvara.uxn.devices[0xa2] == 0);
	CHECK(varvara.uxn.devices[0xa3] == 3);
	host_write_short(&varvara, 0xbe, 0x0503);
	CHECK(varvara.uxn.devices[0xb2] == 0);
	CHECK(varvara.uxn.devices[0xb3] == 3);

	file = fopen(path0, "rb");
	CHECK(file != NULL);
	CHECK(fread(contents, 1, 6, file) == 6);
	CHECK(memcmp(contents, "abcdef", 6) == 0);
	CHECK(fgetc(file) == EOF);
	fclose(file);
	memset(contents, 0, sizeof(contents));
	file = fopen(path1, "rb");
	CHECK(file != NULL);
	CHECK(fread(contents, 1, 6, file) == 6);
	CHECK(memcmp(contents, "XYZ123", 6) == 0);
	CHECK(fgetc(file) == EOF);
	fclose(file);

	/* Resetting File0's name must not reset File1's read position. */
	host_write_short(&varvara, 0xa8, 0x0300);
	host_write_short(&varvara, 0xb8, 0x0340);
	host_write_short(&varvara, 0xaa, 2);
	host_write_short(&varvara, 0xba, 2);
	host_write_short(&varvara, 0xbc, 0x0600);
	CHECK(varvara.uxn.devices[0xb3] == 2);
	CHECK(memcmp(&varvara.uxn.ram[0x0600], "XY", 2) == 0);
	host_write_short(&varvara, 0xac, 0x0700);
	CHECK(varvara.uxn.devices[0xa3] == 2);
	CHECK(memcmp(&varvara.uxn.ram[0x0700], "ab", 2) == 0);
	host_write_short(&varvara, 0xa8, 0x0300);

	/* File1 stat must report exactly six bytes without resetting its stream. */
	host_write_short(&varvara, 0xba, 4);
	host_write_short(&varvara, 0xb4, 0x0800);
	CHECK(varvara.uxn.devices[0xb2] == 0);
	CHECK(varvara.uxn.devices[0xb3] == 4);
	CHECK(memcmp(&varvara.uxn.ram[0x0800], "0006", 4) == 0);
	host_write_short(&varvara, 0xba, 2);
	host_write_short(&varvara, 0xbc, 0x0602);
	CHECK(varvara.uxn.devices[0xb3] == 2);
	CHECK(memcmp(&varvara.uxn.ram[0x0602], "Z1", 2) == 0);

	/* File1 activity must likewise leave File0's reset stream independent. */
	host_write_short(&varvara, 0xac, 0x0710);
	CHECK(varvara.uxn.devices[0xa3] == 2);
	CHECK(memcmp(&varvara.uxn.ram[0x0710], "ab", 2) == 0);
	host_write_short(&varvara, 0xbc, 0x0604);
	CHECK(varvara.uxn.devices[0xb3] == 2);
	CHECK(memcmp(&varvara.uxn.ram[0x0604], "23", 2) == 0);
	host_write_short(&varvara, 0xac, 0x0712);
	CHECK(varvara.uxn.devices[0xa3] == 2);
	CHECK(memcmp(&varvara.uxn.ram[0x0712], "cd", 2) == 0);
	CHECK(memcmp(&varvara.uxn.ram[0x0600], "XYZ123", 6) == 0);
	CHECK(memcmp(&varvara.uxn.ram[0x0710], "abcd", 4) == 0);

	varvara_destroy(&varvara);
	CHECK(remove(path0) == 0);
	CHECK(remove(path1) == 0);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_datetime(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	uint16_t year;
	uint8_t high;

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	high = varvara.uxn.device_read(&varvara.uxn, 0xc0, &varvara);
	year = (uint16_t)((high << 8) | varvara.uxn.devices[0xc1]);
	CHECK(year >= 2020 && year < 2200);
	CHECK(varvara.uxn.device_read(&varvara.uxn, 0xc2, &varvara) <= 11);
	CHECK(varvara.uxn.device_read(&varvara.uxn, 0xc3, &varvara) >= 1);
	CHECK(varvara.uxn.device_read(&varvara.uxn, 0xc3, &varvara) <= 31);
	CHECK(varvara.uxn.device_read(&varvara.uxn, 0xc4, &varvara) <= 23);
	CHECK(varvara.uxn.device_read(&varvara.uxn, 0xc5, &varvara) <= 59);
	CHECK(varvara.uxn.device_read(&varvara.uxn, 0xc6, &varvara) <= 60);
	CHECK(varvara.uxn.device_read(&varvara.uxn, 0xc7, &varvara) <= 6);
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_audio(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	int16_t samples[512 * 2];
	uint8_t finished;
	size_t i;
	bool heard_sample = false;

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	varvara.uxn.ram[0x0700] = 0xff;
	varvara.uxn.ram[0x0701] = 0x00;
	host_write_short(&varvara, 0x38, 0x0000);
	host_write_short(&varvara, 0x3a, 2);
	host_write_short(&varvara, 0x3c, 0x0700);
	host_write_byte(&varvara, 0x3e, 0xff);
	host_write_byte(&varvara, 0x3f, 0xbc);
	CHECK(varvara_audio_active(&varvara));
	finished = varvara_audio_render(&varvara, samples, 512);
	for(i = 0; i < 512 * 2; i++)
		if(samples[i] != 0) heard_sample = true;
	CHECK(heard_sample);
	/* These points match the current uxn2 reference renderer at 44.1 kHz. */
	CHECK(samples[0] == 10834);
	CHECK(samples[1] == 10834);
	CHECK(samples[83 * 2] == 10834);
	CHECK(samples[84 * 2] == -10920);
	CHECK(samples[167 * 2] == -10920);
	CHECK(samples[168 * 2] == 0);
	CHECK((finished & 0x01) != 0);
	CHECK(!varvara_audio_active(&varvara));
	CHECK(varvara.uxn.device_read(&varvara.uxn, 0x32, &varvara) == 0);
	CHECK(varvara.uxn.devices[0x33] == 2);
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
start_test_voice(Varvara *varvara, unsigned int voice, uint16_t address,
	uint8_t volume)
{
	uint8_t base = (uint8_t)(0x30 + voice * 0x10);
	host_write_short(varvara, (uint8_t)(base + 0x08), 0x0000);
	host_write_short(varvara, (uint8_t)(base + 0x0a), 2);
	host_write_short(varvara, (uint8_t)(base + 0x0c), address);
	host_write_byte(varvara, (uint8_t)(base + 0x0e), volume);
	host_write_byte(varvara, (uint8_t)(base + 0x0f), 0xbc);
}

static void
test_varvara_audio_four_voices(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	int16_t samples[169 * 2];
	unsigned int voice;

	CHECK(output != NULL);
	CHECK(errors != NULL);
	for(voice = 0; voice < 4; voice++) {
		uint8_t base = (uint8_t)(0x30 + voice * 0x10);
		uint8_t finished;
		unsigned int other;
		CHECK(varvara_init(&varvara, output, errors));
		varvara.uxn.ram[0x0700] = 0xff;
		varvara.uxn.ram[0x0701] = 0x00;
		start_test_voice(&varvara, voice, 0x0700, 0xff);
		for(other = 0; other < 4; other++)
			CHECK(varvara.voices[other].active == (other == voice));
		finished = varvara_audio_render(&varvara, samples, 169);
		CHECK(samples[0] == 10834);
		CHECK(samples[1] == 10834);
		CHECK(samples[83 * 2] == 10834);
		CHECK(samples[83 * 2 + 1] == 10834);
		CHECK(samples[84 * 2] == -10920);
		CHECK(samples[84 * 2 + 1] == -10920);
		CHECK(samples[167 * 2] == -10920);
		CHECK(samples[167 * 2 + 1] == -10920);
		CHECK(samples[168 * 2] == 0);
		CHECK(samples[168 * 2 + 1] == 0);
		CHECK(finished == (uint8_t)(1u << voice));
		CHECK(!varvara_audio_active(&varvara));
		CHECK(varvara.uxn.device_read(&varvara.uxn,
			(uint8_t)(base + 0x02), &varvara) == 0);
		CHECK(varvara.uxn.devices[base + 0x03] == 2);
		varvara_destroy(&varvara);
	}
	fclose(output);
	fclose(errors);
}

static void
test_varvara_audio_mix_and_pan(void)
{
	Varvara varvara;
	VarvaraAudioVoice voice0;
	VarvaraAudioVoice voice1;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	int16_t samples[169 * 2];
	uint8_t audio0_ports[16];
	uint8_t audio1_ports[16];
	uint8_t finished;

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	varvara.uxn.ram[0x0700] = 0xff;
	varvara.uxn.ram[0x0701] = 0x00;
	varvara.uxn.ram[0x0710] = 0x00;
	varvara.uxn.ram[0x0711] = 0xff;

	start_test_voice(&varvara, 0, 0x0700, 0xf0);
	voice0 = varvara.voices[0];
	memcpy(audio0_ports, &varvara.uxn.devices[0x30], sizeof(audio0_ports));
	start_test_voice(&varvara, 1, 0x0700, 0xf0);
	CHECK(memcmp(&varvara.voices[0], &voice0, sizeof(voice0)) == 0);
	CHECK(memcmp(&varvara.uxn.devices[0x30], audio0_ports,
		sizeof(audio0_ports)) == 0);
	voice1 = varvara.voices[1];
	memcpy(audio1_ports, &varvara.uxn.devices[0x40], sizeof(audio1_ports));
	start_test_voice(&varvara, 2, 0x0710, 0x0f);
	CHECK(memcmp(&varvara.voices[0], &voice0, sizeof(voice0)) == 0);
	CHECK(memcmp(&varvara.voices[1], &voice1, sizeof(voice1)) == 0);
	CHECK(memcmp(&varvara.uxn.devices[0x30], audio0_ports,
		sizeof(audio0_ports)) == 0);
	CHECK(memcmp(&varvara.uxn.devices[0x40], audio1_ports,
		sizeof(audio1_ports)) == 0);
	CHECK(varvara.voices[0].active);
	CHECK(varvara.voices[1].active);
	CHECK(varvara.voices[2].active);
	CHECK(!varvara.voices[3].active);

	finished = varvara_audio_render(&varvara, samples, 169);
	CHECK(samples[0] == 21668);
	CHECK(samples[1] == -10920);
	CHECK(samples[83 * 2] == 21668);
	CHECK(samples[83 * 2 + 1] == -10920);
	CHECK(samples[84 * 2] == -21840);
	CHECK(samples[84 * 2 + 1] == 10834);
	CHECK(samples[167 * 2] == -21840);
	CHECK(samples[167 * 2 + 1] == 10834);
	CHECK(samples[168 * 2] == 0);
	CHECK(samples[168 * 2 + 1] == 0);
	CHECK(finished == 0x07);
	CHECK(!varvara_audio_active(&varvara));
	CHECK(varvara.voices[0].index == 2);
	CHECK(varvara.voices[1].index == 2);
	CHECK(varvara.voices[2].index == 2);
	CHECK(varvara.voices[3].index == 0);

	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_varvara_reboot(void)
{
	Varvara varvara;
	FILE *output = tmpfile();
	FILE *errors = tmpfile();

	CHECK(output != NULL);
	CHECK(errors != NULL);
	CHECK(varvara_init(&varvara, output, errors));
	varvara.uxn.ram[0x0080] = 0x11;
	varvara.uxn.ram[0x0200] = 0x22;
	varvara.uxn.ram[UXN_RAM_SIZE + 0x0200] = 0x33;
	varvara.uxn.devices[0x10] = 0x44;
	varvara.uxn.working.data[0] = 0x55;
	varvara.uxn.working.pointer = 1;
	CHECK(varvara_reboot(&varvara, true));
	CHECK(varvara.uxn.ram[0x0080] == 0x11);
	CHECK(varvara.uxn.ram[0x0200] == 0);
	CHECK(varvara.uxn.ram[UXN_RAM_SIZE + 0x0200] == 0x33);
	CHECK(varvara.uxn.devices[0x10] == 0);
	CHECK(varvara.uxn.working.pointer == 0);
	CHECK(varvara.screen.width == VARVARA_SCREEN_DEFAULT_WIDTH);
	CHECK(varvara.screen.height == VARVARA_SCREEN_DEFAULT_HEIGHT);
	varvara.uxn.ram[0x0080] = 0x66;
	CHECK(varvara_reboot(&varvara, false));
	CHECK(varvara.uxn.ram[0x0080] == 0);
	CHECK(varvara.uxn.ram[UXN_RAM_SIZE + 0x0200] == 0x33);
	varvara_destroy(&varvara);
	fclose(output);
	fclose(errors);
}

static void
test_rom_size_guard(void)
{
	Uxn uxn;
	uint8_t byte = 0;
	uxn_init(&uxn);
	CHECK(!uxn_load(&uxn, &byte, UXN_MEMORY_SIZE - UXN_ROM_START + 1u));
}

static void
test_rom_file_bank_boundary(void)
{
	const char *path = "build/rom-bank-boundary-test.rom";
	const size_t bank_zero_length = UXN_RAM_SIZE - UXN_ROM_START;
	uint8_t *bytes = calloc(bank_zero_length + 1u, 1);
	char error[256];
	Uxn uxn;

	CHECK(bytes != NULL);
	bytes[0] = 0xa5;
	bytes[bank_zero_length - 1u] = 0x5a;
	write_rom_file(path, bytes, bank_zero_length);
	uxn_init(&uxn);
	uxn.ram[UXN_RAM_SIZE] = 0xc3;
	CHECK(rom_load_file(&uxn, path, error, sizeof(error)));
	CHECK(error[0] == '\0');
	CHECK(uxn.ram[UXN_ROM_START] == 0xa5);
	CHECK(uxn.ram[UXN_RAM_SIZE - 1u] == 0x5a);
	CHECK(uxn.ram[UXN_RAM_SIZE] == 0xc3);

	bytes[bank_zero_length] = 0x7e;
	write_rom_file(path, bytes, bank_zero_length + 1u);
	uxn_init(&uxn);
	CHECK(rom_load_file(&uxn, path, error, sizeof(error)));
	CHECK(error[0] == '\0');
	CHECK(uxn.ram[UXN_RAM_SIZE - 1u] == 0x5a);
	CHECK(uxn.ram[UXN_RAM_SIZE] == 0x7e);
	CHECK(remove(path) == 0);
	free(bytes);
}

static void
test_banked_rom_execution(void)
{
	const char *path = "build/banked-rom-test.rom";
	const size_t bank_zero_length = UXN_RAM_SIZE - UXN_ROM_START;
	const uint8_t program[] = {
		/* Ask System/expansion to run the command at 0200. */
		0xa0, 0x02, 0x00, 0x80, 0x02, 0x37,
		/* Read the copied bytes from bank zero and print them. */
		0xa0, 0x03, 0x00, 0x14, 0x80, 0x18, 0x17,
		0xa0, 0x03, 0x01, 0x14, 0x80, 0x18, 0x17,
		0xa0, 0x03, 0x02, 0x14, 0x80, 0x18, 0x17,
		0xa0, 0x03, 0x03, 0x14, 0x80, 0x18, 0x17,
		0x00
	};
	const uint8_t command[] = {
		0x01,             /* copy */
		0x00, 0x04,       /* four bytes */
		0x00, 0x01,       /* from bank one */
		0x00, 0x00,       /* at address 0000 */
		0x00, 0x00,       /* to bank zero */
		0x03, 0x00        /* at address 0300 */
	};
	const uint8_t expected[] = {'B', 'A', 'N', 'K'};
	uint8_t actual[sizeof(expected)];
	uint8_t *rom = calloc(bank_zero_length + sizeof(expected), 1);
	FILE *output = tmpfile();
	FILE *errors = tmpfile();
	char error[256];
	Varvara varvara;

	CHECK(rom != NULL);
	CHECK(output != NULL);
	CHECK(errors != NULL);
	memcpy(rom, program, sizeof(program));
	memcpy(&rom[0x0200 - UXN_ROM_START], command, sizeof(command));
	rom[bank_zero_length - 1u] = 0x6d;
	memcpy(&rom[bank_zero_length], expected, sizeof(expected));
	write_rom_file(path, rom, bank_zero_length + sizeof(expected));

	CHECK(varvara_init(&varvara, output, errors));
	CHECK(rom_load_file(&varvara.uxn, path, error, sizeof(error)));
	CHECK(error[0] == '\0');
	CHECK(varvara.uxn.ram[UXN_RAM_SIZE - 1u] == 0x6d);
	CHECK(memcmp(&varvara.uxn.ram[UXN_RAM_SIZE], expected,
		sizeof(expected)) == 0);
	CHECK(varvara_start(&varvara, 100) == UXN_STOP_BREAK);
	CHECK(memcmp(&varvara.uxn.ram[0x0300], expected, sizeof(expected)) == 0);
	rewind(output);
	CHECK(fread(actual, 1, sizeof(actual), output) == sizeof(actual));
	CHECK(memcmp(actual, expected, sizeof(expected)) == 0);
	CHECK(fgetc(output) == EOF);

	varvara_destroy(&varvara);
	CHECK(remove(path) == 0);
	free(rom);
	fclose(output);
	fclose(errors);
}

int
main(void)
{
	test_literals_and_math();
	test_stack_modes();
	test_memory_addressing();
	test_devices();
	test_control_flow();
	test_instruction_limit();
	test_varvara_console_and_state();
	test_varvara_console_exec();
	test_varvara_expansion();
	test_varvara_expansion_modes();
	test_varvara_system_registers();
	test_varvara_screen();
	test_varvara_sprite();
	test_varvara_screen_auto();
	test_varvara_input_state();
	test_varvara_file();
	test_varvara_file1_independence();
	test_varvara_datetime();
	test_varvara_audio();
	test_varvara_audio_four_voices();
	test_varvara_audio_mix_and_pan();
	test_varvara_reboot();
	test_rom_size_guard();
	test_rom_file_bank_boundary();
	test_banked_rom_execution();
	printf("ok - %u checks\n", tests_run);
	return 0;
}
