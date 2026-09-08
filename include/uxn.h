#ifndef UXN_H
#define UXN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
	UXN_RAM_SIZE = 0x10000,
	UXN_BANK_COUNT = 0x10,
	UXN_MEMORY_SIZE = UXN_RAM_SIZE * UXN_BANK_COUNT,
	UXN_DEVICE_SIZE = 0x100,
	UXN_STACK_SIZE = 0x100,
	UXN_ROM_START = 0x0100
};

typedef struct {
	uint8_t data[UXN_STACK_SIZE];
	uint8_t pointer;
} UxnStack;

struct Uxn;

typedef uint8_t (*UxnDeviceRead)(struct Uxn *uxn, uint8_t port, void *context);
typedef void (*UxnDeviceWrite)(struct Uxn *uxn, uint8_t port, uint8_t value,
	void *context);

typedef struct Uxn {
	/* Bank zero is the core's address space; higher banks belong to the host. */
	uint8_t ram[UXN_MEMORY_SIZE];
	uint8_t devices[UXN_DEVICE_SIZE];
	UxnStack working;
	UxnStack return_stack;
	UxnDeviceRead device_read;
	UxnDeviceWrite device_write;
	void *device_context;
	uint64_t instructions;
} Uxn;

typedef enum {
	UXN_STOP_BREAK,
	UXN_STOP_LIMIT
} UxnStop;

void uxn_init(Uxn *uxn);
void uxn_connect_devices(Uxn *uxn, UxnDeviceRead read,
	UxnDeviceWrite write, void *context);
bool uxn_load(Uxn *uxn, const uint8_t *rom, size_t length);

/* A zero instruction_limit means “run until BRK”. */
UxnStop uxn_eval(Uxn *uxn, uint16_t program_counter,
	uint64_t instruction_limit);

#endif
