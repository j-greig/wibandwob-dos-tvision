#!/usr/bin/env python3
"""Convert a WibWob-DOS workspace JSON to a figlet videographer timeline frame.

Usage:
  workspace_to_timeline.py <workspace.json> [--output timeline.json]
  workspace_to_timeline.py --live [--output timeline.json]

--live mode fetches current workspace via save_workspace + read, instead of a file.

Extracts figlet_text windows as timeline words and frame_player windows as primers.
Outputs a single-frame timeline that play.py can render.

Limitations:
  - All windows land in one frame (no automatic stanza grouping)
  - Timing/transitions/audio must be added by hand or a second pass
  - Multiple primers are supported via a primers[] extension
"""

import json
import os
import sys
import urllib.request


API = "http://127.0.0.1:8089"


def post(cmd, args=None):
    body = {"command": cmd}
    if args:
        body["args"] = args
    data = json.dumps(body).encode()
    req = urllib.request.Request(
        f"{API}/menu/command", data, {"Content-Type": "application/json"}
    )
    return json.loads(urllib.request.urlopen(req, timeout=5).read())


def load_live_workspace():
    """Save current workspace and return the JSON."""
    post("save_workspace")
    # Read the last saved workspace
    workspace_dir = "workspaces"
    files = sorted(
        [f for f in os.listdir(workspace_dir) if f.startswith("last_workspace")],
        key=lambda f: os.path.getmtime(os.path.join(workspace_dir, f)),
        reverse=True,
    )
    if not files:
        print("ERROR: no workspace file found after save")
        sys.exit(1)
    path = os.path.join(workspace_dir, files[0])
    with open(path) as f:
        return json.load(f), path


def load_workspace_file(path):
    with open(path) as f:
        return json.load(f), path


def primer_basename(path):
    """Extract just the filename from a primer path."""
    return os.path.basename(path)


def convert(workspace):
    """Convert workspace dict to a single-frame timeline dict."""
    screen = workspace.get("screen", {})
    desktop = workspace.get("desktop", {})
    windows = workspace.get("windows", [])

    # Build meta
    meta = {
        "title": "converted workspace",
        "author": "workspace_to_timeline.py",
        "canvas": {
            "width": screen.get("width", 156),
            "height": screen.get("height", 46),
        },
        "bg": {
            "fg": desktop.get("fg", 0),
            "bg": desktop.get("bg", 0),
            "texture": desktop.get("char", " "),
        },
        "gallery_mode": desktop.get("gallery", False),
    }

    # Extract words (figlet_text) and primers (frame_player)
    words = []
    primers = []

    for w in windows:
        wtype = w.get("type", "")
        bounds = w.get("bounds", {})
        props = w.get("props", {})

        if wtype == "figlet_text":
            word = {
                "text": props.get("text", w.get("title", "")),
                "font": props.get("font", "standard"),
                "x": bounds.get("x", 0),
                "y": bounds.get("y", 0),
            }
            # Preserve colour if non-default
            fg = props.get("fg", "")
            bg = props.get("bg", "")
            if fg and fg != "#FFFFFF":
                word["fg"] = fg
            if bg and bg != "#000000":
                word["bg"] = bg
            words.append(word)

        elif wtype == "frame_player":
            primer_path = props.get("path", "")
            if primer_path:
                primers.append({
                    "path": primer_basename(primer_path),
                    "x": bounds.get("x", 0),
                    "y": bounds.get("y", 0),
                })

    # Build frame
    frame = {
        "t": 0.0,
        "transition": "cut",
        "words": words,
    }

    # Add primers
    if len(primers) == 1:
        # Single primer: use standard format
        frame["primer"] = primers[0]["path"]
        frame["primer_pos"] = {"x": primers[0]["x"], "y": primers[0]["y"]}
    elif len(primers) > 1:
        # Multiple primers: use extended format
        frame["primers"] = primers

    timeline = {
        "meta": meta,
        "frames": [frame],
    }

    return timeline


def main():
    output = None
    live = False
    workspace_path = None

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--output" and i + 1 < len(args):
            output = args[i + 1]
            i += 2
        elif args[i] == "--live":
            live = True
            i += 1
        elif not args[i].startswith("-"):
            workspace_path = args[i]
            i += 1
        else:
            print(f"Unknown arg: {args[i]}")
            sys.exit(1)

    if live:
        workspace, src = load_live_workspace()
        print(f"Loaded live workspace from {src}")
    elif workspace_path:
        workspace, src = load_workspace_file(workspace_path)
        print(f"Loaded workspace from {src}")
    else:
        print("Usage: workspace_to_timeline.py <workspace.json> [--output file.json]")
        print("       workspace_to_timeline.py --live [--output file.json]")
        sys.exit(1)

    timeline = convert(workspace)

    n_words = len(timeline["frames"][0]["words"])
    n_primers = len(timeline["frames"][0].get("primers", []))
    if "primer" in timeline["frames"][0]:
        n_primers = 1
    print(f"Converted: {n_words} words, {n_primers} primers, 1 frame")

    if output:
        with open(output, "w") as f:
            json.dump(timeline, f, indent=2)
        print(f"Written to {output}")
    else:
        print(json.dumps(timeline, indent=2))


if __name__ == "__main__":
    main()
