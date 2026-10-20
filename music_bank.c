/* SPDX-License-Identifier: MIT */
#include "music_bank.h"
#include "music_data.h"
uint32_t music_select(uint32_t variant, uint32_t note) { (void)note; return variant; }
uint32_t music_frames(uint32_t sample) { return sample < 5u ? MUSIC_SAMPLES[sample].frames : 0; }
float music_step(uint32_t sample, float hz) { return .5f*hz/MUSIC_SAMPLES[sample].hz; }
float music_read(uint32_t sample, float *position, float step)
{
    const music_sample_t *s = &MUSIC_SAMPLES[sample];
    uint32_t a, b;
    float phase, x, y;
    if (s->loop_end) while (*position >= (float)s->loop_end) *position -= (float)(s->loop_end-s->loop_start);
    if (*position >= (float)s->frames) return 0.0f;
    a = (uint32_t)*position; b = a+1u; phase = *position-(float)a;
    if (s->loop_end && b == s->loop_end) b = s->loop_start;
    x = (float)MUSIC_LUT[MUSIC_DATA[s->offset+a]];
    y = b < s->frames ? (float)MUSIC_LUT[MUSIC_DATA[s->offset+b]] : 0.0f;
    *position += step;
    if (s->loop_end) while (*position >= (float)s->loop_end) *position -= (float)(s->loop_end-s->loop_start);
    return (x+(y-x)*phase)*(1.0f/32768.0f);
}
