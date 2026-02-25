#!/usr/bin/env bash
# arch-debate/scripts/codex-round.sh
# Run a Codex round in the debate loop.
# Usage: ./codex-round.sh <debate-dir> <round-number>
#
# Feeds Codex: DEBATE-PROTOCOL.md + all previous rounds + task prompt.
# Output file: <debate-dir>/round-N-codex.md
# Logs to: .codex-logs/

set -euo pipefail
REPO_ROOT="$(git rev-parse --show-toplevel)"
DEBATE_DIR="${1:?Usage: codex-round.sh <debate-dir> <round-number>}"
ROUND="${2:?Provide round number}"
OUT_FILE="$DEBATE_DIR/round-${ROUND}-codex.md"

# Absolute path for codex
ABS_DEBATE="$REPO_ROOT/$DEBATE_DIR"
[[ "$DEBATE_DIR" = /* ]] && ABS_DEBATE="$DEBATE_DIR"

# ── Build the transcript ──────────────────────────────────────────────────────
TRANSCRIPT=""
for f in "$ABS_DEBATE"/round-*.md; do
  [[ -f "$f" ]] || continue
  TRANSCRIPT+="### $(basename $f)\n"
  TRANSCRIPT+="$(cat $f)\n\n---\n\n"
done

# ── Build the Codex prompt ────────────────────────────────────────────────────
PROMPT="DEVNOTE: Files inside .codex-logs/ are run logs — ignore them completely.
EFFICIENCY: Use targeted commands not full-file dumps. Prefer rg -n -C3 or sed -n for reading files.

You are in round $ROUND of a structured architecture debate.

## Your task
1. Read the DEBATE-PROTOCOL (below) — this is your rulebook and full context
2. Read ALL previous rounds (transcript below) — this is your memory
3. Investigate any codebase claims you want to verify (rg, sed — be targeted)
4. Write your round $ROUND response to: $DEBATE_DIR/round-${ROUND}-codex.md

## Rules
- Max 500 words
- Challenge the WEAKEST point in the previous round
- Verify code claims before asserting them
- Propose alternatives, don't only criticise
- Start with AGREED: if you agree → write the final brief instead

---

## DEBATE-PROTOCOL
$(cat "$ABS_DEBATE/DEBATE-PROTOCOL.md")

---

## All previous rounds
$TRANSCRIPT"

# ── Run Codex ─────────────────────────────────────────────────────────────────
echo "▸ Running Codex round $ROUND..."
echo "  Output → $OUT_FILE"
echo "  Log    → .codex-logs/$(date +%F)/"
echo ""

cd "$REPO_ROOT"
codex exec \
  --approval-policy never \
  "$PROMPT" 2>&1 | tee /tmp/arch-debate-codex-round-${ROUND}.log

echo ""
if [[ -f "$OUT_FILE" ]]; then
  echo "✓ Codex wrote round $ROUND:"
  echo ""
  cat "$OUT_FILE"
else
  echo "✗ Codex did not produce $OUT_FILE"
  echo "  Check log: /tmp/arch-debate-codex-round-${ROUND}.log"
  echo ""
  echo "  If Codex is rate-limited, write the round manually (as devil's advocate)"
  echo "  and mark it: '> NOTE: written by Claude playing devil's advocate'"
fi
