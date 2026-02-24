# Retrospective: FIGlet Font Catalogue + Edit Menu Wiring

**Commit:** e375844  
**Date:** 2026-02-24  
**Scope:** Font catalogue loader, Edit menu category submenus, More Fonts dialog

---

## Friction Points

### 1. tvision API guesswork (biggest time sink)
- `TGroup::current` is a **member variable**, not a method → wrote `deskTop->current()` → compile error
- `TMenuItem` submenu constructor signature is non-obvious (no command field for submenus)
- `newLine()` returns `TMenuItem&` — can't use `+` with `TMenuItem*`, need to set `.next` manually
- These are all things I've hit before (see summary context) and will hit again

**Skill idea: `tvision-api` skill**  
A pi skill with a cheatsheet of tvision idioms: menu construction patterns, `TGroup::current` vs `TView::current`, `TMenuItem` constructor overloads, `newLine()` usage, `execView` patterns, `TDrawBuffer` rules, `changeBounds` vs `setState`. Extract from accumulated tribal knowledge in this repo. Would eliminate 80% of the compile-fix-compile cycles.

### 2. No JSON library → 100 LOC hand-rolled parser
- Spent significant time writing `skipWs`, `parseStr`, `parseInt`, `parseStrArray`, `findKey`, `parseFontCatalogue`
- Fragile, schema-coupled, will break if fonts.json structure changes
- Already discussed codegen alternative (Python script → generated .h) but deferred

**Skill idea: `cpp-json-codegen` skill or prompt template**  
"Given a JSON schema, generate a Python script that reads the JSON and emits a C++ header with static const data." One-shot prompt template that produces the codegen script. Kills the parser problem entirely for any data module consumed by C++.

### 3. Namespace/forward-declaration conflicts
- Forward-declared `TMenuItem` inside `namespace figlet` → compiler saw `figlet::TMenuItem` ≠ `::TMenuItem`
- Had to close namespace, forward-declare in global scope, re-open namespace
- Classic C++ footgun when mixing app namespaces with library types

**Not really a skill problem** — just C++ being C++. But the tvision skill could note "never forward-declare tvision types inside your own namespace."

### 4. Build-test cycle overhead
- 6 compile errors after first attempt, each requiring read-edit-build
- The errors were predictable: wrong access specifiers, member-vs-method, namespace scoping
- A pre-flight "does this compile?" check before committing the full edit would help

**Prompt template idea: `preflight-build`**  
After making edits, always run a build before reporting success. Could be a pi hook or a standard suffix in the wibwobdos skill: "After editing C++ files, run `cmake --build build --target <target> 2>&1 | grep error:` and fix before proceeding."  
Actually this should just be in the **wibwobdos skill** as a mandatory step.

### 5. Multi-agent file collision (from summary, not this session)
- pi1 and pi2 both editing `test_pattern_app.cpp` → lost work
- Discussed git worktrees but didn't implement

**Skill idea: `worktree-guard` extension**  
Pi extension that checks if another agent has unstaged changes to files you're about to edit. Warns or auto-creates a worktree. Needs pi's extension API to hook into file writes.

### 6. "Should the parser be in the module?" design discussion
- Good discussion but took multiple turns to resolve
- The answer was obvious in hindsight: Pi modules = data, consumers = accessors, C++ needs codegen bridge

**Prompt template: `module-data-pattern`**  
"When creating a shared data module consumed by C++: (1) module ships JSON as source of truth, (2) add `scripts/generate_header.py` for C++ codegen, (3) consumer #includes generated header, (4) no runtime JSON parsing."

---

## Proposed Actions

| Priority | Action | Type | Effort |
|----------|--------|------|--------|
| **P0** | Add "always build after C++ edits" to wibwobdos skill | Skill update | 5 min |
| **P1** | Create `tvision-api` cheatsheet skill | New skill | 30 min |
| **P1** | Replace hand-rolled JSON parser with codegen script | Code change | 20 min |
| **P2** | Create `cpp-json-codegen` prompt template | Prompt template | 15 min |
| **P2** | Create `module-data-pattern` prompt template | Prompt template | 10 min |
| **P3** | Investigate worktree-guard extension for multi-agent | Spike | 1 hr |

---

## What Went Well
- Font catalogue design (7 categories, 148 fonts, favourites) was right first time
- `buildCategoryMenuItems()` as a shared utility works for both right-click and menu bar
- "More Fonts..." TListBox dialog was straightforward
- Catalogue lazy-loading with fallback to curatedFonts() is resilient
