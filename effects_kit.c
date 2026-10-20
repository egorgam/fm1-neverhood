/* SPDX-License-Identifier: MIT */
#include "effects_kit.h"
#include "effects_data.h"

static const int16_t steps[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,
    130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,
    1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,
    5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,
    24623,27086,29794,32767
};
static const int8_t index_delta[16] = {-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8};

int effects_index(uint32_t note)
{
    return note >= EFFECTS_BASE_NOTE && note < EFFECTS_BASE_NOTE+EFFECTS_COUNT ? (int)(note-EFFECTS_BASE_NOTE) : -1;
}
uint32_t effects_resource(uint32_t index) { return index < EFFECTS_COUNT ? EFFECTS_SAMPLES[index].resource : 0; }
uint32_t effects_visual(uint32_t index) { return index < EFFECTS_COUNT ? EFFECTS_VISUALS[index] : 2u; }
const char *effects_label(uint32_t index) { return index < EFFECTS_COUNT ? EFFECTS_LABELS[index] : ""; }
uint32_t effects_frames(uint32_t index) { return index < EFFECTS_COUNT ? EFFECTS_SAMPLES[index].frames : 0; }
static int32_t decode(effects_voice_t *v)
{
    const effects_sample_t *s = &EFFECTS_SAMPLES[v->sample];
    uint32_t n, code;
    int32_t step, diff;
    if (v->decoded >= s->frames) return 0;
    n = v->decoded++;
    code = EFFECTS_DATA[s->offset+n/2u];
    code = n & 1u ? code & 15u : code >> 4;
    step = steps[v->step_index];
    diff = step >> 3;
    if (code & 1u) diff += step >> 2;
    if (code & 2u) diff += step >> 1;
    if (code & 4u) diff += step;
    v->predictor += code & 8u ? -diff : diff;
    if (v->predictor < -32768) v->predictor = -32768;
    if (v->predictor > 32767) v->predictor = 32767;
    v->step_index += index_delta[code];
    if (v->step_index < 0) v->step_index = 0;
    if (v->step_index > 88) v->step_index = 88;
    return v->predictor;
}
void effects_start(effects_voice_t *v, uint32_t index)
{
    v->sample = index < EFFECTS_COUNT ? index : 0;
    v->decoded = v->position = 0;
    v->predictor = 0; v->step_index = EFFECTS_SAMPLES[v->sample].step;
    v->phase = 0.0f; v->finished = 0;
    v->current = decode(v); v->next = decode(v);
}
float effects_read(effects_voice_t *v, float step, uint32_t length)
{
    float sample;
    if (v->finished || v->position >= length) { v->finished = 1; return 0.0f; }
    sample = ((float)v->current + ((float)v->next-v->current)*v->phase) * (1.0f/32768.0f);
    v->phase += step;
    while (v->phase >= 1.0f) {
        v->phase -= 1.0f;
        v->position++;
        v->current = v->next; v->next = decode(v);
    }
    return sample;
}
