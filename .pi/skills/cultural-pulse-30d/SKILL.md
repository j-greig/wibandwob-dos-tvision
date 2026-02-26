# Cultural Pulse 30d

Scan the internet zeitgeist. Pulls trending content from 17 free, no-auth
sources and produces a structured cultural pulse report. Defaults to last 30 days.

## Quick Start

```bash
uv run ~/Repos/symbient-skills/skills/cultural-pulse-30d/pulse.py
```

## Usage Examples

```bash
# Full pulse (5 default sources)
uv run pulse.py

# Last 7 days
uv run pulse.py --days 7

# Presets — curated combos
uv run pulse.py --preset memes     # Meme subs + Lemmy + Mastodon + Google Trends
uv run pulse.py --preset viral     # What's going viral RIGHT NOW
uv run pulse.py --preset news      # BBC + Google News + global + Wikipedia
uv run pulse.py --preset global    # Pan-cultural, multi-geo trends
uv run pulse.py --preset tech      # HN + Lobste.rs + Product Hunt + arXiv
uv run pulse.py --preset culture   # Art, books, wonder, nature, music
uv run pulse.py --preset social    # Reddit + Mastodon + Bluesky + Lemmy
uv run pulse.py --preset deep      # All 17 sources

# Specific sections
uv run pulse.py --sections reddit,bbc,mastodon

# Filter by topic (word-boundary match)
uv run pulse.py --topic "AI"

# Standalone TL;DR summary with cross-source themes
uv run pulse.py --summary

# Compact output (titles + scores only)
uv run pulse.py --compact

# Limit items per section
uv run pulse.py --limit 5

# JSON output for piping into other tools
uv run pulse.py --json

# Write to file
uv run pulse.py -o pulse-report.md

# Custom subreddits
uv run pulse.py --subreddits "all,memes,dankmemes,me_irl"

# BBC section filter
uv run pulse.py --bbc-sections "arts,tech"

# Google News topic
uv run pulse.py --sections gnews --gnews-topic entertainment

# List all sections and presets
uv run pulse.py --list-sections
```

## Sources (17 total, all free, no auth)

### Default sections (always included unless overridden)
| Section | Source | Signal |
|---------|--------|--------|
| `reddit` | Reddit .json API | Mass culture, memes, viral moments |
| `hn` | Hacker News Firebase API | Tech culture, startup discourse |
| `bbc` | BBC RSS feeds (5 sections) | World news baseline, arts, tech, science |
| `wikipedia` | Wikimedia Pageviews API | What people are actually looking up |
| `gtrends` | Google Trends RSS (10 geos) | What the world is searching (US, GB, JP, BR, IN, DE, FR, KR, NG, MX) |

### Optional sections (via --sections, --preset, or --all)
| Section | Source | Signal |
|---------|--------|--------|
| `mastodon` | Mastodon trends API | Fediverse pulse, trending links & tags |
| `bluesky` | Bluesky public API | Social media trending topics |
| `gnews` | Google News RSS | Editorial news selection (5 topics) |
| `lobsters` | Lobste.rs JSON API | Programming & design culture |
| `lemmy` | Lemmy REST API | Reddit-alternative communities |
| `producthunt` | Product Hunt Atom feed | Startup & product launches |
| `arxiv` | arXiv API | Academic research trends |
| `global` | Al Jazeera, DW, France 24, Guardian, Rest of World | Non-Anglo global news |
| `culture` | Atlas Obscura, Open Culture, Colossal, NASA APOD, XKCD, Bandcamp | Art, wonder, science, music |
| `books` | OpenLibrary trending | What people are reading right now |

## Presets

| Preset | Sources | Use Case |
|--------|---------|----------|
| `default` | reddit, hn, bbc, wikipedia, gtrends | Balanced cultural pulse |
| `memes` | reddit (meme subs), lemmy, mastodon, gtrends | Viral/meme hunting |
| `viral` | reddit (viral subs), gtrends, mastodon, bluesky | What's going viral RIGHT NOW |
| `news` | bbc, gnews, mastodon, wikipedia, global | Hard news + global coverage |
| `global` | gtrends, global, wikipedia, mastodon, reddit | Pan-cultural, non-Anglo |
| `tech` | hn, lobsters, producthunt, arxiv | Tech/startup/research |
| `culture` | culture, books, reddit (art subs), mastodon, wikipedia | Art, books, wonder, music |
| `social` | reddit, mastodon, bluesky, lemmy | Social platform pulse |
| `deep` | all 17 sources | Maximum coverage |

## Output Formats

- **Markdown** (default): Sections with titles, scores, sources, links
- **Compact** (`--compact`): Titles and scores only
- **Summary** (`--summary`): Standalone TL;DR with top stories (descriptions + links), editorial headlines, cross-source themes with examples, source coverage table
- **JSON** (`--json`): Machine-readable for piping into other tools

## How It Works

Zero dependencies beyond Python stdlib. Uses `urllib.request` for HTTP and
`xml.etree.ElementTree` for RSS/Atom parsing. Reddit needs a User-Agent
header (included). Wikipedia pageviews have a ~2 day lag. Google News
links are redirect URLs. Google Trends fetches from 10 country geos for
global coverage.

## Skill Workflow (for Claude)

This skill has two phases. The script is the data layer. You are the synthesis layer.

### Phase 1: Gather

Run the script. Choose preset based on context (what the user needs, what the
downstream creative task is). Use `--json` when you need to digest the data
yourself. Use `--summary` when producing a human-readable report.

```bash
uv run ~/Repos/symbient-skills/skills/cultural-pulse-30d/pulse.py --preset deep --json
```

### Phase 2: Synthesize Narrative Hooks

The script's "Recurring Themes" section outputs keyword clusters — raw word
co-occurrences like "bunny", "movies", "anti". These are data, not insight.

**Your job is to transform each theme cluster into a narrative hook** — a
self-contained cultural capsule that captures WHO + WHAT + WHY IT RESONATES.

A narrative hook is promptable: hand it to any LLM and it immediately knows
the cultural moment, the stakes, the memetic energy, and how to riff on it.

#### What a narrative hook looks like

Bad:
> **bunny** (across BBC, Wikipedia, Google)

Good:
> **Bad Bunny's Grammy-to-Super Bowl coronation cements Latino mainstream dominance**
> Sources: BBC, Wikipedia, Google Trends | Engagement: 144K wiki views, Grammy speech viral

Bad:
> **movies** (across Fauxmoi, Google, popculturechat)

Good:
> **Hollywood A-listers weaponize award show stages as anti-ICE protest platforms**
> Edward Norton, Billie Eilish, Pedro Pascal, Natalie Portman — each using their moment at the mic.
> Sources: Fauxmoi, Google News, popculturechat | Cross-platform amplification

Bad:
> **epstein** (across BBC, Mastodon, Wikipedia)

Good:
> **Epstein files drop implicates new names — Gates STD claim, RFK Jr fossil trip, Mandelson holiday home**
> Each revelation spawns its own news cycle. Wikipedia views >1M.
> Sources: BBC, Mastodon, Wikipedia, Reddit | Multi-day sustained engagement

#### Rules for narrative hooks

- Each hook is one sentence that encapsulates the cultural signal
- Include specific names, specific actions, specific stakes
- Capture WHY it's resonating, not just WHAT happened
- Note cross-platform amplification when present (same story on Reddit + BBC + Wikipedia = real)
- Flag memetic potential: is this something people are riffing on, or just reading about?
- A person's name alone is not a hook. "Billie Eilish" is a name. "Billie Eilish tells the Grammys audience to fuck ICE, splits the internet" is a hook.
- A dictionary word is never a hook. "winter" is a word. "Winter Olympics opening ceremony turns political as Vance gets booed in Milan" is a hook.

### Phase 3: Deliver

Output format depends on downstream use:

- **For human reading**: Narrative hooks + source coverage + top stories with links
- **For creative pipelines** (`/forge`, `/wake2`, `/wake3`): Narrative hooks as a bulleted list of promptable cultural signals, ready to collide with creative briefs
- **For `/tweet`**: Pick the hook with highest memetic potential, draft around it
- **For `/monodraw-ascii-art-via-cli`**: Pick hooks with strong visual/symbolic potential

### Pairing with other skills

- `/monodraw-ascii-art-via-cli` for ASCII art compositions
- `/tweet` for posting cultural commentary
- `/forge` for creative sessions driven by current events
- `/wake2` or `/wake3` for autonomous creative runs anchored in zeitgeist
