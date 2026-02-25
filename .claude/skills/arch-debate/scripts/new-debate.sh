#!/usr/bin/env bash
# arch-debate/scripts/new-debate.sh
# Scaffold a new debate directory with protocol and opening stub.
# Usage: ./new-debate.sh <debate-dir> "<question>"

set -euo pipefail

DEBATE_DIR="${1:?Usage: new-debate.sh <debate-dir> '<question>'}"
QUESTION="${2:?Provide a question as second argument}"
mkdir -p "$DEBATE_DIR"

# ── DEBATE-PROTOCOL.md ────────────────────────────────────────────────────────
cat > "$DEBATE_DIR/DEBATE-PROTOCOL.md" << PROTO
# Architecture Debate Protocol

## Question
$QUESTION

## Purpose
Claude and Codex debate this decision in rounds until one side opens with AGREED:.

## Files
\`\`\`
debate/
  DEBATE-PROTOCOL.md     ← this file (read every round)
  round-0-claude.md      ← Claude's opening position
  round-1-codex.md       ← Codex round 1
  round-2-claude.md      ← Claude round 2
  ...
  round-N-agreed.md      ← final settled architecture
\`\`\`

## Rules for every round
1. Read ALL previous rounds before writing (they are your memory)
2. Challenge the weakest point in the previous round
3. Verify codebase claims with rg/sed before asserting them
4. Propose alternatives — don't only criticise
5. Max 500 words per round
6. Open with \`AGREED:\` to converge → write final brief instead of rebuttal

## Codebase context
<!-- Fill in: repo root, key files, existing constraints, what already works -->

## Options under consideration
<!-- Fill in: the candidate architectures being debated -->

## Open questions
<!-- Fill in: things neither side knows yet that affect the decision -->
PROTO

# ── round-0-claude.md stub ────────────────────────────────────────────────────
cat > "$DEBATE_DIR/round-0-claude.md" << STUB
# Round 0 — Claude's opening position

## Recommended approach
<!-- State your architecture recommendation here -->

## Why this wins
<!-- Key reasons — be concrete, not general -->

## Failure modes I already see
<!-- Name the weaknesses in your own proposal -->

## What I'm least confident about
<!-- Flag uncertainty — this is what Codex will probe -->
STUB

echo "✓ Scaffolded: $DEBATE_DIR"
echo ""
echo "Next steps:"
echo "  1. Fill in DEBATE-PROTOCOL.md — codebase context, options, open questions"
echo "  2. Fill in round-0-claude.md — your opening position"
echo "  3. Run: .claude/skills/arch-debate/scripts/codex-round.sh $DEBATE_DIR 1"
