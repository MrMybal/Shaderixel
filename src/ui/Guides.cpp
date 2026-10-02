// Rulers, guides, grid and snapping (View menu).
//
// Rulers sit along the top/left of the canvas; dragging from a ruler creates a
// guide, dragging a guide with the Move tool (or from anywhere while rulers are
// grabbed) moves it, dropping it back on a ruler deletes it. Guides belong to
// the document state (undoable, saved). Snapping pulls points to guides, grid
// lines and document edges within a few screen pixels.
#include "ui/App.h"
#include "ui/Theme.h"

#include <cmath>

namespace sx {

float App::RulerSize() const { return showRulers ? std::round(18.0f * dpi) : 0.0f; }

float2 App::Snap(const DocView& v, float2 p) const {
    if (!snapEnabled) return p;
    const Document& doc = *v.doc;
    const float tol = 7.0f / std::max(v.zoom, 1e-3f);
    auto snap1 = [&](float x, const std::vector<float>& guides, float extent) {
        float best = x, bestD = tol;
        auto consider = [&](float c) {
            float d = std::abs(c - x);
            if (d < bestD) { bestD = d; best = c; }
        };
        consider(0);
        consider(extent);
        if (showGuides) for (float g : guides) consider(g);
        if (showGrid && gridStep > 0) {
            float step = gridStep / std::max(gridSubdiv, 1);
            consider(std::round(x / step) * step);
        }
        return best;
    };
    const DocState& s = doc.S();
    return {snap1(p.x, s.guidesX, (float)doc.Width()), snap1(p.y, s.guidesY, (float)doc.Height())};
}

void App::DrawGridAndGuides(DocView& v, ImVec2 canvasMin, ImVec2 avail, ImDrawList* dl) {
    Document& doc = *v.doc;
    auto sx = [&](float x) { return canvasMin.x + v.pan.x + x * v.zoom; };
    auto sy = [&](float y) { return canvasMin.y + v.pan.y + y * v.zoom; };
    ImVec2 c1(canvasMin.x + avail.x, canvasMin.y + avail.y);
    dl->PushClipRect(canvasMin, c1, true);
    if (showGrid && gridStep > 0) {
        // Clip to the document; skip levels that would be denser than 6 screen pixels.
        float x0 = std::max(sx(0), canvasMin.x), x1 = std::min(sx((float)doc.Width()), c1.x);
        float y0 = std::max(sy(0), canvasMin.y), y1 = std::min(sy((float)doc.Height()), c1.y);
        int sub = std::max(gridSubdiv, 1);
        float minor = gridStep / sub;
        bool drawMinor = minor * v.zoom >= 6.0f;
        float step = drawMinor ? minor : gridStep;
        if (step * v.zoom >= 6.0f) {
            float docX0 = (x0 - canvasMin.x - v.pan.x) / v.zoom, docY0 = (y0 - canvasMin.y - v.pan.y) / v.zoom;
            for (float x = std::ceil(docX0 / step) * step; sx(x) <= x1; x += step) {
                bool major = std::fmod(std::abs(x) + 0.01f, gridStep) < 0.02f;
                dl->AddLine(ImVec2(sx(x), y0), ImVec2(sx(x), y1), major ? IM_COL32(150, 150, 170, 110) : IM_COL32(150, 150, 170, 45));
            }
            for (float y = std::ceil(docY0 / step) * step; sy(y) <= y1; y += step) {
                bool major = std::fmod(std::abs(y) + 0.01f, gridStep) < 0.02f;
                dl->AddLine(ImVec2(x0, sy(y)), ImVec2(x1, sy(y)), major ? IM_COL32(150, 150, 170, 110) : IM_COL32(150, 150, 170, 45));
            }
        }
    }
    if (showGuides) {
        const ImU32 col = IM_COL32(40, 220, 240, 220);
        for (float g : doc.S().guidesX) dl->AddLine(ImVec2(sx(g), canvasMin.y), ImVec2(sx(g), c1.y), col);
        for (float g : doc.S().guidesY) dl->AddLine(ImVec2(canvasMin.x, sy(g)), ImVec2(c1.x, sy(g)), col);
    }
    if (guideDrag.active && guideDrag.view == &v) {
        const ImU32 col = IM_COL32(40, 220, 240, 255);
        if (guideDrag.vertical) dl->AddLine(ImVec2(sx(guideDrag.pos), canvasMin.y), ImVec2(sx(guideDrag.pos), c1.y), col, 1.5f);
        else dl->AddLine(ImVec2(canvasMin.x, sy(guideDrag.pos)), ImVec2(c1.x, sy(guideDrag.pos)), col, 1.5f);
    }
    dl->PopClipRect();
}

// Returns true while a guide interaction owns the mouse (canvas tools must not react).
bool App::GuideInput(DocView& v, ImVec2 canvasMin, ImVec2 avail, bool canvasHovered) {
    ImGuiIO& io = ImGui::GetIO();
    Document& doc = *v.doc;
    float2 dp = ScreenToDoc(v, canvasMin, io.MousePos);
    if (guideDrag.active) {
        if (guideDrag.view != &v) { guideDrag = {}; return false; }
        float raw = guideDrag.vertical ? dp.x : dp.y;
        guideDrag.pos = io.KeyShift ? std::round(raw / 10.0f) * 10.0f : std::round(raw);  // Shift: 10 px steps
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            bool overCanvas = io.MousePos.x >= canvasMin.x && io.MousePos.y >= canvasMin.y && io.MousePos.x < canvasMin.x + avail.x &&
                              io.MousePos.y < canvasMin.y + avail.y;
            DocState before = doc.S();
            auto& list = guideDrag.vertical ? doc.S().guidesX : doc.S().guidesY;
            if (guideDrag.index >= 0 && guideDrag.index < (int)list.size()) list.erase(list.begin() + guideDrag.index);
            if (overCanvas) list.push_back(guideDrag.pos);
            if (overCanvas || guideDrag.index >= 0)
                v.history.PushState(overCanvas ? (guideDrag.index >= 0 ? "Déplacer le repère" : "Nouveau repère") : "Supprimer le repère", before, doc.S());
            guideDrag = {};
        }
        ImGui::SetMouseCursor(guideDrag.vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        return true;
    }
    // Pick an existing guide with the Move tool.
    if (canvasHovered && showGuides && tool == Tool::Move) {
        const float tol = 4.0f;
        auto pick = [&](const std::vector<float>& list, bool vertical) {
            for (int i = 0; i < (int)list.size(); ++i) {
                float s = vertical ? canvasMin.x + v.pan.x + list[i] * v.zoom : canvasMin.y + v.pan.y + list[i] * v.zoom;
                if (std::abs((vertical ? io.MousePos.x : io.MousePos.y) - s) <= tol) return i;
            }
            return -1;
        };
        int iv = pick(doc.S().guidesX, true), ih = iv < 0 ? pick(doc.S().guidesY, false) : -1;
        if (iv >= 0 || ih >= 0) {
            ImGui::SetMouseCursor(iv >= 0 ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                guideDrag.active = true;
                guideDrag.view = &v;
                guideDrag.vertical = iv >= 0;
                guideDrag.index = iv >= 0 ? iv : ih;
                guideDrag.pos = iv >= 0 ? doc.S().guidesX[iv] : doc.S().guidesY[ih];
                return true;
            }
        }
    }
    return false;
}

void App::DrawRulers(DocView& v, ImVec2 origin, ImVec2 canvasMin, ImVec2 avail) {
    const float R = RulerSize();
    if (R <= 0) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 rulerBg = IM_COL32(30, 31, 36, 255), tick = IM_COL32(150, 152, 165, 255), text = IM_COL32(150, 152, 165, 255);
    ImVec2 end(canvasMin.x + avail.x, canvasMin.y + avail.y);
    dl->AddRectFilled(origin, ImVec2(end.x, canvasMin.y), rulerBg);
    dl->AddRectFilled(origin, ImVec2(canvasMin.x, end.y), rulerBg);
    dl->AddLine(ImVec2(canvasMin.x, canvasMin.y - 1), ImVec2(end.x, canvasMin.y - 1), ui::col::Border);
    dl->AddLine(ImVec2(canvasMin.x - 1, canvasMin.y), ImVec2(canvasMin.x - 1, end.y), ui::col::Border);
    // Tick spacing: 1-2-5 sequence giving >= 50 screen pixels between labels.
    float step = 1;
    for (float k = 1; k < 1e7f; k *= 10) {
        if (k * v.zoom >= 50) { step = k; break; }
        if (2 * k * v.zoom >= 50) { step = 2 * k; break; }
        if (5 * k * v.zoom >= 50) { step = 5 * k; break; }
    }
    float minor = step / 10.0f;
    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize() * 0.72f;
    char buf[32];
    // Horizontal ruler.
    dl->PushClipRect(ImVec2(canvasMin.x, origin.y), ImVec2(end.x, canvasMin.y), true);
    float d0 = (canvasMin.x - canvasMin.x - v.pan.x) / v.zoom, d1 = d0 + avail.x / v.zoom;
    for (float x = std::floor(d0 / minor) * minor; x <= d1; x += minor) {
        float s = canvasMin.x + v.pan.x + x * v.zoom;
        bool major = std::abs(std::fmod(std::abs(x) + minor * 0.01f, step)) < minor * 0.02f;
        bool half = !major && std::abs(std::fmod(std::abs(x) + minor * 0.01f, step * 0.5f)) < minor * 0.02f;
        float h = major ? R : half ? R * 0.45f : R * 0.25f;
        dl->AddLine(ImVec2(s, canvasMin.y - h), ImVec2(s, canvasMin.y), tick);
        if (major) {
            snprintf(buf, sizeof(buf), "%d", (int)std::lround(x));
            dl->AddText(font, fs, ImVec2(s + 2, origin.y + 1), text, buf);
        }
    }
    dl->PopClipRect();
    // Vertical ruler (labels stacked digit by digit like Photoshop).
    dl->PushClipRect(ImVec2(origin.x, canvasMin.y), ImVec2(canvasMin.x, end.y), true);
    d0 = (-v.pan.y) / v.zoom;
    d1 = d0 + avail.y / v.zoom;
    for (float y = std::floor(d0 / minor) * minor; y <= d1; y += minor) {
        float s = canvasMin.y + v.pan.y + y * v.zoom;
        bool major = std::abs(std::fmod(std::abs(y) + minor * 0.01f, step)) < minor * 0.02f;
        bool half = !major && std::abs(std::fmod(std::abs(y) + minor * 0.01f, step * 0.5f)) < minor * 0.02f;
        float w = major ? R : half ? R * 0.45f : R * 0.25f;
        dl->AddLine(ImVec2(canvasMin.x - w, s), ImVec2(canvasMin.x, s), tick);
        if (major) {
            snprintf(buf, sizeof(buf), "%d", (int)std::lround(y));
            float ty = s + 2;
            for (const char* c = buf; *c; ++c) {
                char one[2] = {*c, 0};
                dl->AddText(font, fs, ImVec2(origin.x + 3, ty), text, one);
                ty += fs * 0.85f;
            }
        }
    }
    // Cursor position marks.
    dl->PopClipRect();
    ImVec2 m = ImGui::GetIO().MousePos;
    if (m.x >= canvasMin.x && m.x < end.x) dl->AddLine(ImVec2(m.x, origin.y), ImVec2(m.x, canvasMin.y), IM_COL32(255, 255, 255, 160));
    if (m.y >= canvasMin.y && m.y < end.y) dl->AddLine(ImVec2(origin.x, m.y), ImVec2(canvasMin.x, m.y), IM_COL32(255, 255, 255, 160));

    // Drag a new guide out of a ruler.
    ImGui::SetCursorScreenPos(ImVec2(canvasMin.x, origin.y));
    ImGui::InvisibleButton("##rulerTop", ImVec2(std::max(avail.x, 1.0f), R));
    if (ImGui::IsItemActivated() && !guideDrag.active) {
        guideDrag = {};
        guideDrag.active = true;
        guideDrag.view = &v;
        guideDrag.vertical = false;
        guideDrag.index = -1;
        showGuides = true;
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, canvasMin.y));
    ImGui::InvisibleButton("##rulerLeft", ImVec2(R, std::max(avail.y, 1.0f)));
    if (ImGui::IsItemActivated() && !guideDrag.active) {
        guideDrag = {};
        guideDrag.active = true;
        guideDrag.view = &v;
        guideDrag.vertical = true;
        guideDrag.index = -1;
        showGuides = true;
    }
}

}  // namespace sx
