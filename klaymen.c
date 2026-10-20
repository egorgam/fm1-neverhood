/* SPDX-License-Identifier: MIT */
#include "klaymen.h"
#include "klaymen_audio.h"

static uint32_t state, frame, clip_end, previous_ms, tick_fraction, ticks_left, steps, flip;
static uint32_t quiet_ms, wanted_left, returning, stop_pending;
static int32_t x_q8;
/* One atomic mailbox, UI -> audio. Steps are over half a second apart. */
static volatile uint32_t sound_command;
static uint32_t serial, audio_command, audio_sound, audio_position, audio_pan;

static void sound(uint32_t id)
{
    int32_t pan = ((x_q8 / 256 + KLW_WIDTH / 2) * 255) / 240;
    if (pan < 0) pan = 0;
    if (pan > 255) pan = 255;
    sound_command = (++serial << 16) | ((uint32_t)pan << 8) | id;
}
void klay_init(void)
{
    state = frame = clip_end = previous_ms = tick_fraction = ticks_left = steps = flip = 0;
    quiet_ms = wanted_left = returning = stop_pending = 0;
    sound_command = serial = audio_command = audio_sound = audio_position = audio_pan = 0;
    x_q8 = -KLW_WIDTH * 256;
}
void klay_stop(void) { state = KLAY_OFF; flip = 0; sound(0); }
static void enter_frame(void)
{
    ticks_left = KLW_TICKS[frame];
    if (state == KLAY_WALK || state == KLAY_STOP || state == KLAY_START)
        x_q8 += KLW_DX[frame];
    if (KLW_STEP[frame]) { steps++; sound(KLW_STEP[frame]); }
}
static void clip(uint32_t next_state, uint32_t first, uint32_t count)
{
    state = next_state;
    frame = first;
    clip_end = first + count;
    enter_frame();
}
static void think(void) { clip(KLAY_THINK_IN, KLW_THINK_IN, KLW_THINK_IN_COUNT); }
static void turn(uint32_t going_back)
{
    returning = going_back;
    clip(KLAY_TURN_OUT, KLW_TURN_OUT, KLW_TURN_OUT_COUNT);
}
void klay_start(uint32_t now)
{
    if (state != KLAY_OFF) return;
    previous_ms = now;
    tick_fraction = steps = quiet_ms = wanted_left = returning = flip = stop_pending = 0;
    x_q8 = -KLW_WIDTH * 256;
    sound(0);
    clip(KLAY_WALK, 0, KLW_FRAMES);
}
static void advance(int sounding, int32_t instrument_x)
{
    if (state == KLAY_HOLD) {
        if (quiet_ms >= 2000u || (sounding && wanted_left != flip))
            clip(KLAY_THINK_OUT, KLW_THINK_OUT, KLW_THINK_OUT_COUNT);
        return;
    }
    if (--ticks_left) return;
    if (state == KLAY_WALK) {
        /* These stop clips begin at the next leading-leg pose (walk 2/9).
         * Finish that half-stride instead of jumping out of a crouched frame. */
        if (stop_pending && (frame == 1u || frame == 8u)) {
            uint32_t a = frame == 8u;
            stop_pending = 0;
            clip(KLAY_STOP, a ? KLW_STOP_A : KLW_STOP_B, a ? KLW_STOP_A_COUNT : KLW_STOP_B_COUNT);
        } else {
            frame = (frame + 1u) % KLW_FRAMES;
            enter_frame();
            if (x_q8 >= 240 * 256) klay_stop();
        }
        return;
    }
    if (++frame < clip_end) { enter_frame(); return; }
    switch (state) {
    case KLAY_STOP:
        sound(0);
        wanted_left = instrument_x < klay_x() + KLW_ORIGIN_X;
        if (wanted_left != flip) turn(0); else think();
        break;
    case KLAY_TURN_OUT:
        /* Change handedness only in the shared frontal neutral pose. */
        flip = !flip;
        clip(KLAY_TURN_IN, KLW_TURN_IN, KLW_TURN_IN_COUNT);
        break;
    case KLAY_TURN_IN:
        if (returning) clip(KLAY_START, KLW_START, KLW_START_COUNT);
        else think();
        break;
    case KLAY_THINK_IN:
        quiet_ms = 0;
        clip(KLAY_HOLD, KLW_THINK_HOLD, KLW_THINK_HOLD_COUNT);
        break;
    case KLAY_THINK_OUT:
        if (sounding) {
            if (wanted_left != flip) turn(0); else think();
        } else if (flip) turn(1);
        else clip(KLAY_START, KLW_START, KLW_START_COUNT);
        break;
    case KLAY_START:
        /* The start clip finishes on the same leading leg as cycle frame 0. */
        clip(KLAY_WALK, 0, KLW_FRAMES);
        break;
    default: break;
    }
}
int klay_update(uint32_t now, int sounding, int32_t instrument_x)
{
    uint32_t elapsed = now - previous_ms, old_frame = frame, old_state = state, old_flip = flip;
    int32_t old_x = x_q8;
    previous_ms = now;
    if (!state) return 0;
    /* Preserve unsigned clock wrap and avoid a burst of footsteps after a stall. */
    if (elapsed > 250u) elapsed = 250u;
    if (sounding) quiet_ms = 0;
    else if (quiet_ms < 2000u) quiet_ms += elapsed;
    wanted_left = instrument_x < klay_x() + KLW_ORIGIN_X;
    if (state == KLAY_WALK && sounding && klay_x() >= 0 && klay_x() <= 240 - KLW_WIDTH - 24)
        stop_pending = 1;
    tick_fraction += elapsed * KLW_TICK_RATE;
    while (tick_fraction >= 1000u && state) {
        tick_fraction -= 1000u;
        advance(sounding, instrument_x);
        /* If drawing/input took longer than a source frame, stretch the frame
         * rather than fast-forwarding through an entire turn between paints. */
        if (frame != old_frame || state != old_state) {
            tick_fraction %= 1000u;
            break;
        }
    }
    return frame != old_frame || state != old_state || flip != old_flip || x_q8 != old_x;
}
uint32_t klay_active(void) { return state != KLAY_OFF; }
uint32_t klay_frame(void) { return frame; }
int32_t klay_x(void) { return x_q8 / 256; }
uint32_t klay_steps(void) { return steps; }
uint32_t klay_watching(void) { return state == KLAY_HOLD; }
uint32_t klay_flip(void) { return flip; }
uint32_t klay_state(void) { return state; }

void klay_render(int32_t *lr, uint32_t frames, uint32_t master_q12)
{
    uint32_t command = sound_command, i;
    if (command != audio_command) {
        audio_sound = command & 255u;
        audio_pan = (command >> 8) & 255u;
        audio_position = 0;
        audio_command = command;
    }
    if (!audio_sound) return;
    for (i = 0; i < frames; i++) {
        const int16_t *pcm = audio_sound == 1u ? KLW_SOUND_1 : KLW_SOUND_2;
        uint32_t length = audio_sound == 1u ? KLW_SOUND_1_LENGTH : KLW_SOUND_2_LENGTH;
        uint32_t index = audio_position >> 1, next, channel;
        int32_t sample;
        if (index >= length) { audio_sound = 0; break; }
        next = index + 1u < length ? index + 1u : index;
        /* Original mono 22050 Hz PCM -> 44100 Hz, with linear interpolation. */
        sample = audio_position & 1u ? ((int32_t)pcm[index] + pcm[next]) / 2 : pcm[index];
        sample = (sample * (int32_t)master_q12 / 4096) * 64;
        audio_position++;
        for (channel = 0; channel < 2u; channel++) {
            int32_t pan = channel ? (int32_t)audio_pan : 255 - (int32_t)audio_pan;
            int32_t mixed = lr[i * 2u + channel] + sample * pan / 255;
            if (mixed > 8388607) mixed = 8388607;
            if (mixed < -8388608) mixed = -8388608;
            lr[i * 2u + channel] = mixed;
        }
    }
}
