#include "platform/Platform.h"

#include <commdlg.h>
#include <shellapi.h>
#include <windowsx.h>
#include <imgui.h>

#include <cmath>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace sx::platform {

namespace {
std::vector<PointerSample> g_samples;
PointerSample g_lastPen = {0, 0, 1, false, false};
float g_pressure = 1.0f;
bool g_penActive = false;
bool g_screenCoords = false;
float g_dropX = 0, g_dropY = 0;  // last drop point, in ImGui coordinates
bool g_quit = false;
HWND g_hwnd = nullptr;
std::function<void(u32, u32)> g_onResize;
std::function<void(const std::string&)> g_onDrop;

double Now() {
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)freq.QuadPart;
}

// Screen pixel -> sample coordinates (screen with multi-viewports, else client).
void ToSampleSpace(HWND hwnd, float& x, float& y) {
    if (g_screenCoords) return;
    POINT o = {0, 0};
    ClientToScreen(hwnd, &o);
    x -= (float)o.x;
    y -= (float)o.y;
}

void PushSample(PointerSample s) {
    g_samples.push_back(s);
    if (s.pen) {
        g_lastPen = s;
        g_pressure = s.pressure;
        g_penActive = true;
    }
}

// ------------------------------------------------------------------ Windows Ink

void PushPen(HWND hwnd, UINT32 pointerId) {
    UINT32 count = 0;
    POINTER_PEN_INFO info[64];
    count = 64;
    if (!GetPointerPenInfoHistory(pointerId, &count, info)) {
        count = 1;
        if (!GetPointerPenInfo(pointerId, &info[0])) return;
    }
    // History is newest-first.
    double now = Now();
    for (int i = (int)count - 1; i >= 0; --i) {
        const POINTER_PEN_INFO& pi = info[i];
        float x = (float)pi.pointerInfo.ptPixelLocation.x, y = (float)pi.pointerInfo.ptPixelLocation.y;
        ToSampleSpace(hwnd, x, y);
        PointerSample s{x, y, (pi.penMask & PEN_MASK_PRESSURE) ? pi.pressure / 1024.0f : 1.0f, true,
                        (pi.penFlags & PEN_FLAG_ERASER) != 0 || (pi.penFlags & PEN_FLAG_INVERTED) != 0};
        s.tiltX = (pi.penMask & PEN_MASK_TILT_X) ? (float)pi.tiltX : 0.0f;
        s.tiltY = (pi.penMask & PEN_MASK_TILT_Y) ? (float)pi.tiltY : 0.0f;
        s.twist = (pi.penMask & PEN_MASK_ROTATION) ? (float)pi.rotation : 0.0f;
        s.time = now - 0.001 * i;  // history spans the last milliseconds
        PushSample(s);
    }
}

// ------------------------------------------------------------------ Wintab (wintab32.dll, loaded on demand)

// Minimal Wintab declarations (wintab.h / pktdef.h are not shipped with Windows).
constexpr UINT WTI_DEFSYSCTX = 4, WTI_DEVICES = 100, DVC_NPRESSURE = 15, DVC_ORIENTATION = 17;
constexpr UINT CXO_SYSTEM = 0x0001, CXO_MESSAGES = 0x0004;
constexpr UINT WT_PACKET = 0x7FF0, WT_PROXIMITY = 0x7FF5;
constexpr DWORD PK_STATUS = 0x0002, PK_CURSOR = 0x0020, PK_BUTTONS = 0x0040, PK_X = 0x0080, PK_Y = 0x0100, PK_NORMAL_PRESSURE = 0x0400,
                PK_ORIENTATION = 0x1000;
constexpr DWORD kPacketData = PK_STATUS | PK_CURSOR | PK_BUTTONS | PK_X | PK_Y | PK_NORMAL_PRESSURE | PK_ORIENTATION;
constexpr int kSubPixel = 16;  // output resolution: 1/16 screen pixel

struct WTAxis { LONG axMin, axMax; UINT axUnits; DWORD axResolution; };
struct WTOrientation { int orAzimuth, orAltitude, orTwist; };
struct WTLogContextW {
    WCHAR lcName[40];
    UINT lcOptions, lcStatus, lcLocks, lcMsgBase, lcDevice, lcPktRate;
    DWORD lcPktData, lcPktMode, lcMoveMask, lcBtnDnMask, lcBtnUpMask;
    LONG lcInOrgX, lcInOrgY, lcInOrgZ, lcInExtX, lcInExtY, lcInExtZ;
    LONG lcOutOrgX, lcOutOrgY, lcOutOrgZ, lcOutExtX, lcOutExtY, lcOutExtZ;
    DWORD lcSensX, lcSensY, lcSensZ;
    BOOL lcSysMode;
    int lcSysOrgX, lcSysOrgY, lcSysExtX, lcSysExtY;
    DWORD lcSysSensX, lcSysSensY;
};
// Fields in PK_* bit order.
struct WTPacket {
    UINT pkStatus;
    UINT pkCursor;
    DWORD pkButtons;
    LONG pkX, pkY;
    UINT pkNormalPressure;
    WTOrientation pkOrientation;
};

struct Wintab {
    HMODULE dll = nullptr;
    UINT(WINAPI* info)(UINT, UINT, LPVOID) = nullptr;
    HANDLE(WINAPI* open)(HWND, WTLogContextW*, BOOL) = nullptr;
    BOOL(WINAPI* close)(HANDLE) = nullptr;
    BOOL(WINAPI* packet)(HANDLE, UINT, LPVOID) = nullptr;
    BOOL(WINAPI* queueSizeSet)(HANDLE, int) = nullptr;
    BOOL(WINAPI* enable)(HANDLE, BOOL) = nullptr;
    HANDLE ctx = nullptr;
    LONG pressureMax = 1023;
    int sysOrgX = 0, sysOrgY = 0;
    bool hasOrientation = false;
    bool inProximity = false;
    double lastPacket = 0;
    std::string device;

    bool Load() {
        if (dll) return info != nullptr;
        dll = LoadLibraryW(L"wintab32.dll");
        if (!dll) return false;
        info = (decltype(info))GetProcAddress(dll, "WTInfoW");
        open = (decltype(open))GetProcAddress(dll, "WTOpenW");
        close = (decltype(close))GetProcAddress(dll, "WTClose");
        packet = (decltype(packet))GetProcAddress(dll, "WTPacket");
        queueSizeSet = (decltype(queueSizeSet))GetProcAddress(dll, "WTQueueSizeSet");
        enable = (decltype(enable))GetProcAddress(dll, "WTEnable");
        if (!info || !open || !close || !packet) info = nullptr;
        return info != nullptr && info(0, 0, nullptr) != 0;
    }

    bool Open(HWND hwnd) {
        if (ctx) return true;
        if (!Load()) return false;
        WTLogContextW lc = {};
        if (!info(WTI_DEFSYSCTX, 0, &lc)) return false;
        wcsncpy_s(lc.lcName, L"Shaderixel", _TRUNCATE);
        lc.lcOptions |= CXO_SYSTEM | CXO_MESSAGES;
        lc.lcPktData = kPacketData;
        lc.lcPktMode = 0;  // absolute
        lc.lcMoveMask = kPacketData;
        lc.lcBtnUpMask = lc.lcBtnDnMask;
        // Output in 1/16 screen pixels over the whole virtual desktop, y down.
        sysOrgX = lc.lcSysOrgX;
        sysOrgY = lc.lcSysOrgY;
        lc.lcOutOrgX = 0;
        lc.lcOutOrgY = 0;
        lc.lcOutExtX = lc.lcSysExtX * kSubPixel;
        lc.lcOutExtY = -lc.lcSysExtY * kSubPixel;
        WTAxis np = {};
        if (info(WTI_DEVICES, DVC_NPRESSURE, &np) && np.axMax > 0) pressureMax = np.axMax;
        WTAxis orient[3] = {};
        hasOrientation = info(WTI_DEVICES, DVC_ORIENTATION, orient) && orient[1].axMax > 0;
        wchar_t name[128] = {};
        if (info(WTI_DEVICES, 1 /*DVC_NAME*/, name)) device = Narrow(name);
        ctx = open(hwnd, &lc, TRUE);
        if (!ctx) return false;
        if (queueSizeSet) queueSizeSet(ctx, 128);
        return true;
    }

    void Close() {
        if (ctx && close) close(ctx);
        ctx = nullptr;
        inProximity = false;
    }

    void OnPacket(HWND hwnd, WPARAM serial, LPARAM context) {
        WTPacket pk = {};
        if (!ctx || !packet((HANDLE)context, (UINT)serial, &pk)) return;
        lastPacket = Now();
        float x = sysOrgX + pk.pkX / (float)kSubPixel, y = sysOrgY + pk.pkY / (float)kSubPixel;
        ToSampleSpace(hwnd, x, y);
        PointerSample s{x, y, std::clamp(pk.pkNormalPressure / (float)pressureMax, 0.0f, 1.0f), true, (pk.pkCursor % 3) == 2};
        if (hasOrientation) {
            // Azimuth (0 = north, clockwise, 1/10 deg) + altitude (90 deg = upright) -> tilt x / y.
            float az = pk.pkOrientation.orAzimuth * 0.1f * 3.14159265f / 180.0f;
            float lean = 90.0f - std::abs(pk.pkOrientation.orAltitude * 0.1f);
            s.tiltX = std::sin(az) * lean;
            s.tiltY = -std::cos(az) * lean;
            s.twist = std::fmod(pk.pkOrientation.orTwist * 0.1f + 360.0f, 360.0f);
        }
        s.time = lastPacket;
        PushSample(s);
    }
};
Wintab g_wintab;
TabletApi g_api = TabletApi::WindowsInk;

bool WintabRecent() { return g_api == TabletApi::Wintab && g_wintab.ctx && (g_wintab.inProximity || Now() - g_wintab.lastPacket < 0.15); }

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;
    switch (msg) {
        case WM_POINTERUPDATE:
        case WM_POINTERDOWN:
        case WM_POINTERUP: {
            UINT32 id = GET_POINTERID_WPARAM(wParam);
            POINTER_INPUT_TYPE type = PT_POINTER;
            if (GetPointerType(id, &type) && type == PT_PEN && g_api == TabletApi::WindowsInk) PushPen(hwnd, id);
            break;  // fall through to DefWindowProc so mouse messages are synthesized
        }
        case WT_PACKET:
            if (g_api == TabletApi::Wintab) g_wintab.OnPacket(hwnd, wParam, lParam);
            return 0;
        case WT_PROXIMITY:
            g_wintab.inProximity = LOWORD(lParam) != 0;
            return 0;
        case WM_MOUSEMOVE: {
            // Mouse messages synthesized from the pen are already covered by pen samples.
            if ((GetMessageExtraInfo() & 0xFFFFFF00) == 0xFF515700) break;
            if (WintabRecent()) break;  // the Wintab pen moves the cursor too
            g_penActive = false;
            g_pressure = 1.0f;
            PointerSample s{(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), 1.0f, false, false};
            if (g_screenCoords) {
                POINT o = {0, 0};
                ClientToScreen(hwnd, &o);
                s.x += (float)o.x;
                s.y += (float)o.y;
            }
            s.time = Now();
            g_samples.push_back(s);
            break;
        }
        case WM_ACTIVATE:
            if (g_wintab.ctx && g_wintab.enable) g_wintab.enable(g_wintab.ctx, LOWORD(wParam) != WA_INACTIVE);
            break;
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED && g_onResize) g_onResize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_DROPFILES: {
            HDROP drop = (HDROP)wParam;
            POINT pt = {};
            DragQueryPoint(drop, &pt);  // client coordinates of this window
            if (g_screenCoords) ClientToScreen(hwnd, &pt);
            g_dropX = (float)pt.x;
            g_dropY = (float)pt.y;
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
            g_wintab.Close();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
}  // namespace

HWND CreateMainWindow(const wchar_t* title, int w, int h) {
    WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WndProc, 0, 0, GetModuleHandleW(nullptr), nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"ShaderixelWindow", nullptr};
    wc.hIcon = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    wc.hIconSm = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, w, h, nullptr, nullptr, wc.hInstance, nullptr);
    DragAcceptFiles(hwnd, TRUE);
    g_hwnd = hwnd;
    return hwnd;
}

std::vector<PointerSample> TakeSamples() {
    std::vector<PointerSample> out;
    out.swap(g_samples);
    return out;
}

float LastPressure() { return g_pressure; }
bool PenActive() { return g_penActive; }
const PointerSample& LastPenSample() { return g_lastPen; }
void SetScreenCoordinates(bool screen) { g_screenCoords = screen; }
void LastDropPos(float& x, float& y) { x = g_dropX; y = g_dropY; }

bool WintabAvailable() { return g_wintab.Load(); }

bool SetTabletApi(TabletApi api) {
    if (api == TabletApi::Wintab) {
        if (!g_wintab.Open(g_hwnd)) {
            g_api = TabletApi::WindowsInk;
            return false;
        }
    } else {
        g_wintab.Close();
    }
    g_api = api;
    return true;
}

TabletApi GetTabletApi() { return g_api; }

std::string TabletStatus() {
    std::string s = g_api == TabletApi::Wintab ? "Wintab" : "Windows Ink";
    if (g_api == TabletApi::Wintab && !g_wintab.device.empty()) s += " (" + g_wintab.device + ")";
    if (g_penActive) {
        char b[96];
        snprintf(b, sizeof(b), " : pression %.2f, inclinaison %.0f/%.0f, rotation %.0f", g_lastPen.pressure, g_lastPen.tiltX, g_lastPen.tiltY, g_lastPen.twist);
        s += b;
    }
    return s;
}

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
