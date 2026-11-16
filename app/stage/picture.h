// Colour pictures for the stage: PAINT files and picture films (formats in docs/stage.md, "Colour pictures").
// One parsed copy per file (get), shared by desktop marks (minimal_app.cpp) and picture windows (stage.cpp).
#ifndef WWDOS_PICTURE_H
#define WWDOS_PICTURE_H

#define Uses_TColorAttr
#define Uses_TDrawBuffer
#include <tvision/tv.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pic {

enum : uint8_t { NONE = 255 };   // '.' in a file: no ink (cell left alone) or no paper (glyph over what is below)

struct Cell { uint32_t ch = 0x20; uint8_t ink = NONE, paper = NONE; };   // ch: the glyph's UTF-8 bytes, first byte lowest

class Picture
{
public:
    int w = 0, h = 0;
    double fps = 0;                  // a picture film's own frames a second; 0 for a PAINT file
    bool film = false;
    std::vector<TColorRGB> pal;
    int frames() const { return film ? (int) refs.size() : (int) paintFrames.size(); }
    const std::vector<Cell>& frame(int k);   // k is clamped to the frames there are
    bool parse(const std::string& text);     // false when the text is neither format
private:
    std::vector<std::vector<Cell>> paintFrames;
    std::string text;
    struct Ref { size_t off; char kind; int key; };   // kind K, D or =; key: the key frame this one builds on
    std::vector<Ref> refs;
    std::vector<Cell> cur;
    int curK = -1;
    void apply(int k);
};

// First line "PAINT " or "PFILM ": a colour picture.
bool isPicture(const std::string& path);
// The parsed picture, or nullptr (no such file, or not a picture). Cached by path, file size and time.
std::shared_ptr<Picture> get(const std::string& path);

// A transformed palette: fade toward a colour by a (0-255) and multiply by a tint, for one draw.
struct Fx { TColorRGB fadeTo {0, 0, 0}; int fade = 0; TColorRGB tint {255, 255, 255}; bool any() const { return fade > 0 || tint.getRed() != 255 || tint.getGreen() != 255 || tint.getBlue() != 255; } };
TColorRGB apply(const Fx& fx, TColorRGB c);

// Row y of a frame drawn into out[0..outW) with its left edge at column x0 (cells outside are clipped). Transparent
// cells keep what out holds; a cell with no paper keeps out's paper. pal has one entry per palette index.
void drawRow(const Picture& p, const std::vector<Cell>& fr, int y, TScreenCell* out, int outW, int x0, const TColorRGB* pal);

// Direct cell access to a TDrawBuffer, for composing a row in place.
struct Row : TDrawBuffer { TScreenCell* cells() { return data; } };

}

#endif
