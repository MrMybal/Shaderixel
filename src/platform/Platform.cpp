#include "platform/Platform.h"

#include <commdlg.h>
#include <shellapi.h>
#include <windowsx.h>
#include <imgui.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace sx::platform {

namespace {
std::vector<PointerSample> g_samples;
float g_pressure = 1.0f;
bool g_penActive = false;
bool g_quit = false;
std::function<void(u32, u32)> g_onResize;
std::function<void(const std::string&)> g_onDrop;

void PushPen(HWND hwnd, UINT32 pointerId) {
    UINT32 count = 0;
    POINTER_PEN_INFO info[64];
    count = 64;
    if (!GetPointerPenInfoHistory(pointerId, &count, info)) {
        count = 1;
        if (!GetPointerPenInfo(pointerId, &info[0])) return;
    }
    // History is newest-first.
    for (int i = (int)count - 1; i >= 0; --i) {
        POINT p = info[i].pointerInfo.ptPixelLocation;
        ScreenToClient(hwnd, &p);
        // Use himetric location for sub-pixel precision when available.
        float x = (float)p.x, y = (float)p.y;
        float pressure = info[i].pressure / 1024.0f;
        bool eraser = (info[i].penFlags & PEN_FLAG_ERASER) != 0 || (info[i].penFlags & PEN_FLAG_INVERTED) != 0;
        g_samples.push_back({x, y, pressure, true, eraser});
        g_pressure = pressure;
    }
}

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;
    switch (msg) {
        case WM_POINTERUPDATE:
        case WM_POINTERDOWN:
        case WM_POINTERUP: {
            UINT32 id = GET_POINTERID_WPARAM(wParam);
            POINTER_INPUT_TYPE type = PT_POINTER;
            if (GetPointerType(id, &type) && type == PT_PEN) {
                g_penActive = true;
                PushPen(hwnd, id);
            }
            break;  // fall through to DefWindowProc so mouse messages are synthesized
        }
        case WM_MOUSEMOVE: {
            // Mouse messages synthesized from the pen are already covered by pen samples.
            if ((GetMessageExtraInfo() & 0xFFFFFF00) == 0xFF515700) break;
            g_penActive = false;
            g_pressure = 1.0f;
            g_samples.push_back({(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), 1.0f, false, false});
            break;
        }
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED && g_onResize) g_onResize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_DROPFILES: {
            HDROP drop = (HDROP)wParam;
            wchar_t path[MAX_PATH];
            UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; ++i)
                if (DragQueryFileW(drop, i, path, MAX_PATH) && g_onDrop) g_onDrop(Narrow(path));
            DragFinish(drop);
            return 0;
        }
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;  // no ALT application menu
            break;
        case WM_CLOSE:
            g_quit = true;
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
}  // namespace

HWND CreateMainWindow(const wchar_t* title, int w, int h) {
    WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WndProc, 0, 0, GetModuleHandleW(nullptr), nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"ShaderixelWindow", nullptr};
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, w, h, nullptr, nullptr, wc.hInstance, nullptr);
    DragAcceptFiles(hwnd, TRUE);
    return hwnd;
}

std::vector<PointerSample> TakeSamples() {
    std::vector<PointerSample> out;
    out.swap(g_samples);
    return out;
}

float LastPressure() { return g_pressure; }
bool PenActive() { return g_penActive; }
void SetResizeCallback(std::function<void(u32, u32)> cb) { g_onResize = std::move(cb); }
void SetDropCallback(std::function<void(const std::string&)> cb) { g_onDrop = std::move(cb); }
bool IsQuitRequested() { return g_quit; }
void ClearQuitRequest() { g_quit = false; }

std::string OpenFileDialog(HWND owner, const wchar_t* filter) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&ofn) ? Narrow(file) : std::string();
}

std::string SaveFileDialog(HWND owner, const wchar_t* filter, const wchar_t* defExt) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = defExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetSaveFileNameW(&ofn) ? Narrow(file) : std::string();
}

void SetTitle(HWND hwnd, const std::string& title) { SetWindowTextW(hwnd, Widen(title).c_str()); }

}  // namespace sx::platform
