#include "varvara_internal.h"

#include <stdlib.h>
#include <string.h>

enum {
	SYSTEM_RED = 0x08,
	SYSTEM_GREEN = 0x0a,
	SYSTEM_BLUE = 0x0c,
	SCREEN_VECTOR = 0x20,
	SCREEN_WIDTH = 0x22,
	SCREEN_HEIGHT = 0x24,
	SCREEN_AUTO = 0x26,
	SCREEN_X = 0x28,
	SCREEN_Y = 0x2a,
	SCREEN_ADDRESS = 0x2c,
	SCREEN_PIXEL = 0x2e,
	SCREEN_SPRITE = 0x2f
};

static const uint8_t blend[16][4] = {
	{0, 0, 1, 2}, {0, 1, 2, 3}, {0, 2, 3, 1}, {0, 3, 1, 2},
	{1, 0, 1, 2}, {1, 1, 2, 3}, {1, 2, 3, 1}, {1, 3, 1, 2},
	{2, 0, 1, 2}, {2, 1, 2, 3}, {2, 2, 3, 1}, {2, 3, 1, 2},
	{3, 0, 1, 2}, {3, 1, 2, 3}, {3, 2, 3, 1}, {3, 3, 1, 2}
};

static bool
resize_screen(Varvara *varvara, uint16_t width, uint16_t height)
{
	VarvaraScreen *screen = &varvara->screen;
	size_t count = (size_t)width * height;
	uint8_t *background;
	uint8_t *foreground;

	if(screen->width == width && screen->height == height)
		return true;
	background = calloc(count ? count : 1u, sizeof(*background));
	foreground = calloc(count ? count : 1u, sizeof(*foreground));
	if(!background || !foreground) {
		free(background);
		free(foreground);
		return false;
	}
	free(screen->background);
	free(screen->foreground);
	screen->background = background;
	screen->foreground = foreground;
	screen->width = width;
	screen->height = height;
	varvara_poke_short(&varvara->uxn.devices[SCREEN_WIDTH], width);
	varvara_poke_short(&varvara->uxn.devices[SCREEN_HEIGHT], height);
	screen->dirty = true;
	screen->resized = true;
	return true;
}

bool
varvara_screen_init(Varvara *varvara)
{
	unsigned int i;
	for(i = 0; i < 4; i++)
		varvara->screen.palette[i] = 0xff000000u;
	return resize_screen(varvara, VARVARA_SCREEN_DEFAULT_WIDTH,
		VARVARA_SCREEN_DEFAULT_HEIGHT);
}

void
varvara_screen_destroy(Varvara *varvara)
{
	free(varvara->screen.background);
	free(varvara->screen.foreground);
	varvara->screen.background = NULL;
	varvara->screen.foreground = NULL;
	varvara->screen.width = 0;
	varvara->screen.height = 0;
}

void
varvara_screen_update_palette(Varvara *varvara)
{
	unsigned int i;
	for(i = 0; i < 4; i++) {
		unsigned int shift = (i & 1u) ? 0u : 4u;
		uint8_t red = (uint8_t)((varvara->uxn.devices[SYSTEM_RED + i / 2u]
			>> shift) & 0x0f);
		uint8_t green = (uint8_t)((varvara->uxn.devices[SYSTEM_GREEN + i / 2u]
			>> shift) & 0x0f);
		uint8_t blue = (uint8_t)((varvara->uxn.devices[SYSTEM_BLUE + i / 2u]
			>> shift) & 0x0f);
		varvara->screen.palette[i] = 0xff000000u |
			((uint32_t)(red * 17u) << 16) |
			((uint32_t)(green * 17u) << 8) |
			(uint32_t)(blue * 17u);
	}
	varvara->screen.dirty = true;
}

static void
draw_pixel(Varvara *varvara)
{
	VarvaraScreen *screen = &varvara->screen;
	uint8_t control = varvara->uxn.devices[SCREEN_PIXEL];
	uint8_t color = control & 0x03;
	uint8_t *layer = (control & 0x40) ? screen->foreground : screen->background;
	int x = screen->x;
	int y = screen->y;

	if(control & 0x80) {
		int first_x = (control & 0x10) ? 0 : x;
		int last_x = (control & 0x10) ? x : (int)screen->width - 1;
		int first_y = (control & 0x20) ? 0 : y;
		int last_y = (control & 0x20) ? y : (int)screen->height - 1;
		int px;
		int py;

		if(first_x < 0) first_x = 0;
		if(first_y < 0) first_y = 0;
		if(last_x >= (int)screen->width) last_x = (int)screen->width - 1;
		if(last_y >= (int)screen->height) last_y = (int)screen->height - 1;
		for(py = first_y; py <= last_y; py++)
			for(px = first_x; px <= last_x; px++)
				layer[(size_t)py * screen->width + (size_t)px] = color;
	} else if(x >= 0 && y >= 0 && x < (int)screen->width &&
		y < (int)screen->height) {
		layer[(size_t)y * screen->width + (size_t)x] = color;
	}
	if(!(control & 0x80)) {
		if(varvara->uxn.devices[SCREEN_AUTO] & 0x01)
			screen->x += (control & 0x10) ? -1 : 1;
		if(varvara->uxn.devices[SCREEN_AUTO] & 0x02)
			screen->y += (control & 0x20) ? -1 : 1;
	}
	screen->dirty = true;
}

static void
draw_sprite_tile(Varvara *varvara, int origin_x, int origin_y,
	uint16_t address, uint8_t control)
{
	VarvaraScreen *screen = &varvara->screen;
	bool two_bpp = (control & 0x80) != 0;
	bool flip_x = (control & 0x10) != 0;
	bool flip_y = (control & 0x20) != 0;
	uint8_t *layer = (control & 0x40) ? screen->foreground : screen->background;
	uint8_t blend_index = control & 0x0f;
	int output_y;

	for(output_y = 0; output_y < 8; output_y++) {
		int source_y = flip_y ? 7 - output_y : output_y;
		uint8_t plane_one = varvara->uxn.ram[(uint16_t)(address + source_y)];
		uint8_t plane_two = two_bpp ?
			varvara->uxn.ram[(uint16_t)(address + 8 + source_y)] : 0;
		int output_x;
		for(output_x = 0; output_x < 8; output_x++) {
			int x = origin_x + output_x;
			int y = origin_y + output_y;
			int bit = flip_x ? output_x : 7 - output_x;
			uint8_t source_color = (uint8_t)(((plane_one >> bit) & 1u) |
				(((plane_two >> bit) & 1u) << 1));
			if((blend_index % 5u) == 0 && source_color == 0)
				continue;
			if(x >= 0 && y >= 0 && x < (int)screen->width &&
				y < (int)screen->height)
				layer[(size_t)y * screen->width + (size_t)x] =
					blend[blend_index][source_color];
		}
	}
}

static void
draw_sprite(Varvara *varvara)
{
	VarvaraScreen *screen = &varvara->screen;
	uint8_t control = varvara->uxn.devices[SCREEN_SPRITE];
	uint8_t automatic = varvara->uxn.devices[SCREEN_AUTO];
	unsigned int count = (automatic >> 4) + 1u;
	bool auto_address = (automatic & 0x04) != 0;
	bool two_bpp = (control & 0x80) != 0;
	int tile_x = screen->x;
	int tile_y = screen->y;
	int sequence_x = (automatic & 0x02) ? 8 : 0;
	int sequence_y = (automatic & 0x01) ? 8 : 0;
	unsigned int i;

	if(control & 0x10) sequence_x = -sequence_x;
	if(control & 0x20) sequence_y = -sequence_y;
	for(i = 0; i < count; i++) {
		draw_sprite_tile(varvara, tile_x, tile_y, screen->address, control);
		tile_x += sequence_x;
		tile_y += sequence_y;
		if(auto_address)
			screen->address = (uint16_t)(screen->address + (two_bpp ? 16u : 8u));
	}
	if(automatic & 0x01)
		screen->x += (control & 0x10) ? -8 : 8;
	if(automatic & 0x02)
		screen->y += (control & 0x20) ? -8 : 8;
	screen->dirty = true;
}

uint8_t
varvara_screen_read(Varvara *varvara, uint8_t port)
{
	VarvaraScreen *screen = &varvara->screen;
	if(port == SCREEN_X) {
		varvara_poke_short(&varvara->uxn.devices[SCREEN_X], (uint16_t)screen->x);
	} else if(port == SCREEN_Y) {
		varvara_poke_short(&varvara->uxn.devices[SCREEN_Y], (uint16_t)screen->y);
	} else if(port == SCREEN_ADDRESS) {
		varvara_poke_short(&varvara->uxn.devices[SCREEN_ADDRESS], screen->address);
	}
	return varvara->uxn.devices[port];
}

void
varvara_screen_write(Varvara *varvara, uint8_t port)
{
	VarvaraScreen *screen = &varvara->screen;
	if(port == SCREEN_WIDTH + 1) {
		uint16_t width = varvara_peek_short(&varvara->uxn.devices[SCREEN_WIDTH]) &
			VARVARA_SCREEN_MAX_SIZE;
		if(!resize_screen(varvara, width, screen->height))
			varvara_poke_short(&varvara->uxn.devices[SCREEN_WIDTH], screen->width);
	} else if(port == SCREEN_HEIGHT + 1) {
		uint16_t height = varvara_peek_short(&varvara->uxn.devices[SCREEN_HEIGHT]) &
			VARVARA_SCREEN_MAX_SIZE;
		if(!resize_screen(varvara, screen->width, height))
			varvara_poke_short(&varvara->uxn.devices[SCREEN_HEIGHT], screen->height);
	} else if(port == SCREEN_X + 1) {
		screen->x = (int16_t)varvara_peek_short(&varvara->uxn.devices[SCREEN_X]);
	} else if(port == SCREEN_Y + 1) {
		screen->y = (int16_t)varvara_peek_short(&varvara->uxn.devices[SCREEN_Y]);
	} else if(port == SCREEN_ADDRESS + 1) {
		screen->address = varvara_peek_short(&varvara->uxn.devices[SCREEN_ADDRESS]);
	} else if(port == SCREEN_PIXEL) {
		draw_pixel(varvara);
	} else if(port == SCREEN_SPRITE) {
		draw_sprite(varvara);
	}
}

UxnStop
varvara_screen_frame(Varvara *varvara, uint64_t instruction_limit)
{
	return varvara_eval_port_vector(varvara, SCREEN_VECTOR, instruction_limit);
}

bool
varvara_screen_compose(const Varvara *varvara, uint32_t *pixels,
	size_t pixel_count)
{
	const VarvaraScreen *screen = &varvara->screen;
	size_t count = (size_t)screen->width * screen->height;
	size_t i;
	if(!pixels || pixel_count < count)
		return false;
	for(i = 0; i < count; i++) {
		uint8_t foreground = screen->foreground[i];
		uint8_t color = foreground ? foreground : screen->background[i];
		pixels[i] = screen->palette[color];
	}
	return true;
}

void
varvara_screen_presented(Varvara *varvara)
{
	varvara->screen.dirty = false;
	varvara->screen.resized = false;
}
