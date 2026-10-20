/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
void player_init(void);
void player_toggle(void);
void player_stop(void);
void player_render(int32_t *lr,uint32_t frames,uint32_t master);
void player_tempo(int32_t bpm);
void player_program(uint32_t track,int32_t program);
void player_volume(uint32_t track,int32_t volume);
void player_mute(uint32_t track);
uint32_t player_playing(void);
uint32_t player_tick(void);
uint32_t player_bpm(void);
uint32_t player_preset(uint32_t track);
uint32_t player_gain(uint32_t track);
uint32_t player_muted(uint32_t track);
uint32_t player_bars(void);
uint32_t player_pattern(void);
void player_select(int32_t pattern);
void player_loop(void);
uint32_t player_looping(void);
int player_note(uint32_t track,uint32_t step);
uint32_t player_gate(uint32_t track,uint32_t step);
void player_edit(uint32_t track,uint32_t step,int note,uint32_t gate);
void player_clear(void);
int player_save(void);
