/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "clay_synth.c"
#include "player.h"
static void verify_event(uint32_t t,uint32_t note,uint32_t velocity,uint32_t length);
#define PLAYER_TEST
#include "player.c"
static uint32_t verifying,seen[4];
static void verify_event(uint32_t t,uint32_t note,uint32_t velocity,uint32_t length){
 if(verifying){const theme_note_t *n=&THEME_LANES[t][seen[t]++];assert(n->tick==position && n->note==note && n->velocity==velocity && n->length==length);}
}
#include <string.h>
static unsigned char saved[5][3840];
static unsigned sizes[5];
int plat_store_save(uint32_t o,const void *src,uint32_t n){assert(o<5 && n<=3840);memcpy(saved[o],src,n);sizes[o]=n;return 0;}
int plat_store_load(uint32_t o,void *dst,uint32_t n){if(o>=5 || !sizes[o])return -1;assert(n>=sizes[o]);memcpy(dst,saved[o],sizes[o]);return sizes[o];}
static int32_t out[512];
static void render(unsigned blocks){while(blocks--)clay_render(out,256,4096);}
int main(void)
{
    unsigned t,i;uint32_t samples=0;
    clay_init();
    for(t=0;t<4;t++){clay_track_config_rt(t,19+t%3,80);clay_track_event_rt(t,60+t*3,100);}
    render(8);
    for(t=0;t<4;t++)assert(voices[track_first[t]].active && voices[track_first[t]].channel==17+t && clay_track_level(t)>0);
    clay_program(2);for(i=0;i<4;i++)clay_note_on(60+i*2,100,0);render(5);
    assert(clay_active()==4);
    for(t=0;t<4;t++)assert(voices[track_first[t]].active);
    clay_program(3);render(80);
    for(t=0;t<4;t++)assert(voices[track_first[t]].active);
    clay_track_config_rt(2,21,0);render(80);assert(clay_track_level(2)==0);
    assert(clay_track_level(0)>0 && clay_track_level(1)>0 && clay_track_level(3)>0);
    clay_track_config_rt(2,20,80);clay_track_config_rt(3,18,80);
    for(i=0;i<4;i++)clay_track_event_rt(2,60+i*3,100);
    for(i=0;i<3;i++)clay_track_event_rt(3,53+i,100);
    render(3);
    for(i=6;i<13;i++)assert(voices[i].active);
    assert(clay_track_keys(2,53)==((1u<<7)|(1u<<10)|(1u<<13)|(1u<<16)));
    assert(clay_track_keys(3,53)==7u);
    assert(clay_track_keys(2,65)==((1u<<1)|(1u<<4)));
    assert(clay_track_keys(4,53)==0u);
    clay_track_event_rt(2,63,0);render(1);
    assert(!(clay_track_keys(2,53)&(1u<<10)));
    for(i=0;i<CLAY_TOTAL_VOICES;i++)if(voices[i].active)assert(voices[i].pan>=0 && voices[i].pan<=1);
    clay_all_off();render(2);for(i=0;i<CLAY_TOTAL_VOICES;i++)assert(!voices[i].active);
    for(t=0;t<4;t++)assert(!clay_track_keys(t,53));
    clay_init();player_init();
    assert(player_bars()==70 && player_note(0,0)>=0);
    player_select(69);player_edit(0,15,127,31);player_program(1,2);player_tempo(120);
    assert(!player_save());player_init();assert(player_bpm()==120 && player_preset(1)==2);
    player_select(69);assert(player_note(0,15)==127 && player_gate(0,15)==31);
    /* A torn four-object save must fall back to the complete factory score. */
    saved[2][4]^=1;player_init();assert(player_bpm()==112 && player_preset(1)==1);
    player_select(0);player_edit(0,0,60,2);player_loop();player_toggle();
    while(samples<44100u*12u){player_render(out,256,4096);samples+=256;
        assert(player_playing() && player_tick()<384);
        assert(!clay_faults() && !clay_dropped());
        for(i=0;i<512;i++)assert(out[i]>-8000000 && out[i]<8000000);
    }
    assert(player_save()==-1);
    player_stop();player_render(out,256,4096);assert(!player_playing());
    player_loop();player_select(69);player_toggle();
    for(i=0;i<4000;i++){player_render(out,256,4096);if(player_tick()<384)break;}
    assert(player_playing() && player_tick()<384); /* song wraps to bar one */
    player_stop();player_render(out,256,4096);
    player_clear();assert(player_note(0,0)==-1 && player_note(3,15)==-1);
    /* Replay factory events end-to-end and compare every emitted note to exact MIDI ticks/gates/velocities. */
    for(i=0;i<5;i++)sizes[i]=0;
    clay_init();player_init();verifying=1;player_toggle();samples=0;
    do {player_render(out,256,4096);samples+=256;assert(samples<44100u*190u);assert(!clay_faults() && !clay_dropped());} while(position<THEME_END);
    assert(player_pattern()==position/384u);
    verifying=0;for(t=0;t<4;t++)assert(seen[t]==THEME_COUNTS[t]);
    printf("Exact factory score: %u/%u/%u/%u original events, ticks/gates/velocities matched, duration %.3fs\n",seen[0],seen[1],seen[2],seen[3],samples/44100.0);
    puts("Sequencer: four lanes, edit/gate, last bar, loop/song wrap, save/reload, torn-save fallback, safe save while stopped passed");
    return 0;
}
