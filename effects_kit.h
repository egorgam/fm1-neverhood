/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#define EFFECTS_COUNT 38u
#define EFFECTS_RATE 16000u
#define EFFECTS_BASE_NOTE 53u
typedef struct {
    uint32_t sample, decoded, position;
    int32_t predictor, step_index, current, next;
    float phase;
    uint8_t finished;
} effects_voice_t;
int effects_index(uint32_t note);
uint32_t effects_resource(uint32_t index);
const char *effects_label(uint32_t index);
/* mouth, cymbal, levers, drum, pipe, horn */
uint32_t effects_visual(uint32_t index);
uint32_t effects_frames(uint32_t index);
void effects_start(effects_voice_t *voice, uint32_t index);
float effects_read(effects_voice_t *voice, float step, uint32_t length);
