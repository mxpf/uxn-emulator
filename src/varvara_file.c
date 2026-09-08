#define _POSIX_C_SOURCE 200809L
#include "varvara_internal.h"

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
	FILE_IDLE,
	FILE_READING,
	FILE_WRITING,
	DIRECTORY_READING,
	DIRECTORY_WRITING
};

static void
close_file(VarvaraFile *file)
{
	if(file->state == FILE_READING || file->state == FILE_WRITING) {
		if(file->file)
			fclose(file->file);
	} else if(file->state == DIRECTORY_READING && file->directory) {
		closedir((DIR *)file->directory);
	}
	file->file = NULL;
	file->directory = NULL;
	file->state = FILE_IDLE;
}

bool
varvara_files_init(Varvara *varvara)
{
	varvara->files_sandboxed = true;
	return getcwd(varvara->file_root, sizeof(varvara->file_root)) != NULL;
}

void
varvara_files_set_sandbox(Varvara *varvara, bool sandboxed)
{
	varvara_files_destroy(varvara);
	varvara->files_sandboxed = sandboxed;
}

void
varvara_files_destroy(Varvara *varvara)
{
	close_file(&varvara->files[0]);
	close_file(&varvara->files[1]);
}

static bool
safe_relative_path(const char *path)
{
	const char *part = path;
	if(!path[0] || path[0] == '/')
		return false;
	while(*part) {
		const char *end = part;
		while(*end && *end != '/') end++;
		if((end - part == 2 && part[0] == '.' && part[1] == '.') ||
			(end - part == 0))
			return false;
		part = *end ? end + 1 : end;
	}
	return true;
}

static bool
path_has_symlink(const char *root, const char *relative)
{
	char path[VARVARA_PATH_SIZE];
	size_t length = strlen(root);
	const char *cursor = relative;
	struct stat info;

	if(length + 2 >= sizeof(path))
		return true;
	memcpy(path, root, length);
	path[length++] = '/';
	while(*cursor) {
		const char *slash = strchr(cursor, '/');
		size_t part = slash ? (size_t)(slash - cursor) : strlen(cursor);
		if(length + part >= sizeof(path))
			return true;
		memcpy(path + length, cursor, part);
		length += part;
		path[length] = '\0';
		if(lstat(path, &info) == 0) {
			if(S_ISLNK(info.st_mode)) return true;
		} else if(errno != ENOENT) {
			return true;
		}
		if(!slash)
			break;
		if(length + 1 >= sizeof(path))
			return true;
		path[length++] = '/';
		cursor = slash + 1;
	}
	return false;
}

static bool
select_name(Varvara *varvara, unsigned int id, uint16_t address)
{
	VarvaraFile *file = &varvara->files[id];
	const char *relative = (const char *)&varvara->uxn.ram[address];
	size_t available = UXN_RAM_SIZE - address;
	size_t length = strnlen(relative, available);

	close_file(file);
	file->path[0] = '\0';
	if(length == available || length >= sizeof(file->path))
		return false;
	if(!varvara->files_sandboxed) {
		memcpy(file->path, relative, length + 1u);
		return true;
	}
	if(!safe_relative_path(relative) ||
		path_has_symlink(varvara->file_root, relative))
		return false;
	if(snprintf(file->path, sizeof(file->path), "%s/%s", varvara->file_root,
		relative) >= (int)sizeof(file->path)) {
		file->path[0] = '\0';
		return false;
	}
	return true;
}

static size_t
clamp_length(uint16_t address, uint16_t length)
{
	size_t available = UXN_RAM_SIZE - address;
	return length < available ? length : available;
}

static void
write_status_text(uint8_t *destination, size_t length, const struct stat *info,
	bool missing)
{
	static const char hex[] = "0123456789abcdef";
	size_t i;
	uintmax_t size;
	if(!length) return;
	if(missing || !info) {
		memset(destination, '!', length);
		return;
	}
	if(S_ISDIR(info->st_mode)) {
		memset(destination, '-', length);
		return;
	}
	size = (uintmax_t)info->st_size;
	if(length < sizeof(size) * 2u &&
		size >= ((uintmax_t)1 << (length * 4u))) {
		memset(destination, '?', length);
		return;
	}
	for(i = 0; i < length; i++) {
		destination[length - i - 1u] = hex[size & 0x0fu];
		size >>= 4;
	}
}

static uint16_t
file_stat(Varvara *varvara, unsigned int id, uint16_t address, uint16_t length)
{
	VarvaraFile *file = &varvara->files[id];
	struct stat info;
	size_t count = clamp_length(address, length);
	bool missing = !file->path[0] || lstat(file->path, &info) != 0 ||
		S_ISLNK(info.st_mode);
	write_status_text(&varvara->uxn.ram[address], count,
		missing ? NULL : &info, missing);
	return (uint16_t)count;
}

static bool
open_for_read(VarvaraFile *file)
{
	struct stat info;
	if(file->state == FILE_READING || file->state == DIRECTORY_READING)
		return true;
	close_file(file);
	if(!file->path[0] || lstat(file->path, &info) != 0 || S_ISLNK(info.st_mode))
		return false;
	if(S_ISDIR(info.st_mode)) {
		file->directory = opendir(file->path);
		if(file->directory) file->state = DIRECTORY_READING;
	} else {
		file->file = fopen(file->path, "rb");
		if(file->file) file->state = FILE_READING;
	}
	return file->state != FILE_IDLE;
}

static size_t
append_text(uint8_t *destination, size_t capacity, size_t offset,
	const char *text)
{
	while(*text && offset < capacity)
		destination[offset++] = (uint8_t)*text++;
	return offset;
}

static uint16_t
read_directory(Varvara *varvara, VarvaraFile *file, uint16_t address,
	uint16_t length)
{
	uint8_t *destination = &varvara->uxn.ram[address];
	size_t capacity = clamp_length(address, length);
	size_t used = 0;
	struct dirent *entry;

	while(used < capacity &&
		(entry = readdir((DIR *)file->directory)) != NULL) {
		char child[VARVARA_PATH_SIZE];
		char status[5];
		struct stat info;
		bool missing;
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;
		if(snprintf(child, sizeof(child), "%s/%s", file->path,
			entry->d_name) >= (int)sizeof(child))
			continue;
		missing = lstat(child, &info) != 0 || S_ISLNK(info.st_mode);
		write_status_text((uint8_t *)status, 4, missing ? NULL : &info, missing);
		status[4] = '\0';
		used = append_text(destination, capacity, used, status);
		used = append_text(destination, capacity, used, "\t");
		used = append_text(destination, capacity, used, entry->d_name);
		if(!missing && S_ISDIR(info.st_mode))
			used = append_text(destination, capacity, used, "/");
		used = append_text(destination, capacity, used, "\n");
	}
	return (uint16_t)used;
}

static uint16_t
file_read(Varvara *varvara, unsigned int id, uint16_t address, uint16_t length)
{
	VarvaraFile *file = &varvara->files[id];
	size_t count = clamp_length(address, length);
	size_t read_count;
	if(!open_for_read(file)) return 0;
	if(file->state == DIRECTORY_READING)
		return read_directory(varvara, file, address, (uint16_t)count);
	read_count = fread(&varvara->uxn.ram[address], 1, count, file->file);
	if(count && !read_count) close_file(file);
	return (uint16_t)read_count;
}

static bool
create_parent_directories(const Varvara *varvara, const char *path)
{
	char parent[VARVARA_PATH_SIZE];
	size_t root_length = strlen(varvara->file_root);
	size_t i;
	struct stat info;
	if(strlen(path) >= sizeof(parent)) return false;
	strcpy(parent, path);
	for(i = root_length + 1u; parent[i]; i++) {
		if(parent[i] != '/') continue;
		parent[i] = '\0';
		if(lstat(parent, &info) != 0) {
			if(mkdir(parent, 0755) != 0) return false;
		} else if(!S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) {
			return false;
		}
		parent[i] = '/';
	}
	return true;
}

static uint16_t
file_write(Varvara *varvara, unsigned int id, uint16_t address,
	uint16_t length, bool append)
{
	VarvaraFile *file = &varvara->files[id];
	size_t count = clamp_length(address, length);
	size_t written;
	if(!file->path[0] || !create_parent_directories(varvara, file->path))
		return 0;
	if(file->state == DIRECTORY_WRITING)
		return 1;
	if(file->path[strlen(file->path) - 1u] == '/') {
		struct stat info;
		close_file(file);
		if(stat(file->path, &info) == 0 && S_ISDIR(info.st_mode)) {
			file->state = DIRECTORY_WRITING;
			return 1;
		}
		return 0;
	}
	if(file->state != FILE_WRITING) {
		close_file(file);
		file->file = fopen(file->path, append ? "ab" : "wb");
		if(!file->file) return 0;
		file->state = FILE_WRITING;
	}
	written = fwrite(&varvara->uxn.ram[address], 1, count, file->file);
	if(fflush(file->file) != 0) return 0;
	return (uint16_t)written;
}

static uint16_t
file_delete(Varvara *varvara, unsigned int id)
{
	VarvaraFile *file = &varvara->files[id];
	if(!file->path[0]) return 0;
	close_file(file);
	return unlink(file->path) == 0 ? 1 : 0;
}

void
varvara_file_write_port(Varvara *varvara, uint8_t port)
{
	unsigned int id = (port >= 0xb0) ? 1u : 0u;
	uint8_t base = (uint8_t)(id ? 0xb0 : 0xa0);
	uint8_t offset = port - base;
	uint16_t address;
	uint16_t length = varvara_peek_short(&varvara->uxn.devices[base + 0x0a]);
	uint16_t result = 0;

	if(offset == 0x09) {
		address = varvara_peek_short(&varvara->uxn.devices[base + 0x08]);
		result = select_name(varvara, id, address) ? 0 : 0;
		varvara_poke_short(&varvara->uxn.devices[base + 0x02], result);
	} else if(offset == 0x05) {
		address = varvara_peek_short(&varvara->uxn.devices[base + 0x04]);
		result = file_stat(varvara, id, address, length);
		varvara_poke_short(&varvara->uxn.devices[base + 0x02], result);
	} else if(offset == 0x06) {
		result = file_delete(varvara, id);
		varvara_poke_short(&varvara->uxn.devices[base + 0x02], result);
	} else if(offset == 0x0d) {
		address = varvara_peek_short(&varvara->uxn.devices[base + 0x0c]);
		result = file_read(varvara, id, address, length);
		varvara_poke_short(&varvara->uxn.devices[base + 0x02], result);
	} else if(offset == 0x0f) {
		address = varvara_peek_short(&varvara->uxn.devices[base + 0x0e]);
		result = file_write(varvara, id, address, length,
			(varvara->uxn.devices[base + 0x07] & 0x01) != 0);
		varvara_poke_short(&varvara->uxn.devices[base + 0x02], result);
	}
}
