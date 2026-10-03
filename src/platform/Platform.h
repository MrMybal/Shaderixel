#pragma once
// Win32 window, high-rate pointer input (pen pressure, tilt, rotation,
// sub-frame samples) and native file dialogs.
//
// Tablets: Windows Ink (WM_POINTER) or Wintab (wintab32.dll, Wacom-style
// drivers with Windows Ink disabled), chosen with SetTabletApi.

#include "core/Common.h"

#include <windows.h>
#include <functional>

namespace sx {

struct PointerSample {
    float x, y;            // ImGui coordinates (screen pixels with multi-viewports, else client pixels)
    float pressure;        // 0..1 (1 for mouse)
    bool pen;
    bool eraser;           // pen eraser end
    float tiltX = 0, tiltY = 0;  // degrees, -90..90 (0 = upright)
    float twist = 0;       // barrel rotation, degrees 0..360
    double time = 0;       // seconds (QueryPerformanceCounter)
};

namespace platform {

enum class TabletApi : int { WindowsInk = 0, Wintab = 1 };

HWND CreateMainWindow(const wchar_t* title, int w, int h);
// Samples gathered since the last call (mouse moves + pen history), in order.
std::vector<PointerSample> TakeSamples();
float LastPressure();
bool PenActive();
const PointerSample& LastPenSample();  // latest pen state (tilt, rotation)

// Samples in screen coordinates (ImGui multi-viewports) instead of client coordinates.
void SetScreenCoordinates(bool screen);
// Windows Ink (default) or Wintab; returns false when Wintab is not available (falls back to Ink).
bool SetTabletApi(TabletApi api);
TabletApi GetTabletApi();
bool WintabAvailable();
std::string TabletStatus();  // short description for the UI

void SetResizeCallback(std::function<void(u32, u32)> cb);
void SetDropCallback(std::function<void(const std::string&)> cb);
bool IsQuitRequested();
void ClearQuitRequest();

std::string OpenFileDialog(HWND owner, const wchar_t* filter);
std::string SaveFileDialog(HWND owner, const wchar_t* filter, const wchar_t* defExt);
void SetTitle(HWND hwnd, const std::string& title);

}  // namespace platform
}  // namespace sx
