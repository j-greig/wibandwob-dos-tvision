// The describe contract: lookup and the generic dialog describer. See describe.h.
#define Uses_TWindow
#define Uses_TDialog
#define Uses_TGroup
#define Uses_TButton
#define Uses_TInputLine
#define Uses_TStaticText
#define Uses_TCluster
#define Uses_TListViewer
#include <tvision/tv.h>

#include "describe.h"

#include <cstdlib>
#include <cxxabi.h>
#include <typeinfo>

namespace ww {

namespace {

Describable* findIn(TGroup* g, int depth) {
    if (!g) return nullptr;
    if (auto* d = dynamic_cast<Describable*>(static_cast<TView*>(g))) return d;
    TView* f = g->first();
    if (!f || depth <= 0) return nullptr;
    TView* v = f;
    do {
        if (auto* d = dynamic_cast<Describable*>(v)) return d;
        v = v->next;
    } while (v != f);
    v = f;
    do {
        if (auto* sub = dynamic_cast<TGroup*>(v))
            if (auto* d = findIn(sub, depth - 1)) return d;
        v = v->next;
    } while (v != f);
    return nullptr;
}

}  // namespace

Describable* describableOf(TWindow* w) { return findIn(w, 2); }

std::string dialogState(TWindow* w, std::vector<std::string>& text) {
    int buttons = 0, inputs = 0, clusters = 0, lists = 0;
    TView* f = w->first();
    if (f) {
        TView* v = f;
        do {
            if (dynamic_cast<TButton*>(v)) ++buttons;
            else if (auto* in = dynamic_cast<TInputLine*>(v)) {
                ++inputs;
                if (in->data) text.push_back(std::string("input: ") + in->data);
            } else if (dynamic_cast<TCluster*>(v)) ++clusters;
            else if (dynamic_cast<TListViewer*>(v)) ++lists;
            else if (auto* st = dynamic_cast<TStaticText*>(v)) {
                char buf[1024] = {0};
                st->getText(buf);
                for (auto& l : splitText(buf)) text.push_back(l);
            }
            v = v->next;
        } while (v != f);
    }
    const char* t = w->getTitle(0);
    return std::string("dialog \"") + (t ? t : "") + "\" with " + std::to_string(buttons) + " buttons, " +
           std::to_string(inputs) + " input lines, " + std::to_string(clusters) + " option groups and " +
           std::to_string(lists) + " lists; resident_front lists its controls and how to answer it";
}

namespace {
std::vector<ExternalDescriber>& externals() {
    static std::vector<ExternalDescriber> v;
    return v;
}
}  // namespace

void addExternalDescriber(const ExternalDescriber& d) { externals().push_back(d); }

bool describeWindow(TWindow* w, std::string& state, std::vector<std::string>& text, bool& modelText, std::string& cls) {
    text.clear();
    modelText = false;
    if (auto* d = describableOf(w)) {
        state = d->describeState();
        int st = 0;
        char* dm = abi::__cxa_demangle(typeid(*d).name(), nullptr, nullptr, &st);
        cls = (st == 0 && dm) ? dm : typeid(*d).name();
        std::free(dm);
        modelText = d->describeText(text);
        if (!modelText) text.clear();
        return true;
    }
    for (const auto& e : externals()) {
        if (!e.matches(w)) continue;
        state = e.state(w);
        cls = e.className;
        modelText = e.text && e.text(w, text);
        if (!modelText) text.clear();
        return true;
    }
    return false;
}

}  // namespace ww
