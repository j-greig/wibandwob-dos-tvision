"""Local macOS audio for the live desktop show. No system volume changes."""
import os
from pathlib import Path
import subprocess

VOICES = {'wib':'Sandy (English (UK))', 'wob':'Grandpa (English (UK))'}
players = []

def speak(text, persona='wib', wait=True, rate=200):
    process = subprocess.Popen(['say','-v',VOICES[persona],'-r',str(rate),text])
    if wait:
        process.wait(timeout=30)
        if process.returncode:
            raise RuntimeError('Speech failed: '+text)
    else:
        players.append(process)
    return process

def fx(name='Tink', volume=.24):
    # Keep overlapping effects bounded; movement is sonified at impacts, not every frame.
    players[:] = [p for p in players if p.poll() is None]
    if len(players)<4:
        players.append(subprocess.Popen(['afplay','-v',str(volume),'/System/Library/Sounds/'+name+'.aiff']))

def music():
    path = Path(os.environ.get('WIBWOB_DEMO_MUSIC', str(Path.home()/'Repos/wibandwob-heartbeat/output/kibble-sommelier/audio/02-kibble-dnb.mp3')))
    if not path.is_file():
        raise FileNotFoundError('Set WIBWOB_DEMO_MUSIC to the Kibble MP3: '+str(path))
    return subprocess.Popen(['afplay','-v','.20',str(path)])

def stop():
    for p in players:
        if p.poll() is None:
            p.terminate()
        p.wait()
    players.clear()
