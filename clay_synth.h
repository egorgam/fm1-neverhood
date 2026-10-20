/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>

#define CLAY_INSTRUMENTS 22u
#define CLAY_EFFECTS_PROGRAM 0u
#define CLAY_MUSIC_PROGRAM 19u
#define CLAY_DRUM_PROGRAM 18u
#define CLAY_DRUM_COUNT 8u
#define CLAY_DRUM_BASE_NOTE 53u
#define CLAY_VOICES 4u
#define CLAY_TRACKS 4u
#define CLAY_TOTAL_VOICES 13u
#define CLAY_MACROS 4u
#define CLAY_LOCAL_CHANNEL 16u

typedef struct {
    const char *name, *family;
    uint8_t engine, variant;
    int8_t transpose;
    float attack, decay, sustain, release, gain;
} clay_instrument_t;

extern const clay_instrument_t clay_bank[CLAY_INSTRUMENTS];
void clay_init(void);
void clay_note_on(uint32_t note, uint32_t velocity, uint32_t channel);
void clay_note_off(uint32_t note, uint32_t channel);
void clay_program(uint32_t program);
void clay_macro(uint32_t macro, uint32_t value);
void clay_sustain(uint32_t channel, uint32_t down);
void clay_all_off(void);
void clay_render(int32_t *lr, uint32_t frames, uint32_t master_q12);
uint32_t clay_active(void);
/* Envelope of the selected instrument, 0..1024; read by the screen only. */
uint32_t clay_visual_level(void);
uint32_t clay_faults(void);
uint32_t clay_dropped(void);

/* Independent player lanes; interactive notes retain their own four-voice pool. */
void clay_track_program(uint32_t track,uint32_t program);
void clay_track_volume(uint32_t track,uint32_t volume);
void clay_track_note_on(uint32_t track,uint32_t note,uint32_t velocity);
void clay_track_note_off(uint32_t track,uint32_t note);
void clay_track_stop(uint32_t track);
uint32_t clay_track_level(uint32_t track);

/* Audio callback only: avoids a second producer on the UI event queue. */
void clay_track_event_rt(uint32_t track,uint32_t note,uint32_t velocity);
void clay_track_stop_rt(uint32_t track);

void clay_track_config_rt(uint32_t track,uint32_t program,uint32_t volume);

/* Note-key LEDs for one private track, in the displayed keyboard octave. */
uint32_t clay_track_keys(uint32_t track,int32_t base_note);
