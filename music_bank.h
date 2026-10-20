/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
uint32_t music_select(uint32_t variant, uint32_t note);
uint32_t music_frames(uint32_t sample);
float music_step(uint32_t sample, float hz);
float music_read(uint32_t sample, float *position, float step);
