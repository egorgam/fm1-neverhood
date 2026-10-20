/* SPDX-License-Identifier: MIT */
#pragma once
#include "plat.h"

#define APP_NAME "NEVERHOOD"
#define APP_TAGLINE "THE CLAY ROOM"
#define APP_BATTERY
#define APP_SPLASH
#define APP_BIG_FONT FONT_S
void app_splash(void);
#define APP_PALETTE 1u
#define APP_BLOCK 256u
#define APP_CANVAS_PX (240u * 120u)

/* The platform keeps its panel calibration; this app has no saved settings. */
enum { OBJ_PANEL, OBJ_SEQ0, OBJ_SEQ1, OBJ_SEQ2, OBJ_SEQ3, OBJ_NOBJ };
#define APP_STORE_MAP {0xDC000u,0xD0000u,0xD2000u,0xD4000u,0xD6000u}

void app_init(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_say(const char *a, const char *b);

#ifdef PLAT_HOST
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#endif
