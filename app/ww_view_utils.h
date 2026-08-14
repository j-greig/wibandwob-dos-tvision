// Shared view helpers for the api_* modules (monolith split stage 2).
#pragma once

#define Uses_TView
#define Uses_TWindow
#include <tvision/tv.h>

#include <string>

// Find the first child view of a given type inside a window (z-order walk).
template <typename ViewType>
static ViewType* ww_get_child_view(TWindow* w) {
    if (!w) return nullptr;
    TView* start = w->first();
    if (!start) return nullptr;
    TView* v = start;
    do {
        if (auto* t = dynamic_cast<ViewType*>(v)) return t;
        v = v->next;
    } while (v != start);
    return nullptr;
}

// Primer directory resolution. DEFINITION stays in wwdos_app.cpp — two test
// TUs stub it; see docs/development/monolith-split-plan.md gotcha 3.
std::string findPrimerDir();
