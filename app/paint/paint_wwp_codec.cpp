#include "paint_wwp_codec.h"

#include "paint_canvas.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

std::string buildWwpJson(TPaintCanvasView* cv)
{
    int cols = cv->getCols(), rows = cv->getRows();
    std::ostringstream os;
    os << "{\n  \"version\": 1,\n";
    os << "  \"cols\": " << cols << ",\n";
    os << "  \"rows\": " << rows << ",\n";
    os << "  \"cells\": [\n";
    bool first = true;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const auto& c = cv->cellAt(x, y);
            if (!c.uOn && !c.lOn && c.qMask == 0 && c.textChar == 0)
                continue;
            if (!first) os << ",\n";
            first = false;
            os << "    { \"x\": " << x << ", \"y\": " << y;
            if (c.uOn)  os << ", \"uOn\": true, \"uFg\": " << (int)c.uFg;
            if (c.lOn)  os << ", \"lOn\": true, \"lFg\": " << (int)c.lFg;
            if (c.qMask) os << ", \"qMask\": " << (int)c.qMask << ", \"qFg\": " << (int)c.qFg;
            if (c.textChar) os << ", \"textChar\": " << (int)(unsigned char)c.textChar
                              << ", \"textFg\": " << (int)c.textFg
                              << ", \"textBg\": " << (int)c.textBg;
            os << " }";
        }
    }
    os << "\n  ]\n}\n";
    return os.str();
}

bool saveWwpFile(const std::string& path, const std::string& json)
{
    std::string tmp = path + ".tmp";
    std::ofstream out(tmp, std::ios::out | std::ios::trunc);
    if (!out) return false;
    out << json;
    out.close();
    if (!out.good()) {
        std::remove(tmp.c_str());
        return false;
    }
    std::remove(path.c_str());
    std::rename(tmp.c_str(), path.c_str());
    return true;
}

int parseIntAfter(const std::string& s, size_t pos, const char* key)
{
    std::string needle = std::string("\"") + key + "\":";
    size_t k = s.find(needle, pos);
    if (k == std::string::npos) {
        needle = std::string("\"") + key + "\" :";
        k = s.find(needle, pos);
    }
    if (k == std::string::npos) return -1;
    size_t vStart = s.find_first_of("-0123456789", k + needle.size());
    if (vStart == std::string::npos) return -1;
    return std::atoi(s.c_str() + vStart);
}

bool parseBoolAfter(const std::string& s, size_t pos, const char* key)
{
    std::string needle = std::string("\"") + key + "\":";
    size_t k = s.find(needle, pos);
    if (k == std::string::npos) {
        needle = std::string("\"") + key + "\" :";
        k = s.find(needle, pos);
    }
    if (k == std::string::npos) return false;
    size_t vStart = k + needle.size();
    while (vStart < s.size() && s[vStart] == ' ') vStart++;
    return (vStart < s.size() && s[vStart] == 't');
}

bool loadWwpFromString(const std::string& data, TPaintCanvasView* cv)
{
    if (!cv) return false;

    int fileCols = parseIntAfter(data, 0, "cols");
    int fileRows = parseIntAfter(data, 0, "rows");
    if (fileCols <= 0 || fileRows <= 0) return false;

    cv->clear();

    size_t cellsArr = data.find("\"cells\"");
    if (cellsArr == std::string::npos) return false;
    size_t pos = data.find('[', cellsArr);
    size_t arrEnd = data.find(']', pos);
    if (pos == std::string::npos || arrEnd == std::string::npos) return false;

    while (pos < arrEnd) {
        size_t objStart = data.find('{', pos);
        if (objStart == std::string::npos || objStart >= arrEnd) break;
        size_t objEnd = data.find('}', objStart);
        if (objEnd == std::string::npos) break;

        int cx = parseIntAfter(data, objStart, "x");
        int cy = parseIntAfter(data, objStart, "y");
        if (cx >= 0 && cy >= 0 && cx < cv->getCols() && cy < cv->getRows()) {
            PaintCell& cell = cv->cellAt(cx, cy);
            cell.uOn = parseBoolAfter(data, objStart, "uOn");
            cell.lOn = parseBoolAfter(data, objStart, "lOn");
            int v;
            v = parseIntAfter(data, objStart, "uFg");     if (v >= 0) cell.uFg = (uint8_t)v;
            v = parseIntAfter(data, objStart, "lFg");     if (v >= 0) cell.lFg = (uint8_t)v;
            v = parseIntAfter(data, objStart, "qMask");   if (v >= 0) cell.qMask = (uint8_t)v;
            v = parseIntAfter(data, objStart, "qFg");     if (v >= 0) cell.qFg = (uint8_t)v;
            v = parseIntAfter(data, objStart, "textChar"); if (v >= 0) cell.textChar = (char)v;
            v = parseIntAfter(data, objStart, "textFg");  if (v >= 0) cell.textFg = (uint8_t)v;
            v = parseIntAfter(data, objStart, "textBg");  if (v >= 0) cell.textBg = (uint8_t)v;
        }
        pos = objEnd + 1;
    }

    return true;
}
