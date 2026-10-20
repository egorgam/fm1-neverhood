/* SPDX-License-Identifier: MIT */
/* fm1-base includes the app after its platform and drawing helpers. */
#include "clay_synth.h"
#include "player.h"
#include "clay_room.h"
#include "neverhood_wordmark.h"
#include "klaymen.h"
#include "klaymen_art.h"
#include "fx_panel.h"
#include "menu_room.h"
#include "effects_kit.h"
#include "art_unpack.h"

static int32_t octave, program, last_note;
static int32_t last_effect;
static uint32_t previous_keys, previous_buttons, popup_until;
static uint8_t held_note[NKEYS], knobs[CLAY_INSTRUMENTS][CLAY_MACROS];
static int walk_dirty;
static uint8_t controls_page,player_page,player_track;
static uint32_t player_ui_at;
static uint8_t seq_step,seq_record,held_lane[NKEYS];
static int8_t seq_saved;
static uint32_t rec_pressed_at;
static uint8_t rec_hold;
static int redraw;
static uint32_t battery_at;
static int32_t battery_level;
enum { VOICE, HORNS, AIR, BANJO, GUITARS, BASS, KEYS, DRUMS, EFFECTS, CATEGORIES };
/* The revised bank has one combined drum category and three retained Room voices. */
static const uint8_t category_first[CATEGORIES] = {3, 1, 10, 12, 13, 6, 2, CLAY_DRUM_PROGRAM, CLAY_EFFECTS_PROGRAM};
static uint32_t animation_at, animation_level, animation_note, animation_step;

/* Recorded FX may be quiet or transient; keep their source visibly moving
 * throughout the active one-shot, rather than losing it between waveform peaks. */
static uint32_t instrument_level(void)
{
    uint32_t level=clay_visual_level();
    if(program==CLAY_EFFECTS_PROGRAM && last_effect>=0 && clay_active() && level<512u) level=512u;
    return level;
}

static uint32_t category(void)
{
    uint32_t c;
    for (c = 0; c < CATEGORIES; c++) {
        const char *a = clay_bank[program].family, *b = clay_bank[category_first[c]].family;
        while (*a && *a == *b) { a++; b++; }
        if (*a == *b) return c;
    }
    return DRUMS;
}

static void defaults(uint32_t p)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) knobs[p][i] = i == 3u ? 25u : 50u;
    if (p == CLAY_EFFECTS_PROGRAM) { knobs[p][2] = 100; knobs[p][3] = 0; }
}
static void send_knobs(void)
{
    uint32_t i;
    for (i = 0; i < CLAY_MACROS; i++) clay_macro(i, knobs[program][i]);
}
static uint8_t art_ready;
static void init_art(void)
{
    if (art_ready) return;
    art_unpack(CLAY_ROOM, sizeof CLAY_ROOM, CLAY_ROOM_PACKED, sizeof CLAY_ROOM_PACKED);
    art_unpack(MENU_ROOM, sizeof MENU_ROOM, MENU_ROOM_PACKED, sizeof MENU_ROOM_PACKED);
    art_unpack(KLW_DATA, sizeof KLW_DATA, KLW_DATA_PACKED, sizeof KLW_DATA_PACKED);
    art_ready=1;
}
void app_init(void)
{
    uint32_t i;
    init_art();
    clay_init();
    player_init();
    klay_init();
    octave = program = 0; last_note = -1;
    last_effect = -1;
    previous_keys = previous_buttons = popup_until = 0;
    walk_dirty = 0;
    battery_level = -1; battery_at = 0;
    controls_page = player_page = player_track = 0; player_ui_at=0;rec_hold=0;
    animation_at = animation_level = animation_note = animation_step = 0;
    for (i = 0; i < CLAY_INSTRUMENTS; i++) defaults(i);
    send_knobs();
    for (i = 0; i < NKEYS; i++) {held_note[i] = 255u;held_lane[i]=255u;}
    redraw = 1;
}
void app_panic(void) { player_stop(); clay_all_off(); klay_stop(); last_note = -1; redraw = 1; }
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    player_render(lr, n, master_q12);
    klay_render(lr, n, master_q12);
}

static void choose(int32_t value)
{
    program = (value % (int32_t)CLAY_INSTRUMENTS + (int32_t)CLAY_INSTRUMENTS) % (int32_t)CLAY_INSTRUMENTS;
    defaults((uint32_t)program);
    clay_program((uint32_t)program);
    send_knobs();
    last_note = -1;
    last_effect = -1;
    animation_level = 0;
    redraw = 1;
}
static void change_macro(uint32_t id, int32_t value)
{
    knobs[program][id] = (uint8_t)clamp(value, 0, 100);
    clay_macro(id, knobs[program][id]);
    popup_until = plat_ms() + 1800u;
    redraw = 1;
}
static void input(void)
{
    uint32_t keys = plat_keys(), buttons = plat_buttons(), pressed = buttons & ~previous_buttons;
    uint32_t down = keys & ~previous_keys, up = previous_keys & ~keys, k, packet;
    int32_t turn;
    if (pressed & (1u << B_OCTDN)) { octave = clamp(octave - 1, -2, 2); redraw = 1; }
    if (pressed & (1u << B_OCTUP)) { octave = clamp(octave + 1, -2, 2); redraw = 1; }
    if(pressed & (1u<<B_SEQ)){player_page^=1u;controls_page=0;popup_until=0;redraw=1;}
    if(pressed&(1u<<B_FX)){controls_page^=1u;player_page=0;popup_until=0;redraw=1;pressed&=~(1u<<B_FX);}
    if(!player_page || (pressed&(1u<<B_HOME)))rec_hold=0;
    if(player_page) {
        if(pressed&(1u<<B_REC)){rec_pressed_at=plat_ms();rec_hold=1;}
        if(rec_hold==1 && (buttons&(1u<<B_REC)) && (uint32_t)(plat_ms()-rec_pressed_at)>=700u){
            player_edit(player_track,seq_step,-1,1);seq_saved=0;rec_hold=2;redraw=1;
        }
        if(rec_hold && !(buttons&(1u<<B_REC))){
            if(rec_hold==1){seq_record^=1u;redraw=1;}rec_hold=0;
        }
        if(pressed&(1u<<B_ARP)){player_loop();redraw=1;}
        if(pressed&(1u<<B_EDIT)){
            if(buttons&(1u<<B_GLO))player_clear();else player_edit(player_track,seq_step,-1,1);
            seq_saved=0;redraw=1;
        }
        if(pressed&(1u<<B_SAVE)){seq_saved=player_save()==0?1:-1;redraw=1;}
    }
    if (pressed & (1u << B_HOME)) {
        uint32_t was_home=!controls_page && !player_page;
        controls_page = player_page = 0; popup_until = 0;
        if(was_home)klay_start(plat_ms());else klay_stop();redraw = 1;
    }
    if (pressed & (1u << B_PLAY)) {uint32_t was_playing=player_playing();player_stop();clay_all_off();last_note=-1;if(!was_playing)player_toggle();redraw=1;}
    if(player_page) {
        turn=plat_enc(EN_ALGO);
        if(turn){player_track=(uint8_t)(((int32_t)player_track+turn%4+4)%4);redraw=1;}
        turn=plat_enc(EN_PRESET);
        if(turn){player_program(player_track,(int32_t)player_preset(player_track)+turn);redraw=1;}
        turn=plat_enc(EN_SELECT);
        if(turn){if(buttons&(1u<<B_GLO))player_tempo((int32_t)player_bpm()+turn);else player_select((int32_t)player_pattern()+turn);redraw=1;}
        turn=plat_enc(EN_K1);if(turn){seq_step=(uint8_t)(((int32_t)seq_step+turn%16+16)%16);redraw=1;}
        turn=plat_enc(EN_K2);if(turn){int n=player_note(player_track,seq_step);player_edit(player_track,seq_step,clamp((n<0?59:n)+turn,-1,127),player_gate(player_track,seq_step));seq_saved=0;redraw=1;}
        turn=plat_enc(EN_K3);if(turn){int n=player_note(player_track,seq_step);player_edit(player_track,seq_step,n<0?60:n,clamp((int32_t)player_gate(player_track,seq_step)+turn,1,31));seq_saved=0;redraw=1;}
        turn=plat_enc(EN_K4);if(turn){player_volume(player_track,(int32_t)player_gain(player_track)+turn*2);redraw=1;}
    } else {
    turn = plat_enc(EN_ALGO);
    if (turn) {
        int32_t c = ((int32_t)category() + turn % CATEGORIES + CATEGORIES) % CATEGORIES;
        choose(category_first[c]);
    }
    turn = plat_enc(EN_PRESET) + plat_enc(EN_SELECT);
    if (turn) choose(program + turn);
    for (k = 0; k < CLAY_MACROS; k++) {
        turn = plat_enc(EN_K1 + (int32_t)k);
        if (turn) change_macro(k, (int32_t)knobs[program][k] + turn * 2);
    }
    }
    for (k = 0; k < NKEYS; k++) {
        if (down & (1u << k)) {
            held_note[k] = (uint8_t)(53 + (int32_t)k + 12 * octave);
            if(player_page && seq_record){
                uint32_t step=player_playing()?player_tick()/24u%16u:seq_step;
                if(player_playing())player_select((int32_t)(player_tick()/384u));
                player_edit(player_track,step,held_note[k],1);seq_saved=0;
                if(!player_playing())seq_step=(seq_step+1u)%16u;
            }
            held_lane[k]=player_page?player_track:255u;
            if(player_page)clay_track_note_on(player_track,held_note[k],100);else clay_note_on(held_note[k], 100u, CLAY_LOCAL_CHANNEL);
            if (program == CLAY_EFFECTS_PROGRAM) last_effect = effects_index(held_note[k]);
            animation_note = held_note[k];
            last_note = held_note[k]; redraw = 1;
        }
        if (up & (1u << k)) {
            if(held_lane[k]<4)clay_track_note_off(held_lane[k],held_note[k]);else clay_note_off(held_note[k], CLAY_LOCAL_CHANNEL);
            if (last_note == held_note[k]) last_note = -1;
            held_note[k] = 255u; redraw = 1;
        }
    }
    while (plat_midi_in(&packet, 0)) {
        uint32_t status = (packet >> 8) & 0xFFu, type = status & 0xF0u;
        uint32_t channel = status & 15u, note = (packet >> 16) & 0x7Fu, value = (packet >> 24) & 0x7Fu;
        if (type == 0x90u && value) {
            if(player_page){
                if(seq_record){uint32_t step=player_playing()?player_tick()/24u%16u:seq_step;
                    if(player_playing())player_select((int32_t)(player_tick()/384u));
                    player_edit(player_track,step,note,1);seq_saved=0;if(!player_playing())seq_step=(seq_step+1u)%16u;}
                clay_track_note_on(player_track,note,value);
            }else clay_note_on(note, value, channel); last_note = (int32_t)note; animation_note = note;
            if (program == CLAY_EFFECTS_PROGRAM) last_effect = effects_index(note);
        }
        else if (type == 0x80u || type == 0x90u) {
            if(player_page)clay_track_note_off(player_track,note);else clay_note_off(note, channel);
            if (last_note == (int32_t)note) last_note = -1;
        } else if (type == 0xC0u && note < CLAY_INSTRUMENTS) choose((int32_t)note);
        else if (type == 0xB0u) {
            if (note >= 16u && note <= 19u) change_macro(note - 16u, (int32_t)(value * 100u / 127u));
            else if (note == 74u) change_macro(0, (int32_t)(value * 100u / 127u));
            else if (note == 1u) change_macro(1, (int32_t)(value * 100u / 127u));
            else if (note == 91u) change_macro(3, (int32_t)(value * 100u / 127u));
            else if (note == 64u) clay_sustain(channel, value >= 64u);
            else if (note == 120u || note == 123u) app_panic();
        }
        redraw = 1;
    }
    previous_keys = keys; previous_buttons = buttons;
    plat_leds(buttons | (controls_page ? 1u << B_FX : 0u) | (player_page ? (1u << B_SEQ) | (seq_record ? 1u<<B_REC : 0u) | (player_looping()?1u<<B_ARP:0u) : 0u) | (klay_active() ? 1u << B_HOME : 0u), keys | ((player_page && player_playing())?clay_track_keys(player_track,53+12*octave):0u));
    if (popup_until && plat_ms() >= popup_until) { popup_until = 0; redraw = 1; }
}
void app_input_only(void) { input(); }

static void centered(int32_t y, const felucca_font_t *font, const char *text, uint16_t color)
{
    cv_text((240 - text_w(font, text)) / 2, y, font, text, color);
}
static void decimal(char *out, uint32_t value)
{
    char reverse[12];
    uint32_t i = 0;
    do { reverse[i++] = (char)('0' + value % 10u); value /= 10u; } while (value);
    while (i) *out++ = reverse[--i];
    *out = 0;
}
/* Stock FM-1 thresholds on ADC ch3: 531 / 561 / 591. The holes fill bottom up.
 * Two ADC counts of hysteresis suppress a flickering boundary. Unknown is dark. */
static void update_battery(void)
{
    int32_t raw = plat_battery(), level;
    uint32_t now = plat_ms();
    if (battery_level >= 0 && (uint32_t)(now-battery_at) < 1000u) return;
    battery_at = now;
    level = raw < 0 ? 0 : raw >= 591 ? 3 : raw >= 561 ? 2 : raw >= 531 ? 1 : 0;
    if (battery_level >= 0 && level != battery_level) {
        static const int16_t edge[] = {531,561,591};
        if (level > battery_level && raw < edge[battery_level]+2) level = battery_level;
        else if (level < battery_level && raw >= edge[battery_level-1]-2) level = battery_level;
    }
    if (level != battery_level) { battery_level = level; redraw = 1; }
}
static void draw_battery(uint32_t strip)
{
    int32_t hole, x, y;
    for (hole=0; hole<3; hole++) {
        int32_t cy = 92-16*hole;
        if (hole >= battery_level) continue;
        for (y=-5; y<=5; y++) for (x=-4; x<=4; x++) {
            int32_t py=cy+y, d=x*x*25+y*y*16;
            uint16_t color;
            if(d>400 || py<(int32_t)strip || py>=(int32_t)strip+(int32_t)cv_h) continue;
            {
                int32_t shade=clamp(62-d/10-x*5-y*4,0,80);
                color=RGB(92+shade,112+shade,38+shade/2);
                if(x==-1 && y==-2) color=RGB(222,223,143);
            }
            cv_px[(py-(int32_t)strip)*240+227+x]=swap16(color);
        }
    }
}
static int menu_mode(void){return player_page || controls_page || (popup_until && !klay_active());}
static void clay_background(uint32_t y)
{
    uint32_t i;
    cv_begin(240, 120, C_BLACK);
    for (i = 0; i < 240u * 120u; i++) {
        if(menu_mode()){
            cv_px[i]=MENU_PALETTE[MENU_ROOM[((y+i/240u)*MENU_SIZE/240u)*MENU_SIZE+(i%240u)*MENU_SIZE/240u]];
        }
        else cv_px[i] = CLAY_PALETTE[CLAY_ROOM[y * 240u + i]];
    }
}
/* Bend the original clay pixels locally. The displacement tapers to zero at
 * each edge, so the surrounding illustration stays intact, without cutouts.
 * All coordinates refer to the full picture, even across the two LCD strips. */
static void bend(uint32_t strip, int32_t x, int32_t y, int32_t w, int32_t h,
                 int32_t dx, int32_t dy, int mode)
{
    int32_t u, v;
    for (v = 1; v < h - 1; v++) {
        int32_t py = y + v, wy = (v < h / 2 ? v : h - 1 - v) * 512 / h;
        if (py < (int32_t)strip || py >= (int32_t)strip + 120) continue;
        for (u = 1; u < w - 1; u++) {
            int32_t wx = (u < w / 2 ? u : w - 1 - u) * 512 / w;
            int32_t weight = wx * wy / 256;
            int32_t sx = dx * weight / 65536, sy = dy * weight / 65536;
            if (mode == 1) sx = (u - w / 2) * dx * weight / (65536 * (w / 2));
            if (mode == 2) sy = (u - w / 2) * dy * weight / (65536 * (w / 2));
            cv_px[(py - (int32_t)strip) * 240 + x + u] =
                CLAY_PALETTE[CLAY_ROOM[clamp(py + sy, 0, 239) * 240 + clamp(x + u + sx, 0, 239)]];
        }
    }
}
static void animate(uint32_t strip)
{
    int32_t phase = (int32_t)(animation_step % 16u);
    int32_t wave = phase < 8 ? phase * 2 - 7 : 23 - phase * 2;
    int32_t amount = (int32_t)animation_level;
    int32_t wobble = wave * amount / 6;
    int32_t bar = (int32_t)(animation_note % 5u);
    if (amount < 32) return;
    switch (category()) {
    case VOICE: /* The teal mouth breathes and shapes its vowel. */
        bend(strip, 19, 65, 42, 51, wobble * 3, -amount, 1); break;
    case HORNS:
        bend(strip, 163, 59, 46, 56, wobble * 3, -amount, 1); break;
    case AIR:
        bend(strip, 202, 99, 20, 19, wobble * 2, amount, 1); break;
    case BANJO: case GUITARS: /* Clay bars stand in for the plucked strings. */
        bend(strip, 92 + 14 * bar, 104, 23, 40, wobble, 0, 0); break;
    case BASS: /* The left wall's resonating holes breathe with the bass. */
        bend(strip,0,32,12,17,wobble*3,0,1);
        bend(strip,1,50,15,17,wobble*3,0,1);
        bend(strip,4,75,17,19,wobble*3,0,1);
        bend(strip,11,99,16,20,wobble*3,0,1); break;
    case KEYS:
        bend(strip, 92 + 14 * bar, 104, 23, 40, 0, -amount * 2, 0);
        bend(strip, 89, 140, 86, 23, wobble / 2, 0, 0); break;
    case EFFECTS:
        switch(effects_visual(last_effect>=0 ? (uint32_t)last_effect : EFFECTS_COUNT)) {
        case 0: bend(strip,19,65,42,51,wobble*3,-amount,1); break;
        case 1: bend(strip,62,61,56,25,0,wobble*4,2); break;
        case 2:
            bend(strip,143,75,23,39,wobble*3,-amount,1);
            bend(strip,110+12*(int32_t)(animation_note%3u),89,12,25,0,-amount*2,0); break;
        case 3: bend(strip,54,100,33,26,0,-amount*2,0); break;
        case 4: bend(strip,202,99,20,19,wobble*2,amount,1); break;
        case 5: bend(strip,163,59,46,56,wobble*3,-amount,1); break;
        }
        break;
    case DRUMS:
        bend(strip, 54, 100, 33, 26, 0, -amount * 2, 0);
        bend(strip, 62, 61, 56, 25, 0, wobble * 4, 2); break;
    }
}
static void blend_pixel(uint16_t *dest, uint16_t c, uint32_t a)
{
    if (a == 15u) *dest = c;
    else if (a) {
        uint32_t fg = swap16(c), bg = swap16(*dest);
        uint32_t r = ((fg >> 11) * a + (bg >> 11) * (15u - a)) / 15u;
        uint32_t g = (((fg >> 5) & 63u) * a + ((bg >> 5) & 63u) * (15u - a)) / 15u;
        uint32_t b = ((fg & 31u) * a + (bg & 31u) * (15u - a)) / 15u;
        *dest = swap16((uint16_t)((r << 11) | (g << 5) | b));
    }
}
static void clay_arrow(int32_t x, int32_t y, int left)
{
    uint32_t i;
    /* Reuse the clay sprite, rotated into fixed outward-pointing end arrows. */
    for (i = 0; i < ARROW_WIDTH * ARROW_HEIGHT; i++) {
        int32_t u = (int32_t)(i % ARROW_WIDTH), v = (int32_t)(i / ARROW_WIDTH);
        int32_t px = x + (left ? ARROW_HEIGHT - 1 - v : v);
        int32_t py = y + (left ? u : ARROW_WIDTH - 1 - u);
        uint32_t a = ARROW_ALPHA[i/2u];
        if (py < 0 || py >= (int32_t)cv_h) continue;
        a = i & 1u ? a & 15u : a >> 4;
        blend_pixel(&cv_px[py*240+px], ARROW_PIXELS[i], a);
    }
}
static void clay_slider(int32_t y, uint32_t value)
{
    static const int8_t ripple[] = {0,0,1,0,-1,-1,0};
    uint32_t u, position = value * 58u / 100u;
    int32_t dx, dy, thumb_x = 111 + (int32_t)position;
    int32_t thumb_y = y + 7 + ripple[(position/9u)%7u];
    clay_arrow(96, y + 3, 1);
    clay_arrow(173, y + 3, 0);
    for (u = 0; u <= 58u; u++) {
        int32_t x = 111 + (int32_t)u, cy = y + 7 + ripple[(u/9u)%7u];
        cv_pset(x, cy-1, RGB(34,17,44));
        cv_pset(x, cy, RGB(43,22,55));
        cv_pset(x, cy+1, RGB(65,33,78));
        if (u > 1u && u < 57u) cv_pset(x, cy+2, RGB(143,92,157));
    }
    /* A small rounded clay peg shows the value; the end arrows stay put. */
    for (dy = -3; dy <= 3; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            if ((dx == -2 || dx == 2) && (dy == -3 || dy == 3)) continue;
            cv_pset(thumb_x+dx+1, thumb_y+dy+1, RGB(37,18,43));
        }
    }
    for (dy = -3; dy <= 3; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            uint16_t c;
            if ((dx == -2 || dx == 2) && (dy == -3 || dy == 3)) continue;
            c = dx == -2 || dy == -3 ? RGB(255,224,157) :
                (dx == 2 || dy == 3 ? RGB(145,90,39) : RGB(222,170,89));
            cv_pset(thumb_x+dx, thumb_y+dy, c);
        }
    }
}
static void draw_player(uint32_t strip)
{
    uint32_t t,j;char text[12];int32_t offset=(int32_t)strip+6;
    uint16_t cream=RGB(255,223,157),dim=RGB(157,114,144),gold=RGB(231,166,71);

    cv_text(20,14-offset,&FONT_XS,"SEQ",cream);
    decimal(text,player_pattern()+1u);cv_text(52,14-offset,&FONT_XS,text,gold);
    cv_text(78,14-offset,&FONT_XS,player_looping()?"LOOP":"SONG",cream);
    cv_text(132,14-offset,&FONT_XS,seq_record?"REC":"EDIT",seq_record?gold:cream);
    for(t=0;t<4;t++){
        int32_t y=38+24*(int32_t)t-offset;
        decimal(text,t+1u);cv_text(19,y,&FONT_XS,text,t==player_track?gold:dim);
        for(j=0;j<16;j++){
            int32_t x=37+11*(int32_t)j;
            uint16_t cell=player_note(t,j)<0?RGB(62,33,71):(player_muted(t)?dim:gold);
            cv_rect(x,y+2,8,8,RGB(34,18,39));cv_rect(x+1,y+3,6,6,cell);
            if(t==player_track && j==seq_step){cv_rect(x,y,8,1,cream);cv_rect(x,y+11,8,1,cream);}
            if(player_playing() && player_tick()/384u==player_pattern() && player_tick()/24u%16u==j)cv_rect(x,y+13,8,2,cream);
        }
    }
    cv_text(23,131-offset,&FONT_XS,clay_bank[player_preset(player_track)].name,cream);
    if(seq_saved)cv_text(166,14-offset,&FONT_XS,seq_saved>0?"SAVED":"STOP!",gold);
}
static void draw_controls(uint32_t strip)
{
    static const char *const labels[] = {"K1 TONE", "K2 MOTION", "K3 DECAY", "K4 ROOM"};
    static const char *const kit_labels[] = {"K1 TONE", "K2 SPEED", "K3 LENGTH", "K4 ROOM"};
    uint32_t k;

    for (k = 0; k < 4u; k++) {
        char value[8];
        const char *label = program == CLAY_EFFECTS_PROGRAM ? kit_labels[k] : labels[k];
        int32_t y = 20 + 32 * (int32_t)k - (int32_t)strip;
        decimal(value, knobs[program][k]);
        cv_text(29, y + 1, &FONT_XS, label, RGB(41, 20, 46));
        cv_text(29, y, &FONT_XS, label, RGB(255, 223, 157));
        cv_text(211 - text_w(&FONT_XS, value), y, &FONT_XS, value, RGB(255, 223, 157));
        clay_slider(y, knobs[program][k]);
    }
}
static void draw_walker(uint32_t strip)
{
    int32_t x = klay_x(), y = 170 - KLW_HEIGHT, u, v;
    uint32_t frame = klay_frame();
    if (!klay_active()) return;
    const uint8_t *data = KLW_DATA + KLW_OFFSETS[frame];
    for (v = 0; v < KLW_SOURCE_HEIGHT; v++) {
        int32_t row, y0=v*KLW_HEIGHT/KLW_SOURCE_HEIGHT, y1=(v+1)*KLW_HEIGHT/KLW_SOURCE_HEIGHT;
        uint32_t spans = *data++, span;
        for (span = 0; span < spans; span++) {
            uint32_t start = data[0], length = data[1], i;
            const uint8_t *colors = data + 2;
            data = colors + length;
            for (row=y0;row<y1;row++) {
                int32_t py=y+row-(int32_t)strip;
                if(py<0 || py>=(int32_t)cv_h) continue;
                for (i = 0; i < length; i++) {
                    int32_t x0=(int32_t)(start+i)*KLW_WIDTH/KLW_SOURCE_WIDTH;
                    int32_t x1=(int32_t)(start+i+1u)*KLW_WIDTH/KLW_SOURCE_WIDTH;
                    uint32_t index=colors[i],a=KLW_ALPHA[index];
                    uint16_t c=KLW_PALETTE[index];
                    for(u=x0;u<x1;u++) {
                        int32_t px=x+(klay_flip()?2*KLW_ORIGIN_X-u:u);
                        if(px>=0 && px<240) blend_pixel(&cv_px[py*240+px],c,a);
                    }
                }
            }
        }
    }
}
static void draw_animation(void)
{
    uint32_t i;
    /* Only the mechanism is sent to the LCD on animation ticks. */
    cv_begin(240, 114, C_BLACK);
    for (i = 0; i < 240u * 114u; i++) cv_px[i] = CLAY_PALETTE[CLAY_ROOM[60u * 240u + i]];
    animate(60);
    draw_battery(60);
    draw_walker(60);
    cv_blit(0, 60);
}
static void draw(void)
{
    static const char *const notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const uint16_t ink = RGB(92, 46, 25), cream = RGB(255, 223, 157), faded = RGB(228, 191, 133);
    char preset[] = "01/00", note_text[8] = "--", oct[] = "OCT +0", effect_title[32] = "FX 00 ";
    uint32_t index = (uint32_t)program + 1u;
    int show_controls = controls_page || (popup_until && !klay_active());
    const char *family = clay_bank[program].family;
    const char *title = clay_bank[program].name;
    preset[0] = (char)('0' + index / 10u);
    preset[1] = (char)('0' + index % 10u);
    preset[3] = (char)('0' + CLAY_INSTRUMENTS/10u);
    preset[4] = (char)('0' + CLAY_INSTRUMENTS%10u);
    if (program == CLAY_EFFECTS_PROGRAM && last_effect >= 0) {
        const char *name=effects_label((uint32_t)last_effect);
        uint32_t i=6;
        effect_title[3] = (char)('0' + (last_effect+1)/10);
        effect_title[4] = (char)('0' + (last_effect+1)%10);
        while(*name && i<sizeof(effect_title)-1u) effect_title[i++]=*name++;
        effect_title[i]=0;
        title = effect_title;
    }
    if (last_note >= 0) {
        const char *name = notes[last_note % 12];
        uint32_t i = 0;
        int32_t o = last_note / 12 - 1;
        while (*name) note_text[i++] = *name++;
        if (o < 0) note_text[i++] = '-';
        note_text[i++] = (char)('0' + (o < 0 ? -o : o)); note_text[i] = 0;
    }
    oct[4] = octave < 0 ? '-' : '+'; oct[5] = (char)('0' + (octave < 0 ? -octave : octave));
    clay_background(0);
    if(!menu_mode()){animate(0);draw_walker(0);centered(7, &NEVERHOOD_MARK, "N", ink);}
    draw_battery(0);
    if(player_page)draw_player(0);
    else if (show_controls) draw_controls(0);
    cv_blit(0, 0);
    clay_background(120);
    if(!menu_mode()){animate(120);draw_walker(120);}
    draw_battery(120);
    if(player_page)draw_player(120);
    else if (show_controls) draw_controls(120);
    if(player_page){
        char value[12],line[32];int n=player_note(player_track,seq_step);
        centered(65,&FONT_S,player_playing()?"PLAYING":"STEP SEQUENCER",cream);
        decimal(value,seq_step+1u);cv_text(22,89,&FONT_XS,value,cream);
        if(n<0)cv_text(45,89,&FONT_XS,"REST",cream);else {
            const char *name=notes[n%12];uint32_t i=0;while(*name)line[i++]=*name++;
            if(n<12)line[i++]='-';line[i++]=(char)('0'+(n<12?1:n/12-1));line[i]=0;cv_text(45,89,&FONT_XS,line,cream);
        }
        decimal(value,player_gate(player_track,seq_step));cv_text(82,89,&FONT_XS,"G",faded);cv_text(94,89,&FONT_XS,value,cream);
        decimal(value,player_bpm());cv_text(122,89,&FONT_XS,value,cream);cv_text(148,89,&FONT_XS,"BPM",faded);
        decimal(value,player_gain(player_track));cv_text(184,89,&FONT_XS,value,cream);
    } else {
    centered(66, &FONT_S, title, RGB(48, 24, 15));
    centered(65, &FONT_S, title, cream);
    cv_text(24, 86, &FONT_S, note_text, cream);
    {
        int32_t end = cv_text(83, 89, &FONT_XS, family, faded);
        cv_text(end + 5, 89, &FONT_XS, preset, faded);
    }
    cv_text(174, 89, &FONT_XS, oct, faded);
    }
    cv_blit(0, 120);
    redraw = 0;
}
void app_splash(void) { init_art(); last_note=last_effect=-1; draw(); }
void app_ui_init(void) { update_battery(); redraw = 1; draw(); }
void app_frame(void)
{
    uint32_t now, level;
    int animate_frame = 0;
    input();
    update_battery();
    /* Advance the actor only when this call can actually paint him. Input-only
     * polls and note events must never consume unseen transition frames. */
    {
        static const int16_t targets[CATEGORIES] = {40, 186, 218, 130, 130, 12, 130, 83, 151};
        uint32_t c = category();
        int32_t target = (c == BANJO || c == GUITARS || c == KEYS) ?
            103 + 14 * (int32_t)(animation_note % 5u) : targets[c];
        if(c==EFFECTS) {
            static const int16_t fx_targets[]={40,88,151,70,211,186};
            target=fx_targets[effects_visual(last_effect>=0 ? (uint32_t)last_effect : EFFECTS_COUNT)];
        }
        if (klay_update(plat_ms(), instrument_level() >= 32u, target)) {
            walk_dirty = 1;
            if (!klay_active()) redraw = 1;
        }
    }
    now = plat_ms();
    if(player_page && (uint32_t)(now-player_ui_at)>=100u){player_ui_at=now;redraw=1;}
    /* Stop-motion at 20 fps, including a final clean frame after release. */
    if ((uint32_t)(now - animation_at) >= 50u) {
        level = instrument_level();
        if (level < 32u) level = 0;
        if (level || animation_level || walk_dirty) animate_frame = 1;
        animation_level = level;
        animation_step = now / 50u;
        animation_at = now;
    }
    if (redraw) draw();
    else if (animate_frame || walk_dirty) {
        if (menu_mode() || category()==BASS) draw();
        else if (!popup_until || klay_active()) draw_animation();
    }
    walk_dirty = 0;
}
void app_say(const char *a, const char *b) { (void)a; (void)b; redraw = 1; }

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if(!strcmp(what,"seq_step")) *got=seq_step;
    else if(!strcmp(what,"seq_note")) *got=player_note(player_track,seq_step);
    else if(!strcmp(what,"seq_gate")) *got=(int)player_gate(player_track,seq_step);
    else if(!strcmp(what,"seq_pattern")) *got=(int)player_pattern();
    else if(!strcmp(what,"seq_loop")) *got=(int)player_looping();
    else if(!strcmp(what,"seq_saved")) *got=seq_saved;
    else if(!strcmp(what,"seq_record")) *got=seq_record;
    else if(!strcmp(what,"player")) *got=(int)player_playing();
    else if(!strcmp(what,"player_page")) *got=player_page;
    else if(!strcmp(what,"player_tick")) *got=(int)player_tick();
    else if(!strcmp(what,"player_bpm")) *got=(int)player_bpm();
    else if(!strcmp(what,"player_track")) *got=player_track;
    else if(!strcmp(what,"player_preset")) *got=(int)player_preset(player_track);
    else if(!strcmp(what,"player_gain")) *got=(int)player_gain(player_track);
    else if(!strcmp(what,"player_muted")) *got=(int)player_muted(player_track);
    else if (!strcmp(what, "battery")) *got = battery_level;
    else if (!strcmp(what, "program")) *got = program;
    else if (!strcmp(what, "category")) *got = (int)category();
    else if (!strcmp(what, "walking")) *got = (int)klay_active();
    else if (!strcmp(what, "walk_x")) *got = (int)klay_x();
    else if (!strcmp(what, "steps")) *got = (int)klay_steps();
    else if (!strcmp(what, "klay_frame")) *got = (int)klay_frame();
    else if (!strcmp(what, "klay_state")) *got = (int)klay_state();
    else if (!strcmp(what, "watching")) *got = (int)klay_watching();
    else if (!strcmp(what, "look_left")) *got = (int)klay_flip();
    else if (!strcmp(what, "controls")) *got = controls_page;
    else if (!strcmp(what, "octave")) *got = octave;
    else if (!strcmp(what, "effect")) *got = last_effect;
    else if (!strcmp(what, "voices")) *got = (int)clay_active();
    else if (!strcmp(what, "faults")) *got = (int)clay_faults();
    else if (!strcmp(what, "dropped")) *got = (int)clay_dropped();
    else if (!strcmp(what, "tone")) *got = knobs[program][0];
    else if (!strcmp(what, "motion")) *got = knobs[program][1];
    else if (!strcmp(what, "decay")) *got = knobs[program][2];
    else if (!strcmp(what, "room")) *got = knobs[program][3];
    else return 0;
    return 1;
}
#endif
