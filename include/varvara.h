#ifndef VARVARA_H
#define VARVARA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "uxn.h"

enum {
	VARVARA_SCREEN_DEFAULT_WIDTH = 512,
	VARVARA_SCREEN_DEFAULT_HEIGHT = 320,
	VARVARA_SCREEN_MAX_SIZE = 0x0fff,
	VARVARA_PATH_SIZE = 1024
};

typedef struct {
	uint16_t width;
	uint16_t height;
	uint8_t *background;
	uint8_t *foreground;
	uint32_t palette[4];
	int16_t x;
	int16_t y;
	uint16_t address;
	bool dirty;
	bool resized;
} VarvaraScreen;

typedef struct {
	FILE *file;
	void *directory;
	char path[VARVARA_PATH_SIZE];
	uint8_t state;
} VarvaraFile;

typedef struct {
	uint32_t count;
	uint32_t advance;
	uint32_t period;
	uint32_t age;
	uint32_t attack;
	uint32_t decay;
	uint32_t sustain;
	uint32_t release;
	uint16_t index;
	uint16_t length;
	uint16_t address;
	uint8_t volume[2];
	bool repeat;
	bool active;
} VarvaraAudioVoice;

typedef struct {
	Uxn uxn;
	FILE *standard_output;
	FILE *standard_error;
	VarvaraScreen screen;
	VarvaraFile files[2];
	char file_root[VARVARA_PATH_SIZE];
	bool files_sandboxed;
	void *exec_state;
	bool exec_allowed;
	VarvaraAudioVoice voices[4];
	uint32_t sample_rate;
} Varvara;

bool varvara_init(Varvara *varvara, FILE *standard_output,
	FILE *standard_error);
void varvara_destroy(Varvara *varvara);
bool varvara_reboot(Varvara *varvara, bool soft);
void varvara_files_set_sandbox(Varvara *varvara, bool sandboxed);
void varvara_exec_set_allowed(Varvara *varvara, bool allowed);
void varvara_exec_poll(Varvara *varvara);
bool varvara_exec_running(const Varvara *varvara);
UxnStop varvara_start(Varvara *varvara, uint64_t instruction_limit);
UxnStop varvara_console_input(Varvara *varvara, uint8_t byte,
	uint8_t type, uint64_t instruction_limit);
bool varvara_has_console_vector(const Varvara *varvara);
bool varvara_is_halted(const Varvara *varvara);
int varvara_exit_code(const Varvara *varvara);

UxnStop varvara_screen_frame(Varvara *varvara, uint64_t instruction_limit);
bool varvara_screen_compose(const Varvara *varvara, uint32_t *pixels,
	size_t pixel_count);
void varvara_screen_presented(Varvara *varvara);

void varvara_audio_set_sample_rate(Varvara *varvara, uint32_t sample_rate);
uint8_t varvara_audio_render(Varvara *varvara, int16_t *samples,
	size_t frame_count);
bool varvara_audio_active(const Varvara *varvara);
UxnStop varvara_audio_finished(Varvara *varvara, unsigned int voice,
	uint64_t instruction_limit);

UxnStop varvara_controller_down(Varvara *varvara, uint8_t buttons,
	uint64_t instruction_limit);
UxnStop varvara_controller_up(Varvara *varvara, uint8_t buttons,
	uint64_t instruction_limit);
UxnStop varvara_controller_key(Varvara *varvara, uint8_t key,
	uint64_t instruction_limit);
UxnStop varvara_mouse_move(Varvara *varvara, uint16_t x, uint16_t y,
	uint64_t instruction_limit);
UxnStop varvara_mouse_down(Varvara *varvara, uint8_t buttons,
	uint64_t instruction_limit);
UxnStop varvara_mouse_up(Varvara *varvara, uint8_t buttons,
	uint64_t instruction_limit);
UxnStop varvara_mouse_scroll(Varvara *varvara, int16_t x, int16_t y,
	uint64_t instruction_limit);

#endif
