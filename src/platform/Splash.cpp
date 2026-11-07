#include "platform/Splash.h"
#include "core/Common.h"

#include <windows.h>
#pragma warning(push, 0)
#include <stb_image.h>
#include <stb_image_write.h>
#pragma warning(pop)
#include <algorithm>
#include <cmath>
#include <vector>

namespace sx::splash {
namespace {

HWND gWnd = nullptr;
std::string gVersion;
float gScale = 1;
int gW = 0, gH = 0;
std::vector<unsigned char> gLogo;  // RGBA straight
int gLogoW = 0, gLogoH = 0;
bool gLogoTried = false;

struct Canvas {
    int w, h;
    std::vector<float> px;  // premultiplied RGBA, 0..1
    Canvas(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_ * 4, 0.0f) {}
    // Source-over a straight color with coverage a.
    void Blend(int x, int y, float r, float g, float b, float a) {
        if (x < 0 || y < 0 || x >= w || y >= h || a <= 0) return;
        float* p = &px[((size_t)y * w + x) * 4];
        const float k = 1 - a;
        p[0] = r * a + p[0] * k;
        p[1] = g * a + p[1] * k;
        p[2] = b * a + p[2] * k;
        p[3] = a + p[3] * k;
    }
};

void LoadLogo() {
    gLogoTried = true;
    HMODULE mod = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(mod, MAKEINTRESOURCEW(2), MAKEINTRESOURCEW(10));  // RT_RCDATA (assets/logo.png)
    HGLOBAL h = res ? LoadResource(mod, res) : nullptr;
    const void* data = h ? LockResource(h) : nullptr;
    if (!data) return;
    int comp = 0;
    stbi_uc* p = stbi_load_from_memory((const stbi_uc*)data, (int)SizeofResource(mod, res), &gLogoW, &gLogoH, &comp, 4);
    if (!p) return;
    gLogo.assign(p, p + (size_t)gLogoW * gLogoH * 4);
    stbi_image_free(p);
}

// Rounded rectangle filled with a straight color (anti-aliased edges).
void RoundRect(Canvas& c, float x0, float y0, float x1, float y1, float rad, float r, float g, float b, float a) {
    for (int y = (int)y0; y < (int)std::ceil(y1); ++y)
        for (int x = (int)x0; x < (int)std::ceil(x1); ++x) {
            const float px = x + 0.5f, py = y + 0.5f;
            const float qx = std::max(std::max(x0 + rad - px, px - (x1 - rad)), 0.0f);
            const float qy = std::max(std::max(y0 + rad - py, py - (y1 - rad)), 0.0f);
            const float d = std::sqrt(qx * qx + qy * qy) - rad;
            const float cov = std::clamp(0.5f - d, 0.0f, 1.0f) * std::clamp(std::min({px - x0, x1 - px, py - y0, y1 - py}) + 0.5f, 0.0f, 1.0f);
            c.Blend(x, y, r, g, b, a * cov);
        }
}

// Text drawn by GDI into a grayscale mask, then blended in a straight color. align: 0 left, 1 center, 2 right.
void Text(Canvas& c, int x, int y, int w, int h, const std::string& text, int height, int weight, float r, float g, float b, float a,
          int align) {
    if (w <= 0 || h <= 0 || text.empty()) return;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) { DeleteDC(dc); return; }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    memset(bits, 0, (size_t)w * h * 4);
    HFONT font = CreateFontW(-height, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    RECT rc{0, 0, w, h};
    std::wstring wt = Widen(text);
    DrawTextW(dc, wt.c_str(), (int)wt.size(), &rc,
              DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | (align == 1 ? DT_CENTER : align == 2 ? DT_RIGHT : DT_LEFT));
    GdiFlush();
    const unsigned char* m = (const unsigned char*)bits;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            const unsigned char* p = m + ((size_t)j * w + i) * 4;
            const int cov = std::max({p[0], p[1], p[2]});
            if (cov) c.Blend(x + i, y + j, r, g, b, a * cov / 255.0f);
        }
    SelectObject(dc, oldFont);
    DeleteObject(font);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

Canvas Draw(float progress, const std::string& text, const std::string& version) {
    const float s = gScale;
    Canvas c(gW, gH);
    if (!gLogoTried) LoadLogo();
    // The logo, large, its own transparency kept (bilinear).
    const float L = 360 * s;
    const float lx = (gW - L) * 0.5f, ly = 0;
    if (!gLogo.empty())
        for (int y = (int)ly; y < (int)(ly + L); ++y)
            for (int x = (int)lx; x < (int)(lx + L); ++x) {
                const float u = (x + 0.5f - lx) / L * gLogoW - 0.5f, v = (y + 0.5f - ly) / L * gLogoH - 0.5f;
                const int x0 = std::clamp((int)std::floor(u), 0, gLogoW - 1), y0 = std::clamp((int)std::floor(v), 0, gLogoH - 1);
                const int x1 = std::min(x0 + 1, gLogoW - 1), y1 = std::min(y0 + 1, gLogoH - 1);
                const float fx = std::clamp(u - x0, 0.0f, 1.0f), fy = std::clamp(v - y0, 0.0f, 1.0f);
                float acc[4] = {0, 0, 0, 0};
                const int xs[2] = {x0, x1}, ys[2] = {y0, y1};
                for (int k = 0; k < 4; ++k) {
                    const unsigned char* p = &gLogo[((size_t)ys[k >> 1] * gLogoW + xs[k & 1]) * 4];
                    const float wgt = ((k & 1) ? fx : 1 - fx) * ((k >> 1) ? fy : 1 - fy);
                    const float al = p[3] / 255.0f;
                    for (int ch = 0; ch < 3; ++ch) acc[ch] += p[ch] / 255.0f * al * wgt;  // premultiplied filtering
                    acc[3] += al * wgt;
                }
                if (acc[3] > 0) c.Blend(x, y, acc[0] / acc[3], acc[1] / acc[3], acc[2] / acc[3], acc[3]);
            }
    // The information card under the logo.
    const float cx0 = 40 * s, cx1 = gW - 40 * s, cy0 = L + 14 * s, cy1 = gH - 2 * s;
    RoundRect(c, cx0, cy0, cx1, cy1, 12 * s, 0.075f, 0.07f, 0.11f, 0.94f);
    const int pad = (int)(22 * s);
    const int tx = (int)cx0 + pad, tw = (int)(cx1 - cx0) - 2 * pad;
    Text(c, tx, (int)(cy0 + 12 * s), tw, (int)(40 * s), "Shaderixel", (int)(28 * s), FW_SEMIBOLD, 0.96f, 0.95f, 1.0f, 1, 0);
    Text(c, tx, (int)(cy0 + 12 * s), tw, (int)(40 * s), "Version " + version, (int)(14 * s), FW_NORMAL, 0.72f, 0.68f, 0.86f, 1, 2);
    Text(c, tx, (int)(cy0 + 56 * s), tw, (int)(22 * s), text, (int)(14 * s), FW_NORMAL, 0.80f, 0.80f, 0.86f, 1, 0);
    char pct[16];
    snprintf(pct, sizeof(pct), "%d %%", (int)std::lround(std::clamp(progress, 0.0f, 1.0f) * 100));
    Text(c, tx, (int)(cy0 + 56 * s), tw, (int)(22 * s), pct, (int)(14 * s), FW_NORMAL, 0.62f, 0.60f, 0.70f, 1, 2);
    // Progress bar.
    const float by0 = cy0 + 88 * s, by1 = by0 + 6 * s, bx0 = (float)tx, bx1 = (float)(tx + tw);
    RoundRect(c, bx0, by0, bx1, by1, 3 * s, 1, 1, 1, 0.10f);
    const float fill = bx0 + (bx1 - bx0) * std::clamp(progress, 0.0f, 1.0f);
    if (fill > bx0 + 1) RoundRect(c, bx0, by0, std::max(fill, bx0 + 6 * s), by1, 3 * s, 0.56f, 0.40f, 0.98f, 1);
    return c;
}

void Present(float progress, const std::string& text) {
    if (!gWnd) return;
    Canvas c = Draw(progress, text, gVersion);
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = gW;
    bi.bmiHeader.biHeight = -gH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp) {
        unsigned char* o = (unsigned char*)bits;
        for (size_t i = 0; i < (size_t)gW * gH; ++i) {  // premultiplied BGRA
            o[i * 4 + 0] = (unsigned char)std::lround(std::clamp(c.px[i * 4 + 2], 0.0f, 1.0f) * 255);
            o[i * 4 + 1] = (unsigned char)std::lround(std::clamp(c.px[i * 4 + 1], 0.0f, 1.0f) * 255);
            o[i * 4 + 2] = (unsigned char)std::lround(std::clamp(c.px[i * 4 + 0], 0.0f, 1.0f) * 255);
            o[i * 4 + 3] = (unsigned char)std::lround(std::clamp(c.px[i * 4 + 3], 0.0f, 1.0f) * 255);
        }
        HGDIOBJ old = SelectObject(dc, bmp);
        RECT wr;
        GetWindowRect(gWnd, &wr);
        POINT dst{wr.left, wr.top}, src{0, 0};
        SIZE size{gW, gH};
        BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(gWnd, screen, &dst, &size, dc, &src, 0, &bf, ULW_ALPHA);
        SelectObject(dc, old);
        DeleteObject(bmp);
    }
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    // Keep the system from flagging the process as frozen while it loads.
    MSG msg;
    while (PeekMessageW(&msg, gWnd, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
}

void Measure() {
    HDC screen = GetDC(nullptr);
    gScale = std::clamp(GetDeviceCaps(screen, LOGPIXELSX) / 96.0f, 1.0f, 3.0f);
    ReleaseDC(nullptr, screen);
    gW = (int)(520 * gScale);
    gH = (int)(490 * gScale);
}

}  // namespace

void Show(const char* version) {
    if (gWnd) return;
    gVersion = version ? version : "";
    Measure();
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ShaderixelSplash";
    wc.hCursor = LoadCursorW(nullptr, IDC_APPSTARTING);
    RegisterClassW(&wc);
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    const int x = wa.left + (wa.right - wa.left - gW) / 2, y = wa.top + (wa.bottom - wa.top - gH) / 2;
    gWnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, L"Shaderixel", WS_POPUP, x, y, gW, gH,
                           nullptr, nullptr, wc.hInstance, nullptr);
    if (!gWnd) return;
    Present(0, "Starting...");
    ShowWindow(gWnd, SW_SHOWNOACTIVATE);
}

void Step(float progress, const std::string& text) {
    if (gWnd) Present(progress, text);
}

void Close() {
    if (gWnd) DestroyWindow(gWnd);
    gWnd = nullptr;
}

bool Active() { return gWnd != nullptr; }

bool SavePng(const std::string& path, float progress, const std::string& text, const char* version) {
    if (!gW) Measure();
    Canvas c = Draw(progress, text, version ? version : "");
    // Over a mid-gray desktop, as seen on screen.
    std::vector<unsigned char> out((size_t)gW * gH * 4);
    for (size_t i = 0; i < (size_t)gW * gH; ++i) {
        const float a = c.px[i * 4 + 3];
        for (int ch = 0; ch < 3; ++ch) out[i * 4 + ch] = (unsigned char)std::lround(std::clamp(c.px[i * 4 + ch] + 0.45f * (1 - a), 0.0f, 1.0f) * 255);
        out[i * 4 + 3] = 255;
    }
    return stbi_write_png(path.c_str(), gW, gH, 4, out.data(), gW * 4) != 0;
}

}  // namespace sx::splash
