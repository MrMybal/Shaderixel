// Position parameters placed on the image: a float2 parameter of a layer's filter or adjustment (0..1 range, such
// as the center of Iris Blur) is set by clicking / dragging on the canvas while its target button is on.
#include "ui/App.h"

namespace sx {

EffectInstance* App::ParamPickTarget() {
    if (!paramPick.active) return nullptr;
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == paramPick.view;
    if (!alive) return nullptr;
    Layer* l = paramPick.view->doc->FindLayer(paramPick.layer);
    if (!l) return nullptr;
    EffectInstance* fx = paramPick.filter < 0 ? &l->effect : paramPick.filter < (int)l->filters.size() ? &l->filters[paramPick.filter] : nullptr;
    if (!fx || paramPick.param >= fx->params.size()) return nullptr;
    return fx;
}

bool App::ParamPickButton(EffectInstance& fx, size_t param) {
    DocView* v = Active();
    if (!v) return false;
    // Where this instance lives (a layer's own effect or one of its filters).
    Id layer = 0;
    int filter = -2;
    for (auto& l : v->doc->S().layers) {
        if (&l.effect == &fx) { layer = l.id; filter = -1; }
        for (size_t k = 0; k < l.filters.size(); ++k)
            if (&l.filters[k] == &fx) { layer = l.id; filter = (int)k; }
    }
    if (!layer) return false;
    const bool on = paramPick.active && paramPick.view == v && paramPick.layer == layer && paramPick.filter == filter && paramPick.param == param;
    if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    const bool pressed = ImGui::SmallButton("+");
    if (on) ImGui::PopStyleColor();
    ImGui::SetItemTooltip(on ? "Placement actif : cliquez ou glissez sur l'image (Échap ou ce bouton pour finir)" : "Placer sur l'image (cliquer / glisser sur le canevas)");
    if (pressed) {
        if (on) paramPick.active = false;
        else paramPick = {true, v, layer, filter, param, false, {}};
    }
    return pressed;
}

bool App::ParamPickInput(DocView& v, float2 dp, bool hovered) {
    if (!paramPick.active || paramPick.view != &v) return false;
    EffectInstance* fx = ParamPickTarget();
    if (!fx || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        paramPick.active = false;
        return false;
    }
    Document& d = *v.doc;
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        paramPick.dragging = true;
        paramPick.before = d.S();
    }
    if (paramPick.dragging) {
        const float2 uv = {std::clamp(dp.x / std::max(1u, d.Width()), 0.0f, 1.0f), std::clamp(dp.y / std::max(1u, d.Height()), 0.0f, 1.0f)};
        float4& val = fx->params[paramPick.param];
        if (val.x != uv.x || val.y != uv.y) {
            val.x = uv.x;
            val.y = uv.y;
            if (d.Mode() == DocMode::Layers) d.SyncGraph();
            d.dirty = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            paramPick.dragging = false;
            if (paramPick.before) v.history.PushState("Position sur l'image", *paramPick.before, d.S());
            paramPick.before.reset();
        }
    }
    return true;
}

void App::ParamPickOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    if (!paramPick.active || paramPick.view != &v) return;
    EffectInstance* fx = ParamPickTarget();
    if (!fx) return;
    const float4 val = fx->params[paramPick.param];
    const ImVec2 c(canvasMin.x + v.pan.x + val.x * v.doc->Width() * v.zoom, canvasMin.y + v.pan.y + val.y * v.doc->Height() * v.zoom);
    for (int pass = 0; pass < 2; ++pass) {
        const ImU32 col = pass ? IM_COL32_WHITE : IM_COL32(0, 0, 0, 180);
        const float w = pass ? 1.5f : 3.5f;
        dl->AddCircle(c, 10, col, 0, w);
        dl->AddLine(ImVec2(c.x - 18, c.y), ImVec2(c.x - 6, c.y), col, w);
        dl->AddLine(ImVec2(c.x + 6, c.y), ImVec2(c.x + 18, c.y), col, w);
        dl->AddLine(ImVec2(c.x, c.y - 18), ImVec2(c.x, c.y - 6), col, w);
        dl->AddLine(ImVec2(c.x, c.y + 6), ImVec2(c.x, c.y + 18), col, w);
    }
}

}  // namespace sx
