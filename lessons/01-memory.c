#include <stdint.h>
#include <stdio.h>

enum {
	MEMORY_SIZE = 65536,
	ROM_START = 0x0100
};

int
main(void)
{
	uint8_t memory[MEMORY_SIZE] = {0};

	memory[ROM_START] = 0x48;

	printf("Address 0x%04x holds the byte 0x%02x.\n",
		(unsigned int)ROM_START,
		(unsigned int)memory[ROM_START]);
	printf("As a number, that byte is %u.\n",
		(unsigned int)memory[ROM_START]);
	printf("As an ASCII character, that byte is %c.\n",
		memory[ROM_START]);

	return 0;
}
