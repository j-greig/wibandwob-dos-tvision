#!/usr/bin/env python3
"""Compare the canonical voices at three measured speaking rates."""
import subprocess
import time
from demo_audio import VOICES

for rate in (90,120,150):
    print(f'RATE {rate} WPM — Wib, then Wob',flush=True)
    for name, text in [('wib',f'{rate}. Chaos has paws. We share one world.'),('wob','Human. Machine. One shared world.')]:
        started=time.monotonic()
        subprocess.run(['say','-v',VOICES[name],'-r',str(rate),text],check=True)
        print(f'{name}: {time.monotonic()-started:.1f}s',flush=True)
        time.sleep(.6)
    time.sleep(1)
