#include "rom.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

bool
rom_load_file(Uxn *uxn, const char *path, char *error, size_t error_size)
{
	FILE *file;
	long size;
	size_t bytes_read;

	file = fopen(path, "rb");
	if(!file) {
		snprintf(error, error_size, "cannot open %s: %s", path,
			strerror(errno));
		return false;
	}
	if(fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
		fseek(file, 0, SEEK_SET) != 0) {
		snprintf(error, error_size, "cannot measure %s", path);
		fclose(file);
		return false;
	}
	if((unsigned long)size > UXN_MEMORY_SIZE - UXN_ROM_START) {
		snprintf(error, error_size, "ROM is larger than %u bytes",
			(unsigned int)(UXN_MEMORY_SIZE - UXN_ROM_START));
		fclose(file);
		return false;
	}
	bytes_read = fread(&uxn->ram[UXN_ROM_START], 1, (size_t)size, file);
	fclose(file);
	if(bytes_read != (size_t)size) {
		snprintf(error, error_size, "could not read all of %s", path);
		return false;
	}
	if(error_size)
		error[0] = '\0';
	return true;
}
