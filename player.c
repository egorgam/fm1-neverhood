/* SPDX-License-Identifier: MIT */
#include "player.h"
#include "clay_synth.h"
#include "theme_data.h"
#include "plat.h"
#include "app.h"

#define CLOCK 2646000u
#define MAGIC 0x53455133u
#define BARS ((THEME_END+383u)/384u)
/* One A/B flash object per lane; each copy fits one 4 KiB sector. */
typedef struct {uint32_t magic,generation;uint16_t tempo;uint8_t program,gain;uint16_t cells[BARS][16];} lane_t;
static lane_t lanes[4];
static volatile uint32_t playing,command,bpm,position,looping,pattern;
static volatile uint8_t preset[4],volume[4],muted[4];
static uint32_t phase,generation,cursor[4],tempo_index;
static uint16_t off_at[4][128];
static int32_t bound(int32_t x,int32_t a,int32_t b){return x<a?a:x>b?b:x;}
static void factory(void){
    uint32_t t,i;bpm=112;generation=0;
    for(t=0;t<4;t++){
        lanes[t].magic=lanes[t].generation=0;lanes[t].tempo=0;lanes[t].program=lanes[t].gain=0;
        preset[t]=THEME_PRESETS[t];volume[t]=THEME_VOLUMES[t];muted[t]=0;
        for(i=0;i<BARS*16;i++)lanes[t].cells[i/16][i%16]=65535u;
    }
}
static uint32_t source_at(uint32_t t,uint32_t tick){
    uint32_t lo=0,hi=THEME_COUNTS[t];while(lo<hi){uint32_t m=(lo+hi)/2;if(THEME_LANES[t][m].tick<tick)lo=m+1;else hi=m;}return lo;
}
static const theme_note_t *source_cell(uint32_t t,uint32_t s){
    uint32_t tick=(pattern*16u+s)*24u,i=source_at(t,tick);
    return i<THEME_COUNTS[t] && THEME_LANES[t][i].tick<tick+24u?&THEME_LANES[t][i]:0;
}

void player_init(void){
    uint32_t t;int valid=1;playing=command=position=phase=looping=pattern=0;factory();
    for(t=0;t<4;t++){
        if(plat_store_load(OBJ_SEQ0+t,&lanes[t],sizeof lanes[t])!=(int)sizeof lanes[t] || lanes[t].magic!=MAGIC || lanes[t].program>=CLAY_INSTRUMENTS || lanes[t].gain>100 || lanes[t].tempo<40 || lanes[t].tempo>200)valid=0;
        if(t && lanes[t].generation!=lanes[0].generation)valid=0;
    }
    if(!valid)factory();else {
        generation=lanes[0].generation;bpm=lanes[0].tempo;
        for(t=0;t<4;t++){uint32_t s;for(s=0;s<BARS*16;s++){uint16_t c=lanes[t].cells[s/16][s%16];if(c!=65535u && ((c&255)>128 || (c>>8)>31))valid=0;}
            preset[t]=lanes[t].program;volume[t]=lanes[t].gain;}
        if(!valid)factory();
    }
    for(t=0;t<4;t++){uint32_t n;for(n=0;n<128;n++)off_at[t][n]=0;}
}
int player_save(void){
    uint32_t t;if(playing || command==1)return -1;
    generation++;
    for(t=0;t<4;t++){
        lanes[t].magic=MAGIC;lanes[t].generation=generation;lanes[t].tempo=bpm;lanes[t].program=preset[t];lanes[t].gain=volume[t];
        if(plat_store_save(OBJ_SEQ0+t,&lanes[t],sizeof lanes[t]))return -1;
    }
    return 0;
}
void player_toggle(void){command=playing?2u:1u;}
void player_stop(void){command=2u;}
void player_tempo(int32_t x){bpm=(uint32_t)bound(x,40,200);}
void player_program(uint32_t t,int32_t x){if(t<4u)preset[t]=(uint8_t)((x%(int32_t)CLAY_INSTRUMENTS+(int32_t)CLAY_INSTRUMENTS)%(int32_t)CLAY_INSTRUMENTS);}
void player_volume(uint32_t t,int32_t x){if(t<4u)volume[t]=(uint8_t)bound(x,0,100);}
void player_mute(uint32_t t){if(t<4u)muted[t]^=1u;}
uint32_t player_playing(void){return playing;}
uint32_t player_tick(void){return position;}
uint32_t player_bpm(void){return bpm;}
uint32_t player_preset(uint32_t t){return t<4u?preset[t]:0;}
uint32_t player_gain(uint32_t t){return t<4u?volume[t]:0;}
uint32_t player_muted(uint32_t t){return t<4u?muted[t]:0;}
uint32_t player_bars(void){return BARS;}
uint32_t player_pattern(void){return pattern;}
void player_select(int32_t p){pattern=(uint32_t)bound(p,0,BARS-1);}
void player_loop(void){looping^=1;}
uint32_t player_looping(void){return looping;}
int player_note(uint32_t t,uint32_t s){
    uint16_t c;const theme_note_t *n;if(t>=4 || s>=16)return -1;c=lanes[t].cells[pattern][s];
    if(c!=65535u)return (int)(c&255)-1;n=source_cell(t,s);return n?n->note:-1;
}
uint32_t player_gate(uint32_t t,uint32_t s){
    uint16_t c;const theme_note_t *n;if(t>=4 || s>=16)return 0;c=lanes[t].cells[pattern][s];
    if(c!=65535u)return c>>8;n=source_cell(t,s);return n?(uint32_t)bound((n->length+23u)/24u,1,31):0;
}
void player_edit(uint32_t t,uint32_t s,int note,uint32_t gate){
    if(t>=4 || s>=16)return;
    /* A single aligned halfword is also the audio reader's atomic snapshot. */
    lanes[t].cells[pattern][s]=note<0?0:(uint16_t)(bound(note,0,127)+1+(bound(gate,1,31)<<8));
}
void player_clear(void){uint32_t t,s;for(t=0;t<4;t++)for(s=0;s<16;s++)lanes[t].cells[pattern][s]=0;}
static void stop_lanes(void){uint32_t t,n;for(t=0;t<4u;t++){clay_track_stop_rt(t);for(n=0;n<128;n++)off_at[t][n]=0;}playing=0;}
static void seek(uint32_t tick){
    uint32_t t;position=tick;tempo_index=0;
    while(tempo_index+1<THEME_TEMPOS && THEME_TEMPO_TICK[tempo_index+1]<=position)tempo_index++;
    for(t=0;t<4;t++)cursor[t]=source_at(t,tick);
}
static void trigger(uint32_t t,uint32_t note,uint32_t velocity,uint32_t length){
    #ifdef PLAYER_TEST
    verify_event(t,note,velocity,length);
    #endif
    clay_track_event_rt(t,note,velocity);off_at[t][note]=(uint16_t)(position+length);
}
static void tick(void){
    uint32_t t,n,bar=position/384u,step=position/24u%16u;
    if(!looping && position%384u==0)pattern=bar;
    while(tempo_index+1<THEME_TEMPOS && THEME_TEMPO_TICK[tempo_index+1]<=position)tempo_index++;
    for(t=0;t<4;t++){
        for(n=0;n<128;n++)if(off_at[t][n] && position>=off_at[t][n]){clay_track_event_rt(t,n,0);off_at[t][n]=0;}
        if(position%24u==0){uint16_t c=lanes[t].cells[bar][step];
            if(c!=65535u && (c&255))trigger(t,(c&255)-1,100,(c>>8)*24u);
        }
        while(cursor[t]<THEME_COUNTS[t] && THEME_LANES[t][cursor[t]].tick<=position){
            const theme_note_t *event=&THEME_LANES[t][cursor[t]++];
            uint32_t cell=event->tick/24u;
            if(lanes[t].cells[cell/16u][cell%16u]==65535u)trigger(t,event->note,event->velocity,event->length);
        }
    }
}
void player_render(int32_t *lr,uint32_t frames,uint32_t master){
    uint32_t t,cmd=command;command=0;
    for(t=0;t<4;t++)clay_track_config_rt(t,preset[t],muted[t]?0u:volume[t]);
    if(cmd==2)stop_lanes();else if(cmd==1){stop_lanes();seek(pattern*384u);phase=0;playing=1;tick();}
    while(frames){uint32_t n=frames,rate=(uint32_t)((float)(bpm*96u)*(535714.0f/(float)THEME_TEMPO_US[tempo_index]));if(!rate)rate=1;
        if(playing){n=(CLOCK-phase+rate-1u)/rate;if(n>frames)n=frames;}
        clay_render(lr,n,master);lr+=n*2;frames-=n;
        if(playing){phase+=n*rate;if(phase>=CLOCK){phase-=CLOCK;position++;
            if((looping && position/384u!=pattern) || position>=BARS*384u){stop_lanes();seek((looping?pattern:0u)*384u);playing=1;}
            tick();}}
    }
}
