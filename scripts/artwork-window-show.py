#!/usr/bin/env python3
"""Animate the live collage for 30 seconds, then restore positions and stacking.

Run from repo root: python3 scripts/artwork-window-show.py
Uses the same local IPC protocol as the REST bridge; no dependencies.
"""
import json
import math
import os
import socket
import subprocess
import shutil
import time
import demo_audio as audio
from urllib.parse import quote


def ipc(command, **args):
    line = 'cmd:' + command + ''.join(' '+k+'='+quote(str(v), safe='') for k,v in args.items())+'\n'
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.settimeout(3)
        sock.connect('/tmp/wibwob_'+os.environ.get('WIBWOB_INSTANCE', '1')+'.sock')
        sock.sendall(line.encode())
        chunks = []
        while True:
            data = sock.recv(65536)
            if not data:
                break
            chunks.append(data)
    result = b''.join(chunks).decode().strip()
    if result.startswith('err'):
        raise RuntimeError(result)
    return result


def command(name, **args):
    return ipc('exec_command', name=name, **args)


def main():
    state = json.loads(ipc('get_state'))
    windows = state['windows']
    for w in windows:
        w['rect'] = {k:w[k] for k in ('x','y','w','h')}
    chaos = next(w for w in windows if 'chaos-vs-order' in w.get('title', ''))
    cat = next(w for w in windows if 'cat-cat' in w.get('title', ''))
    canvas = json.loads(ipc('get_canvas_size'))
    width, height = canvas['width'], canvas['height']-1
    timings = []
    frames = 0
    start = time.monotonic()
    last_phase = -1
    speech = None
    duration = float(os.environ.get('WIBWOB_MOTION_SECONDS','30'))
    previous_bounce = None
    def bounce(t, speed, limit):
        return round(limit-abs((t*speed) % (2*limit)-limit))
    try:
        command('raise_window', id=chaos['id'])
        while (t := time.monotonic()-start) < duration:
            tick = time.monotonic()
            phase = min(2,int(t/(duration/3)))
            if phase != last_phase:
                print(['ACT I: chaos/order DVD bounce', 'ACT II: cat counterpoint', 'ACT III: whole collage breathing'][phase], flush=True)
                last_phase = phase
                audio.fx(['Glass','Purr','Pop'][phase])
                if phase == 0:
                    speech = audio.speak('Order is merely chaos with a window manager.','wob',wait=False)
                if phase == 1 and shutil.which('say'):
                    speech = audio.speak('Meow. Chaos has paws. Keep up.',wait=False)
                if phase == 2:
                    speech = audio.speak('The desktop has developed a pulse.','wob',wait=False)
            r = chaos['rect']
            bounce_cell = (int(t*85/(width-r['w'])),int(t*31/(height-r['h'])))
            if previous_bounce is not None and bounce_cell != previous_bounce:
                audio.fx('Tink' if bounce_cell[0] != previous_bounce[0] else 'Pop')
            previous_bounce = bounce_cell
            command('move_window', id=chaos['id'], x=bounce(t, 85, width-r['w']), y=bounce(t, 31, height-r['h']))
            if phase >= 1:
                r = cat['rect']
                command('move_window', id=cat['id'], x=bounce(t+2, 51, width-r['w']), y=bounce(t+3, 23, height-r['h']))
            if phase == 2:
                for i,w in enumerate(windows):
                    if w['id'] in (chaos['id'], cat['id']):
                        continue
                    r = w['rect']
                    x = max(0,min(width-r['w'],r['x']+round(3*math.sin(t*2+i))))
                    y = max(0,min(height-r['h'],r['y']+round(2*math.cos(t*2+i))))
                    command('move_window', id=w['id'], x=x,y=y)
            timings.append(time.monotonic()-tick)
            frames += 1
            time.sleep(max(0, start+frames/30-time.monotonic()))
    finally:
        if speech and speech.poll() is None:
            speech.terminate()
            speech.wait()
        audio.stop()
        for w in windows:
            command('move_window', id=w['id'], x=w['rect']['x'], y=w['rect']['y'])
        # State enumerates front to back: raise back to front to restore stacking.
        for w in reversed(windows):
            command('raise_window', id=w['id'])
        focused = next((w for w in windows if w.get('focused')), None)
        if focused:
            command('focus_window', id=focused['id'])
        print('Original positions and stacking restored.', flush=True)
    elapsed = time.monotonic()-start
    ordered = sorted(timings)
    print(f'{frames} frames / {elapsed:.2f}s = {frames/elapsed:.1f} fps; p95 control work {ordered[int(len(ordered)*.95)]*1000:.1f}ms', flush=True)


if __name__ == '__main__':
    main()
