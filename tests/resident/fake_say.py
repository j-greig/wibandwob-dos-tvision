#!/usr/bin/env python3
"""A stand-in for macOS 'say' in the sound tests: answers 'say -v ?' with a voice list and
writes a WAV for 'say -v <voice> -o <file> --file-format=WAVE --data-format=LEI16@22050 <word>':
a tone at the voice's own pitch, 0.25 s plus 0.06 s a letter. FAKE_SAY_VOICES (comma list)
replaces the voice list; FAKE_SAY_LOG gets one line per render.
"""
import math
import os
import struct
import sys
import wave

PITCH = {"Sandy (English (UK))": 210.0, "Grandpa (English (UK))": 115.0}
VOICES = os.environ.get("FAKE_SAY_VOICES", "Sandy (English (UK)),Grandpa (English (UK)),Reed (English (UK)),Daniel").split(",")

args = sys.argv[1:]
if args[:2] == ["-v", "?"]:
    for v in VOICES:
        print(f"{v} en_GB    # Hello! My name is {v}.")
    sys.exit(0)
voice = args[args.index("-v") + 1]
out = args[args.index("-o") + 1]
word = args[-1]
rate = 22050
hz = PITCH.get(voice, 160.0)
n = int(rate * (0.25 + 0.06 * len(word)))
frames = bytearray()
for i in range(n):
    env = min(1.0, i / 400.0, (n - i) / 400.0)
    frames += struct.pack("<h", int(12000 * env * math.sin(2 * math.pi * hz * i / rate)))
with wave.open(out, "wb") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(bytes(frames))
log = os.environ.get("FAKE_SAY_LOG")
if log:
    with open(log, "a") as f:
        f.write(f"{voice}|{word}\n")
