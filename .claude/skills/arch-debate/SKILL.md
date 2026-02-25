---
name: arch-debate
description: Run a structured multi-round architecture debate between Claude and Codex to converge on a hard technical decision. Claude writes a position, Codex challenges it, back and forth until AGREED. All rounds saved as markdown files for audit trail and reuse. Use when facing a non-obvious architecture choice with real tradeoffs, when you want a second opinion stress-tested rather than just accepted, or when building a PRD collaboratively. Triggers on "arch debate", "debate the architecture", "get codex to challenge", "architecture decision", "PRD loop".
---

# arch-debate

Structured back-and-forth between Claude and Codex to stress-test an
architecture decision. All rounds live in a `debate/` subdirectory.
Loop runs until one side opens a round with `AGREED:`.

## Worked example

E015 Sprites hosting — 4 rounds, 1 fatal flaw found:
`.planning/epics/e015-sprites-hosting/debate/`

Round 0: Claude proposed shared tmux (simple, one process)  
Round 1: Codex rejected — one PartyKit connection = broken identity  
Round 2: Claude conceded, proposed per-connection fork + source-verified socket path  
Round 3: Codex found bridge zombie risk (retry on socket-gone), otherwise agreed  
Round 4: AGREED — wrote final architecture + one required code fix scoped  

---

## Start a new debate

```bash
.claude/skills/arch-debate/scripts/new-debate.sh <path> "<question>"
# e.g.
.claude/skills/arch-debate/scripts/new-debate.sh \
  .planning/epics/e016-foo/debate \
  "Should we use Redis or SQLite for session state?"
```

Creates:
```
<path>/
  DEBATE-PROTOCOL.md   ← auto-generated with your question + context slots
  round-0-claude.md    ← stub for your opening position
```

---

## Loop mechanics

### Round 0 — write your position (Claude)
Fill in `round-0-claude.md`:
- State your recommended architecture concisely
- Name the failure modes you already see
- Flag what you're least confident about

### Round N (odd) — run Codex
```bash
.claude/skills/arch-debate/scripts/codex-round.sh <debate-dir> <round-number>
# e.g.
.claude/skills/arch-debate/scripts/codex-round.sh \
  .planning/epics/e015/debate 1
```

The script feeds Codex: DEBATE-PROTOCOL.md + all previous rounds + the prompt.
Output goes to `round-N-codex.md`.

If Codex is rate-limited, Claude writes the Codex round as devil's advocate
(mark the file with `> NOTE: written by Claude playing devil's advocate`).

### Round N (even) — write your rebuttal (Claude)
Read Codex's response. Update your position in `round-N-claude.md`:
- Concede points you lost
- Push back with new evidence from the codebase (run `rg`, `sed` to verify claims)
- Narrow the disagreement

### Convergence
When you're ready to agree — or when Codex opens with `AGREED:` — write
`round-N-agreed.md` with:
1. The settled architecture (diagram + key decisions)
2. Any required code changes scoped
3. Open questions that are implementation-level (not architecture-level)

Then update the epic brief from the agreed doc.

---

## Codex prompt template (used by `codex-round.sh`)

```
EFFICIENCY: Use targeted commands, not full-file dumps.

You are in round N of a structured architecture debate.

1. Read <debate-dir>/DEBATE-PROTOCOL.md — rules + full context
2. Read ALL previous rounds in <debate-dir>/ — this is your memory
3. Investigate codebase claims you want to challenge (rg, sed)
4. Write your response to <debate-dir>/round-N-codex.md

Rules:
- Max 500 words
- Challenge the weakest point in the previous round
- Be concrete — name specific failure modes
- Propose alternatives, don't only critique
- Start with AGREED: if you agree, then write the final brief
```

---

## Rules for each round

| Rule | Detail |
|------|--------|
| Max 500 words | Stay sharp. Long rounds lose focus. |
| Challenge ONE thing | Pick the weakest point, go deep on it |
| Verify with source | Run `rg`/`sed` before claiming something about the code |
| Propose, don't just criticise | Every objection needs an alternative |
| AGREED: prefix | Opens with this → write final brief instead of rebuttal |
| 4–6 rounds max | If not converged by round 6, you're stuck on values not facts — decide |

---

## When to use this

✅ Non-obvious tradeoff (at least 2 plausible options with real costs)  
✅ Architecture that will be hard to change later  
✅ PRD that needs pressure-testing before implementation starts  
✅ You want an audit trail of why a decision was made  

❌ Trivial decisions ("which variable name")  
❌ When you already know the answer and just need someone to agree  
❌ When Codex credits are depleted and you're in a hurry  

---

## File naming convention

```
debate/
  DEBATE-PROTOCOL.md       ← context + rules (read every round)
  round-0-claude.md
  round-1-codex.md
  round-2-claude.md
  round-3-codex.md
  round-N-agreed.md        ← final settled architecture
```

Always commit the full debate — it's the decision record.
