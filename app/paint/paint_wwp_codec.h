#ifndef APP_PAINT_PAINT_WWP_CODEC_H
#define APP_PAINT_PAINT_WWP_CODEC_H

#include <cstddef>
#include <string>

class TPaintCanvasView;

std::string buildWwpJson(TPaintCanvasView* cv);
bool saveWwpFile(const std::string& path, const std::string& json);
bool loadWwpFromString(const std::string& data, TPaintCanvasView* cv);
int parseIntAfter(const std::string& s, size_t pos, const char* key);
bool parseBoolAfter(const std::string& s, size_t pos, const char* key);

#endif
