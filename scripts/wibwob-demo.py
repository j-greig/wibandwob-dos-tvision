#!/usr/bin/env python3
"""Replay the JSON score on a running WibWob-DOS collage, restoring it on exit.

python3 scripts/wibwob-demo.py [path/to/score.json]
Requires the collage's chaos-vs-order and cat-cat windows, macOS say, and IPC.
"""
import json
from pathlib import Path
import runpy
import shutil
import subprocess
import sys
import time
import os
import demo_audio as audio

ROOT = Path(__file__).resolve().parent
control = runpy.run_path(str(ROOT/'artwork-window-show.py'))
ipc, command = control['ipc'], control['command']


def main():
    score = json.loads(Path(sys.argv[1] if len(sys.argv)>1 else ROOT/'wibwob-demo.json').read_text())
    original = json.loads(ipc('get_state'))['windows']
    ids = {w['id'] for w in original}
    titles = [w.get('title','') for w in original]
    if not all(any(name in t for t in titles) for name in ('chaos-vs-order','cat-cat')):
        raise SystemExit('Open workspaces/github-collage.json before starting the show.')
    if not shutil.which('say'):
        raise SystemExit('macOS say is required.')
    print(score['title'],flush=True)
    music = audio.music()
    try:
        for cue in score['cues']:
            kind = cue['type']
            print('CUE:',kind,flush=True)
            if kind == 'island':
                canvas = json.loads(ipc('get_canvas_size'))
                W,H = canvas['width'],canvas['height']-1
                spawn = runpy.run_path(str(ROOT/'window-pixel-show.py'))['spawn']
                before = {w['id'] for w in json.loads(ipc('get_state'))['windows']}
                spawn(ROOT/'window-stage.txt',0,0,W,H,'W I B W O B W O R L D')
                island = Path.home()/'Repos/wibandwob-heartbeat/primers/www-island.txt'
                iw,ih = min(140,W-4),min(62,H-10)
                spawn(island,(W-iw)//2,9,iw,ih,'www-island.txt')
                command('open_figlet_text',text=cue.get('title','WIBWOB-DOS'),font='standard',x=max(1,(W-90)//2),y=0,shadow='false')
                command('figlet_set_color',id='auto',fg='#FFFFFF',bg='#000000')
                audio.fx('Glass',.2)
                audio.speak('Wib Wob DOS. Welcome to our little world.','wib')
                audio.speak('One island. Two minds. Every window is alive.','wob')
                time.sleep(.8)
                for w in json.loads(ipc('get_state'))['windows']:
                    if w['id'] not in before:
                        command('close_window',id=w['id'])
            elif kind == 'speech':
                subprocess.run([sys.executable,str(ROOT/'figlet-say-show.py'),cue['words']]+(['--stagger'] if cue.get('layout')=='stagger' else []),check=True)
            elif kind == 'donut':
                current = json.loads(ipc('get_state'))['windows']
                donut = next((w for w in current if w['type']=='torus'),None)
                if donut is None:
                    before = {w['id'] for w in current}
                    command('open_torus')
                    donut = next(w for w in json.loads(ipc('get_state'))['windows'] if w['id'] not in before and w['type']=='torus')
                    canvas = json.loads(ipc('get_canvas_size'))
                    command('resize_window',id=donut['id'],w=48,h=22)
                    command('move_window',id=donut['id'],x=max(1,canvas['width']//2-24),y=max(1,canvas['height']//2-11))
                    command('window_title',id=donut['id'],title='TORUS / ORDER IN MOTION')
                audio.fx('Bottle',.2)
            elif kind == 'motion':
                subprocess.run([sys.executable,str(ROOT/'artwork-window-show.py')],check=True,env={**os.environ,'WIBWOB_MOTION_SECONDS':'18'})
            elif kind == 'pixels':
                subprocess.run([sys.executable,str(ROOT/'window-pixel-show.py')],check=True)
            elif kind == 'hold':
                time.sleep(min(30,max(0,float(cue['seconds']))))
            elif kind == 'finale':
                # Remove only show-created windows, then mask the preserved collage with a clean stage.
                for w in json.loads(ipc('get_state'))['windows']:
                    if w['id'] not in ids:
                        command('close_window',id=w['id'])
                canvas = json.loads(ipc('get_canvas_size'))
                W,H = canvas['width'],canvas['height']-1
                spawn = runpy.run_path(str(ROOT/'window-pixel-show.py'))['spawn']
                spawn(ROOT/'window-stage.txt',0,0,W,H,'W I B  /  W O B')
                art = Path.home()/'Repos/tvision/app/primers'
                spawn(art/'wibwob-portrait-1.txt',4,4,40,21,'つ◕‿◕‿⚆༽つ  WIB')
                spawn(art/'wibwob-portrait-3.txt',W-54,4,50,25,'つ⚆‿◕‿◕༽つ  WOB')
                if music.poll() is None:
                    music.terminate()
                    music.wait()
                audio.fx('Hero',.22)
                subprocess.run([sys.executable,str(ROOT/'figlet-say-show.py'),cue['words'],'--stagger'],check=True,env={**os.environ,'WIBWOB_FINALE':'1'})
            else:
                raise ValueError('Unknown cue: '+kind)
    finally:
        if music.poll() is None:
            music.terminate()
        music.wait()
        audio.stop()
        current = json.loads(ipc('get_state'))['windows']
        for w in current:
            if w['id'] not in ids:
                command('close_window',id=w['id'])
        for w in reversed(original):
            command('move_window',id=w['id'],x=w['x'],y=w['y'])
            command('raise_window',id=w['id'])
        print('Show complete; original collage restored.',flush=True)


if __name__ == '__main__':
    main()
