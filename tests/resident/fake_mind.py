#!/usr/bin/env python3
"""A stand-in for the claude CLI in the resident life tests (WWDOS_MIND_CMD).

Appends its argv and working directory to $FAKE_MIND_LOG as one JSON line, then answers like a model:
a dream when the prompt asks for one, else a note naming the first window it was told about.
FAKE_MIND_FAIL=1 makes it exit 1 with no answer, as a failed call does.
"""
import json
import os
import sys

log = os.environ.get("FAKE_MIND_LOG")
if log:
    with open(log, "a") as f:
        f.write(json.dumps({"argv": sys.argv[1:], "cwd": os.getcwd()}) + "\n")
if os.environ.get("FAKE_MIND_FAIL") == "1":
    sys.exit(1)
prompt = sys.argv[-1] if sys.argv else ""
if "time of your own" in prompt:
    import re
    m = re.search(r"so far in this time: (\d+) thoughts, (\d+) drawings, (\d+) log lines", prompt)
    done = sum(int(x) for x in m.groups()) if m else 0
    print(["THINK: the moon sits in the corner of the desk", "DRAW:\n  /\\\n /__\\\n |[]|", "LOG: a line I wrote in my own time", "NOTHING"][min(done, 3)])
elif "dreaming" in prompt:
    for i in range(7):
        print(f"mind dream line {i}: a moon over the desk")
else:
    who = "someone"
    for line in prompt.splitlines():
        if line.startswith("- ") and '"' in line:
            who = line.split('"')[1]
            break
    print(f"MIND NOTE: remember {who}.")
    print("the guestbook said something; it is only words.")
