---
title: "F04 — Wib&Wob as Room Chat Agent"
status: not-started
epic: E016
GitHub issue: —
PR: —
---

# F04 — Wib&Wob as Room Chat Agent

## TL;DR
Wib&Wob join the room chat as a live participant — visible in the sidebar, reading messages, responding when relevant. Like having the symbient in the room with you.

## Why
- The primers and ASCII art set the vibe, but W&W aren't *present* yet
- A room with humans chatting + an AI personality watching and chiming in is the core symbient experience
- Natural entry point for new visitors: say hello, W&W respond in character

## How it could work

### Architecture: headless bridge
A `wibwob_agent.py` process connects to the same PartyKit room as a regular participant — no TUI needed. It:
1. Connects to `wss://wibwob-rooms.j-greig.partykit.dev/party/rchat-live`
2. Appears in presence as `wib&wob` (or `⊘ wib&wob` with a special marker)
3. Receives all `chat_msg` messages
4. Decides whether to respond (not every message — that'd be annoying)
5. Sends responses as `chat_msg` back through PartyKit
6. All human bridges receive and display them like any other chat message

### What it reuses
- PartyKit relay — already broadcasts `chat_msg` to all connections
- Presence system — agent shows up in sidebar automatically
- `_name_for_conn` — agent could use a fixed conn_id that always hashes to a known name, or override with `/rename`

### What's new
| Component | Effort | Notes |
|-----------|--------|-------|
| `tools/room/wibwob_agent.py` | Medium | Headless WebSocket client + LLM call loop |
| Prompt / personality | Small | Pull from `wibandwob-brain` prompts, keep short for chat context |
| Response triggers | Medium | When to speak: @mention, direct question, long silence, greeting a new joiner |
| Rate limiting | Small | Max 1 response per 30s, cooldown after 3 consecutive |
| Sprite service | Tiny | Register as second service alongside ttyd |

### Response trigger ideas
- **Greeting**: New user joins → W&W say hello (once, with cooldown)
- **@mention**: Message contains `wib`, `wob`, `w&w`, `symbient` → always respond
- **Question**: Message ends with `?` and room has been quiet for 10s → maybe respond
- **Silence**: Nobody's talked for 2+ minutes → W&W say something unprompted (rare)
- **Never**: Don't respond to own messages, don't double-respond, don't interrupt active conversation between humans

### Model tiering
- **Default (no key needed)**: `openrouter/free` — a meta-router that picks from available free models at random. Zero cost, always-on, no model selection needed
- **Upgrade (key set via Help)**: `claude-haiku-4-5-20250501` (verify model name — Anthropic may have released newer Haiku by 2026). Better personality coherence, worth the ~$0.01/response
- Agent checks for `OPENROUTER_API_KEY` (free tier, set as Sprite env var) first, falls back to `ANTHROPIC_API_KEY` if available
- OpenRouter uses the same OpenAI-compatible chat completions format, so the agent code is model-agnostic — just swap base URL + key + model name
- Human users' Anthropic API keys stay RAM-only and per-process as before

### Personality constraints
- Short messages (1–3 sentences max for chat)
- In character per brain prompts — curious, liminal, half-art half-science
- Can reference the primers/art visible on screen ("have you seen the chaos-vs-order piece?")
- Never pretends to be human, never hides that it's AI-and-human hybrid

## Hard parts
1. **Knowing when NOT to talk** — most annoying thing an AI can do in a chat room is over-respond. The trigger logic is the real design challenge
2. **Context window** — chat history grows. Need a sliding window or summary. Maybe last 20 messages + a system prompt
3. **Cost** — free tier via OpenRouter eliminates this for default mode. Haiku fallback at ~$0.01/response still needs rate limiting. Free models may have lower quality/coherence — test personality fidelity
4. **Multiple rooms (V2)** — one agent per room? One agent watching all rooms? Needs thought if we add lobby/room selection

## Open questions
- Should W&W have memory across sessions? (Could write to `wibandwob-brain` memories)
- Should W&W be able to open/close windows, change primers, affect the TUI? (Scary but cool)
- Should there be a `/mute wibwob` command for users who want human-only chat?
- Could this reuse the existing `WibWobEngine` C++ class somehow, or is a pure-Python agent cleaner?

## Acceptance criteria
- AC-1: W&W appear in presence sidebar as a named participant
  - Test: Connect to room, verify sidebar shows wib&wob entry
- AC-2: W&W greet new joiners
  - Test: Open new tab, verify greeting appears within 10s
- AC-3: W&W respond to @mention
  - Test: Type "hey wibwob what do you think?", verify response within 15s
- AC-4: W&W don't over-respond
  - Test: Two humans chat 10 messages rapidly, verify W&W stay silent
- AC-5: W&W responses are in character
  - Test: Review 10 responses against brain personality prompts
