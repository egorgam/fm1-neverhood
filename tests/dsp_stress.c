/* SPDX-License-Identifier: MIT */
/* Exercise the real engine, including high notes, macro corners, stealing and queue overflow. */
#include "clay_synth.h"
#include <stdio.h>
#include <stdint.h>

static int32_t buffer[512];
static void render(unsigned blocks)
{
    while (blocks--) clay_render(buffer, 256, 4096);
}
int main(void)
{
    static const unsigned notes[] = {0, 24, 48, 60, 84, 108, 127};
    unsigned p, n, corner, k, cases = 0;
    clay_init();
    for (p = 0; p < CLAY_INSTRUMENTS; p++) for (corner = 0; corner < 2; corner++) {
        for (n = 0; n < sizeof notes / sizeof notes[0]; n++) {
            clay_all_off(); clay_program(p);
            for (k = 0; k < CLAY_MACROS; k++) clay_macro(k, corner * 100);
            for (k = 0; k < CLAY_VOICES; k++) clay_note_on(notes[n], 127 - k * 20, k);
            render(40);
            for (k = 0; k < CLAY_VOICES; k++) clay_note_off(notes[n], k);
            render(8);
            if (clay_faults()) { fprintf(stderr, "DSP fault: preset %u note %u corner %u\n", p, notes[n], corner); return 1; }
            cases++;
        }
    }
    /* Duplicate pitches on independent channels, velocity zero and voice stealing. */
    clay_all_off(); clay_program(20);
    clay_note_on(60, 127, 0); clay_note_on(60, 127, 1); render(5);
    clay_note_off(60, 0); render(200);
    if (clay_active() != 1) return 2;
    clay_note_on(60, 0, 1); render(200);
    if (clay_active()) return 3;
    for (k = 0; k < 12; k++) clay_note_on(48 + k, 100, 0);
    render(10);
    if (clay_active() != CLAY_VOICES) return 4;
    clay_all_off(); render(2);
    /* Overload must fail silent, with a diagnostic, rather than stick notes. */
    for (k = 0; k < 200; k++) clay_note_on(60, 100, 0);
    render(2);
    if (clay_active() || !clay_dropped() || clay_faults()) return 5;
    printf("DSP: %u preset/note/macro cases; channels, velocity zero, stealing and overflow OK\n", cases);
    return 0;
}
