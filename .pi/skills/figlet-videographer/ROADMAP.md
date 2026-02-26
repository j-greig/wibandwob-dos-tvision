# SPK: Figlet Typographic Videographer

## Concept

A pi skill / TUI tool that creates **stop-frame typographic animations** using figlet word arrangements as keyframes. Think concrete poetry meets motion graphics, rendered entirely in text mode.

## Core Idea

- Each **frame** is a workspace state: a set of figlet windows with positions, fonts, colours, text
- A **timeline** is a sequence of frames with hold durations and transitions
- The skill composes frames programmatically via the API (open_figlet_text, move_window, resize_window, figlet_set_color, figlet_set_font)
- Playback = stepping through frames, applying each workspace state, optionally capturing screenshots

## What We Proved Today

- Auto-sized figlet windows work (text measured → window fitted)
- move_window / resize_window via API work
- 15 words placed simultaneously at precise x,y positions
- Different fonts (mini, small, standard, big, banner) give typographic hierarchy
- figlet_set_color gives per-window fg/bg control
- desktop_gallery mode hides chrome for clean canvas
- desktop_color/texture sets background
- Screenshots capture the framebuffer as .txt and .ans

## Architecture Sketch

```
timeline.json
├── meta: { title, canvas_size, fps, bg_color }
├── frames: [
│   { t: 0,    words: [{text, font, x, y, fg, bg}, ...], bg: {...}, transition: "cut" },
│   { t: 2.0,  words: [...], transition: "fade" },
│   { t: 4.5,  words: [...], transition: "cut" },
│   ...
│ ]
```

### Playback Engine

1. Read timeline.json
2. For each frame:
   - close_all (or diff against previous frame)
   - Apply desktop_color/texture
   - Open each word with open_figlet_text
   - Move/resize each to target position
   - Set colours with figlet_set_color
   - Sleep for hold duration
   - Optionally: screenshot for export

### Transitions

- **cut**: instant swap (close_all → rebuild)
- **fade**: change colours through grey ramp between frames
- **slide**: interpolate x,y positions over N steps
- **typewriter**: words appear one at a time with delay
- **scatter → gather**: words start random, animate to final positions

### Export Modes

- **Screenshot sequence**: .txt/.ans files per frame → can be played back with frame_file_player
- **GIF**: capture terminal screenshots → ImageMagick convert
- **Workspace saves**: each frame saved as a .wwdos workspace file (existing feature)
- **Live**: real-time playback in the TUI

## Relationship to Existing Features

- **Workspaces**: frames ARE workspace states — could literally be saved/loaded workspace files
- **frame_file_player**: already plays back .ans sequences — this is the playback engine
- **Gallery mode**: desktop_gallery hides chrome for clean canvas presentation
- **Paint canvas**: could combine figlet words with paint_cell drawings for mixed media

## Skill API (what the pi skill would expose)

```
figlet_video_compose(timeline_json)  → creates/validates a timeline
figlet_video_play(timeline, loop?)   → plays back in TUI
figlet_video_render(timeline, dir)   → renders to screenshot sequence
figlet_video_preview(timeline, frame_index) → shows single frame
```

## Example Use Cases

- Lyric videos for songs (words appear/move in time)
- Poetry readings with typographic emphasis
- Title sequences / splash screens
- Data visualisation with big numbers changing
- Ambient art installations (loop mode)
- AI-directed: agent composes poetry AND choreographs the visual arrangement

## Open Questions

- Frame diffing vs rebuild: smarter to diff previous→next and only move/recolour changed windows?
- Timing precision: API round-trip latency (~50ms per command) limits animation smoothness
- Max windows: how many figlet windows before TUI slows down? (tested 15, need to test 30+)
- Font availability: different systems have different figlet fonts installed
- Frameless by default? Today "listen" kept its frame because it was focused — need frameless spawn option via API

## Audio-Synced VJ Mode

The heartbeat repo has 221 chiptune mp3s at `/Users/james/Repos/wibandwob-heartbeat/output/`. The idea: sync timeline frames to audio timestamps, creating a live ASCII VJ set.

- **Backing track**: `afplay` / `mpv --no-video` plays in background
- **Stingers**: short mp3s (2-5s) fire on word reveals via `afplay &`
- **Timeline `t` values**: seconds relative to audio start
- **Mixed media**: figlet words + primer ASCII art + desktop colour changes + audio cues all on one timeline
- Interstitial stingers from `scramble-microfilm-v2/interstitials/` are perfect for word-by-word typewriter reveals
- `memory-architecture-*` tracks are thematically ideal for the symbient poem

## Next Steps

1. ~~Create a simple timeline.json format~~ ✅ done
2. ~~Write a Python playback script~~ ✅ done (examples/play.py)
3. ~~Test with multi-word composition~~ ✅ done (01-drift-poem, 02-symbient-poem)
4. Add `afplay` audio sync to play.py
5. Add primer placement to timeline format (open_primer + move_window)
6. Add desktop state changes (colour/texture per frame)
7. Build a "VJ deck" mode — two timelines crossfading
8. Add to pi skills once stable
