/* SPDX-License-Identifier: MIT */
/* Inspect the actual DSP state to verify last-note priority and continuous retuning. */
#include <assert.h>
#include <stdio.h>
#include "clay_synth.c"
static int32_t samples[512];
static void run(unsigned blocks) { while(blocks--) clay_render(samples,256,4096); }
int main(void)
{
    unsigned p,i,count=0;
    for(p=0;p<CLAY_INSTRUMENTS;p++) if(melodic_synth(p)) {
        clay_init();clay_program(p);clay_note_on(60,100,0);run(8);
        clay_note_on(67,100,0);run(1);
        assert(clay_active()==1);assert(voices[0].note==67);
        assert(voices[0].hz<voices[0].target_hz && voices[0].hz>0);
        run(40);
        assert(fm_fabsf(voices[0].hz-voices[0].target_hz)<.1f);
        clay_note_off(67,0);run(1);assert(voices[0].note==60 && clay_active()==1);
        clay_note_on(64,100,1);run(1);assert(voices[0].note==64 && clay_active()==1);
        clay_note_off(60,0);run(1);assert(voices[0].note==64 && clay_active()==1);
        clay_note_off(64,1);run(200);assert(!clay_active());
        for(i=0;i<16;i++) {clay_note_on(48+i,100,0);run(1);assert(clay_active()==1);}
        clay_all_off();run(2);assert(!clay_active() && mono_count==0);
        assert(!clay_faults()); count++;
    }
    assert(count==15);
    puts("Mono: 15 synthesized presets, one voice, smooth pitch convergence, held-note fallback, channels and panic passed");
    return 0;
}
