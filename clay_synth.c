/* SPDX-License-Identifier: MIT */
/* Original synthesis sketches inspired by the documented Neverhood ensemble.
 * The drum kit keeps seven original synthesis patches on separate keys.
 * The same engine is compiled for the FM-1, host tests and audition renders. */
#include "clay_synth.h"
#include "plat.h"
#include "fastmath.h"
#include "synth_tables.h"
#include "effects_kit.h"
#include "music_bank.h"

#define RATE 44100.0f
#define STRING_SIZE 2048u
#define ROOM_SIZE 8192u
#define EVENT_SIZE 64u
enum { VOX, REED, THEREMIN, WHISTLE, STRING, ORGAN, MALLET, JUG, DROP, DRUM, BRUSH, BELLOWS, FXKIT, MUSIC, DRUMKIT };
enum { E_ON, E_OFF, E_PROGRAM, E_MACRO, E_SUSTAIN, E_PANIC, E_TRACK_PROGRAM, E_TRACK_VOLUME, E_TRACK_STOP };

const clay_instrument_t clay_bank[CLAY_INSTRUMENTS] = {
    /* name, family, engine, variant, transpose, attack, decay, sustain, release, gain */
    {"EFFECTS KIT",    "EFFECTS",FXKIT,   0,   0, .001f, 1.0f,1.00f, .02f, .65f},
    {"WEASEL SAX",     "HORNS", MUSIC,   4,   0, .012f, .30f, .85f, .18f, .65f},
    {"MUSIC BOX",      "KEYS",  MUSIC,   3,  12, .002f, 1.40f, .00f, .24f, .65f},
    {"KLAY HUM",       "VOICE", VOX,      0,   0, .022f, .25f, .80f, .14f, 3.0f},
    {"HOMINA MOUTH",   "VOICE", VOX,      1,   0, .010f, .18f, .85f, .12f, 2.5f},
    {"SCAT RADIO",     "VOICE", VOX,      2,   0, .004f, .25f, .72f, .10f, 2.3f},
    {"LOWDEE BASS",    "BASS", VOX,      3, -24, .012f, .22f, .80f, .16f, 3.4f},
    {"OXEN CHOIR",     "VOICE", VOX,      4,   0, .085f, .40f, .85f, .30f, 2.4f},
    {"LIP TRUMPET",    "HORNS", REED,     0,   0, .018f, .20f, .75f, .12f, 1.9f},
    {"CROOKED BRASS",  "HORNS", REED,     1, -12, .040f, .30f, .80f, .18f, 1.4f},
    {"WOBBLE THEREMIN","AIR",   THEREMIN, 0,   0, .080f, .40f, 1.00f, .25f, .34f},
    {"PORCH WHISTLE",  "AIR",   WHISTLE,  0,  12, .035f, .22f, .90f, .14f, .35f},
    {"BANJO BITS",     "BANJO",STRING,   0,   0, .001f, 1.30f,.00f, .10f, .80f},
    {"WAH BOOGIE",     "GUITAR",STRING,   2, -12, .002f, 1.80f,.00f, .14f, 1.30f},
    {"RUBBER BASS",    "BASS",STRING,   3, -24, .002f, 1.60f,.00f, .13f, 1.10f},
    {"RADIO B3",       "KEYS",  ORGAN,    0,   0, .008f, .20f, 1.00f, .09f, .33f},
    {"TOY BELLOWS",    "KEYS",  BELLOWS,  0,   0, .065f, .30f, .90f, .18f, .29f},
    {"SLIDE TWANG",    "GUITAR",STRING,   4,   0, .003f, 2.30f,.00f, .20f, .90f},
    {"DRUM KIT",       "DRUMS", DRUMKIT, 0,   0, .001f, 1.0f, .00f, .10f, .65f},
    {"ROOM BASS",      "BASS",MUSIC,   0, -24, .003f, .45f, .75f, .13f, .75f},
    {"ROOM GUITAR",    "GUITAR",MUSIC,   1,   0, .002f, .65f, .30f, .16f, .65f},
    {"ROOM KEYS",      "KEYS",  MUSIC,   2,   0, .003f, .60f, .65f, .18f, .65f},
};

static const clay_instrument_t drum_patches[CLAY_DRUM_COUNT] = {
    {"WOOD BONES",     "CLAY",  MALLET,   0,   0, .001f, .85f, .00f, .15f, .62f},
    {"CLAY JUG",       "CLAY",  JUG,      0, -12, .002f, .65f, .00f, .14f, .58f},
    {"WATER DROP",     "CLAY",  DROP,     0,   0, .001f, .55f, .00f, .12f, .48f},
    {"CLAY DRUM",      "DRUMS", DRUM,     0, -24, .001f, .65f, .00f, .10f, .68f},
    {"BRUSH RATTLE",   "DRUMS", BRUSH,    0,   0, .003f, .48f, .00f, .10f, .95f},
    {"TIN CHIMES",     "CLAY",  MALLET,   1,  12, .001f, 2.40f,.00f, .30f, .44f},
    {"GARGLE TOMS",    "DRUMS", JUG,      1, -12, .002f, .85f, .00f, .16f, .65f},
    {"COUGH DROPS",    "VOICE", VOX,      5, -12, .002f, .32f, .00f, .10f, 1.9f},
};

typedef struct { float c, r2, gain, y1, y2, x1, x2; } resonator_t;
typedef struct {
    float string[STRING_SIZE];
    resonator_t formant[3];
    float phase[4], mode_level[4], mode_decay[4];
    float hz, target_hz, velocity, env, attack_step, decay_mul, release_mul;
    float time, lfo, vib, delay, feedback, low, band, previous, air, noise_lp;
    float f1, f2, f3, filter_f, pulse_width, pan;
    effects_voice_t sample;
    float sample_release, cross_from, last_out;
    uint32_t crossfade;
    uint32_t rng, write, age, instrument, note, channel;
    uint8_t active, stage, gate, sustained;
} voice_t;
typedef struct { uint8_t type, a, b, channel; } event_t;
static voice_t voices[CLAY_TOTAL_VOICES] PLAT_POOL;
static const uint8_t track_first[4]={4,5,6,10},track_count[4]={1,1,4,3};
static float room[ROOM_SIZE] PLAT_POOL;
static event_t events[EVENT_SIZE];
static volatile uint32_t event_write, event_read, overflow, fault_count, dropped_count, active_count;
static volatile uint32_t visual_level;
static uint32_t current_program, clock_age, room_write, control_due;
static uint8_t sustain[17];
static uint8_t track_program[CLAY_TRACKS], track_volume[CLAY_TRACKS];
static float track_gain[CLAY_TRACKS];
static volatile uint32_t track_level[CLAY_TRACKS];
static volatile uint32_t track_notes[CLAY_TRACKS][4];
static const float track_macro[4]={.5f,.5f,.5f,.25f};
static const float *voice_macro(const voice_t *v);

/* Last-note priority for the melodic synthesized engines; kits/samples stay polyphonic. */
static uint8_t mono_note[128], mono_channel[128], mono_velocity[128];
static uint32_t mono_count;
static int melodic_synth(uint32_t instrument)
{
    uint32_t engine=clay_bank[instrument].engine;
    return engine != FXKIT && engine != MUSIC && engine != DRUMKIT;
}
static void mono_remove(uint32_t note, uint32_t channel)
{
    uint32_t i,j;
    for(i=0;i<mono_count;i++) if(mono_note[i]==note && mono_channel[i]==channel) {
        for(j=i+1;j<mono_count;j++) {
            mono_note[j-1]=mono_note[j]; mono_channel[j-1]=mono_channel[j]; mono_velocity[j-1]=mono_velocity[j];
        }
        mono_count--; return;
    }
}
static float macro[4], master_gain, room_tone, dc_x[2], dc_y[2];
static const float *voice_macro(const voice_t *v) { return v->channel>=17u ? track_macro : macro; }

/* No hosted C library headers are available in the JieLi freestanding build. */
static void clear(void *memory, uint32_t bytes)
{
    uint8_t *p = memory;
    while (bytes--) *p++ = 0;
}
static void fence(void) { __asm__ volatile("" ::: "memory"); }
static void post(uint32_t type, uint32_t a, uint32_t b, uint32_t channel)
{
    uint32_t w = event_write;
    if (w - event_read >= EVENT_SIZE) { overflow = 1; dropped_count++; return; }
    events[w & (EVENT_SIZE - 1u)] = (event_t){(uint8_t)type, (uint8_t)a, (uint8_t)b, (uint8_t)channel};
    fence();
    event_write = w + 1u;
}
void clay_note_on(uint32_t n, uint32_t v, uint32_t c)
{
    if (n < 128u && c < 17u) post(v ? E_ON : E_OFF, n, v > 127u ? 127u : v, c);
}
void clay_note_off(uint32_t n, uint32_t c) { if (n < 128u && c < 17u) post(E_OFF, n, 0, c); }
void clay_program(uint32_t p) { if (p < CLAY_INSTRUMENTS) post(E_PROGRAM, p, 0, 0); }
void clay_macro(uint32_t m, uint32_t v) { if (m < 4u) post(E_MACRO, m, v > 100u ? 100u : v, 0); }
void clay_sustain(uint32_t c, uint32_t down) { if (c < 17u) post(E_SUSTAIN, down != 0, 0, c); }
void clay_track_program(uint32_t t,uint32_t p) { if(t<4u && p<CLAY_INSTRUMENTS) post(E_TRACK_PROGRAM,p,0,17u+t); }
void clay_track_volume(uint32_t t,uint32_t v) { if(t<4u) post(E_TRACK_VOLUME,v>100u?100u:v,0,17u+t); }
void clay_track_note_on(uint32_t t,uint32_t n,uint32_t v) { if(t<4u && n<128u) post(v?E_ON:E_OFF,n,v>127u?127u:v,17u+t); }
void clay_track_note_off(uint32_t t,uint32_t n) { if(t<4u && n<128u) post(E_OFF,n,0,17u+t); }
void clay_track_stop(uint32_t t) { if(t<4u) post(E_TRACK_STOP,0,0,17u+t); }
uint32_t clay_track_level(uint32_t t) { return t<4u?track_level[t]:0u; }
uint32_t clay_track_keys(uint32_t t,int32_t base){
    uint32_t k,keys=0;if(t>=4u || !track_volume[t])return 0;
    for(k=0;k<NKEYS;k++){int32_t n=base+(int32_t)k;if(n>=0 && n<128 && (track_notes[t][(uint32_t)n/32u]&(1u<<((uint32_t)n%32u))))keys|=1u<<k;}
    return keys;
}
void clay_all_off(void) { post(E_PANIC, 0, 0, 0); }
uint32_t clay_active(void) { return active_count; }
uint32_t clay_visual_level(void) { return visual_level; }
uint32_t clay_faults(void) { return fault_count; }
uint32_t clay_dropped(void) { return dropped_count; }

static float sine(float phase)
{
    float wrapped = phase - (float)(int32_t)phase;
    float u;
    if (wrapped < 0.0f) wrapped += 1.0f;
    u = wrapped * 1024.0f;
    if (u >= 1024.0f) u = 0.0f;
    uint32_t i = (uint32_t)u;
    return CLAY_SINE[i] + (CLAY_SINE[i + 1u] - CLAY_SINE[i]) * (u - (float)i);
}
static float advance(float *phase, float step)
{
    *phase += step;
    *phase -= (float)(uint32_t)*phase;
    return *phase;
}
static float noise(voice_t *v)
{
    uint32_t n = v->rng;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    v->rng = n;
    return (float)(n >> 8) * (1.0f / 8388608.0f) - 1.0f;
}
static float blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
static float buzz(float p, float dt, float width)
{
    float shifted = p - width;
    if (shifted < 0.0f) shifted += 1.0f;
    return (p < width ? 1.0f : -1.0f) + 1.0f - 2.0f * width + blep(p, dt) - blep(shifted, dt);
}
static void tune(resonator_t *r, float hz, float bandwidth)
{
    float radius = fm_expf(-FM_PI * bandwidth / RATE);
    hz = fm_clampf(hz, 100.0f, 7500.0f);
    r->c = 2.0f * radius * fm_cosf(FM_TWO_PI * hz / RATE);
    r->r2 = radius * radius;
    r->gain = 1.0f - radius;
}
static float resonate(resonator_t *r, float x)
{
    /* A band-pass numerator rejects DC even at low formant frequencies. */
    float y = r->gain * (x - r->x2) + r->c * r->y1 - r->r2 * r->y2;
    r->x2 = r->x1; r->x1 = x;
    r->y2 = r->y1; r->y1 = fm_flush(y);
    return y;
}
static void release_voice(voice_t *v)
{
    v->gate = v->sustained = 0;
    v->stage = 2;
}
static void panic_now(void)
{
    uint32_t i;
    for (i = 0; i < CLAY_TOTAL_VOICES; i++) voices[i].active = 0;
    clear(room, sizeof room);
    clear(sustain, sizeof sustain);
    clear(dc_x, sizeof dc_x);
    clear(dc_y, sizeof dc_y);
    room_tone = 0.0f;
    active_count = visual_level = mono_count = 0;
}
void clay_init(void)
{
    clear(voices, sizeof voices);
    clear(events, sizeof events);
    event_write = event_read = overflow = fault_count = dropped_count = 0;
    clear(track_program,sizeof track_program); clear((void *)track_level,sizeof track_level);clear((void *)track_notes,sizeof track_notes);
    {uint32_t t;for(t=0;t<4u;t++){track_volume[t]=80;track_gain[t]=.8f;}}
    current_program = clock_age = room_write = control_due = 0;
    macro[0] = macro[1] = macro[2] = .5f; macro[3] = .25f;
    master_gain = 0.0f;
    panic_now();
}

static const clay_instrument_t *voice_patch(const voice_t *v)
{
    return v->instrument == CLAY_DRUM_PROGRAM ? &drum_patches[v->write] : &clay_bank[v->instrument];
}

static void start_voice(uint32_t note, uint32_t velocity, uint32_t channel)
{
    uint32_t i, pick = 0, oldest = 0xFFFFFFFFu;
    voice_t *v;
    uint32_t instrument=channel>=17u?track_program[channel-17u]:current_program;
    const float *params=channel>=17u?track_macro:macro;
    float previous;
    const clay_instrument_t *p = &clay_bank[instrument];
    uint32_t drum = (note + 7u*128u - CLAY_DRUM_BASE_NOTE) % CLAY_DRUM_COUNT;
    int sample = effects_index(note);
    if (channel<17u && melodic_synth(instrument)) {
        mono_remove(note,channel);
        if(mono_count==128u) mono_remove(mono_note[0],mono_channel[0]);
        mono_note[mono_count]=(uint8_t)note; mono_channel[mono_count]=(uint8_t)channel;
        mono_velocity[mono_count++]=(uint8_t)velocity;
        for(i=0;i<CLAY_VOICES;i++) if(voices[i].active && voices[i].instrument==instrument && voices[i].channel<17u) {
            v=&voices[i];
            v->note=note; v->channel=channel; v->gate=1; v->sustained=0;
            v->target_hz=fm_clampf(440.0f*fm_exp2f(((float)note+p->transpose-69.0f)/12.0f),24.0f,6000.0f);
            v->velocity=fm_sqrtf((float)velocity/127.0f);
            /* Keep oscillator/filter/string state; a short pitch glide avoids hard cuts. */
            v->stage=0; control_due=0;
            return;
        }
    }
    if (p->engine == DRUMKIT) p = &drum_patches[drum];
    if (p->engine == FXKIT && sample < 0) return;
    {uint32_t first=channel>=17u?track_first[channel-17u]:0;
    uint32_t end=channel>=17u?first+track_count[channel-17u]:CLAY_VOICES;
    pick=first;
    for (i = first; i < end; i++) {
        if (!voices[i].active) { pick = i; break; }
        if (voices[i].note == note && voices[i].channel == channel) { pick = i; break; }
        if (voices[i].age < oldest) { oldest = voices[i].age; pick = i; }
    }
    }
    v = &voices[pick];
    previous=v->active?v->last_out:0.0f;
    clear(v, sizeof *v);
    v->cross_from=previous;v->crossfade=128u;
    v->active = v->gate = 1;
    v->note = note; v->channel = channel; v->instrument = instrument; v->age = ++clock_age;
    v->rng = 0xA51C32E9u ^ (note * 7919u + v->age * 65537u + instrument * 131u);
    if (!v->rng) v->rng = 1;
    v->write = drum;
    v->hz = fm_clampf(440.0f * fm_exp2f(((float)(instrument == CLAY_DRUM_PROGRAM ? 60u : note) + p->transpose - 69.0f) / 12.0f), 24.0f, 6000.0f);
    v->target_hz = v->hz;
    v->velocity = fm_sqrtf((float)velocity / 127.0f);
    v->attack_step = 1.0f / (RATE * p->attack);
    v->pan = channel>=17u ? .35f+.10f*(float)(channel-17u) : .30f + .40f * (float)pick / (float)(CLAY_VOICES - 1u);
    v->lfo = (float)(v->rng & 255u) / 256.0f;
    if (p->engine == FXKIT) {
        effects_start(&v->sample, (uint32_t)sample);
        v->stage = 1;
        v->sample_release = 1.0f;
    }
    if (p->engine == MUSIC) v->write = music_select(p->variant, note);
    for (i = 0; i < 4u; i++) v->mode_level[i] = 1.0f;
    if (p->engine == STRING) {
        v->write = 0;
        float color = p->variant == 0 ? .90f : p->variant == 3 ? .14f : .42f;
        float low = 0.0f;
        color *= .6f + params[0];
        color = fm_minf(color, 1.0f);
        for (i = 0; i < STRING_SIZE; i++) {
            low += color * (noise(v) - low);
            v->string[i] = low * .65f;
        }
    }
    control_due = 0;
}

void clay_track_config_rt(uint32_t t,uint32_t p,uint32_t volume)
{
    if(t>=4u || p>=CLAY_INSTRUMENTS) return;
    if(track_program[t]!=p) {track_program[t]=(uint8_t)p;clay_track_stop_rt(t);}
    track_volume[t]=(uint8_t)(volume>100u?100u:volume);
}
void clay_track_stop_rt(uint32_t t)
{
    uint32_t i;if(t<4u)for(i=track_first[t];i<track_first[t]+track_count[t];i++)release_voice(&voices[i]);
}
void clay_track_event_rt(uint32_t t,uint32_t note,uint32_t velocity)
{
    voice_t *v;
    if(t>=4u || note>=128u) return;
    if(velocity) {start_voice(note,velocity,17u+t);return;}
    {uint32_t i;for(i=track_first[t];i<track_first[t]+track_count[t];i++){
    v=&voices[i];
    if(v->active && v->note==note) {
        const clay_instrument_t *p=&clay_bank[v->instrument];
        if(p->engine!=FXKIT && p->engine!=DRUMKIT && !(p->engine==MUSIC && p->variant==3u)) release_voice(v);
    }
    }}
}

static void drain_events(void)
{
    while (event_read != event_write) {
        uint32_t i, r = event_read;
        event_t e;
        fence(); e = events[r & (EVENT_SIZE - 1u)]; fence(); event_read = r + 1u;
        if (e.type == E_ON) start_voice(e.a, e.b, e.channel);
        else if (e.type == E_OFF) {
            if(e.channel<17u && melodic_synth(current_program)) mono_remove(e.a,e.channel);
            for (i = 0; i < CLAY_TOTAL_VOICES; i++) {
                voice_t *v = &voices[i];
                if (v->active && v->note == e.a && v->channel == e.channel) {
                    if (clay_bank[v->instrument].engine == FXKIT ||
                        clay_bank[v->instrument].engine == DRUMKIT ||
                        (clay_bank[v->instrument].engine == MUSIC &&
                         clay_bank[v->instrument].variant == 3u)) continue;
                    if(e.channel<17u && melodic_synth(v->instrument) && mono_count) {
                        uint32_t last=mono_count-1u;
                        start_voice(mono_note[last],mono_velocity[last],mono_channel[last]);
                        continue;
                    }
                    v->gate = 0;
                    if (e.channel<17u && sustain[e.channel]) v->sustained = 1;
                    else release_voice(v);
                }
            }
        } else if (e.type == E_PROGRAM) {
            current_program = e.a; mono_count=0;
            for (i = 0; i < CLAY_VOICES; i++) release_voice(&voices[i]);
        } else if (e.type == E_MACRO) {
            macro[e.a] = (float)e.b * .01f;
            control_due = 0;
        } else if (e.type == E_SUSTAIN) {
            sustain[e.channel] = e.a;
            if (!e.a) for (i = 0; i < CLAY_TOTAL_VOICES; i++)
                if (voices[i].channel == e.channel && voices[i].sustained) release_voice(&voices[i]);
        } else if(e.type==E_TRACK_PROGRAM) {
            uint32_t t=e.channel-17u; track_program[t]=e.a;
            clay_track_stop_rt(t);
        } else if(e.type==E_TRACK_VOLUME) track_volume[e.channel-17u]=e.a;
        else if(e.type==E_TRACK_STOP) clay_track_stop_rt(e.channel-17u);
        else if (e.type == E_PANIC) panic_now();
    }
    if (overflow) { panic_now(); overflow = 0; }
}

static void controls(voice_t *v)
{
    static const float vowel[6][3] = {{350, 850, 2350}, {700, 1250, 2600}, {320, 2150, 3000},
                                     {450, 1050, 2450}, {280, 1450, 2450}, {600, 1300, 2500}};
    const clay_instrument_t *p = voice_patch(v);
    const float *params=voice_macro(v);
    float time_scale = fm_exp2f((params[2] - .5f) * 3.0f);
    float movement = params[1], tone = params[0], pitch;
    if(melodic_synth(v->instrument)) v->hz += (v->target_hz-v->hz)*.06f;
    pitch=v->hz;
    uint32_t j;
    if (p->engine == FXKIT) {
        v->f1 = ((float)EFFECTS_RATE/RATE) * fm_exp2f((params[1]-.5f)*2.0f); /* speed 0.5..2 */
        v->filter_f = tone < .5f ? .01f+tone*1.98f : 1.0f;
        return;
    }
    v->decay_mul = fm_expf(-6.9078f / (RATE * p->decay * time_scale));
    v->release_mul = fm_expf(-9.2103f / (RATE * p->release * time_scale));
    advance(&v->lfo, (3.8f + 3.5f * movement) * (32.0f / RATE));
    v->vib = sine(v->lfo);
    if (p->engine == MUSIC) {
        v->f1 = music_step(v->write, pitch) * (1.0f+v->vib*movement*.006f);
        v->filter_f = tone < .5f ? .01f+tone*1.98f : 1.0f;
        return;
    }
    if (p->engine == THEREMIN || (p->engine == STRING && p->variant == 4))
        pitch *= fm_exp2f(-(.08f + movement * .65f) * fm_expf(-v->time * 9.0f));
    v->f1 = pitch * (1.0f + v->vib * movement * (p->engine == THEREMIN ? .018f : .0035f));
    v->delay = fm_clampf(RATE / v->f1 - .5f, 6.0f, (float)STRING_SIZE - 2.0f);
    v->feedback = fm_expf(-6.9078f / (v->hz * p->decay * time_scale * 2.5f));
    v->filter_f = 2.0f * fm_sinf(FM_PI * (220.0f + tone * 3100.0f) / RATE);
    v->pulse_width = .20f + .16f * tone;
    if (p->engine == VOX || p->engine == REED) {
        float morph = .5f + .5f * sine(v->time * (p->variant == 2 ? 5.3f : 2.8f));
        uint32_t a = p->engine == REED ? 1u : p->variant % 6u;
        uint32_t b = p->variant == 3 ? 0u : (a + 1u) % 6u;
        float scale = .70f + .60f * tone;
        if (p->variant == 0 || p->variant == 4) morph *= .12f;
        else morph *= movement;
        for (j = 0; j < 3u; j++) {
            float hz = (vowel[a][j] + (vowel[b][j] - vowel[a][j]) * morph) * scale;
            if (p->engine == REED) hz *= p->variant ? .77f : 1.14f;
            tune(&v->formant[j], hz, 75.0f + 55.0f * (float)j + movement * 35.0f);
        }
    }
    for (j = 0; j < 4u; j++)
        v->mode_decay[j] = fm_expf(-(1.0f + (float)j * .75f) / (RATE * p->decay * time_scale));
}

static float voice_sample(voice_t *v)
{
    const clay_instrument_t *p = voice_patch(v);
    const float *params=voice_macro(v);
    if (p->engine == FXKIT) {
        uint32_t length = (uint32_t)((float)effects_frames(v->sample.sample) * (.05f+.95f*params[2]));
        float value = effects_read(&v->sample, v->f1, length), edge = 1.0f;
        if (v->sample.finished) { v->active = 0; v->env = 0.0f; return 0.0f; }
        if (v->sample.position < 16u) edge = (float)v->sample.position / 16.0f;
        if (v->sample.position >= length) edge = 0.0f;
        else if (length-v->sample.position < 32u) edge *= (float)(length-v->sample.position)/32.0f;
        if (v->stage == 2) {
            v->sample_release *= .99f;
            if (v->sample_release < .00001f) { v->active = 0; return 0.0f; }
        }
        v->noise_lp += v->filter_f*(value-v->noise_lp);
        value = v->noise_lp * edge * v->sample_release;
        v->env = fm_maxf(fm_fabsf(value), v->env*.999f);
        return value * v->velocity * p->gain;
    }
    float dt = v->f1 / RATE, ph = p->engine == MUSIC ? 0.0f : advance(&v->phase[0], dt), x = 0.0f;
    float tone = params[0], motion = params[1], n;
    uint32_t j;
    if (v->stage == 0) {
        v->env += v->attack_step;
        if (v->env >= 1.0f) { v->env = 1.0f; v->stage = 1; }
    } else if (v->stage == 1) v->env = p->sustain + (v->env - p->sustain) * v->decay_mul;
    else v->env *= v->release_mul;
    if (v->env < .00001f && v->stage != 0) { v->active = 0; return 0.0f; }
    v->time += 1.0f / RATE;

    if (p->engine == MUSIC) {
        if (v->phase[0] >= (float)music_frames(v->write)) { v->active = 0; v->env = 0.0f; return 0.0f; }
        x = music_read(v->write, &v->phase[0], v->f1);
        v->noise_lp += v->filter_f*(x-v->noise_lp);
        return v->noise_lp*v->env*v->velocity*p->gain;
    }

    if (p->engine == VOX || p->engine == REED) {
        float source = buzz(ph, dt, v->pulse_width) * .55f + sine(ph) * .16f;
        float syllable = 1.0f;
        if (p->variant == 4 && p->engine == VOX) {
            float q = advance(&v->phase[1], dt * 1.0041f), r = advance(&v->phase[2], dt * .9953f);
            source = (source + .4f * buzz(q, dt, .31f) + .4f * buzz(r, dt, .27f)) * .6f;
        }
        n = noise(v);
        v->noise_lp += .20f * (n - v->noise_lp);
        source += v->noise_lp * (.012f + motion * .045f);
        if (p->variant == 2 && p->engine == VOX) {
            float sp = sine(v->time * (2.5f + motion * 6.5f));
            syllable = .20f + .80f * fm_maxf(0.0f, sp);
            source += (n - v->noise_lp) * .16f * fm_maxf(0.0f, 1.0f - v->time * 20.0f);
        }
        if (p->variant == 5 && p->engine == VOX) {
            float choke = sine(v->time * (17.0f + motion * 22.0f));
            source = .65f * n + source * .4f;
            syllable = .20f + .80f * fm_fabsf(choke);
        }
        x = (resonate(&v->formant[0], source) + .65f * resonate(&v->formant[1], source)
             + .32f * resonate(&v->formant[2], source)) * syllable;
        if (p->engine == REED) x += .11f * source + .08f * sine(ph);
        if (p->variant == 3 && p->engine == VOX) x += .045f * sine(ph);
    } else if (p->engine == THEREMIN) {
        x = sine(ph) + tone * .13f * sine(ph * 2.0f);
    } else if (p->engine == WHISTLE) {
        n = noise(v);
        v->noise_lp += .06f * (n - v->noise_lp);
        x = sine(ph) * (.90f + .10f * v->vib * motion) + tone * .1f * sine(ph * 2.0f)
            + v->noise_lp * (.05f + motion * .15f);
    } else if (p->engine == STRING) {
        float read = (float)v->write - v->delay, fraction, old, average;
        uint32_t index;
        if (read < 0.0f) read += (float)STRING_SIZE;
        index = (uint32_t)read; fraction = read - (float)index;
        old = v->string[index] + (v->string[(index + 1u) & (STRING_SIZE - 1u)] - v->string[index]) * fraction;
        average = .5f * (old + v->previous);
        v->previous = old;
        v->string[v->write] = fm_flush(average * v->feedback);
        v->write = (v->write + 1u) & (STRING_SIZE - 1u);
        x = old;
        if (p->variant == 0) x += .17f * sine(ph * 2.7f) * v->env;
        if (p->variant == 2) {
            float f = v->filter_f * (.3f + .7f * v->env);
            v->low += f * v->band;
            v->band += f * (old - v->low - .55f * v->band);
            x = (v->band * 2.5f + v->low * .20f);
            x = x / (1.0f + fm_fabsf(x));
        } else if (p->variant == 3) {
            v->low += (.08f + tone * .15f) * (old - v->low);
            x = v->low + .12f * sine(ph) * v->env;
        }
    } else if (p->engine == ORGAN) {
        x = .7f * sine(ph) + .35f * sine(ph * 2.0f) + .23f * sine(ph * 3.0f)
            + tone * (.20f * sine(ph * 4.0f) + .13f * sine(ph * 6.0f) + .10f * sine(ph * 8.0f));
        x *= .90f + .10f * v->vib * motion;
        x += noise(v) * .035f * fm_maxf(0.0f, 1.0f - v->time * 120.0f);
    } else if (p->engine == BELLOWS) {
        float q = advance(&v->phase[1], dt * 1.006f), r = advance(&v->phase[2], dt * 2.003f);
        x = .45f * (buzz(ph, dt, .48f) + .7f * buzz(q, dt, .51f) + .3f * buzz(r, dt * 2.003f, .45f));
        v->low += v->filter_f * .5f * (x - v->low);
        x = v->low * (.90f + .10f * motion * v->vib);
    } else if (p->engine == MALLET) {
        static const float ratios[2][4] = {{1.0f, 2.76f, 5.40f, 8.93f}, {1.0f, 2.13f, 3.91f, 6.26f}};
        static const float gains[4] = {.80f, .40f, .22f, .12f};
        for (j = 0; j < 4u; j++) {
            float f = v->f1 * ratios[p->variant][j];
            /* Drop modes above Nyquist instead of folding them into the audible band. */
            if (f < 18000.0f) {
                float q = j ? advance(&v->phase[j], f / RATE) : ph;
                x += sine(q) * gains[j] * v->mode_level[j] * (j ? .3f + 1.4f * tone : 1.0f);
            }
            v->mode_level[j] *= v->mode_decay[j];
        }
    } else if (p->engine == JUG) {
        float bubble = .25f * motion * sine(v->time * (p->variant ? 21.0f : 8.0f));
        float q = advance(&v->phase[1], dt * (1.46f + bubble));
        x = .8f * sine(ph + .08f * sine(q)) + .20f * tone * sine(q);
        x += noise(v) * .2f * fm_maxf(0.0f, 1.0f - v->time * 25.0f);
        if (p->variant) x *= .35f + .65f * fm_fabsf(sine(v->time * (12.0f + motion * 20.0f)));
    } else if (p->engine == DROP) {
        float sweep = 1.0f + (1.0f + motion * 3.0f) * v->env * v->env;
        float q = advance(&v->phase[1], dt * sweep);
        x = sine(q + .12f * tone * sine(q * 2.7f));
    } else if (p->engine == DRUM) {
        float q = advance(&v->phase[1], dt * (1.0f + v->env * v->env * (1.2f + motion * 2.0f)));
        x = sine(q) + noise(v) * (.05f + tone * .3f) * fm_maxf(0.0f, 1.0f - v->time * 100.0f);
    } else if (p->engine == BRUSH) {
        n = noise(v);
        v->noise_lp += (.02f + tone * .35f) * (n - v->noise_lp);
        x = (n - v->noise_lp) * (.60f + .40f * motion * sine(v->time * (35.0f + tone * 70.0f)));
        x *= .4f + tone * .4f;
    }
    return x * v->env * v->velocity * p->gain;
}

void clay_render(int32_t *lr, uint32_t frames, uint32_t master_q12)
{
    uint32_t i, j, count = 0, level = 0;
    float master_target = (float)master_q12 * (1.0f / 4096.0f);
    drain_events();
    for (i = 0; i < frames; i++) {
        float left = 0.0f, right = 0.0f, send, a, b, wet, output[2];
        if (!control_due) {
            for (j = 0; j < CLAY_TOTAL_VOICES; j++) if (voices[j].active) controls(&voices[j]);
            control_due = 32u;
        }
        control_due--;
        for(j=0;j<4u;j++) track_gain[j]+=((float)track_volume[j]*.01f-track_gain[j])*.004f;
        for (j = 0; j < CLAY_TOTAL_VOICES; j++) if (voices[j].active) {
            float x = voice_sample(&voices[j]);
            if(voices[j].crossfade) {
                float old=(float)voices[j].crossfade*(1.0f/128.0f);
                x=x*(1.0f-old)+voices[j].cross_from*old;voices[j].crossfade--;
            }
            voices[j].last_out=x;
            if(j>=CLAY_VOICES) x*=track_gain[voices[j].channel-17u];
            left += x * (1.0f - voices[j].pan);
            right += x * voices[j].pan;
        }
        send = (left + right) * .45f;
        a = room[(room_write - 2851u) & (ROOM_SIZE - 1u)];
        b = room[(room_write - 4211u) & (ROOM_SIZE - 1u)];
        room_tone += .35f * ((a + b) * .5f - room_tone);
        room[room_write] = fm_flush(send + room_tone * (.15f + .38f * macro[3]));
        room_write = (room_write + 1u) & (ROOM_SIZE - 1u);
        wet = macro[3] * .65f;
        output[0] = left + wet * (a + .35f * b);
        output[1] = right + wet * (b + .35f * a);
        master_gain += (master_target - master_gain) * .004f;
        for (j = 0; j < 2u; j++) {
            float x = output[j], y;
            /* Catch NaN/unstable DSP before converting it to an integer sample. */
            if (!(x >= -64.0f && x <= 64.0f)) { x = 0.0f; fault_count++; }
            y = x - dc_x[j] + .995f * dc_y[j];
            dc_x[j] = x; dc_y[j] = fm_flush(y);
            y = y * 2.0f / (1.0f + fm_fabsf(y * 2.0f));
            lr[2u * i + j] = (int32_t)(y * master_gain * 8000000.0f);
        }
    }
    clear((void *)track_level,sizeof track_level);clear((void *)track_notes,sizeof track_notes);
    for (j = 0; j < CLAY_TOTAL_VOICES; j++) if (voices[j].active) {
        uint32_t envelope = (uint32_t)(voices[j].env * voices[j].velocity * 1024.0f);
        if(j>=CLAY_VOICES) {uint32_t t=voices[j].channel-17u;if(voices[j].gate)track_notes[t][voices[j].note/32u]|=1u<<(voices[j].note%32u);uint32_t level=(uint32_t)((float)envelope*track_gain[t]);if(level>track_level[t])track_level[t]=level;continue;}
        count++;
        if (voices[j].instrument == current_program && envelope > level) level = envelope;
    }
    active_count = count;
    visual_level = level;
}
