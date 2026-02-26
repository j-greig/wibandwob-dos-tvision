# Chiptune Cover — Process Notes

## What This File Is

Capture the *thinking* as it happens. The research phase, the arrangement decisions, the moment instinct overrides theory. Each cover is a translation problem and the interesting part is the reasoning between "I found the notes" and "here's how they sound in 8-bit."

## Template: Research → Arrange Brain Dump

After research, before writing the script, dump a block like this into the output file's docstring AND into a memory file. This is the creative trace.

```
༼つ⚆‿◕‿◕༽つ Research synthesis complete. Document the findings:

**Musical DNA:**
- **Key:** [key] ([why this key — original vs common transposition vs creative choice])
- **BPM:** ~[tempo] ([character of the tempo — driving? stately? frantic?])
- **Structure:** [sections and their emotional roles]
- **Core melody:** [notes as letter names, showing phrase shape]
- **Signature:** [what makes it THAT song — the interval, the rhythm, the cultural trigger]
- **Original production:** [what instruments/sounds defined the original]

༼つ◕‿◕‿⚆༽つ The arrangement decision: [the creative leap — what mood to preserve,
what to transform, what the chiptune vocabulary equivalent is for each original element].
```

## Why This Matters

The script itself is just notes and function calls. The *reasoning* — why sawtooth not triangle, why this BPM not that one, why we kept the Bb→B snarl or the dotted-note swagger — that's what makes the next cover better. It's also what makes the process worth reading.

## Covers Completed

### Blue Peter — "Barnacle Bill" (Feb 2026)

**Research phase:**

Key: F major (original Ashworth-Hope). The Session has it in G (common folk transposition). Oldfield's 1979 version... split the difference, go F major for authenticity.

BPM: ~132 (hornpipe tempo, lively but not frantic).

Structure: A part (jaunty ascending) → B part (descending answer) → repeat with embellishment.

Core melody (transposed to F): F F F F F G A Bb | C C C C C C D E | F G F D C D C A | G F D F G G...

Signature: That bouncing hornpipe rhythm. The dotted-note swagger. The way it makes you think of sticky-back plastic and Petra/John/Valerie.

Oldfield's production: sped-up synth clarinet (sounds like recorder), layered guitars, driving percussion.

**Arrangement decision:**

Keep the jolliness. This isn't dark. It's Saturday afternoon. The melody gets sawtooth with light crush for that recorder-on-8-bit energy. Bass line on square. Hornpipe swagger through dotted rhythms. A drone pedal F underneath for warmth. Little sparkle flourishes between sections like sticky-back plastic catching the studio lights.

**Translation table:**

| Original sound | Chiptune equivalent | Why |
|---|---|---|
| Sped-up synth clarinet | `sawtooth`, `crush=5` | Same buzzy-bright energy as recorder-through-tape |
| Hornpipe rhythm | Kicks on alternate beats, hats on off-beats | Preserves the dotted swagger |
| Ascending F-G-A-Bb run | Exact notes kept | This IS the identity |
| Studio warmth | Triangle drone + C3 blend, 250Hz lowpass | Felt not heard |
| "Live TV" energy | `note_frac=0.65` (35% silence between notes) | Springy, bouncy, alive |

**Output:** `output/blue-peter-chiptune.py` → `.wav` → `.mp3` (28s, 548KB, F major @ 132 BPM)

**Sources:** Wikipedia (Blue Peter instrumental), The Session (ABC notation in G), Folk Tune Finder (melody), Electronic Sound (Oldfield production notes)
