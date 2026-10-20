#!/usr/bin/env python3
"""Inspect the attributed fatalM4 Klaymen's Theme MIDI; preserve source timing."""
from pathlib import Path
import struct,json,hashlib,re
R=Path(__file__).resolve().parents[1]
path=R/'assets/references/klaymen-fatalM4.mid';data=path.read_bytes()
assert hashlib.md5(data).hexdigest()=='db96b1959e92c615b7aca254adad2aff','Incomplete/unexpected source MIDI'
fmt,count,ppqn=struct.unpack_from('>HHH',data,8);pos=14;tracks=[];tempos=[]
for track in range(count):
 assert data[pos:pos+4]==b'MTrk';size=struct.unpack_from('>I',data,pos+4)[0];p=pos+8;end=p+size;tick=0;running=0;notes=[];held={};name='';programs=[]
 def vlq():
  global p
  n=0
  while True:
   c=data[p];p+=1;n=n*128+(c&127)
   if c<128:return n
 while p<end:
  tick+=vlq();status=data[p]
  if status>=128:p+=1;running=status
  else:status=running
  if status==255:
   kind=data[p];p+=1;n=vlq();payload=data[p:p+n];p+=n
   if kind==3:name=payload.decode('latin1')
   if kind==81:tempos.append([tick,int.from_bytes(payload,'big')])
   continue
  if status in (240,247):
   n=vlq();p+=n;continue
  kind=status&240;ch=status&15;a=data[p];p+=1;b=0
  if kind not in (192,208):b=data[p];p+=1
  if kind==192:programs.append([tick,ch,a])
  if kind==144 and b:held[(ch,a)]=(tick,b)
  elif kind==128 or (kind==144 and not b):
   if (ch,a) in held:
    start,vel=held.pop((ch,a));notes.append([start,tick-start,a,vel,ch])
 assert not held,(track,held)
 tracks.append({'index':track,'name':name,'end':tick,'programs':programs,'notes':sorted(notes)})
 pos=end
report={'ppqn':ppqn,'tempos':tempos,'tracks':tracks}
(R/'build/check/theme-midi.json').write_text(json.dumps(report,indent=2)+'\n')
for t in tracks:
 ns=t['notes'];print(t['index'],t['name'],len(ns),'range',min((n[2] for n in ns),default=0),max((n[2] for n in ns),default=0),'end',t['end'],'programs',t['programs'][:4])
print('PPQN',ppqn,'tempos',tempos[:12])

# Four monophonic lanes, preserving the source's original event positions.
# Bass uses acoustic-bass notes, supplemented by baritone-sax bass where it is silent.
def monoline(primary,secondary=()):
    chosen=list(primary)
    for n in secondary:
        if not any(a[0]<n[0]+n[1] and n[0]<a[0]+a[1] for a in primary): chosen.append(n)
    result=[]
    for n in sorted(chosen,key=lambda n:(n[0],-n[2],-n[3])):
        if result and n[0]==result[-1][0]: continue
        result.append(n[:])
    return result
bass=monoline(tracks[8]['notes'],tracks[2]['notes'])
lead=monoline(tracks[1]['notes'],tracks[10]['notes'])
# Preserve every original guitar note, chord onset, gate and velocity.
comp=[n[:] for n in tracks[5]['notes']]
def drum(note):
    if note in (35,36):return 56
    if note in (38,40):return 57
    if note in (49,51,55,57):return 58
    if note in (42,44,46):return 53
    if note in (41,43,45,47,48,50):return 59
    return 54
drums=[[n[0],n[1],drum(n[2]),n[3],9] for n in tracks[4]['notes']]
origin=min(n[0] for lane in (bass,lead,comp,drums) for n in lane)
lanes=[]
for index,lane in enumerate((bass,lead,comp,drums)):
    cleaned=[]
    for n in sorted(lane):
        tick=max(0,n[0]-origin);length=max(1,n[1]);note=n[2]+(24 if index==0 else 0)
        cleaned.append([tick,length,note,n[3]])
    assert all(cleaned[i][0]<=cleaned[i+1][0] for i in range(len(cleaned)-1))
    lanes.append(cleaned)
end_tick=max(n[0]+n[1] for lane in lanes for n in lane)+192
bank=re.findall(r'^\s*\{\"([^\"]+)\",',(R/'clay_synth.c').read_text().split('const clay_instrument_t clay_bank')[1].split('};')[0],re.M)
factory=[bank.index(name) for name in ['ROOM BASS','WEASEL SAX','ROOM GUITAR','DRUM KIT']]
score={'title':"Klaymen's Theme",'composer':'Terry Scott Taylor','midi_arranger':'fatalM4 (2004)',
       'source_url':'https://www.vgmusic.com/file/db96b1959e92c615b7aca254adad2aff.html',
       'source_midi_md5':'db96b1959e92c615b7aca254adad2aff','reference_video':'https://www.youtube.com/shorts/hcacDhk5EUc',
       'arrangement':'Four instrument tracks; original guitar chords and all mapped drum events, exact 96 PPQN timing and source tempo map. Not a transcription of the video.',
       'default_volumes':[55,100,85,50],'ppqn':96,'bpm':112,'origin_ticks':origin,'end_tick':end_tick,'programs':factory,
       'tempos':[[max(0,t-origin),us] for t,us in tempos], 'lanes':lanes}
(R/'assets/references/theme-project.json').write_text(json.dumps(score,indent=2)+'\n')
import argparse
parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path,default=R/'build/gen/theme_data.h');args=parser.parse_args()
lines=['/* Derived from fatalM4 (2004), Klaymen\'s Theme / Terry Scott Taylor; see theme-project.json. */',
       '#pragma once','typedef struct { uint16_t tick,length; uint8_t note,velocity; } theme_note_t;',
       f'#define THEME_END {end_tick}u',f'#define THEME_TEMPOS {len(tempos)}u',
       'static const uint16_t THEME_TEMPO_TICK[]={'+','.join(str(max(0,t-origin)) for t,us in tempos)+'};',
       'static const uint32_t THEME_TEMPO_US[]={'+','.join(str(us) for t,us in tempos)+'};']
lines += ['static const uint8_t THEME_PRESETS[]={'+','.join(map(str,score['programs']))+'};',
          'static const uint8_t THEME_VOLUMES[]={'+','.join(map(str,score['default_volumes']))+'};']
for i,lane in enumerate(lanes):
    lines.append(f'static const theme_note_t THEME_{i}[]={{')
    lines += ['{'+','.join(str(x) for x in n)+'},' for n in lane];lines.append('};')
lines+=['static const theme_note_t *const THEME_LANES[]={THEME_0,THEME_1,THEME_2,THEME_3};',
        'static const uint16_t THEME_COUNTS[]={'+','.join(str(len(lane)) for lane in lanes)+'};']
args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text('\n'.join(lines)+'\n')
print('Four-track theme:',[len(lane) for lane in lanes],'notes,',sum(map(len,lanes))*6,'B,',end_tick,'ticks')
