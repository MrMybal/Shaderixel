// Text layers: rasterization sync, text tool, move, properties.
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_stdlib.h>

namespace sx {

void App::SyncTextLayers(DocView& v) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) return;
    bool any = false;
    for (auto& l : d.S().layers) {
        if (l.kind != LayerKind::Text || !l.surface) continue;
        u64 h = l.text.Hash() ^ ((u64)d.Width() << 32) ^ d.Height();
        auto it = v.textHashes.find(l.surface);
        if (it != v.textHashes.end() && it->second == h) continue;
        std::vector<float> rgba;
        Rect r;
        if (!RasterizeText(l.text, (int)d.Width(), (int)d.Height(), rgba, r)) continue;
        d.pendingClears.push_back({l.surface, {0, 0, 0, 0}});
        v.renderer.Prepare(d, *ctx);
        if (!r.Empty() && !rgba.empty())
            v.renderer.Import(d, *ctx, l.surface, rgba.data(), (u32)r.Width(), (u32)r.Height(), true, {r.x0, r.y0});
        v.textHashes[l.surface] = h;
        v.textRects[l.surface] = r;
        any = true;
    }
    (void)any;
}

void App::TextInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) {
        if (hovered && ImGui::IsMouseClicked(0)) Status("Les calques de texte sont disponibles en mode calques");
        return;
    }
    float2 dp = ScreenToDoc(v, canvasMin, io.MousePos);
    Layer* al = d.ActiveLayer();
    bool onActiveText = false;
    if (al && al->kind == LayerKind::Text) {
        auto it = v.textRects.find(al->surface);
        if (it != v.textRects.end()) {
            const Rect& r = it->second;
            onActiveText = dp.x >= r.x0 - 4 && dp.x <= r.x1 + 4 && dp.y >= r.y0 - 4 && dp.y <= r.y1 + 4;
        }
    }
    if (hovered && ImGui::IsMouseClicked(0)) {
        if (tool == Tool::Move || onActiveText) {
            if (al && al->kind == LayerKind::Text) {
                textDragging = true;
                textDragStart = dp;
                textPosStart = al->text.pos;
                editBefore = frameBefore;
            }
            if (tool == Tool::Text) focusTextEdit = true;
        } else {
            Edit e(d, v.history, "Nouveau texte");
            TextStyle s = textDefaults;
            s.color = fg;
            s.pos = {std::round(dp.x), std::round(dp.y)};
            if (s.content.empty()) s.content = "Texte";
            d.AddTextLayer(s);
            e.Commit();
            focusTextEdit = true;
            ImGui::SetWindowFocus("Propriétés");
        }
    }
    if (textDragging) {
        Layer* l = d.ActiveLayer();
        if (l && l->kind == LayerKind::Text) {
            float2 delta = dp - textDragStart;
            if (io.KeyShift) { if (std::abs(delta.x) > std::abs(delta.y)) delta.y = 0; else delta.x = 0; }
            l->text.pos = {std::round(textPosStart.x + delta.x), std::round(textPosStart.y + delta.y)};
        }
        if (!ImGui::IsMouseDown(0)) {
            textDragging = false;
            if (editBefore && l && !(l->text.pos == textPosStart)) v.history.PushState("Déplacer le texte", *editBefore, d.S());
            editBefore.reset();
        }
    }
    if (hovered && tool == Tool::Text) ImGui::SetMouseCursor(onActiveText ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_TextInput);
}

void App::TextOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    Document& d = *v.doc;
    Layer* al = d.ActiveLayer();
    if (!al || al->kind != LayerKind::Text || (tool != Tool::Text && tool != Tool::Move)) return;
    auto it = v.textRects.find(al->surface);
    if (it == v.textRects.end() || it->second.Empty()) return;
    const Rect& r = it->second;
    ImVec2 a(canvasMin.x + v.pan.x + r.x0 * v.zoom, canvasMin.y + v.pan.y + r.y0 * v.zoom);
    ImVec2 b(canvasMin.x + v.pan.x + r.x1 * v.zoom, canvasMin.y + v.pan.y + r.y1 * v.zoom);
    dl->AddRect(a, b, ui::col::Accent2, 0, 0, 1.0f);
    ImVec2 anchor(canvasMin.x + v.pan.x + al->text.pos.x * v.zoom, canvasMin.y + v.pan.y + al->text.pos.y * v.zoom);
    dl->AddCircleFilled(anchor, 3.5f, ui::col::Accent2);
}

void App::TextPropertiesUI(DocView& v, Layer& l) {
    Document& d = *v.doc;
    TextStyle& t = l.text;
    bool changed = false;
    ImGui::SeparatorText("Texte");
    if (focusTextEdit) {
        ImGui::SetKeyboardFocusHere();
        focusTextEdit = false;
    }
    if (ImGui::InputTextMultiline("##content", &t.content, ImVec2(-1, 90 * dpi))) {
        changed = true;
        std::string first = t.content.substr(0, std::min<size_t>(t.content.find('\n'), 32));
        if (!first.empty()) l.name = first;
    }
    TrackUndo("Modifier le texte");
    std::string fname = t.font.empty() ? "segoeui" : Narrow(Utf8Path(t.font).stem().wstring());
    if (ImGui::BeginCombo("Police", fname.c_str(), ImGuiComboFlags_HeightLarge)) {
        ImGui::InputTextWithHint("##ff", "Filtrer...", fontFilter, sizeof(fontFilter));
        for (auto& f : SystemFonts()) {
            if (fontFilter[0] && f.name.find(fontFilter) == std::string::npos) continue;
            if (ImGui::Selectable(f.name.c_str(), f.path == t.font)) {
                t.font = f.path;
                changed = true;
                v.history.PushState("Police", frameBefore, d.S());
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::SliderFloat("Taille", &t.size, 4, 2000, "%.0f px", ImGuiSliderFlags_Logarithmic)) changed = true;
    TrackUndo("Taille du texte");
    float c[4] = {t.color.x, t.color.y, t.color.z, t.color.w};
    if (ImGui::ColorEdit4("Couleur", c, ImGuiColorEditFlags_AlphaBar)) { t.color = {c[0], c[1], c[2], c[3]}; changed = true; }
    TrackUndo("Couleur du texte");
    if (ImGui::Combo("Alignement", &t.align, "Gauche\0Centre\0Droite\0")) {
        changed = true;
        v.history.PushState("Alignement", frameBefore, d.S());
    }
    if (ImGui::SliderFloat("Interligne", &t.lineSpacing, 0.5f, 3.0f, "%.2f")) changed = true;
    TrackUndo("Interligne");
    if (ImGui::SliderFloat("Approche", &t.tracking, -200, 800, "%.0f")) changed = true;
    TrackUndo("Approche");
    if (ImGui::Checkbox("Faux gras", &t.faux)) changed = true;
    TrackUndo("Faux gras");
    if (ImGui::DragFloat2("Position", &t.pos.x, 1.0f, -100000, 100000, "%.0f")) changed = true;
    TrackUndo("Position du texte");
    if (changed) d.dirty = true;
}

}  // namespace sx
