#!/bin/bash
# Regression test: TMenuItem with command=0 causes null submenu crash (E015)
#
# tvision treats command==0 as "this item has a submenu" and dereferences
# subMenu->items without null check in findHotKey(). This causes a segfault
# on EVERY keypress because the menu bar scans all items for hotkeys.
#
# This test greps the source for the dangerous pattern to prevent regression.
# A proper C++ unit test would need to link tvision, so this static check
# is the pragmatic alternative.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAIL=0

echo "=== Menu null-submenu regression test ==="

# Pattern: new TMenuItem("...", 0, ...) where the second arg is 0
# This matches: TMenuItem("label", 0, kbNoKey) and similar
# Exclude comments and this test file itself
HITS=$(grep -rn 'new TMenuItem(' "$REPO_ROOT/app/" --include="*.cpp" --include="*.h" \
    | grep -v '// *test\|test_menu_null' \
    | grep -E 'TMenuItem\([^,]+,\s*0\s*,' || true)

if [ -n "$HITS" ]; then
    echo "FAIL: Found TMenuItem with command=0 (causes null submenu crash):"
    echo "$HITS"
    echo ""
    echo "Fix: use cmNoOp (999) instead of 0. See room_chat_view.h."
    FAIL=1
else
    echo "PASS: No TMenuItem with command=0 found"
fi

# Also check for cmValid (which is 0 in tvision)
HITS2=$(grep -rn 'new TMenuItem(' "$REPO_ROOT/app/" --include="*.cpp" --include="*.h" \
    | grep -v '// *test\|test_menu_null' \
    | grep -E 'TMenuItem\([^,]+,\s*cmValid\s*,' || true)

if [ -n "$HITS2" ]; then
    echo "FAIL: Found TMenuItem with cmValid (=0, same crash):"
    echo "$HITS2"
    FAIL=1
else
    echo "PASS: No TMenuItem with cmValid found"
fi

exit $FAIL
