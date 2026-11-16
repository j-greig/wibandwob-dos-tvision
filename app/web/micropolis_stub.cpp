// Browser build: the Micropolis engine (vendor/MicropolisCore, 121 MB with assets)
// is not compiled in. This bridge keeps the window code linking; the city stays empty.
#include "micropolis/micropolis_bridge.h"

static void noDelete(Micropolis *) {}

MicropolisBridge::MicropolisBridge() : sim_(nullptr, &noDelete) {}
MicropolisBridge::~MicropolisBridge() = default;
bool MicropolisBridge::initialize_new_city(int, short) { return false; }
void MicropolisBridge::tick(int) {}
std::uint16_t MicropolisBridge::cell_at(int, int) const { return 0; }
std::uint16_t MicropolisBridge::tile_at(int, int) const { return 0; }
char MicropolisBridge::glyph_for_tile(std::uint16_t) const { return ' '; }
std::string MicropolisBridge::glyph_pair_for_tile(std::uint16_t) const { return "  "; }
MicropolisSnapshot MicropolisBridge::snapshot() const { return {}; }
ToolApplyResult MicropolisBridge::apply_tool(int, int, int) { return {0, "Micropolis is not in the browser build"}; }
CityIOResult MicropolisBridge::save_city(const std::string &) { return {false, "not in the browser build"}; }
CityIOResult MicropolisBridge::load_city(const std::string &) { return {false, "not in the browser build"}; }
std::string MicropolisBridge::render_ascii_excerpt(int, int, int, int) const { return {}; }
std::uint64_t MicropolisBridge::hash_map_bytes() const { return 0; }
