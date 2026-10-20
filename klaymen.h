/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>

enum { KLAY_OFF, KLAY_WALK, KLAY_STOP, KLAY_TURN_OUT, KLAY_TURN_IN,
       KLAY_THINK_IN, KLAY_HOLD, KLAY_THINK_OUT, KLAY_START };
uint32_t klay_state(void);
void klay_init(void);
void klay_start(uint32_t now);
void klay_stop(void);
int klay_update(uint32_t now, int sounding, int32_t instrument_x);
uint32_t klay_active(void);
uint32_t klay_frame(void);
int32_t klay_x(void);
uint32_t klay_steps(void);
uint32_t klay_watching(void);
uint32_t klay_flip(void);
void klay_render(int32_t *lr, uint32_t frames, uint32_t master_q12);
