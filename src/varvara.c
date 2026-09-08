#include "varvara_internal.h"

#include <string.h>

enum {
	SYSTEM_EXPANSION = 0x02,
	SYSTEM_WST = 0x04,
	SYSTEM_RST = 0x05,
	SYSTEM_DEBUG = 0x0e,
	SYSTEM_STATE = 0x0f,
	CONSOLE_VECTOR = 0x10,
	CONSOLE_READ = 0x12,
	CONSOLE_TYPE = 0x17,
	CONSOLE_WRITE = 0x18,
	CONSOLE_ERROR = 0x19,
	CONSOLE_DEBUG_BYTE = 0x1a,
	CONSOLE_DEBUG_SHORT = 0x1b,
	CONSOLE_EXEC = 0x1f,
	SCREEN_FIRST = 0x20,
	SCREEN_LAST = 0x2f,
	SYSTEM_RED_LOW = 0x09,
	SYSTEM_GREEN_LOW = 0x0b,
	SYSTEM_BLUE_LOW = 0x0d,
	CONTROLLER_VECTOR = 0x80,
	CONTROLLER_BUTTON = 0x82,
	CONTROLLER_KEY = 0x83,
	MOUSE_VECTOR = 0x90,
	MOUSE_X = 0x92,
	MOUSE_Y = 0x94,
	MOUSE_STATE = 0x96,
	MOUSE_SCROLL_X = 0x9a,
	MOUSE_SCROLL_Y = 0x9c,
	FILE_FIRST = 0xa0,
	FILE_LAST = 0xbf,
	DATETIME_FIRST = 0xc0,
	DATETIME_LAST = 0xcf,
	AUDIO_FIRST = 0x30,
	AUDIO_LAST = 0x6f
};

static uint16_t
device_short(const Uxn *uxn, uint8_t port)
{
	return (uint16_t)((uxn->devices[port] << 8) |
		uxn->devices[(uint8_t)(port + 1u)]);
}

static void
print_stack(FILE *stream, const char *name, const UxnStack *stack)
{
	uint8_t i;
	fprintf(stream, "%s%c", name, stack->pointer == 8 ? '|' : ' ');
	for(i = (uint8_t)(stack->pointer - 8u); i != stack->pointer; i++)
		fprintf(stream, "%02x%c", stack->data[i], i == 0xff ? '|' : ' ');
	fprintf(stream, " <%02x\n", stack->pointer);
}

static void
system_expansion(Varvara *varvara)
{
	Uxn *uxn = &varvara->uxn;
	uint16_t command_address = device_short(uxn, SYSTEM_EXPANSION);
	uint8_t *command = &uxn->ram[command_address];
	uint16_t length = (uint16_t)((command[1] << 8) | command[2]);
	uint16_t source_bank = (uint16_t)((command[3] << 8) | command[4]);
	uint16_t source_address = (uint16_t)((command[5] << 8) | command[6]);
	size_t source_offset = (size_t)source_bank * UXN_RAM_SIZE + source_address;

	if(length > UXN_RAM_SIZE - source_address)
		length = (uint16_t)(UXN_RAM_SIZE - source_address);

	if(command[0] == 0x00) {
		if(source_bank < UXN_BANK_COUNT)
			memset(&uxn->ram[source_offset], command[7], length);
	} else if(command[0] == 0x01 || command[0] == 0x02) {
		uint16_t destination_bank =
			(uint16_t)((command[7] << 8) | command[8]);
		uint16_t destination_address =
			(uint16_t)((command[9] << 8) | command[10]);
		size_t destination_offset =
			(size_t)destination_bank * UXN_RAM_SIZE + destination_address;

		if(length > UXN_RAM_SIZE - destination_address)
			length = (uint16_t)(UXN_RAM_SIZE - destination_address);
		if(source_bank < UXN_BANK_COUNT &&
			destination_bank < UXN_BANK_COUNT)
			memmove(&uxn->ram[destination_offset],
				&uxn->ram[source_offset], length);
	} else {
		fprintf(varvara->standard_error,
			"Varvara: unknown System/expansion command %02x\n", command[0]);
	}
}

static uint8_t
varvara_read(Uxn *uxn, uint8_t port, void *context)
{
	Varvara *varvara = context;
	if(port == SYSTEM_WST)
		return uxn->working.pointer;
	if(port == SYSTEM_RST)
		return uxn->return_stack.pointer;
	if(port == 0x15 || port == 0x16)
		return varvara_exec_read(varvara, port);
	if(port >= SCREEN_FIRST && port <= SCREEN_LAST)
		return varvara_screen_read(varvara, port);
	if(port >= AUDIO_FIRST && port <= AUDIO_LAST)
		return varvara_audio_read(varvara, port);
	if(port >= DATETIME_FIRST && port <= DATETIME_LAST)
		return varvara_datetime_read(varvara, port);
	return uxn->devices[port];
}

static void
varvara_write(Uxn *uxn, uint8_t port, uint8_t value, void *context)
{
	Varvara *varvara = context;

	if(port == SYSTEM_WST)
		uxn->working.pointer = value;
	else if(port == SYSTEM_RST)
		uxn->return_stack.pointer = value;
	else if(port == SYSTEM_EXPANSION + 1)
		system_expansion(varvara);
	else if(port == SYSTEM_RED_LOW || port == SYSTEM_GREEN_LOW ||
		port == SYSTEM_BLUE_LOW)
		varvara_screen_update_palette(varvara);
	else if(port == SYSTEM_DEBUG && (value & 0x01)) {
		print_stack(varvara->standard_error, "WST", &uxn->working);
		print_stack(varvara->standard_error, "RST", &uxn->return_stack);
	} else if(port == CONSOLE_WRITE) {
		if(!varvara_exec_write_byte(varvara, value)) {
			fputc(value, varvara->standard_output);
			fflush(varvara->standard_output);
		}
	} else if(port == CONSOLE_ERROR) {
		fputc(value, varvara->standard_error);
		fflush(varvara->standard_error);
	} else if(port == CONSOLE_DEBUG_BYTE) {
		fprintf(varvara->standard_error, "%02x", value);
		fflush(varvara->standard_error);
	} else if(port == CONSOLE_DEBUG_SHORT) {
		fprintf(varvara->standard_error, "%02x%02x",
			uxn->devices[CONSOLE_DEBUG_BYTE], value);
		fflush(varvara->standard_error);
	} else if(port == CONSOLE_EXEC) {
		varvara_exec_start(varvara);
	} else if(port >= SCREEN_FIRST && port <= SCREEN_LAST)
		varvara_screen_write(varvara, port);
	else if(port >= FILE_FIRST && port <= FILE_LAST)
		varvara_file_write_port(varvara, port);
	else if(port >= AUDIO_FIRST && port <= AUDIO_LAST)
		varvara_audio_write(varvara, port);
}

bool
varvara_init(Varvara *varvara, FILE *standard_output, FILE *standard_error)
{
	memset(varvara, 0, sizeof(*varvara));
	varvara->standard_output = standard_output;
	varvara->standard_error = standard_error;
	varvara_audio_set_sample_rate(varvara, 44100);
	uxn_connect_devices(&varvara->uxn, varvara_read, varvara_write, varvara);
	if(!varvara_files_init(varvara))
		return false;
	if(!varvara_screen_init(varvara)) {
		varvara_files_destroy(varvara);
		return false;
	}
	return true;
}

void
varvara_destroy(Varvara *varvara)
{
	varvara_exec_close(varvara);
	varvara_files_destroy(varvara);
	varvara_screen_destroy(varvara);
}

bool
varvara_reboot(Varvara *varvara, bool soft)
{
	size_t first = soft ? UXN_ROM_START : 0u;
	uint32_t sample_rate = varvara->sample_rate;

	varvara_exec_close(varvara);
	varvara_files_destroy(varvara);
	varvara_screen_destroy(varvara);
	memset(&varvara->uxn.ram[first], 0, UXN_RAM_SIZE - first);
	memset(varvara->uxn.devices, 0, sizeof(varvara->uxn.devices));
	memset(&varvara->uxn.working, 0, sizeof(varvara->uxn.working));
	memset(&varvara->uxn.return_stack, 0,
		sizeof(varvara->uxn.return_stack));
	memset(varvara->files, 0, sizeof(varvara->files));
	memset(varvara->voices, 0, sizeof(varvara->voices));
	memset(&varvara->screen, 0, sizeof(varvara->screen));
	varvara->uxn.instructions = 0;
	varvara_audio_set_sample_rate(varvara, sample_rate);
	return varvara_screen_init(varvara);
}

UxnStop
varvara_start(Varvara *varvara, uint64_t instruction_limit)
{
	return uxn_eval(&varvara->uxn, UXN_ROM_START, instruction_limit);
}

UxnStop
varvara_console_input(Varvara *varvara, uint8_t byte, uint8_t type,
	uint64_t instruction_limit)
{
	uint16_t vector;
	varvara->uxn.devices[CONSOLE_READ] = byte;
	varvara->uxn.devices[CONSOLE_TYPE] = type;
	vector = device_short(&varvara->uxn, CONSOLE_VECTOR);
	if(!vector)
		return UXN_STOP_BREAK;
	return uxn_eval(&varvara->uxn, vector, instruction_limit);
}

bool
varvara_has_console_vector(const Varvara *varvara)
{
	return device_short(&varvara->uxn, CONSOLE_VECTOR) != 0;
}

bool
varvara_is_halted(const Varvara *varvara)
{
	return varvara->uxn.devices[SYSTEM_STATE] != 0;
}

int
varvara_exit_code(const Varvara *varvara)
{
	return varvara->uxn.devices[SYSTEM_STATE] & 0x7f;
}

UxnStop
varvara_eval_port_vector(Varvara *varvara, uint8_t port,
	uint64_t instruction_limit)
{
	uint16_t vector = device_short(&varvara->uxn, port);
	if(!vector || varvara_is_halted(varvara))
		return UXN_STOP_BREAK;
	return uxn_eval(&varvara->uxn, vector, instruction_limit);
}

UxnStop
varvara_controller_down(Varvara *varvara, uint8_t buttons,
	uint64_t instruction_limit)
{
	if(!buttons)
		return UXN_STOP_BREAK;
	varvara->uxn.devices[CONTROLLER_BUTTON] |= buttons;
	return varvara_eval_port_vector(varvara, CONTROLLER_VECTOR,
		instruction_limit);
}

UxnStop
varvara_controller_up(Varvara *varvara, uint8_t buttons,
	uint64_t instruction_limit)
{
	if(!buttons)
		return UXN_STOP_BREAK;
	varvara->uxn.devices[CONTROLLER_BUTTON] &= (uint8_t)~buttons;
	return varvara_eval_port_vector(varvara, CONTROLLER_VECTOR,
		instruction_limit);
}

UxnStop
varvara_controller_key(Varvara *varvara, uint8_t key,
	uint64_t instruction_limit)
{
	UxnStop result;
	if(!key)
		return UXN_STOP_BREAK;
	varvara->uxn.devices[CONTROLLER_KEY] = key;
	result = varvara_eval_port_vector(varvara, CONTROLLER_VECTOR,
		instruction_limit);
	varvara->uxn.devices[CONTROLLER_KEY] = 0;
	return result;
}

UxnStop
varvara_mouse_move(Varvara *varvara, uint16_t x, uint16_t y,
	uint64_t instruction_limit)
{
	varvara_poke_short(&varvara->uxn.devices[MOUSE_X], x);
	varvara_poke_short(&varvara->uxn.devices[MOUSE_Y], y);
	return varvara_eval_port_vector(varvara, MOUSE_VECTOR, instruction_limit);
}

UxnStop
varvara_mouse_down(Varvara *varvara, uint8_t buttons,
	uint64_t instruction_limit)
{
	varvara->uxn.devices[MOUSE_STATE] |= buttons;
	return varvara_eval_port_vector(varvara, MOUSE_VECTOR, instruction_limit);
}

UxnStop
varvara_mouse_up(Varvara *varvara, uint8_t buttons,
	uint64_t instruction_limit)
{
	varvara->uxn.devices[MOUSE_STATE] &= (uint8_t)~buttons;
	return varvara_eval_port_vector(varvara, MOUSE_VECTOR, instruction_limit);
}

UxnStop
varvara_mouse_scroll(Varvara *varvara, int16_t x, int16_t y,
	uint64_t instruction_limit)
{
	UxnStop result;
	varvara_poke_short(&varvara->uxn.devices[MOUSE_SCROLL_X], (uint16_t)x);
	varvara_poke_short(&varvara->uxn.devices[MOUSE_SCROLL_Y], (uint16_t)y);
	result = varvara_eval_port_vector(varvara, MOUSE_VECTOR, instruction_limit);
	varvara_poke_short(&varvara->uxn.devices[MOUSE_SCROLL_X], 0);
	varvara_poke_short(&varvara->uxn.devices[MOUSE_SCROLL_Y], 0);
	return result;
}
