// app_chrome.h - chrome (menu bar / status line) declarations.
// TCustomMenuBar / TCustomStatusLine definitions live in app_chrome.cpp
// (monolith split stage 8). Nothing outside app_chrome.cpp currently
// constructs these by name (TWwdosApp::initMenuBar/initStatusLine return
// the tvision base pointers TMenuBar*/TStatusLine*) — this header exists
// so other TUs can forward-reference the concrete types if that changes.
#pragma once

class TCustomMenuBar;
class TCustomStatusLine;
