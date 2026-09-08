#ifndef ROM_H
#define ROM_H

#include <stdbool.h>
#include <stddef.h>

#include "uxn.h"

bool rom_load_file(Uxn *uxn, const char *path, char *error,
	size_t error_size);

#endif
