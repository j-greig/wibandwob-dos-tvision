---
type: spike
status: not-started
tags: [refactor, ui, DRY]
tldr: "Extract 3 window sizing helpers from 6+ duplicated sites in test_pattern_app.cpp"
created: 2026-02-24
---

# SPK01 — Window Sizing Helpers

## Simple Description

`test_pattern_app.cpp` has 6+ places that compute "get desktop size, calculate window size, centre it." They all do the same maths slightly differently. There's already a helper (`api_centered_bounds`) but it's only used by the API spawn functions, not by the handleEvent menu commands. Extract and unify.

## What Already Exists

### `api_centered_bounds()` — line 4596

```cpp
static TRect api_centered_bounds(TTestPatternApp& app, int width, int height) {
    TRect d = TProgram::deskTop->getExtent();
    int dw = d.b.x - d.a.x;
    int dh = d.b.y - d.a.y;
    if (dw <= 0) dw = 80;
    if (dh <= 0) dh = 24;
    int left = d.a.x + (dw - width)  / 2;
    int top  = d.a.y + (dh - height) / 2;
    return TRect(left, top, left + width, top + height);
}
```

Used by ~20 `api_spawn_*` functions (lines 4608–4742, 4893).

### Problem: takes `TTestPatternApp&` but doesn't use it

The `app` param is unused — it accesses `TProgram::deskTop` directly. This is why handleEvent sites can't call it (they'd need to pass `*this` awkwardly). Drop the param, take `TDeskTop*` or use `TProgram::deskTop` directly.

## Duplicated Sites (grep-verified)

### Pattern 1: Fixed-size centred (4 sites)

All do: get desktop extent → compute centre offset → build TRect.

| Line | Context | Size | Notes |
|------|---------|------|-------|
| 1461 | `cmAppLauncher` | 63×20 fixed | Hand-rolls `(desk.b.x - ww) / 2` |
| 4596 | `api_centered_bounds()` | parameterised | The existing helper — but signature wrong |
| 4608–4734 | 20× `api_spawn_*` functions | various fixed sizes | All use `api_centered_bounds` ✓ |

### Pattern 2: Percentage centred (3 sites)

All do: get desktop extent → compute 90% of size → centre.

| Line | Context | Size | Notes |
|------|---------|------|-------|
| 1482 | `cmAsciiGallery` | 90% × 90% | `desk.b.x * 9 / 10` |
| 1802 | `cmNewPaintCanvas` | 90% × 90% | `(int)(dw * 0.9)` |
| 5066 | `api_spawn_paint` | 90% × 90% | `std::min((int)(dw * 0.9), dw)` — identical to 1802 |
| 4742 | `api_spawn_gallery` | 90% × 90% | Uses `api_centered_bounds(app, desk.b.x * 9/10, ...)` |

### Pattern 3: Cascade (1 site)

| Line | Context | Size | Notes |
|------|---------|------|-------|
| 1843 | `cmOpenImageFile` | 68×24, offset by `windowNumber % 10` | `int offset = (windowNumber - 1) % 10; TRect bounds(2 + offset*2, 1 + offset, ...)` |

## Proposed Helpers

Add to a new header or at top of `test_pattern_app.cpp` (near existing `api_centered_bounds`):

### 1. `centredRect` — replaces `api_centered_bounds`

```cpp
// Centre a window of fixed size on the desktop.
// Clamps to desktop bounds if larger.
static TRect centredRect(int w, int h) {
    TRect d = TProgram::deskTop->getExtent();
    int dw = d.b.x - d.a.x;
    int dh = d.b.y - d.a.y;
    if (dw <= 0) dw = 80;
    if (dh <= 0) dh = 24;
    w = std::min(w, dw);
    h = std::min(h, dh);
    int left = d.a.x + (dw - w) / 2;
    int top  = d.a.y + (dh - h) / 2;
    return TRect(left, top, left + w, top + h);
}
```

### 2. `centredRectPct` — for percentage-of-desktop pattern

```cpp
// Centre a window sized as a percentage of the desktop.
// pctW/pctH are 0.0–1.0 (e.g. 0.9 = 90%).
static TRect centredRectPct(float pctW, float pctH) {
    TRect d = TProgram::deskTop->getExtent();
    int dw = d.b.x - d.a.x;
    int dh = d.b.y - d.a.y;
    if (dw <= 0) dw = 80;
    if (dh <= 0) dh = 24;
    int w = (int)(dw * pctW);
    int h = (int)(dh * pctH);
    int left = d.a.x + (dw - w) / 2;
    int top  = d.a.y + (dh - h) / 2;
    return TRect(left, top, left + w, top + h);
}
```

### 3. `cascadedRect` — for stacked window spawns

```cpp
// Cascade windows with offset based on index (wraps every 10).
static TRect cascadedRect(int w, int h, int index) {
    int offset = index % 10;
    return TRect(2 + offset * 2, 1 + offset,
                 2 + offset * 2 + w, 1 + offset + h);
}
```

## Dev Handover (Codex Execution Notes)

### Preflight

```bash
rg -n "api_centered_bounds" app/test_pattern_app.cpp | head -5
rg -n "deskTop->getExtent" app/test_pattern_app.cpp | wc -l
rg -n "/ 2;" app/test_pattern_app.cpp | grep -i "left\|top\|x =\|y ="
```

Verify counts match this doc before patching.

### Step 1: Add helpers near line 4596

Add `centredRect()`, `centredRectPct()`, `cascadedRect()` as static functions immediately above `api_centered_bounds()`.

### Step 2: Rewrite `api_centered_bounds` to delegate

```cpp
static TRect api_centered_bounds(TTestPatternApp& /*app*/, int width, int height) {
    return centredRect(width, height);
}
```

This preserves all 20+ existing call sites unchanged. Zero risk.

### Step 3: Replace handleEvent duplicates

**cmAppLauncher (line ~1461):**
```cpp
// Before:
TRect desk = deskTop->getExtent();
int ww = 63, hh = 20;
int x = (desk.b.x - ww) / 2;
int y = (desk.b.y - hh) / 2;
if (x < 0) x = 0;
if (y < 0) y = 0;
TRect r(x, y, x + ww, y + hh);

// After:
TRect r = centredRect(63, 20);
```

**cmAsciiGallery (line ~1482):**
```cpp
// Before:
TRect desk = deskTop->getExtent();
int ww = desk.b.x * 9 / 10;
int hh = desk.b.y * 9 / 10;
int x = (desk.b.x - ww) / 2;
int y = (desk.b.y - hh) / 2;
TRect r(x, y, x + ww, y + hh);

// After:
TRect r = centredRectPct(0.9f, 0.9f);
```

**cmNewPaintCanvas (line ~1802):**
```cpp
// Before:
TRect d = deskTop->getExtent();
int dw = d.b.x - d.a.x;
int dh = d.b.y - d.a.y;
int w = (int)(dw * 0.9);
int h = (int)(dh * 0.9);
int left = d.a.x + (dw - w) / 2;
int top  = d.a.y + (dh - h) / 2;
TRect r(left, top, left + w, top + h);

// After:
TRect r = centredRectPct(0.9f, 0.9f);
```

**api_spawn_paint (line ~5066):**
```cpp
// Before:
int w = std::min((int)(dw * 0.9), dw);
int h = std::min((int)(dh * 0.9), dh);
int left = d.a.x + (dw - w) / 2;
int top  = d.a.y + (dh - h) / 2;
r = TRect(left, top, left + w, top + h);

// After:
r = centredRectPct(0.9f, 0.9f);
```

**cmOpenImageFile (line ~1843):**
```cpp
// Before:
windowNumber++;
int offset = (windowNumber - 1) % 10;
TRect bounds(2 + offset * 2, 1 + offset, 70 + offset * 2, 25 + offset);

// After:
windowNumber++;
TRect bounds = cascadedRect(68, 24, windowNumber - 1);
```

### Step 4: Build + verify

```bash
cmake --build build --target test_pattern -j4 2>&1 | grep error:
```

Must be zero errors. No functional change — all window positions should be identical.

### Step 5: Spot-check (optional)

Launch app, open each window type, verify it appears centred / cascaded as expected:
- View → App Launcher (should be centred 63×20)
- View → ASCII Gallery (should be centred 90%)
- Tools → New Paint Canvas (should be centred 90%)
- File → Open Image (should cascade)

## Acceptance Criteria

- AC-1: Three helper functions exist (`centredRect`, `centredRectPct`, `cascadedRect`)
  - Test: `rg "centredRect\|centredRectPct\|cascadedRect" app/test_pattern_app.cpp | wc -l` → ≥8
- AC-2: No hand-rolled centring maths remains in handleEvent
  - Test: `rg "/ 2;" app/test_pattern_app.cpp | grep -i "left\|top\|x =\|y ="` → only in helper functions
- AC-3: `api_centered_bounds` delegates to `centredRect`
  - Test: function body is one line
- AC-4: Clean build, no functional change
  - Test: `cmake --build build --target test_pattern -j4` → zero errors

## Effort

~30 minutes. Pure mechanical refactor, no new behaviour.
