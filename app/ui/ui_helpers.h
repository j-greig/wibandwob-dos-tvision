#ifndef APP_UI_UI_HELPERS_H
#define APP_UI_UI_HELPERS_H

#define Uses_TStringCollection
#include <tvision/tv.h>

#include <string>
#include <vector>

inline TStringCollection* makeStringCollection(const std::vector<std::string>& items)
{
    TStringCollection* col = new TStringCollection((short) items.size(), 1);
    for (const auto& item : items)
        col->insert(newStr(item.c_str()));
    return col;
}

#endif
