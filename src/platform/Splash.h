#pragma once
// Startup splash: the logo floating over the desktop (a borderless layered window with per-pixel transparency),
// with the version, the current loading step and a progress bar under it. Shown while the application loads
// (shader compiler, libraries, plugins, first frames), before the main window appears. Drawn with GDI only, so it
// shows before the GPU is ready.

#include <string>

namespace sx::splash {

void Show(const char* version);
// progress in 0..1; text: what is loading now (already translated).
void Step(float progress, const std::string& text);
void Close();
bool Active();
// The splash as it would be drawn now, written as a PNG (tests: SX_SPLASH_PNG).
bool SavePng(const std::string& path, float progress, const std::string& text, const char* version);

}  // namespace sx::splash
