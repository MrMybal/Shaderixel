// "Palette" plugin for Shaderixel (API 3): the dominant colors of the active layer.
//
// Window Plugins > Fenêtres > Palette: "Extraire" reads the layer's pixels (pixels_read), groups them
// with k-means (k-means++ start) and shows the colors by importance. Left click: brush color,
// right click: background color. Settings (number of colors, automatic update) are kept between
// sessions (setting_get / setting_set); "Auto" follows the edits of the document (on_event).
// Operation "palette.extraire" ({count}) for the assistant and actions: the colors as hex.
#include "shaderixel_plugin.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

const SxHost* H = nullptr;

struct Color {
    float r, g, b;
    float share;  // part of the opaque pixels
};

struct State {
    int count = 6;
    int autoUpdate = 0;
    std::vector<Color> colors;
    std::string info = "Cliquez sur Extraire.";
    bool pending = false, dirty = false;
    std::chrono::steady_clock::time_point lastEdit;
    std::string opResult;
} S;

float Dist2(const float* a, const float* b) {
    const float dr = a[0] - b[0], dg = a[1] - b[1], db = a[2] - b[2];
    return dr * dr * 0.3f + dg * dg * 0.59f + db * db * 0.11f;  // weighted like the eye
}

// k-means on up to 40 000 opaque pixels (8-bit sRGB straight).
std::vector<Color> Extract(const unsigned char* px, int w, int h, int k) {
    std::vector<float> pts;
    const size_t n = (size_t)w * h;
    const size_t step = std::max<size_t>(1, n / 40000);
    for (size_t i = 0; i < n; i += step) {
        const unsigned char* p = px + i * 4;
        if (p[3] < 128) continue;
        pts.push_back(p[0] / 255.0f);
        pts.push_back(p[1] / 255.0f);
        pts.push_back(p[2] / 255.0f);
    }
    const size_t m = pts.size() / 3;
    if (m == 0) return {};
    k = std::max(1, std::min<int>(k, (int)m));
    // k-means++: each new center far from the previous ones.
    std::vector<float> c((size_t)k * 3), d(m, 1e9f);
    unsigned seed = 12345;
    auto rnd = [&] { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0f; };
    size_t first = (size_t)(rnd() * m) % m;
    std::copy(&pts[first * 3], &pts[first * 3] + 3, &c[0]);
    for (int j = 1; j < k; ++j) {
        double total = 0;
        for (size_t i = 0; i < m; ++i) {
            d[i] = std::min(d[i], Dist2(&pts[i * 3], &c[(size_t)(j - 1) * 3]));
            total += d[i];
        }
        double t = rnd() * total;
        size_t pick = 0;
        for (size_t i = 0; i < m; ++i) {
            t -= d[i];
            if (t <= 0) { pick = i; break; }
        }
        std::copy(&pts[pick * 3], &pts[pick * 3] + 3, &c[(size_t)j * 3]);
    }
    std::vector<int> label(m, 0);
    for (int it = 0; it < 12; ++it) {
        std::vector<double> sum((size_t)k * 3, 0.0);
        std::vector<int> cnt(k, 0);
        for (size_t i = 0; i < m; ++i) {
            int best = 0;
            float bd = 1e9f;
            for (int j = 0; j < k; ++j) {
                const float dd = Dist2(&pts[i * 3], &c[(size_t)j * 3]);
                if (dd < bd) { bd = dd; best = j; }
            }
            label[i] = best;
            ++cnt[best];
            for (int q = 0; q < 3; ++q) sum[(size_t)best * 3 + q] += pts[i * 3 + q];
        }
        for (int j = 0; j < k; ++j)
            if (cnt[j])
                for (int q = 0; q < 3; ++q) c[(size_t)j * 3 + q] = (float)(sum[(size_t)j * 3 + q] / cnt[j]);
    }
    std::vector<int> cnt(k, 0);
    for (int l : label) ++cnt[l];
    std::vector<Color> out;
    for (int j = 0; j < k; ++j)
        if (cnt[j]) out.push_back({c[(size_t)j * 3], c[(size_t)j * 3 + 1], c[(size_t)j * 3 + 2], (float)cnt[j] / m});
    std::sort(out.begin(), out.end(), [](const Color& a, const Color& b) { return a.share > b.share; });
    return out;
}

std::string Hex(const Color& c) {
    char b[16];
    std::snprintf(b, sizeof(b), "#%02X%02X%02X", (int)std::lround(c.r * 255), (int)std::lround(c.g * 255), (int)std::lround(c.b * 255));
    return b;
}

void OnPixels(void*, int ok, int w, int h, const void* pixels) {
    S.pending = false;
    if (!ok) { S.info = "Le calque actif n'est pas un calque de pixels."; return; }
    S.colors = Extract((const unsigned char*)pixels, w, h, S.count);
    char b[96];
    std::snprintf(b, sizeof(b), "%d couleur(s) sur %d x %d px", (int)S.colors.size(), w, h);
    S.info = S.colors.empty() ? "Calque vide (aucun pixel opaque)." : b;
}

void Request() {
    if (S.pending || H->api_version < 2) return;
    S.pending = true;
    H->pixels_read(0, 0, 0, 0, 0, SX_PIXELS_RGBA8, OnPixels, nullptr);
}

void OnEvent(void*, int event, const char*) {
    if (!S.autoUpdate) return;
    if (event == SX_EVENT_DOCUMENT_EDITED || event == SX_EVENT_DOCUMENT_ACTIVATED) {
        S.dirty = true;
        S.lastEdit = std::chrono::steady_clock::now();
    }
}

void DrawPanel(void*) {
    const SxUi* ui = H->ui;
    // Automatic update: half a second after the last edit.
    if (S.dirty && std::chrono::steady_clock::now() - S.lastEdit > std::chrono::milliseconds(500)) {
        S.dirty = false;
        Request();
    }
    if (ui->button(S.pending ? "Lecture..." : "Extraire")) Request();
    ui->tooltip("Couleurs dominantes du calque actif.");
    ui->same_line();
    ui->set_next_width(120);
    if (ui->slider_int("Couleurs", &S.count, 2, 16)) {
        char b[8];
        std::snprintf(b, sizeof(b), "%d", S.count);
        H->setting_set("count", b);
    }
    if (ui->checkbox("Auto", &S.autoUpdate)) H->setting_set("auto", S.autoUpdate ? "1" : "0");
    ui->tooltip("Extraire de nouveau après chaque modification du document.");
    ui->text_disabled(S.info.c_str());
    ui->separator(nullptr);
    const float cell = 34.0f;
    const int perRow = std::max(1, (int)(ui->content_width() / (cell + 8)));
    for (size_t i = 0; i < S.colors.size(); ++i) {
        const Color& c = S.colors[i];
        const float rgba[4] = {c.r, c.g, c.b, 1};
        ui->push_id((int)i);
        if (i % perRow) ui->same_line();
        const int click = ui->color_button("##c", rgba, cell);
        if (click) {
            H->set_color(click == 2 ? 1 : 0, rgba);
            H->status((Hex(c) + (click == 2 ? " : couleur d'arrière-plan" : " : couleur du pinceau")).c_str());
        }
        char tip[96];
        std::snprintf(tip, sizeof(tip), "%s  (%.0f %%)\nClic : pinceau, clic droit : arrière-plan", Hex(c).c_str(), c.share * 100);
        ui->tooltip(tip);
        ui->pop_id();
    }
    if (!S.colors.empty() && ui->button("Copier la liste (barre d'état)")) {
        std::string all;
        for (auto& c : S.colors) all += (all.empty() ? "" : " ") + Hex(c);
        H->status(all.c_str());
        H->log(all.c_str());
    }
}

const char* ExtractOperation(void*, const char* params) {
    // Uses the last extraction (the pixels come a frame later): call it again after "Extraire" / a first call.
    const char* p = params ? std::strstr(params, "\"count\":") : nullptr;
    if (p) S.count = std::max(2, std::min(16, std::atoi(p + 8)));
    Request();
    S.opResult = "{\"colors\":[";
    for (size_t i = 0; i < S.colors.size(); ++i) S.opResult += (i ? ",\"" : "\"") + Hex(S.colors[i]) + "\"";
    S.opResult += "],\"note\":\"couleurs de la dernière extraction (une nouvelle est lancée)\"}";
    return S.opResult.c_str();
}

void OpenWindow(void*) { H->open_panel("Palette", 1); }

}  // namespace

extern "C" SX_PLUGIN_EXPORT int sx_plugin_load(const SxHost* host, SxPlugin*) {
    if (!host || host->api_version < 3) return 0;  // needs pixels, settings, events
    H = host;
    S.count = std::max(2, std::min(16, std::atoi(host->setting_get("count", "6"))));
    S.autoUpdate = std::atoi(host->setting_get("auto", "0"));
    host->add_panel("Palette", DrawPanel, nullptr);
    host->add_command("Palette/Ouvrir la fenêtre", OpenWindow, nullptr);
    host->on_event(OnEvent, nullptr);
    host->add_operation("palette.extraire", "Couleurs dominantes du calque actif (hex), plugin Palette.",
                        "{\"type\":\"object\",\"properties\":{\"count\":{\"type\":\"integer\"}}}", 0, ExtractOperation, nullptr);
    return 1;
}
