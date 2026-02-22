/*---------------------------------------------------------*/
/*  text_wrap.h - Shared word-wrapping utility            */
/*---------------------------------------------------------*/

#ifndef TEXT_WRAP_H
#define TEXT_WRAP_H

#include <string>
#include <vector>

// Word-wrap a single string to the given column width.
// Breaks at word boundaries when possible, hard-breaks otherwise.
// Handles embedded newlines as paragraph separators.
std::vector<std::string> wrapText(const std::string& text, int width);

#endif // TEXT_WRAP_H
