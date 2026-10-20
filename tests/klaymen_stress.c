/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include "klaymen.h"

int main(void)
{
    int32_t samples[514];
    uint32_t pass, t, i, base;
    for (pass = 0; pass < 3; pass++) {
        base = pass == 2 ? UINT32_MAX - 500u : 0;
        klay_init();
        klay_start(base);
        for (t = 0; t < 7000u; t += 6u) {
            klay_update(base + t, 0, 130);
            samples[0] = samples[513] = 123456789;
            for (i = 1; i < 513u; i++) samples[i] = pass == 0 ? 0 : (i & 1u ? 8388600 : -8388600);
            klay_render(samples + 1, 256, 4096);
            assert(samples[0] == 123456789 && samples[513] == 123456789);
            for (i = 1; i < 513u; i++) assert(samples[i] >= -8388608 && samples[i] <= 8388607);
        }
        assert(!klay_active());
        assert(klay_steps() >= 6);
        klay_start(base + 8000u);
        klay_update(base + 9000u, 0, 130);
        klay_stop();
        for (i = 0; i < 512u; i++) samples[i] = 0;
        klay_render(samples, 256, 4096);
        for (i = 0; i < 512u; i++) assert(samples[i] == 0);
    }
    /* Exercise the complete sequence, including renewed notes during the dwell,
     * a changed instrument direction, clock wrap and panic in every pose. */
    for (pass = 0; pass < 2; pass++) {
        base = pass ? UINT32_MAX - 2200u : 0;
        klay_init();
        klay_start(base);
        for (t = 0; t < 1800u; t += 6u) klay_update(base + t, 0, 0);
        assert(klay_x() >= 0);
        {
            int32_t before = klay_x(), stopped;
            uint32_t steps;
            klay_update(base + t, 1, 0);
            assert(!klay_watching()); /* No instantaneous pose replacement. */
            for (; t < 5400u; t += 6u) klay_update(base + t, 1, 0);
            assert(klay_watching() && klay_flip());
            stopped = klay_x();
            assert(stopped >= before && stopped < before + 30);
            steps = klay_steps();
            for (; t < 25400u; t += 6u) {
                klay_update(base + t, 1, 0);
                for (i = 0; i < 512u; i++) samples[i] = 0;
                klay_render(samples, 256, 4096);
            }
            assert(klay_watching() && klay_x() == stopped && klay_steps() == steps);
            for (i = 0; i < 512u; i++) samples[i] = 0;
            klay_render(samples, 256, 4096);
            for (i = 0; i < 512u; i++) assert(samples[i] == 0);
            /* 1.5s of quiet keeps him watching; a renewed note resets the wait. */
            for (; t < 26900u; t += 6u) klay_update(base + t, 0, 0);
            assert(klay_watching() && klay_x() == stopped);
            klay_update(base + t, 1, 0);
            for (; t < 28400u; t += 6u) klay_update(base + t, 0, 0);
            assert(klay_watching());
            /* A different instrument is reached by lowering his hand and turning. */
            for (; t < 32000u; t += 6u) klay_update(base + t, 1, 240);
            assert(klay_watching() && !klay_flip() && klay_x() == stopped);
            for (; t < 33500u; t += 6u) klay_update(base + t, 0, 240);
            assert(klay_watching());
            for (; t < 38000u; t += 6u) klay_update(base + t, 0, 240);
            assert(!klay_watching() && klay_x() > stopped && klay_steps() > steps);
            for (; t < 45000u; t += 6u) klay_update(base + t, 0, 240);
            assert(!klay_active());
        }
    }
    /* The same stop/turn/think sequence survives staccato retriggers. */
    {
        uint32_t reference[700], count = 0;
        for (pass = 0; pass < 2; pass++) {
            klay_init(); klay_start(0);
            for (t = 0; t < 1800u; t += 6u) klay_update(t, 0, 0);
            for (i = 0; t < 5400u; t += 6u, i++) {
                int sounding = pass == 0 || ((t - 1800u) % 180u < 70u);
                uint32_t pose;
                klay_update(t, sounding, 0);
                pose = klay_frame() | klay_state() << 16 | klay_flip() << 24;
                if (pass == 0) reference[count++] = pose;
                else assert(i < count && reference[i] == pose);
            }
            assert(klay_watching());
        }
    }
    /* A slow repaint may stretch time, but cannot consume two source frames
     * in one update. This used to skip the middle of the turn on note redraws. */
    klay_init(); klay_start(0);
    for (t = 0; t < 1800u; t += 6u) klay_update(t, 0, 0);
    for (i = 0; i < 100u && !klay_watching(); i++) {
        uint32_t before_frame = klay_frame(), before_state = klay_state();
        t += i % 3u == 0 ? 180u : (i % 3u == 1 ? 75u : 16u);
        klay_update(t, 1, 0);
        if (klay_state() == before_state)
            assert(klay_frame() == before_frame || klay_frame() == before_frame + 1u);
    }
    assert(klay_watching());
    /* A single note at any point in the gait waits for the matching leg,
     * then turns exactly once through the shared frontal pose. */
    for (pass = 0; pass < 14u; pass++) {
        uint32_t stopped = 0, turns = 0, start = 1400u + pass * 83u;
        klay_init(); klay_start(0);
        for (t = 0; t < start; t += 6u) klay_update(t, 0, 0);
        for (; t < start + 5000u && !klay_watching(); t += 6u) {
            uint32_t before = klay_state(), f = klay_frame(), facing = klay_flip();
            klay_update(t, 1, 0);
            if (before == KLAY_WALK && klay_state() == KLAY_STOP) {
                assert(f == 1u || f == 8u);
                stopped++;
            }
            if (klay_flip() != facing) {
                assert(before == KLAY_TURN_OUT && klay_state() == KLAY_TURN_IN);
                turns++;
            }
        }
        assert(klay_watching() && stopped == 1u && turns == 1u);
    }
    /* Every transition can be interrupted safely by PLAY/panic. */
    for (pass = 0; pass < 50; pass++) {
        klay_init(); klay_start(0);
        for (t = 0; t < 1800u + pass * 120u; t += 6u)
            klay_update(t, t >= 1800u && t < 5700u, 0);
        klay_stop();
        assert(!klay_active() && !klay_watching());
        for (i = 0; i < 512u; i++) samples[i] = 0;
        klay_render(samples, 256, 4096);
        for (i = 0; i < 512u; i++) assert(samples[i] == 0);
    }
    return 0;
}
