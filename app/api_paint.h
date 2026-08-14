// Declarations for the paint-canvas slice of the api_* bridge functions
// defined in wwdos_app.cpp (monolith split stage 5).
// Do NOT include wwdos_app.h here.
#pragma once

#include <cstdint>
#include <string>

class TWwdosApp;
class TRect;
class TPaintCanvasView;

void api_spawn_paint(TWwdosApp&, const TRect* bounds);
void api_spawn_paint_with_file(TWwdosApp&, const std::string& path);
TPaintCanvasView* api_find_paint_canvas(TWwdosApp&, const std::string& id);

std::string api_paint_cell(TWwdosApp&, const std::string& id, int x, int y, uint8_t fg, uint8_t bg);
std::string api_paint_text(TWwdosApp&, const std::string& id, int x, int y,
                            const std::string& text, uint8_t fg, uint8_t bg);
std::string api_paint_line(TWwdosApp&, const std::string& id, int x0, int y0, int x1, int y1, bool erase);
std::string api_paint_rect(TWwdosApp&, const std::string& id, int x0, int y0, int x1, int y1, bool erase);
std::string api_paint_clear(TWwdosApp&, const std::string& id);
std::string api_paint_export(TWwdosApp&, const std::string& id);
std::string api_paint_save(TWwdosApp&, const std::string& id, const std::string& path);
std::string api_paint_load(TWwdosApp&, const std::string& id, const std::string& path);
std::string api_paint_stamp_figlet(TWwdosApp&, const std::string& id,
                                    const std::string& text, const std::string& font,
                                    int x, int y, uint8_t fg, uint8_t bg);
