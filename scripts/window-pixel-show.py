#!/usr/bin/env python3
"""Assemble CHATGPT from real miniature windows; leave the installation open."""
import json
import math
from pathlib import Path
import runpy
import subprocess
import time
import demo_audio as audio

root = Path(__file__).resolve().parent
control = runpy.run_path(str(root/'artwork-window-show.py'))
ipc, command = control['ipc'], control['command']
glyphs = {
    'C':['111','100','100','100','111'],
    'H':['101','101','111','101','101'],
    'A':['010','101','111','101','101'],
    'T':['111','010','010','010','010'],
    'G':['111','100','101','101','111'],
    'P':['111','101','111','100','100'],
}

def spawn(path, x,y,w,h,title=' '):
    before = {v['id'] for v in json.loads(ipc('get_state'))['windows']}
    command('open_primer',path=str(path),x=x,y=y,w=w,h=h,title=title,shadowless='true')
    return next(v['id'] for v in json.loads(ipc('get_state'))['windows'] if v['id'] not in before)

def main():
    canvas = json.loads(ipc('get_canvas_size'))
    W,H = canvas['width'],canvas['height']-1
    dx = min(6,(W-8)//27)
    if dx < 4 or H < 40:
        raise SystemExit('Needs at least 116 columns and 41 rows for miniature bordered windows.')
    x0=(W-27*dx)//2
    y0=(H-20)//2
    spawn(root/'window-stage.txt',max(0,x0-3),y0-3,27*dx+6,25,'C H A T G P T — every pixel is a window')
    pixels=[]
    for letter,char in enumerate('CHATGPT'):
        for row,bits in enumerate(glyphs[char]):
            for col,bit in enumerate(bits):
                if bit=='1':
                    x,y=x0+(letter*4+col)*dx,y0+row*4
                    ident=spawn(root/'window-pixel.txt',x,y,dx-1,3)
                    pixels.append((ident,x,y,letter))
        print('ASSEMBLED',char,flush=True)
        audio.fx('Pop',.2)
        audio.speak({'C':'see','H':'aitch','A':'ay','T':'tee','G':'gee','P':'pee'}[char], 'wib' if letter%2==0 else 'wob')
    print(f'{len(pixels)} real pixel windows. Wave test...',flush=True)
    # One passing ripple: stagger individual windows, then settle them back.
    for ident,x,y,letter in pixels:
        command('move_window',id=ident,x=x,y=y+1)
    for ident,x,y,letter in pixels:
        command('move_window',id=ident,x=x,y=y)
    art=Path('/Users/james/Repos/tvision/app/primers')
    cat=spawn(art/'cat-cat.txt',W-23,H-16,20,14,'CAT')
    chaos=spawn(art/'chaos-vs-order.txt',2,H-15,47,12,'CHAOS / ORDER')
    monster=spawn(Path.cwd()/'modules/example-primers/primers/monster-a.txt',W//2-18,H-15,36,12,'MONSTER')
    print('Cat, chaos/order and monster added. Brief orbit...',flush=True)
    speech = audio.speak('Kibble detected. All pixels belong to me.',wait=False)
    start=time.monotonic()
    while (t:=time.monotonic()-start)<6:
        command('move_window',id=cat,x=W-24+round(2*math.sin(t*3)),y=H-16+round(math.cos(t*3)))
        time.sleep(.04)
    command('move_window',id=cat,x=W-23,y=H-16)
    speech.wait(timeout=15)
    audio.stop()
    print(f'Finished. {len(pixels)} pixel windows plus stage and three artwork guests; original collage preserved underneath.',flush=True)

if __name__=='__main__':
    main()
