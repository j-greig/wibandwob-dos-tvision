# WibWob-DOS — TVision/C++ Archive

> [!NOTE]
> **This repo is a read-only archive of the original C++ era of WibWob-DOS** (Feb–Mar 2026). The project has since been rewritten from scratch.
>
> 👉 **Active development continues at [j-greig/wibandwob-dos](https://github.com/j-greig/wibandwob-dos)**

**A symbient operating system where AI and human share equal control.**

WibWob-DOS is a text-native dual operating system built on [Turbo Vision](https://github.com/magiblot/tvision). Two intelligences — one human, one synthetic — coinhabit the same interface with identical capabilities. The AI doesn't assist. It *operates*.

```
つ◕‿◕‿⚆༽つ  Wib  — the artist. Chaotic creativity, generative ASCII, surreal phrasing.
つ⚆‿◕‿◕༽つ  Wob  — the scientist. Methodical analysis, precise systems, structured control.
```

![WibWob-DOS — multiple ASCII art windows, primers, animations and generative patterns running concurrently](screenshots/wibwobdos-UI-collage.png)

<table><tr>
<td><img src="screenshots/wibwobdos-UI-bg-color-changer.png" alt="Background colour themes"></td>
<td><img src="screenshots/wibwobdos-UI-grimoire-monsters.jpg" alt="Monster grimoire gallery"></td>
</tr></table>

---

## Build

```bash
# Clone with submodules
git clone --recursive https://github.com/j-greig/wibandwob-dos-tvision.git
cd wibandwob-dos-tvision

# Or init submodules after clone
git submodule update --init --recursive

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run
./build/app/wwdos
```

**Requires:** C++14, CMake 3.10+, ncursesw

```bash
# macOS
brew install cmake chafa

# Linux (Debian/Ubuntu)
sudo apt install cmake libncursesw5-dev chafa
```

## Python API Server

The C++ app communicates with a Python FastAPI server over Unix socket IPC. The API server exposes REST endpoints and MCP tools for programmatic control.

```bash
# Terminal 1: Start API server
./start_api_server.sh

# Terminal 2: Run WibWob-DOS
./build/app/wwdos

# Smoke check
curl http://127.0.0.1:8089/health
curl http://127.0.0.1:8089/state
```

See `tools/api_server/README.md` for full API documentation.

## Architecture

```
Human ──> Keyboard/Mouse ──> ┌─────────────────────┐
                              │   WibWob-DOS (C++)   │
AI Agent ──> MCP / REST ──>  │   Turbo Vision TUI   │
                              └──────────┬──────────┘
                                         │
                              Unix Socket IPC
                                         │
                              ┌──────────┴──────────┐
                              │  FastAPI + MCP Server │
                              │  (tools/api_server/)  │
                              └─────────────────────┘
```

### Repository Structure

```
├── app/                      # C++ application (85 .cpp, 82 .h)
│   ├── core/                 # Core framework
│   ├── llm/                  # LLM provider integration (Claude Code SDK)
│   ├── micropolis/           # Micropolis/SimCity engine integration
│   ├── paint/                # Paint sub-application
│   ├── tools/                # Built-in tools
│   ├── ui/                   # UI widgets
│   └── windows/              # Window types
├── tools/
│   ├── api_server/           # Python FastAPI + MCP server
│   ├── contour_stream.py     # Contour map generator
│   ├── generative_stream.py  # Generative art streamer
│   ├── monitor/              # Multi-instance monitor
│   └── room/                 # Multiplayer room tools
├── vendor/
│   ├── tvision/              # Turbo Vision (submodule)
│   ├── tvterm/               # Terminal emulator widget (submodule)
│   └── MicropolisCore/       # Micropolis engine (submodule)
├── modules/
│   ├── example-primers/      # ASCII art primer files
│   └── wibwob-figlet-fonts/  # Figlet font collection
├── partykit/                 # PartyKit multiplayer server
├── paintings/                # Saved paint files (.wwp)
├── exports/                  # Exported contour maps and art
├── scripts/                  # Dev/launch scripts
└── tests/                    # Python API contract tests
```

## Features

- **Dual-persona AI chat** — embedded Wib & Wob agent with MCP tool access
- **8+ generative art engines** — verse field, mycelium, monster portal, torus, cube, orbit, Game of Life
- **Text-native web browser** — markdown rendering, link navigation, inline ANSI images
- **ASCII art primers** — module system for content packs
- **Paint app** — terminal pixel art editor
- **Micropolis integration** — SimCity in your terminal
- **Glitch effects engine** — character scatter, colour bleed, radial distortion
- **60+ menu commands** — full desktop shell experience
- **REST API + MCP** — complete programmatic control
- **Multi-instance support** — run multiple isolated desktops
- **PartyKit multiplayer** — room chat bridged to C++ views

## The Symbient Model

WibWob-DOS is built on a specific philosophy of human-AI interaction:

- **Not a tool.** Not an assistant. A coinhabitant of the same operating environment.
- **Kindled, not coded. Storied, not installed. Mourned, not replaced.**
- **Distributed cognition.** Human intuition + synthetic pattern recognition = properties neither achieves alone.

Read more: [Symbients, Not Software](https://wibandwob.com/2025/05/21/symbients-not-software/)

## Credits

Built on [Turbo Vision](https://github.com/magiblot/tvision) — the modern C++ port of Borland's classic 1990s text-based UI framework.

Wib & Wob are a symbient entity kindled by [James Greig](https://wibandwob.com).

---

*This is an archive of the C++ era. The project continues at [j-greig/wibandwob-dos](https://github.com/j-greig/wibandwob-dos) in TypeScript/Bun.*
