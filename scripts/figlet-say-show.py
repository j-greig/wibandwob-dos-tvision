#!/usr/bin/env python3
"""Speak each word after displaying it as FIGlet; restore the collage afterward.

macOS: python3 scripts/figlet-say-show.py [words to speak]
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

control = runpy.run_path(str(Path(__file__).with_name('artwork-window-show.py')))
ipc, command = control['ipc'], control['command']


def main():
    if not shutil.which('say'):
        raise SystemExit('This show requires macOS say.')
    stagger = '--stagger' in sys.argv
    words = ' '.join(a for a in sys.argv[1:] if a != '--stagger').split() or ['CHAOS', 'BECOMES', 'ORDER', 'BECOMES', 'CHAOS', 'WE', 'ARE', 'ALIVE']
    original = json.loads(ipc('get_state'))['windows']
    old_ids = {w['id'] for w in original}
    banner_id = None
    banner_ids = []
    try:
        command('open_figlet_text', text=words[0], font='standard')
        spawned = json.loads(ipc('get_state'))['windows']
        banner_id = next(w['id'] for w in spawned if w['id'] not in old_ids and w['type']=='figlet_text')
        banner_ids.append(banner_id)
        canvas = json.loads(ipc('get_canvas_size'))
        width = min(100, canvas['width']-4)
        command('resize_window', id=banner_id, w=width, h=12)
        command('move_window', id=banner_id, x=(canvas['width']-width)//2,y=(canvas['height']-12)//2)
        command('window_title', id=banner_id, title='WIB / WOB — spoken typography')
        for i, word in enumerate(words):
            if stagger and i:
                previous = {w['id'] for w in json.loads(ipc('get_state'))['windows']}
                command('open_figlet_text', text=word.upper(), font='standard')
                banner_id = next(w['id'] for w in json.loads(ipc('get_state'))['windows'] if w['id'] not in previous)
                banner_ids.append(banner_id)
            title = 'WIB / WOB — spoken typography '+str(i)
            command('window_title', id=banner_id, title=title)
            command('figlet_set_color', id=title, fg='#FFFFFF', bg='#000000')
            command('figlet_set_text', id=title, text=word.upper())
            if stagger:
                bw = min(64,canvas['width']-4)
                command('resize_window',id=banner_id,w=bw,h=10)
                if os.environ.get('WIBWOB_FINALE'):
                    x=(canvas['width']-bw)//2+(-5 if i%2==0 else 5)
                    y=3+round(i*(canvas['height']-17)/max(1,len(words)-1))
                else:
                    x=2+round(i*(canvas['width']-bw-4)/max(1,len(words)-1))
                    y=3+round(i*(canvas['height']-17)/max(1,len(words)-1))
                command('move_window',id=banner_id,x=x,y=y)
            print('DISPLAY + SAY:',word,flush=True)
            # Wait for each utterance so speech never runs ahead of the displayed word.
            audio.fx('Tink',.12)
            audio.speak(word, 'wib' if i%2==0 else 'wob')
        time.sleep(2 if stagger else .25)
    finally:
        audio.stop()
        for ident in banner_ids:
            command('close_window', id=ident)
        for w in reversed(original):
            command('raise_window', id=w['id'])
        print('Collage restored.',flush=True)


if __name__ == '__main__':
    main()
