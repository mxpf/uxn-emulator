#include "varvara_internal.h"

#include <limits.h>
#include <string.h>

static const uint32_t note_advances[12] = {
	0x80000, 0x879c8, 0x8facd, 0x9837f, 0xa1451, 0xaadc1,
	0xb504f, 0xbfc88, 0xcb2ff, 0xd7450, 0xe411f, 0xf1a1c
};

static int32_t
envelope(VarvaraAudioVoice *voice)
{
	uint32_t age = voice->age;
	if(!voice->release) return 0x0888;
	if(age < voice->attack)
		return voice->attack ?
			(int32_t)(0x0888u * age / voice->attack) : 0x0888;
	if(age < voice->decay)
		return (int32_t)(0x0444u *
			(2u * voice->decay - voice->attack - age) /
			(voice->decay - voice->attack));
	if(age < voice->sustain)
		return 0x0444;
	if(age < voice->release)
		return (int32_t)(0x0444u * (voice->release - age) /
			(voice->release - voice->sustain));
	voice->active = false;
	return 0;
}

static int16_t
clamp_sample(int32_t value)
{
	if(value > INT16_MAX) return INT16_MAX;
	if(value < INT16_MIN) return INT16_MIN;
	return (int16_t)value;
}

static void
start_voice(Varvara *varvara, unsigned int id)
{
	VarvaraAudioVoice *voice = &varvara->voices[id];
	uint8_t base = (uint8_t)(0x30 + id * 0x10);
	uint8_t pitch = varvara->uxn.devices[base + 0x0f] & 0x7f;
	uint16_t adsr = varvara_peek_short(&varvara->uxn.devices[base + 0x08]);
	uint32_t step = varvara->sample_rate / 15u;
	uint64_t period;

	memset(voice, 0, sizeof(*voice));
	voice->address = varvara_peek_short(&varvara->uxn.devices[base + 0x0c]);
	voice->length = varvara_peek_short(&varvara->uxn.devices[base + 0x0a]);
	if(voice->length > UXN_RAM_SIZE - voice->address)
		voice->length = (uint16_t)(UXN_RAM_SIZE - voice->address);
	voice->volume[0] = varvara->uxn.devices[base + 0x0e] >> 4;
	voice->volume[1] = varvara->uxn.devices[base + 0x0e] & 0x0f;
	voice->repeat = (varvara->uxn.devices[base + 0x0f] & 0x80) == 0;
	if(pitch >= 108 || !voice->length)
		return;
	voice->advance = note_advances[pitch % 12u] >> (8u - pitch / 12u);
	voice->attack = step * (adsr >> 12);
	voice->decay = voice->attack + step * ((adsr >> 8) & 0x0f);
	voice->sustain = voice->decay + step * ((adsr >> 4) & 0x0f);
	voice->release = voice->sustain + step * (adsr & 0x0f);
	period = (uint64_t)varvara->sample_rate * 0x4000u / 11025u;
	if(voice->length <= 0x100)
		period = period * 337u / 2u / voice->length;
	voice->period = (uint32_t)period;
	voice->active = voice->period != 0 && voice->advance != 0;
}

uint8_t
varvara_audio_read(Varvara *varvara, uint8_t port)
{
	unsigned int id = (port - 0x30u) / 0x10u;
	uint8_t base = (uint8_t)(0x30 + id * 0x10);
	VarvaraAudioVoice *voice = &varvara->voices[id];
	if((port - base) == 0x02) {
		varvara_poke_short(&varvara->uxn.devices[base + 0x02], voice->index);
	} else if((port - base) == 0x04) {
		int32_t level = envelope(voice);
		unsigned int left = voice->active && voice->volume[0] ?
			1u + (unsigned int)level * voice->volume[0] / 0x800u : 0u;
		unsigned int right = voice->active && voice->volume[1] ?
			1u + (unsigned int)level * voice->volume[1] / 0x800u : 0u;
		if(left > 15) left = 15;
		if(right > 15) right = 15;
		varvara->uxn.devices[base + 0x04] = (uint8_t)((left << 4) | right);
	}
	return varvara->uxn.devices[port];
}

void
varvara_audio_write(Varvara *varvara, uint8_t port)
{
	unsigned int id = (port - 0x30u) / 0x10u;
	uint8_t base = (uint8_t)(0x30 + id * 0x10);
	if((port - base) == 0x0f)
		start_voice(varvara, id);
}

void
varvara_audio_set_sample_rate(Varvara *varvara, uint32_t sample_rate)
{
	varvara->sample_rate = sample_rate ? sample_rate : 44100u;
}

uint8_t
varvara_audio_render(Varvara *varvara, int16_t *samples, size_t frame_count)
{
	uint8_t finished = 0;
	size_t frame;
	memset(samples, 0, frame_count * 2u * sizeof(*samples));
	for(frame = 0; frame < frame_count; frame++) {
		int32_t left = 0;
		int32_t right = 0;
		unsigned int id;
		for(id = 0; id < 4; id++) {
			VarvaraAudioVoice *voice = &varvara->voices[id];
			bool was_active = voice->active;
			int32_t level;
			int8_t sample;
			if(!voice->active) continue;
			voice->count += voice->advance;
			voice->index = (uint16_t)(voice->index +
				voice->count / voice->period);
			voice->count %= voice->period;
			if(voice->index >= voice->length) {
				if(voice->repeat)
					voice->index %= voice->length;
				else
					voice->active = false;
			}
			if(!voice->active) {
				if(was_active) finished |= (uint8_t)(1u << id);
				continue;
			}
			sample = (int8_t)(uint8_t)
				(varvara->uxn.ram[voice->address + voice->index] + 0x80u);
			level = envelope(voice);
			voice->age++;
			if(!voice->active) {
				finished |= (uint8_t)(1u << id);
				continue;
			}
			left += sample * level * voice->volume[0] / 0x180;
			right += sample * level * voice->volume[1] / 0x180;
		}
		samples[frame * 2u] = clamp_sample(left);
		samples[frame * 2u + 1u] = clamp_sample(right);
	}
	return finished;
}

bool
varvara_audio_active(const Varvara *varvara)
{
	unsigned int i;
	for(i = 0; i < 4; i++)
		if(varvara->voices[i].active) return true;
	return false;
}

UxnStop
varvara_audio_finished(Varvara *varvara, unsigned int voice,
	uint64_t instruction_limit)
{
	if(voice >= 4) return UXN_STOP_BREAK;
	return varvara_eval_port_vector(varvara,
		(uint8_t)(0x30 + voice * 0x10), instruction_limit);
}
