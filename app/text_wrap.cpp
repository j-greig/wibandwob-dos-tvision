/*---------------------------------------------------------*/
/*  text_wrap.cpp - Shared word-wrapping utility          */
/*---------------------------------------------------------*/

#include "text_wrap.h"

std::vector<std::string> wrapText(const std::string& text, int width)
{
    std::vector<std::string> lines;
    if (width <= 0) {
        lines.emplace_back("");
        return lines;
    }

    // Split on newlines first, then wrap each segment
    size_t lineStart = 0;
    const size_t length = text.size();

    while (lineStart <= length) {
        size_t newlinePos = text.find('\n', lineStart);
        std::string segment;
        if (newlinePos == std::string::npos) {
            segment = text.substr(lineStart);
        } else {
            segment = text.substr(lineStart, newlinePos - lineStart);
        }

        // Strip trailing \r
        if (!segment.empty() && segment.back() == '\r')
            segment.pop_back();

        if (segment.empty()) {
            lines.emplace_back("");
        } else {
            size_t pos = 0;
            while (pos < segment.size()) {
                size_t remaining = segment.size() - pos;
                size_t slice = remaining > static_cast<size_t>(width)
                               ? static_cast<size_t>(width) : remaining;
                bool trimmedSpace = false;

                if (remaining > static_cast<size_t>(width)) {
                    size_t breakPos = segment.find_last_of(" \t", pos + width - 1);
                    if (breakPos != std::string::npos && breakPos >= pos) {
                        size_t candidate = breakPos - pos;
                        if (candidate > 0) {
                            slice = candidate;
                            trimmedSpace = true;
                        }
                    }
                }

                lines.push_back(segment.substr(pos, slice));
                pos += slice;

                if (trimmedSpace) {
                    while (pos < segment.size() && segment[pos] == ' ')
                        ++pos;
                }
            }
        }

        if (newlinePos == std::string::npos)
            break;
        lineStart = newlinePos + 1;
    }

    if (lines.empty())
        lines.emplace_back("");

    return lines;
}
