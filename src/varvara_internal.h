#ifndef VARVARA_INTERNAL_H
#define VARVARA_INTERNAL_H

#include "varvara.h"

static inline uint16_t
varvara_peek_short(const uint8_t *bytes)
{
	return (uint16_t)((bytes[0] << 8) | bytes[1]);
}

static inline void
varvara_poke_short(uint8_t *bytes, uint16_t value)
{
	bytes[0] = (uint8_t)(value >> 8);
	bytes[1] = (uint8_t)value;
}

UxnStop varvara_eval_port_vector(Varvara *varvara, uint8_t port,
	uint64_t instruction_limit);

bool varvara_screen_init(Varvara *varvara);
void varvara_screen_destroy(Varvara *varvara);
uint8_t varvara_screen_read(Varvara *varvara, uint8_t port);
void varvara_screen_write(Varvara *varvara, uint8_t port);
void varvara_screen_update_palette(Varvara *varvara);

bool varvara_files_init(Varvara *varvara);
void varvara_files_destroy(Varvara *varvara);
void varvara_file_write_port(Varvara *varvara, uint8_t port);
uint8_t varvara_datetime_read(Varvara *varvara, uint8_t port);

void varvara_exec_close(Varvara *varvara);
uint8_t varvara_exec_read(Varvara *varvara, uint8_t port);
void varvara_exec_start(Varvara *varvara);
bool varvara_exec_write_byte(Varvara *varvara, uint8_t byte);

uint8_t varvara_audio_read(Varvara *varvara, uint8_t port);
void varvara_audio_write(Varvara *varvara, uint8_t port);

#endif
