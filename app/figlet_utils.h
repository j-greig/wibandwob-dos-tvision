#ifndef FIGLET_UTILS_H
#define FIGLET_UTILS_H

#include <string>
#include <vector>
#include <map>

namespace figlet {

// ── Render ────────────────────────────────────────────────────────────────────

// Render text using the figlet CLI. Returns rendered ASCII art string.
// If width > 0, passes -w <width> to figlet for wrapping.
// Returns empty string on failure (binary not found, bad font, etc.).
std::string render(const std::string& text, const std::string& font = "standard",
                   int width = 0);

// Split rendered figlet output into individual lines.
std::vector<std::string> renderLines(const std::string& text,
                                     const std::string& font = "standard",
                                     int width = 0);

// ── Font discovery ────────────────────────────────────────────────────────────

// Return a curated list of popular figlet font names.
const std::vector<std::string>& curatedFonts();

// List all available .flf font files from the figlet font directory.
// Falls back to curated list if font dir can't be read.
std::vector<std::string> listFonts();

// Check whether the figlet binary is available on PATH.
bool isAvailable();

// Return the figlet font directory path (from `figlet -I2`).
std::string fontDir();

// ── Font catalogue (from modules/wibwob-figlet-fonts/fonts.json) ─────────────

struct FontMeta {
    int height;
    int width;
};

struct FontCategory {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> fonts;
};

struct FontCatalogue {
    std::vector<FontCategory> categories;
    std::vector<std::string> favourites;
    std::map<std::string, FontMeta> metadata;
    bool loaded;
};

// Load and cache the font catalogue from fonts.json.
// Safe to call multiple times — loads once, returns cached reference.
const FontCatalogue& catalogue();

// Sorted list of all font names (from catalogue metadata keys).
// Stable ordering for command ID mapping: cmFigletCatFontBase + index.
const std::vector<std::string>& allFontsSorted();

// Look up font name by index into allFontsSorted(). Returns "" if out of range.
const std::string& fontByIndex(int index);

// Look up index for a font name. Returns -1 if not found.
int fontIndex(const std::string& name);

// Return the character height for a font (lines per row of text).
// Uses the catalogue font_metadata. Returns 0 if unknown.
int fontHeight(const std::string& name);

} // namespace figlet

// Forward-declare tvision types (avoids pulling in tv.h from this header)
class TMenuItem;

namespace figlet {

// Build tvision TMenuItem* chain for a font category submenu.
// Uses cmBase + fontIndex() for each item. Marks currentFont with checkmark.
TMenuItem* buildCategoryMenuItems(const FontCategory& cat,
                                  unsigned short cmBase,
                                  const std::string& currentFont);

} // namespace figlet

#endif // FIGLET_UTILS_H
