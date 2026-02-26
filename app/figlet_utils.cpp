#include "figlet_utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

namespace figlet {

// ── Font directory cache ──────────────────────────────────────────────────────

static std::string cachedFontDir;
static bool fontDirResolved = false;

std::string fontDir() {
    if (fontDirResolved) return cachedFontDir;
    fontDirResolved = true;

    // Ask figlet for its font directory
    FILE* pipe = popen("figlet -I2 2>/dev/null", "r");
    if (pipe) {
        char buf[1024];
        if (fgets(buf, sizeof(buf), pipe)) {
            cachedFontDir = buf;
            // Strip trailing newline
            while (!cachedFontDir.empty() &&
                   (cachedFontDir.back() == '\n' || cachedFontDir.back() == '\r'))
                cachedFontDir.pop_back();
        }
        pclose(pipe);
    }
    return cachedFontDir;
}

// ── Availability check ───────────────────────────────────────────────────────

static int cachedAvailable = -1;  // -1 = unknown

bool isAvailable() {
    if (cachedAvailable >= 0) return cachedAvailable != 0;
    FILE* pipe = popen("figlet -v </dev/null 2>/dev/null", "r");
    if (!pipe) { cachedAvailable = 0; return false; }
    char buf[256];
    bool gotOutput = (fgets(buf, sizeof(buf), pipe) != nullptr);
    int rc = pclose(pipe);
    cachedAvailable = (rc == 0 && gotOutput) ? 1 : 0;
    return cachedAvailable != 0;
}

// ── Render ───────────────────────────────────────────────────────────────────

std::string render(const std::string& text, const std::string& font, int width) {
    if (!isAvailable()) return {};

    // Escape double quotes in text
    std::string escaped = text;
    size_t pos = 0;
    while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\"");
        pos += 2;
    }

    // Build command
    std::string dir = fontDir();
    std::string cmd = "figlet";
    if (!dir.empty())
        cmd += " -d \"" + dir + "\"";
    cmd += " -f \"" + font + "\"";
    if (width > 0)
        cmd += " -w " + std::to_string(width);
    cmd += " \"" + escaped + "\" </dev/null 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};

    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe))
        output += buffer;
    pclose(pipe);
    return output;
}

std::vector<std::string> renderLines(const std::string& text,
                                     const std::string& font, int width) {
    std::string raw = render(text, font, width);
    std::vector<std::string> lines;
    if (raw.empty()) return lines;

    // Strip trailing newlines
    while (!raw.empty() && raw.back() == '\n') raw.pop_back();

    size_t start = 0;
    while (start < raw.size()) {
        size_t nl = raw.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(raw.substr(start));
            break;
        }
        lines.push_back(raw.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

// ── Curated font list ────────────────────────────────────────────────────────

const std::vector<std::string>& curatedFonts() {
    static const std::vector<std::string> fonts = {
        "standard", "big", "banner", "block", "bubble", "digital",
        "doom", "gothic", "graffiti", "ivrit", "lean", "mini",
        "script", "shadow", "slant", "small", "speed", "starwars",
        "thick", "thin"
    };
    return fonts;
}

// ── List all fonts ───────────────────────────────────────────────────────────

std::vector<std::string> listFonts() {
    std::string dir = fontDir();
    std::vector<std::string> fonts;

#ifndef _WIN32
    if (!dir.empty()) {
        DIR* d = opendir(dir.c_str());
        if (d) {
            struct dirent* entry;
            while ((entry = readdir(d)) != nullptr) {
                std::string name = entry->d_name;
                // Match *.flf files
                if (name.size() > 4 && name.substr(name.size() - 4) == ".flf") {
                    fonts.push_back(name.substr(0, name.size() - 4));
                }
            }
            closedir(d);
            std::sort(fonts.begin(), fonts.end());
        }
    }
#endif

    if (fonts.empty()) {
        // Fallback to curated list
        return std::vector<std::string>(curatedFonts().begin(), curatedFonts().end());
    }
    return fonts;
}

// ── Font catalogue loader ─────────────────────────────────────────────────────

// Minimal JSON helpers — just enough to parse fonts.json structure.
// Not a general JSON parser.

static std::string readFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string buf(sz, '\0');
    size_t got = fread(&buf[0], 1, sz, f);
    fclose(f);
    buf.resize(got);
    return buf;
}

// Skip whitespace
static size_t skipWs(const std::string& s, size_t i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t'))
        ++i;
    return i;
}

// Parse a JSON string (starting at the opening quote). Returns the string and advances pos.
static std::string parseStr(const std::string& s, size_t& pos) {
    if (pos >= s.size() || s[pos] != '"') return {};
    ++pos;
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            ++pos;
            if (s[pos] == 'n') result += '\n';
            else result += s[pos];
        } else {
            result += s[pos];
        }
        ++pos;
    }
    if (pos < s.size()) ++pos; // skip closing "
    return result;
}

// Parse a JSON integer
static int parseInt(const std::string& s, size_t& pos) {
    int val = 0;
    bool neg = false;
    if (pos < s.size() && s[pos] == '-') { neg = true; ++pos; }
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
        val = val * 10 + (s[pos] - '0');
        ++pos;
    }
    return neg ? -val : val;
}

// Parse a JSON array of strings
static std::vector<std::string> parseStrArray(const std::string& s, size_t& pos) {
    std::vector<std::string> result;
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != '[') return result;
    ++pos;
    while (true) {
        pos = skipWs(s, pos);
        if (pos >= s.size() || s[pos] == ']') { ++pos; break; }
        if (s[pos] == ',') { ++pos; continue; }
        result.push_back(parseStr(s, pos));
    }
    return result;
}

// Find a key in the current JSON object scope (pos should be just inside '{')
// Returns position after the colon, or std::string::npos
static size_t findKey(const std::string& s, size_t from, const std::string& key) {
    // Simple scan: look for "key" :
    std::string needle = "\"" + key + "\"";
    size_t pos = s.find(needle, from);
    if (pos == std::string::npos) return std::string::npos;
    pos += needle.size();
    pos = skipWs(s, pos);
    if (pos < s.size() && s[pos] == ':') ++pos;
    pos = skipWs(s, pos);
    return pos;
}

static std::string findFontsJsonPath() {
    const char* moduleDirs[] = { "modules-private", "modules" };
    for (const char* base : moduleDirs) {
        std::string path = std::string(base) + "/wibwob-figlet-fonts/fonts.json";
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            return path;
    }
    return {};
}

static FontCatalogue parseFontCatalogue(const std::string& json) {
    FontCatalogue cat;
    cat.loaded = false;
    if (json.empty()) return cat;

    // Parse categories array
    size_t catPos = findKey(json, 0, "categories");
    if (catPos != std::string::npos && json[catPos] == '[') {
        ++catPos;
        // Each category is an object
        while (true) {
            catPos = skipWs(json, catPos);
            if (catPos >= json.size() || json[catPos] == ']') break;
            if (json[catPos] == ',') { ++catPos; continue; }
            if (json[catPos] != '{') break;

            // Find the end of this object
            size_t objStart = catPos;
            int depth = 1;
            ++catPos;
            while (catPos < json.size() && depth > 0) {
                if (json[catPos] == '{') ++depth;
                else if (json[catPos] == '}') --depth;
                ++catPos;
            }
            std::string obj = json.substr(objStart, catPos - objStart);

            FontCategory fc;
            size_t p;
            p = findKey(obj, 0, "id");
            if (p != std::string::npos) fc.id = parseStr(obj, p);
            p = findKey(obj, 0, "name");
            if (p != std::string::npos) fc.name = parseStr(obj, p);
            p = findKey(obj, 0, "description");
            if (p != std::string::npos) fc.description = parseStr(obj, p);
            p = findKey(obj, 0, "fonts");
            if (p != std::string::npos) fc.fonts = parseStrArray(obj, p);
            cat.categories.push_back(fc);
        }
    }

    // Parse favourites
    size_t favPos = findKey(json, 0, "favourites");
    if (favPos != std::string::npos)
        cat.favourites = parseStrArray(json, favPos);

    // Parse font_metadata
    size_t metaPos = findKey(json, 0, "font_metadata");
    if (metaPos != std::string::npos && json[metaPos] == '{') {
        ++metaPos;
        while (true) {
            metaPos = skipWs(json, metaPos);
            if (metaPos >= json.size() || json[metaPos] == '}') break;
            if (json[metaPos] == ',') { ++metaPos; continue; }
            if (json[metaPos] != '"') break;

            std::string fontName = parseStr(json, metaPos);
            metaPos = skipWs(json, metaPos);
            if (metaPos < json.size() && json[metaPos] == ':') ++metaPos;
            metaPos = skipWs(json, metaPos);

            FontMeta fm = {0, 0};
            if (metaPos < json.size() && json[metaPos] == '{') {
                size_t objStart = metaPos;
                int depth = 1;
                ++metaPos;
                while (metaPos < json.size() && depth > 0) {
                    if (json[metaPos] == '{') ++depth;
                    else if (json[metaPos] == '}') --depth;
                    ++metaPos;
                }
                std::string obj = json.substr(objStart, metaPos - objStart);
                size_t hp = findKey(obj, 0, "height");
                if (hp != std::string::npos) fm.height = parseInt(obj, hp);
                size_t wp = findKey(obj, 0, "width");
                if (wp != std::string::npos) fm.width = parseInt(obj, wp);
            }
            cat.metadata[fontName] = fm;
        }
    }

    cat.loaded = !cat.categories.empty();
    return cat;
}

const FontCatalogue& catalogue() {
    static FontCatalogue cached;
    static bool tried = false;
    if (!tried) {
        tried = true;
        std::string path = findFontsJsonPath();
        if (!path.empty()) {
            std::string json = readFile(path);
            cached = parseFontCatalogue(json);
            if (cached.loaded)
                fprintf(stderr, "[figlet] Loaded font catalogue: %zu categories, %zu favourites, %zu fonts\n",
                    cached.categories.size(), cached.favourites.size(), cached.metadata.size());
        }
        if (!cached.loaded) {
            // Fallback: single category from curatedFonts
            FontCategory fc;
            fc.id = "favourites";
            fc.name = "Favourites";
            fc.fonts = std::vector<std::string>(curatedFonts().begin(), curatedFonts().end());
            cached.categories.push_back(fc);
            cached.favourites = fc.fonts;
            cached.loaded = true;
        }
    }
    return cached;
}

// ── Sorted font list and index helpers ────────────────────────────────────────

const std::vector<std::string>& allFontsSorted() {
    static std::vector<std::string> sorted;
    static bool built = false;
    if (!built) {
        built = true;
        const auto& cat = catalogue();
        for (const auto& kv : cat.metadata)
            sorted.push_back(kv.first);
        std::sort(sorted.begin(), sorted.end());
        // Fallback if catalogue empty
        if (sorted.empty())
            sorted = std::vector<std::string>(curatedFonts().begin(), curatedFonts().end());
    }
    return sorted;
}

static const std::string emptyStr;

const std::string& fontByIndex(int index) {
    const auto& fonts = allFontsSorted();
    if (index < 0 || index >= (int)fonts.size()) return emptyStr;
    return fonts[index];
}

int fontIndex(const std::string& name) {
    const auto& fonts = allFontsSorted();
    auto it = std::lower_bound(fonts.begin(), fonts.end(), name);
    if (it != fonts.end() && *it == name)
        return (int)(it - fonts.begin());
    return -1;
}

int fontHeight(const std::string& name) {
    const auto& cat = catalogue();
    auto it = cat.metadata.find(name);
    if (it != cat.metadata.end() && it->second.height > 0)
        return it->second.height;
    return 0;
}

} // namespace figlet

// ── Menu builder (outside namespace — uses tvision types) ────────────────────

#define Uses_TMenuItem
#define Uses_TMenu
#define Uses_TKeys
#include <tvision/tv.h>

TMenuItem* figlet::buildCategoryMenuItems(const figlet::FontCategory& cat,
                                          ushort cmBase,
                                          const std::string& currentFont) {
    TMenuItem* items = nullptr;
    // Build in reverse for correct linked-list order
    for (int i = (int)cat.fonts.size() - 1; i >= 0; i--) {
        const std::string& fname = cat.fonts[i];
        int idx = figlet::fontIndex(fname);
        if (idx < 0) continue;

        std::string label = (fname == currentFont)
            ? ("\xFB " + fname)   // checkmark
            : ("  " + fname);

        char* str = new char[label.size() + 1];
        std::strcpy(str, label.c_str());
        items = new TMenuItem(str, cmBase + (ushort)idx,
                              kbNoKey, hcNoContext, nullptr, items);
    }
    return items;
}
