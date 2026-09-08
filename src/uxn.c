#include "uxn.h"

#include <string.h>

typedef struct {
	UxnStack *stack;
	uint8_t cursor;
	bool keep;
} StackReader;

static uint8_t
pop_byte(StackReader *reader)
{
	reader->cursor--;
	return reader->stack->data[reader->cursor];
}

static uint16_t
pop_short(StackReader *reader)
{
	uint16_t low = pop_byte(reader);
	uint16_t high = pop_byte(reader);
	return (uint16_t)((high << 8) | low);
}

static uint16_t
pop_value(StackReader *reader, bool short_mode)
{
	return short_mode ? pop_short(reader) : pop_byte(reader);
}

static void
finish_reading(StackReader *reader)
{
	if(!reader->keep)
		reader->stack->pointer = reader->cursor;
}

static void
push_byte(UxnStack *stack, uint8_t value)
{
	stack->data[stack->pointer] = value;
	stack->pointer++;
}

static void
push_short(UxnStack *stack, uint16_t value)
{
	push_byte(stack, (uint8_t)(value >> 8));
	push_byte(stack, (uint8_t)value);
}

static void
push_value(UxnStack *stack, uint16_t value, bool short_mode)
{
	if(short_mode)
		push_short(stack, value);
	else
		push_byte(stack, (uint8_t)value);
}

static uint8_t
read_device(Uxn *uxn, uint8_t port)
{
	if(uxn->device_read)
		return uxn->device_read(uxn, port, uxn->device_context);
	return uxn->devices[port];
}

static void
write_device(Uxn *uxn, uint8_t port, uint8_t value)
{
	uxn->devices[port] = value;
	if(uxn->device_write)
		uxn->device_write(uxn, port, value, uxn->device_context);
}

static uint8_t
read_memory(const Uxn *uxn, uint16_t address)
{
	return uxn->ram[address];
}

static uint16_t
read_memory_short(const Uxn *uxn, uint16_t address, uint16_t mask)
{
	uint8_t high = uxn->ram[address];
	uint8_t low = uxn->ram[(address + 1u) & mask];
	return (uint16_t)((high << 8) | low);
}

static void
write_memory(Uxn *uxn, uint16_t address, uint16_t value,
	bool short_mode, uint16_t mask)
{
	if(short_mode) {
		uxn->ram[address] = (uint8_t)(value >> 8);
		uxn->ram[(address + 1u) & mask] = (uint8_t)value;
	} else {
		uxn->ram[address] = (uint8_t)value;
	}
}

void
uxn_init(Uxn *uxn)
{
	memset(uxn, 0, sizeof(*uxn));
}

void
uxn_connect_devices(Uxn *uxn, UxnDeviceRead read,
	UxnDeviceWrite write, void *context)
{
	uxn->device_read = read;
	uxn->device_write = write;
	uxn->device_context = context;
}

bool
uxn_load(Uxn *uxn, const uint8_t *rom, size_t length)
{
	if(length > UXN_MEMORY_SIZE - UXN_ROM_START)
		return false;
	memcpy(&uxn->ram[UXN_ROM_START], rom, length);
	return true;
}

UxnStop
uxn_eval(Uxn *uxn, uint16_t pc, uint64_t instruction_limit)
{
	uint64_t first_instruction = uxn->instructions;

	for(;;) {
		uint8_t instruction;
		uint8_t operation;
		bool short_mode;
		bool return_mode;
		bool keep_mode;
		UxnStack *source;
		UxnStack *other;
		StackReader reader;
		uint16_t a;
		uint16_t b;
		uint16_t c;

		if(instruction_limit &&
			uxn->instructions - first_instruction >= instruction_limit)
			return UXN_STOP_LIMIT;

		instruction = uxn->ram[pc++];
		uxn->instructions++;

		/* Opcode zero's mode bits encode BRK and seven immediate instructions. */
		switch(instruction) {
		case 0x00: /* BRK */
			return UXN_STOP_BREAK;
		case 0x20: /* JCI */
			a = read_memory_short(uxn, pc, 0xffff);
			pc += 2;
			uxn->working.pointer--;
			if(uxn->working.data[uxn->working.pointer])
				pc = (uint16_t)(pc + a);
			break;
		case 0x40: /* JMI */
			a = read_memory_short(uxn, pc, 0xffff);
			pc += 2;
			pc = (uint16_t)(pc + a);
			break;
		case 0x60: /* JSI */
			a = read_memory_short(uxn, pc, 0xffff);
			pc += 2;
			push_short(&uxn->return_stack, pc);
			pc = (uint16_t)(pc + a);
			break;
		case 0x80: /* LIT */
			push_byte(&uxn->working, uxn->ram[pc++]);
			break;
		case 0xa0: /* LIT2 */
			push_short(&uxn->working,
				read_memory_short(uxn, pc, 0xffff));
			pc += 2;
			break;
		case 0xc0: /* LITr */
			push_byte(&uxn->return_stack, uxn->ram[pc++]);
			break;
		case 0xe0: /* LIT2r */
			push_short(&uxn->return_stack,
				read_memory_short(uxn, pc, 0xffff));
			pc += 2;
			break;
		default:
			operation = instruction & 0x1f;
			short_mode = (instruction & 0x20) != 0;
			return_mode = (instruction & 0x40) != 0;
			keep_mode = (instruction & 0x80) != 0;
			source = return_mode ? &uxn->return_stack : &uxn->working;
			other = return_mode ? &uxn->working : &uxn->return_stack;
			reader = (StackReader){source, source->pointer, keep_mode};

			switch(operation) {
			case 0x01: /* INC */
				a = pop_value(&reader, short_mode);
				finish_reading(&reader);
				push_value(source, (uint16_t)(a + 1u), short_mode);
				break;
			case 0x02: /* POP */
				(void)pop_value(&reader, short_mode);
				finish_reading(&reader);
				break;
			case 0x03: /* NIP */
				a = pop_value(&reader, short_mode);
				(void)pop_value(&reader, short_mode);
				finish_reading(&reader);
				push_value(source, a, short_mode);
				break;
			case 0x04: /* SWP */
				a = pop_value(&reader, short_mode);
				b = pop_value(&reader, short_mode);
				finish_reading(&reader);
				push_value(source, a, short_mode);
				push_value(source, b, short_mode);
				break;
			case 0x05: /* ROT */
				a = pop_value(&reader, short_mode);
				b = pop_value(&reader, short_mode);
				c = pop_value(&reader, short_mode);
				finish_reading(&reader);
				push_value(source, b, short_mode);
				push_value(source, a, short_mode);
				push_value(source, c, short_mode);
				break;
			case 0x06: /* DUP */
				a = pop_value(&reader, short_mode);
				finish_reading(&reader);
				push_value(source, a, short_mode);
				push_value(source, a, short_mode);
				break;
			case 0x07: /* OVR */
				a = pop_value(&reader, short_mode);
				b = pop_value(&reader, short_mode);
				finish_reading(&reader);
				push_value(source, b, short_mode);
				push_value(source, a, short_mode);
				push_value(source, b, short_mode);
				break;
			case 0x08: /* EQU */
			case 0x09: /* NEQ */
			case 0x0a: /* GTH */
			case 0x0b: /* LTH */
				a = pop_value(&reader, short_mode);
				b = pop_value(&reader, short_mode);
				finish_reading(&reader);
				if(operation == 0x08) c = b == a;
				else if(operation == 0x09) c = b != a;
				else if(operation == 0x0a) c = b > a;
				else c = b < a;
				push_byte(source, (uint8_t)c);
				break;
			case 0x0c: /* JMP */
				a = pop_value(&reader, short_mode);
				finish_reading(&reader);
				pc = short_mode ? a : (uint16_t)(pc + (int8_t)a);
				break;
			case 0x0d: /* JCN */
				a = pop_value(&reader, short_mode);
				b = pop_byte(&reader);
				finish_reading(&reader);
				if(b)
					pc = short_mode ? a :
						(uint16_t)(pc + (int8_t)a);
				break;
			case 0x0e: /* JSR */
				a = pop_value(&reader, short_mode);
				finish_reading(&reader);
				push_short(other, pc);
				pc = short_mode ? a : (uint16_t)(pc + (int8_t)a);
				break;
			case 0x0f: /* STH */
				a = pop_value(&reader, short_mode);
				finish_reading(&reader);
				push_value(other, a, short_mode);
				break;
			case 0x10: /* LDZ */
				a = pop_byte(&reader);
				finish_reading(&reader);
				b = short_mode ?
					read_memory_short(uxn, a, 0xff) : read_memory(uxn, a);
				push_value(source, b, short_mode);
				break;
			case 0x11: /* STZ */
				a = pop_byte(&reader);
				b = pop_value(&reader, short_mode);
				finish_reading(&reader);
				write_memory(uxn, a, b, short_mode, 0xff);
				break;
			case 0x12: /* LDR */
				a = pop_byte(&reader);
				finish_reading(&reader);
				b = (uint16_t)(pc + (int8_t)a);
				c = short_mode ?
					read_memory_short(uxn, b, 0xffff) : read_memory(uxn, b);
				push_value(source, c, short_mode);
				break;
			case 0x13: /* STR */
				a = pop_byte(&reader);
				b = pop_value(&reader, short_mode);
				finish_reading(&reader);
				write_memory(uxn, (uint16_t)(pc + (int8_t)a), b,
					short_mode, 0xffff);
				break;
			case 0x14: /* LDA */
				a = pop_short(&reader);
				finish_reading(&reader);
				b = short_mode ?
					read_memory_short(uxn, a, 0xffff) : read_memory(uxn, a);
				push_value(source, b, short_mode);
				break;
			case 0x15: /* STA */
				a = pop_short(&reader);
				b = pop_value(&reader, short_mode);
				finish_reading(&reader);
				write_memory(uxn, a, b, short_mode, 0xffff);
				break;
			case 0x16: /* DEI */
				a = pop_byte(&reader);
				finish_reading(&reader);
				b = read_device(uxn, (uint8_t)a);
				if(short_mode)
					b = (uint16_t)((b << 8) |
						uxn->devices[(uint8_t)(a + 1u)]);
				push_value(source, b, short_mode);
				break;
			case 0x17: /* DEO */
				a = pop_byte(&reader);
				b = pop_value(&reader, short_mode);
				finish_reading(&reader);
				if(short_mode) {
					uxn->devices[(uint8_t)a] = (uint8_t)(b >> 8);
					write_device(uxn, (uint8_t)(a + 1u), (uint8_t)b);
				} else {
					write_device(uxn, (uint8_t)a, (uint8_t)b);
				}
				break;
			case 0x18: /* ADD */
			case 0x19: /* SUB */
			case 0x1a: /* MUL */
			case 0x1b: /* DIV */
			case 0x1c: /* AND */
			case 0x1d: /* ORA */
			case 0x1e: /* EOR */
				a = pop_value(&reader, short_mode);
				b = pop_value(&reader, short_mode);
				finish_reading(&reader);
				if(operation == 0x18) c = (uint16_t)(b + a);
				else if(operation == 0x19) c = (uint16_t)(b - a);
				else if(operation == 0x1a) c = (uint16_t)(b * a);
				else if(operation == 0x1b) c = a ? (uint16_t)(b / a) : 0;
				else if(operation == 0x1c) c = b & a;
				else if(operation == 0x1d) c = b | a;
				else c = b ^ a;
				push_value(source, c, short_mode);
				break;
			case 0x1f: /* SFT */
				a = pop_byte(&reader);
				b = pop_value(&reader, short_mode);
				finish_reading(&reader);
				c = (uint16_t)((b >> (a & 0x0f)) << (a >> 4));
				push_value(source, c, short_mode);
				break;
			default:
				/* All five-bit operation values are covered above. */
				break;
			}
			break;
		}
	}
}
