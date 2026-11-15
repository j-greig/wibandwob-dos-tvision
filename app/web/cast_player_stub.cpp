// Browser stand-in for the cast player (app/cast_player.cpp needs tvterm's emulator, not built here).
#include "cast_player.h"

TCastPlayerWindow::TCastPlayerWindow(const TRect& r, const std::string& file, TCastView* view)
    : TWindowInit(&TWindow::initFrame), TWindow(r, "Cast", wnNoNumber), view_(view), file_(file) {}

std::string TCastPlayerWindow::describeState() const { return "cast player stand-in: casts do not play in the browser"; }
bool TCastPlayerWindow::describeText(std::vector<std::string>& out) const { out.clear(); return false; }
std::string TCastPlayerWindow::control(const std::map<std::string, std::string>&) { return "err casts do not play in the browser"; }
std::string TCastPlayerWindow::stateJson() const { return "{\"ok\":false}"; }

TCastPlayerWindow* createCastPlayer(const std::string&, int, int, std::string& err) {
    err = "casts do not play in the browser";
    return nullptr;
}
