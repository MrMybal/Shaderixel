#pragma once
// Win32 window, high-rate pointer input (pen pressure, sub-frame samples) and
// native file dialogs.

#include "core/Common.h"

#include <windows.h>
#include <functional>

namespace sx {

struct PointerSample {
    float x, y;         // client pixels
    float pressure;     // 0..1 (1 for mouse)
    bool pen;
    bool eraser;        // pen eraser end
};

namespace platform {

HWND CreateMainWindow(const wchar_t* title, int w, int h);
// Samples gathered since the last call (mouse moves + pen history), in order.
std::vector<PointerSample> TakeSamples();
float LastPressure();
bool PenActive();

void SetResizeCallback(std::function<void(u32, u32)> cb);
void SetDropCallback(std::function<void(const std::string&)> cb);
bool IsQuitRequested();

std::string OpenFileDialog(HWND owner, const wchar_t* filter);
std::string SaveFileDialog(HWND owner, const wchar_t* filter, const wchar_t* defExt);
void SetTitle(HWND hwnd, const std::string& title);

}  // namespace platform
}  // namespace sx
