// Layers, properties, color and brush panels.
#include "ui/App.h"
#include "ui/Filter.h"
#include "io/ColorMgmt.h"
#include "doc/Brushes.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace sx {


namespace {
// Interactive tone-curve editor. Returns true while points changed this frame.
// `active` reports whether the widget is being dragged (for undo grouping).
bool CurveEditor(const char* id, CurvePoints& pts, ImU32 color, float size, bool& activated, bool& released, const float* histogram = nullptr) {
    activated = released = false;
    if (pts.size() < 2) pts = {{0, 0}, {1, 1}};
    ImGui::PushID(id);
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1(p0.x + size, p0.y + size);
    ImGui::InvisibleButton("##curve", ImVec2(size, size), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    ImGuiStorage* st = ImGui::GetStateStorage();
    ImGuiID dragKey = ImGui::GetID("drag");
    int drag = st->GetInt(dragKey, -1);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto toScreen = [&](float2 c) { return ImVec2(p0.x + c.x * size, p1.y - c.y * size); };
    auto toCurve = [&](ImVec2 s) { return float2{std::clamp((s.x - p0.x) / size, 0.0f, 1.0f), std::clamp((p1.y - s.y) / size, 0.0f, 1.0f)}; };

    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 22, 255), 4);
    if (histogram) {
        // Image histogram behind the curve (like Photoshop).
        for (int i = 0; i < 256; ++i) {
            float x = p0.x + size * (i + 0.5f) / 256.0f, h = histogram[i] * size * 0.9f;
            dl->AddLine(ImVec2(x, p1.y), ImVec2(x, p1.y - h), IM_COL32(150, 150, 160, 70), std::max(1.0f, size / 256.0f));
        }
    }
    for (int i = 1; i < 4; ++i) {
        float t = i / 4.0f;
        dl->AddLine(ImVec2(p0.x + size * t, p0.y), ImVec2(p0.x + size * t, p1.y), IM_COL32(255, 255, 255, 18));
        dl->AddLine(ImVec2(p0.x, p0.y + size * t), ImVec2(p1.x, p0.y + size * t), IM_COL32(255, 255, 255, 18));
    }
    dl->AddLine(ImVec2(p0.x, p1.y), ImVec2(p1.x, p0.y), IM_COL32(255, 255, 255, 40));
    dl->AddRect(p0, p1, IM_COL32(70, 72, 84, 255), 4);

    bool changed = false;
    ImVec2 mouse = ImGui::GetIO().MousePos;
    int hot = -1;
    for (int i = 0; i < (int)pts.size(); ++i) {
        ImVec2 s = toScreen(pts[i]);
        if (std::abs(s.x - mouse.x) < 7 && std::abs(s.y - mouse.y) < 7) hot = i;
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        activated = true;
        if (hot >= 0) drag = hot;
        else {
            float2 c = toCurve(mouse);
            auto it = std::lower_bound(pts.begin(), pts.end(), c, [](const float2& a, const float2& b) { return a.x < b.x; });
            drag = (int)(it - pts.begin());
            pts.insert(it, c);
            changed = true;
        }
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && hot > 0 && hot < (int)pts.size() - 1) {
        activated = true;
        pts.erase(pts.begin() + hot);
        changed = true;
        released = true;
    }
    if (drag >= 0 && drag < (int)pts.size()) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float2 c = toCurve(mouse);
            bool endpoint = drag == 0 || drag == (int)pts.size() - 1;
            float lo = drag > 0 ? pts[drag - 1].x + 0.01f : 0.0f;
            float hi = drag + 1 < (int)pts.size() ? pts[drag + 1].x - 0.01f : 1.0f;
            float outside = std::max(p0.y - mouse.y, mouse.y - p1.y);
            if (!endpoint && outside > 24.0f && pts.size() > 2) {  // drag out to delete
                pts.erase(pts.begin() + drag);
                drag = -1;
                changed = true;
            } else {
                float2 np = {std::clamp(c.x, lo, std::max(lo, hi)), c.y};
                if (!(np == pts[drag])) { pts[drag] = np; changed = true; }
            }
        } else {
            drag = -1;
            released = true;
        }
    }
    st->SetInt(dragKey, drag);

    // Curve
    ImVec2 line[129];
    for (int i = 0; i <= 128; ++i) {
        float x = i / 128.0f;
        line[i] = toScreen({x, EvalCurve(pts, x)});
    }
    dl->PushClipRect(p0, p1, true);
    dl->AddPolyline(line, 129, color, 0, 2.0f);
    dl->PopClipRect();
    for (int i = 0; i < (int)pts.size(); ++i) {
        ImVec2 s = toScreen(pts[i]);
        bool on = i == drag || i == hot;
        dl->AddRectFilled(ImVec2(s.x - 4, s.y - 4), ImVec2(s.x + 4, s.y + 4), on ? IM_COL32_WHITE : IM_COL32(20, 20, 24, 255));
        dl->AddRect(ImVec2(s.x - 4, s.y - 4), ImVec2(s.x + 4, s.y + 4), color);
    }
    if (hot >= 0 || drag >= 0) {
        int i = drag >= 0 ? drag : hot;
        if (i < (int)pts.size()) ImGui::SetTooltip("Entrée %d  Sortie %d", (int)std::lround(pts[i].x * 255), (int)std::lround(pts[i].y * 255));
    }
    ImGui::PopID();
    return changed;
}
}  // namespace

// Photoshop-style Blend If bar: black-to-white gradient with 4 handles
// (lo0, lo1, hi0, hi1). Each pair moves together until split with Alt-drag.
static bool BlendIfSlider(const char* id, u8 v[4], ImU32 hiColor = IM_COL32(255, 255, 255, 255)) {
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    const float dpi = ImGui::GetFontSize() / 16.0f;
    const float w = ImGui::GetContentRegionAvail().x, barH = 10 * dpi, handle = 6 * dpi;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float x0 = p0.x + handle, x1 = p0.x + w - handle;
    ImGui::InvisibleButton(id, ImVec2(w, barH + handle * 2 + ImGui::GetFontSize()));
    ImGuiID gid = ImGui::GetItemID();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilledMultiColor(ImVec2(x0, p0.y), ImVec2(x1, p0.y + barH), IM_COL32(0, 0, 0, 255), hiColor, hiColor, IM_COL32(0, 0, 0, 255));
    auto xOf = [&](int val) { return x0 + (x1 - x0) * val / 255.0f; };
    static int dragging = -1;
    static bool dragPair = true;
    static ImGuiID dragId = 0;
    bool changed = false;
    if (ImGui::IsItemActivated()) {
        float mx = ImGui::GetIO().MousePos.x, best = 1e9f;
        for (int i = 0; i < 4; ++i) {
            float dd = std::abs(xOf(v[i]) - mx);
            // On ties prefer the outer handle in the direction of the click.
            if (dd < best - 0.01f || (dd < best + 0.01f && ((i < 2 && mx < xOf(v[i])) || (i >= 2 && mx > xOf(v[i]))))) { best = dd; dragging = i; }
        }
        dragId = gid;
        dragPair = !ImGui::GetIO().KeyAlt;
    }
    if (ImGui::IsItemActive() && dragId == gid && dragging >= 0) {
        int val = (int)std::lround((ImGui::GetIO().MousePos.x - x0) / (x1 - x0) * 255.0f);
        int i = dragging, j = i ^ 1;  // partner handle of the same pair
        int lo = i < 2 ? 0 : v[1], hi = i < 2 ? v[2] : 255;
        if (dragPair && v[i] == v[j]) {
            int delta = std::clamp(val, lo, hi) - v[i];
            int a = std::clamp(v[i] + delta, lo, hi), b = std::clamp(v[j] + delta, lo, hi);
            if (a != v[i] || b != v[j]) { v[i] = (u8)a; v[j] = (u8)b; changed = true; }
        } else {
            // Split: keep lo0 <= lo1 and hi0 <= hi1.
            int mn = (i == 1) ? v[0] : (i == 3 ? v[2] : lo), mx = (i == 0) ? v[1] : (i == 2 ? v[3] : hi);
            mn = std::max(mn, lo);
            mx = std::min(mx, hi);
            int a = std::clamp(val, mn, std::max(mn, mx));
            if (a != v[i]) { v[i] = (u8)a; changed = true; }
        }
    }
    if (!ImGui::IsItemActive() && dragId == gid) { dragging = -1; dragId = 0; }
    for (int i = 0; i < 4; ++i) {
        float x = xOf(v[i]), y = p0.y + barH + 1;
        bool white = i >= 2;
        ImU32 fill = white ? IM_COL32(240, 240, 240, 255) : IM_COL32(20, 20, 20, 255);
        // Split handles are drawn as half triangles.
        bool split = v[(i & 2)] != v[(i & 2) + 1];
        float l = split && (i & 1) == 0 ? 0 : handle, r = split && (i & 1) == 1 ? 0 : handle;
        if (!split && (i & 1)) continue;  // joined pair: draw once
        dl->AddTriangleFilled(ImVec2(x, y), ImVec2(x + r, y + handle * 1.6f), ImVec2(x - l, y + handle * 1.6f), fill);
        dl->AddTriangle(ImVec2(x, y), ImVec2(x + r, y + handle * 1.6f), ImVec2(x - l, y + handle * 1.6f), IM_COL32(128, 128, 128, 255));
    }
    char txt[64];
    if (v[0] == v[1]) snprintf(txt, sizeof(txt), "%d", v[0]);
    else snprintf(txt, sizeof(txt), "%d / %d", v[0], v[1]);
    dl->AddText(ImVec2(x0, p0.y + barH + handle * 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), txt);
    if (v[2] == v[3]) snprintf(txt, sizeof(txt), "%d", v[3]);
    else snprintf(txt, sizeof(txt), "%d / %d", v[2], v[3]);
    dl->AddText(ImVec2(x1 - ImGui::CalcTextSize(txt).x, p0.y + barH + handle * 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), txt);
    if (changed) ImGui::MarkItemEdited(gid);
    (void)win;
    return changed;
}

void App::TrackUndo(const char* label) {
    DocView* v = Active();
    if (!v || !frameBeforeValid) return;
    if (ImGui::IsItemActivated()) editBefore = frameBefore;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        v->history.PushState(label, editBefore ? *editBefore : frameBefore, v->doc->S());
        editBefore.reset();
    } else if (ImGui::IsItemDeactivated()) {
        editBefore.reset();
    }
}

bool App::EffectParamsUI(EffectInstance& fx, const char* label, const char* onlyGroup) {
    EffectDefPtr def = fx.Def();
    if (!def) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Effet introuvable : %s", fx.effectId.c_str());
        return false;
    }
    if (fx.params.size() < def->params.size())
        for (size_t i = fx.params.size(); i < def->params.size(); ++i) fx.params.push_back(def->params[i].def);
    bool changed = false;
    ImGui::PushID(label);
    if (fx.curves.size() < def->params.size()) fx.curves.resize(def->params.size());
    std::vector<size_t> curveParams;
    std::string group;
    if (fx.effectId == "lens_profile" && !onlyGroup) {
        // Lens profile from the lensfun database (correction or simulation).
        if (ImGui::CollapsingHeader("Profil d'objectif (lensfun)", ImGuiTreeNodeFlags_DefaultOpen))
            changed |= LensfunPickerUI(fx, Active() ? &*Active()->doc : nullptr);
    }
    for (size_t i = 0; i < def->params.size(); ++i) {
        const ParamDef& p = def->params[i];
        if (onlyGroup && p.group != onlyGroup) continue;
        if (p.type == ParamType::Curve) { curveParams.push_back(i); continue; }
        if (p.group != group && !onlyGroup) {
            group = p.group;
            if (!group.empty()) ImGui::SeparatorText(group.c_str());
        }
        float4& val = fx.params[i];
        ImGui::PushID((int)i);
        bool c = false;
        switch (p.type) {
            case ParamType::Float: c = ImGui::SliderFloat(p.label.c_str(), &val.x, p.min, p.max, "%.3f"); break;
            case ParamType::Int: {
                int iv = (int)std::lround(val.x);
                c = ImGui::SliderInt(p.label.c_str(), &iv, (int)p.min, (int)p.max);
                val.x = (float)iv;
                break;
            }
            case ParamType::Bool: {
                bool b = val.x > 0.5f;
                c = ImGui::Checkbox(p.label.c_str(), &b);
                val.x = b ? 1.0f : 0.0f;
                break;
            }
            case ParamType::Color: {
                // Stored linear; edited in sRGB like every other color in the UI.
                float s[4] = {LinearToSrgb(val.x), LinearToSrgb(val.y), LinearToSrgb(val.z), val.w};
                c = ImGui::ColorEdit4(p.label.c_str(), s, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_Float);
                if (c) val = {SrgbToLinear(s[0]), SrgbToLinear(s[1]), SrgbToLinear(s[2]), s[3]};
                break;
            }
            case ParamType::Float2:
                if (p.min == 0 && p.max == 1) {
                    // A position in the image (0..1): also placed on the canvas.
                    // (the button first: the slider stays the last item, for the undo tracking and the reset below)
                    const float pickW = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x;
                    const float sliderW = ImGui::CalcItemWidth() - pickW;
                    ParamPickButton(fx, i);
                    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                    ImGui::SetNextItemWidth(sliderW);
                    c = ImGui::SliderFloat2(p.label.c_str(), &val.x, p.min, p.max, "%.3f");
                } else {
                    c = ImGui::SliderFloat2(p.label.c_str(), &val.x, p.min, p.max, "%.3f");
                }
                break;
            case ParamType::Float3: c = ImGui::SliderFloat3(p.label.c_str(), &val.x, p.min, p.max, "%.3f"); break;
            case ParamType::Float4: c = ImGui::SliderFloat4(p.label.c_str(), &val.x, p.min, p.max, "%.3f"); break;
            case ParamType::Lut: {
                if (fx.luts.size() < def->params.size()) fx.luts.resize(def->params.size());
                const Lut3D* l = fx.luts[i].get();
                if (l) ImGui::Text("%s  (%d³)", l->name.c_str(), l->size);
                else ImGui::TextDisabled("Aucune LUT chargée");
                if (ImGui::Button("Charger un fichier .cube...")) {
                    std::string path = platform::OpenFileDialog(hwnd, L"LUT 3D (*.cube)\0*.cube\0");
                    if (!path.empty()) {
                        std::string err;
                        if (auto lut = LoadCubeLut(path, &err)) {
                            fx.luts[i] = lut;
                            c = true;
                            if (Active()) Active()->history.PushState(p.label, frameBefore, Active()->doc->S());
                        } else {
                            Status("LUT invalide : " + err);
                        }
                    }
                }
                break;
            }
            case ParamType::Enum: {
                int iv = (int)std::lround(val.x);
                const char* cur = iv >= 0 && iv < (int)p.enumItems.size() ? p.enumItems[iv].c_str() : "?";
                if (ImGui::BeginCombo(p.label.c_str(), cur)) {
                    for (int k = 0; k < (int)p.enumItems.size(); ++k)
                        if (ImGui::Selectable(p.enumItems[k].c_str(), k == iv)) { val.x = (float)k; c = true; }
                    ImGui::EndCombo();
                }
                if (c && Active()) Active()->history.PushState(p.label, frameBefore, Active()->doc->S());
                break;
            }
        }
        if (p.type != ParamType::Enum && p.type != ParamType::Lut) {
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && p.type != ParamType::Bool) {
                val = p.def;  // double-click resets
                c = true;
            }
            TrackUndo(p.label.c_str());
        }
        changed |= c;
        ImGui::PopID();
    }
    if (!curveParams.empty() && ui::FilterSkip(("Courbe " + def->params[curveParams[0]].label).c_str())) curveParams.clear();  // filtered out
    if (!curveParams.empty()) {
        ImGuiStorage* st = ImGui::GetStateStorage();
        ImGuiID chanKey = ImGui::GetID("curvechan");
        int chan = std::clamp(st->GetInt(chanKey, 0), 0, (int)curveParams.size() - 1);
        static const ImU32 colors[] = {IM_COL32(230, 230, 235, 255), IM_COL32(240, 80, 80, 255), IM_COL32(90, 220, 110, 255), IM_COL32(90, 140, 250, 255)};
        for (size_t k = 0; k < curveParams.size(); ++k) {
            if (k) ImGui::SameLine();
            if (ImGui::RadioButton(def->params[curveParams[k]].label.c_str(), chan == (int)k)) chan = (int)k;
        }
        st->SetInt(chanKey, chan);
        size_t pi = curveParams[chan];
        // Square editor: as wide as the panel, but leave the rest of the properties visible.
        float edSize = std::clamp(std::min(ImGui::GetContentRegionAvail().x, ImGui::GetWindowHeight() * 0.55f), 120 * dpi, 300 * dpi);
        bool act = false, rel = false;
        DocView* v = Active();
        const float* hist = histo.valid ? histo.norm[std::min<size_t>(chan, 3)] : nullptr;
        if (CurveEditor(def->params[pi].id.c_str(), fx.curves[pi], colors[std::min<size_t>(chan, 3)], edSize, act, rel, hist)) changed = true;
        if (act && v) editBefore = frameBefore;
        if (rel && v && editBefore) {
            v->history.PushState("Courbes", *editBefore, v->doc->S());
            editBefore.reset();
        }
        if (ImGui::SmallButton("Réinitialiser la courbe")) {
            fx.curves[pi] = {{0, 0}, {1, 1}};
            changed = true;
            if (v) v->history.PushState("Courbes", frameBefore, v->doc->S());
        }
        ImGui::TextDisabled("Clic : ajouter un point - clic droit / glisser dehors : supprimer");
    }
    ImGui::PopID();
    return changed;
}

// ------------------------------------------------------------------ layers

void App::DrawLayers() {
    if (!ImGui::Begin("Calques", PanelFlag("Calques"))) { ImGui::End(); return; }
    PopOutCorner("Calques");
    DocView* v = Active();
    if (!v) { ImGui::End(); return; }
    Document& d = *v->doc;
    static std::string layerFilter;

    if (d.Mode() == DocMode::Nodes) {
        ImGui::TextWrapped("Ce document est en mode nodal : la composition se fait dans le panneau Graphe.");
        std::string why;
        bool can = d.CanConvertToLayers(&why);
        ImGui::BeginDisabled(!can);
        if (ImGui::Button("Convertir en calques")) {
            Edit e(d, v->history, "Convertir en calques");
            d.ConvertToLayers();
            e.Commit();
        }
        ImGui::EndDisabled();
        if (!can) ImGui::TextColored(ImVec4(1, 0.7f, 0.35f, 1), "Graphe non linéaire : %s", why.c_str());
        ImGui::End();
        return;
    }

    Layer* active = d.ActiveLayer();
    // Blend + opacity of the active layer (Photoshop header).
    ImGui::BeginDisabled(!active);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    bool isGroup = active && active->kind == LayerKind::Group;
    const char* curBlend = !active ? "Normal" : (isGroup && active->passThrough) ? "Traverser"
                           : !active->blendFx.customCode.empty() ? "Personnalisé (HLSL)"
                           : active->HasCustomBlend() ? (active->blendFx.Def() ? active->blendFx.Def()->name.c_str() : "Fusion d'effet")
                           : BlendModeName(active->blend);
    if (ImGui::BeginCombo("##blend", curBlend)) {
        if (isGroup && ImGui::Selectable("Traverser", active->passThrough)) {
            active->passThrough = true;
            d.SyncGraph();
            v->history.PushState("Mode de fusion", frameBefore, d.S());
        }
        for (int i = 0; i < kBlendMenuCount; ++i) {
            int m = kBlendMenuOrder[i];
            if (m < 0) { ImGui::Separator(); continue; }
            if (ImGui::Selectable(BlendModeName((BlendMode)m), active && (int)active->blend == m && !(isGroup && active->passThrough) && !active->HasCustomBlend())) {
                active->blend = (BlendMode)m;
                active->passThrough = false;
                active->blendFx = {};
                d.SyncGraph();
                v->history.PushState("Mode de fusion", frameBefore, d.S());
            }
        }
        ImGui::Separator();
        // Library blend effects (inputs Fond, Calque, Masque), e.g. imported CyTexture blends.
        if (ImGui::BeginMenu("Fusions d'effets")) {
            for (auto& def : Effects().All()) {
                if (!def->blend) continue;
                if (ImGui::MenuItem(def->name.c_str(), def->category.c_str(), active && active->blendFx.effectId == def->id)) {
                    active->blendFx = {};
                    active->blendFx.effectId = def->id;
                    active->blendFx.params = def->DefaultParams();
                    active->passThrough = false;
                    d.SyncGraph();
                    v->history.PushState("Mode de fusion", frameBefore, d.S());
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::Selectable("Personnalisé (HLSL)...", active && !active->blendFx.customCode.empty())) {
            if (active->blendFx.customCode.empty()) {
                active->blendFx = {};
                active->blendFx.customCode = kCustomBlendTemplate;
                if (auto def = active->blendFx.Def()) active->blendFx.params = def->DefaultParams();
            }
            active->passThrough = false;
            d.SyncGraph();
            v->history.PushState("Fusion personnalisée", frameBefore, d.S());
            FocusPanel("Propriétés");
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    float opacity = active ? active->opacity * 100.0f : 100.0f;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##opacity", &opacity, 0, 100, "Opacité %.0f %%") && active) {
        active->opacity = opacity / 100.0f;
        d.SyncGraph();
    }
    TrackUndo("Opacité");
    ImGui::EndDisabled();

    // Layer list, top layer first; children of collapsed groups are hidden.
    float thumb = 38.0f * dpi;
    ImGui::BeginChild("##layerlist", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4), ImGuiChildFlags_Borders);
    auto& layers = d.S().layers;
    Id dragId = 0, dropTarget = 0;
    std::pair<Id, std::string> fxDrop;  // effect dropped on a layer row
    bool dropInto = false;
    auto hiddenByCollapse = [&](const Layer& l) {
        Id p = l.parent;
        while (p) {
            Layer* g = d.FindLayer(p);
            if (!g) break;
            if (g->collapsed) return true;
            p = g->parent;
        }
        return false;
    };
    // Name filter: matching layers (and the groups holding them), collapsed groups opened meanwhile.
    ui::ListFilter(layerFilter, "Filtrer les calques (Ctrl+F)...");
    auto shownByFilter = [&](const Layer& l) {
        if (layerFilter.empty() || ui::FilterMatch(layerFilter, l.name)) return true;
        if (l.kind != LayerKind::Group) return false;
        for (auto& c : layers) {
            if (c.kind == LayerKind::Group || !ui::FilterMatch(layerFilter, c.name)) continue;
            for (Id p = c.parent; p; p = d.FindLayer(p) ? d.FindLayer(p)->parent : 0)
                if (p == l.id) return true;
        }
        return false;
    };
    for (int i = (int)layers.size() - 1; i >= 0; --i) {
        Layer& l = layers[i];
        if (layerFilter.empty() ? hiddenByCollapse(l) : !shownByFilter(l)) continue;
        ImGui::PushID((int)l.id);
        bool selected = l.id == d.S().activeLayer;
        bool group = l.kind == LayerKind::Group;
        int depth = d.LayerDepth(l.id);
        float indent = depth * 16.0f * dpi + (l.clipped ? 14.0f * dpi : 0.0f);
        ImVec2 rowMin = ImGui::GetCursorScreenPos();
        float rowH = group ? ImGui::GetFrameHeight() + 8 : thumb + 8;
        float w = ImGui::GetContentRegionAvail().x;
        if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick, ImVec2(w, rowH))) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && !group) {  // Ctrl+click: load layer pixels as selection
                SelMode m = io.KeyShift && io.KeyAlt ? SelMode::Intersect : io.KeyShift ? SelMode::Add : io.KeyAlt ? SelMode::Subtract : SelMode::Replace;
                SelectFromLayer(*v, l, m);
            } else {
                d.S().activeLayer = l.id;
                d.S().activeNode = 0;  // the layer itself (not one of its filter nodes)
                d.S().editMask = false;
                if (group && ImGui::IsMouseDoubleClicked(0)) l.collapsed = !l.collapsed;
                if (l.kind == LayerKind::Smart && ImGui::IsMouseDoubleClicked(0)) smartEditRequest = l.id;
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && !group) ImGui::SetTooltip("Ctrl+clic : sélectionner les pixels du calque");
        if (ImGui::BeginDragDropSource()) {
            Id id = l.id;
            ImGui::SetDragDropPayload("SX_LAYER", &id, sizeof(Id));
            ImGui::Text("%s", l.name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("SX_LAYER")) {
                dragId = *(const Id*)p->Data;
                dropTarget = l.id;
                // Dropping on the lower half of a group row puts the layer inside it.
                dropInto = group && ImGui::GetMousePos().y > rowMin.y + rowH * 0.5f;
            }
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kEffectPayload)) fxDrop = {l.id, std::string((const char*)p->Data)};
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("##ctx")) {
            // The menu can add, remove or reorder layers (or restore a state): `l` may then dangle,
            // so the rest of the list is skipped this frame.
            Id rowId = l.id;
            size_t count = layers.size();
            const Layer* data = layers.data();
            d.S().activeLayer = l.id;
            LayerMenuItems();
            ImGui::EndPopup();
            if (layers.size() != count || layers.data() != data || layers[i].id != rowId) {
                ImGui::PopID();
                break;
            }
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        // Visibility toggle
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x + 2, rowMin.y + (rowH - 22 * dpi) * 0.5f));
        if (ui::IconButton("vis", l.visible ? ui::IconEye : ui::IconEyeOff, false, 22 * dpi, "Afficher / masquer")) {
            DocState before = d.S();
            l.visible = !l.visible;
            d.SyncGraph();
            v->history.PushState(l.visible ? "Afficher le calque" : "Masquer le calque", before, d.S());
        }
        float x0 = rowMin.x + 30 * dpi + indent;
        if (l.clipped) {  // clipping arrow
            float cx = x0 - 9 * dpi, cy = rowMin.y + rowH * 0.5f;
            dl->AddLine(ImVec2(cx, cy - 8 * dpi), ImVec2(cx, cy + 4 * dpi), ui::col::TextDim, 1.5f);
            dl->AddTriangleFilled(ImVec2(cx - 4 * dpi, cy + 3 * dpi), ImVec2(cx + 4 * dpi, cy + 3 * dpi), ImVec2(cx, cy + 8 * dpi), ui::col::TextDim);
        }
        float textX;
        if (group) {
            // Collapse arrow + folder icon
            float h = rowH;
            ImGui::SetCursorScreenPos(ImVec2(x0, rowMin.y + (h - 18 * dpi) * 0.5f));
            if (ImGui::InvisibleButton("##fold", ImVec2(16 * dpi, 18 * dpi))) l.collapsed = !l.collapsed;
            ImVec2 c(x0 + 8 * dpi, rowMin.y + h * 0.5f);
            float a = 4.5f * dpi;
            if (l.collapsed) dl->AddTriangleFilled(ImVec2(c.x - a * 0.6f, c.y - a), ImVec2(c.x + a, c.y), ImVec2(c.x - a * 0.6f, c.y + a), ui::col::Text);
            else dl->AddTriangleFilled(ImVec2(c.x - a, c.y - a * 0.6f), ImVec2(c.x + a, c.y - a * 0.6f), ImVec2(c.x, c.y + a), ui::col::Text);
            ui::DrawIcon(dl, ui::IconFolder, ImVec2(x0 + 30 * dpi, c.y), 18 * dpi, l.passThrough ? ui::col::TextDim : ui::col::Accent2);
            textX = x0 + 46 * dpi;
        } else {
            // Thumbnail(s)
            ImVec2 tp(x0, rowMin.y + 4);
            gpu::Texture* src = nullptr;
            u64 hash = 0;
            if (l.kind == LayerKind::Pixel || l.kind == LayerKind::Text || l.kind == LayerKind::Shape || l.kind == LayerKind::Smart) {
                if (Surface* s = d.GetSurface(l.surface)) { src = s->tex.get(); hash = s->version; }
            } else {
                for (auto& n : d.S().graph.nodes)
                    if (n.layer == l.id && n.kind == NodeKind::Effect) {
                        src = v->renderer.NodeOutput(n.id);
                        hash = v->renderer.NodeHash(n.id);
                        break;
                    }
            }
            if (src) {
                gpu::Texture* t = v->renderer.Thumbnail(*ctx, ((u64)l.id << 1), src, hash, (u32)thumb, (u32)thumb);
                dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(t), tp, ImVec2(tp.x + thumb, tp.y + thumb));
            }
            if (l.kind == LayerKind::Text) ui::DrawIcon(dl, ui::IconText, ImVec2(tp.x + thumb * 0.5f, tp.y + thumb * 0.5f), thumb * 0.55f, IM_COL32(255, 255, 255, 220));
            else if (l.kind == LayerKind::Smart) {
                // Smart object badge (bottom right of the thumbnail).
                ImVec2 b0(tp.x + thumb - 12 * dpi, tp.y + thumb - 12 * dpi), b1(tp.x + thumb, tp.y + thumb);
                dl->AddRectFilled(b0, b1, IM_COL32(30, 30, 36, 230), 2);
                dl->AddRect(ImVec2(b0.x + 2 * dpi, b0.y + 2 * dpi), ImVec2(b1.x - 2 * dpi, b1.y - 2 * dpi), IM_COL32(230, 230, 240, 255), 1);
                SmartObject* so = d.GetSmart(l.smart);
                if (so && !so->linkedPath.empty()) dl->AddLine(ImVec2(b0.x + 3 * dpi, b1.y - 3 * dpi), ImVec2(b1.x - 3 * dpi, b0.y + 3 * dpi), ui::col::Accent2, 1.5f);
            } else if (l.kind != LayerKind::Pixel) ui::DrawIcon(dl, ui::IconFx, ImVec2(tp.x + thumb * 0.5f, tp.y + thumb * 0.5f), thumb * 0.5f, IM_COL32(255, 255, 255, 200));
            bool contentSel = selected && !d.S().editMask;
            dl->AddRect(ImVec2(tp.x - 1, tp.y - 1), ImVec2(tp.x + thumb + 1, tp.y + thumb + 1), contentSel ? ui::col::Accent2 : ui::col::Border, 0, 0, contentSel ? 2.0f : 1.0f);
            textX = tp.x + thumb + 8;
            if (l.mask) {
                ImVec2 mp(textX, tp.y);
                if (Surface* s = d.GetSurface(l.mask)) {
                    gpu::Texture* t = v->renderer.Thumbnail(*ctx, ((u64)l.id << 1) | 1, s->tex.get(), s->version, (u32)thumb, (u32)thumb);
                    dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(t), mp, ImVec2(mp.x + thumb, mp.y + thumb));
                }
                bool maskSel = selected && d.S().editMask;
                dl->AddRect(ImVec2(mp.x - 1, mp.y - 1), ImVec2(mp.x + thumb + 1, mp.y + thumb + 1), maskSel ? ui::col::Accent2 : ui::col::Border, 0, 0, maskSel ? 2.0f : 1.0f);
                if (!l.maskEnabled) dl->AddLine(mp, ImVec2(mp.x + thumb, mp.y + thumb), ui::col::Error, 2);
                ImGui::SetCursorScreenPos(mp);
                if (ImGui::InvisibleButton("##mask", ImVec2(thumb, thumb))) {
                    d.S().activeLayer = l.id;
                    d.S().editMask = true;
                }
                ui::HelpTooltip("Clic : éditer le masque");
                ImGui::SetCursorScreenPos(tp);
                if (ImGui::InvisibleButton("##content", ImVec2(thumb, thumb))) {
                    d.S().activeLayer = l.id;
                    d.S().activeNode = 0;
                    d.S().editMask = false;
                }
                textX += thumb + 8;
            }
        }
        ImVec2 tpos(textX, rowMin.y + (rowH - ImGui::GetFontSize()) * 0.5f);
        dl->AddText(tpos, l.visible ? ui::col::Text : ui::col::TextDim, l.name.c_str());
        if (l.clipped) dl->AddLine(ImVec2(tpos.x, tpos.y + ImGui::GetFontSize() + 1), ImVec2(tpos.x + ImGui::CalcTextSize(l.name.c_str()).x, tpos.y + ImGui::GetFontSize() + 1), ui::col::TextDim);
        if (l.locked) ui::DrawIcon(dl, ui::IconLock, ImVec2(rowMin.x + w - 14 * dpi, rowMin.y + rowH * 0.5f), 12 * dpi, ui::col::TextDim);
        if (l.filters.size()) {
            char fx[16];
            snprintf(fx, sizeof(fx), "fx %zu", l.filters.size());
            dl->AddText(ImVec2(rowMin.x + w - 48 * dpi, tpos.y), ui::col::Accent2, fx);
        }
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y + rowH + ImGui::GetStyle().ItemSpacing.y));
        ImGui::PopID();
    }
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndChild();
    if (smartEditRequest) {
        if (Layer* sl = d.FindLayer(smartEditRequest)) EditSmartContents(*v, *sl);
        smartEditRequest = 0;
    }
    if (fxDrop.first) DropEffectOnLayer(*v, fxDrop.first, fxDrop.second);
    if (dragId && dropTarget && dragId != dropTarget) {
        Edit e(d, v->history, "Déplacer le calque");
        d.MoveLayerTo(dragId, dropTarget, dropInto);
        e.Commit();
    }

    // Bottom toolbar (the row menus or the drop above may have changed the layer list).
    active = d.ActiveLayer();
    float bs = ImGui::GetFrameHeight();
    if (ui::IconButton("new", ui::IconPlus, false, bs, "Nouveau calque (Ctrl+Maj+N)")) {
        Edit e(d, v->history, "Nouveau calque");
        d.AddPixelLayer("Calque " + std::to_string(layers.size() + 1));
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("grp", ui::IconFolder, false, bs, "Grouper le calque (Ctrl+G)") && active) {
        Edit e(d, v->history, "Grouper");
        d.GroupLayer(active->id);
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("fx", ui::IconFx, false, bs, "Nouveau calque de remplissage ou de réglage")) ImGui::OpenPopup("##newfx");
    if (ImGui::BeginPopup("##newfx")) {
        if (ImGui::BeginMenu("Remplissage")) {
            EffectMenu([&](const std::string& id) { Edit e(d, v->history, "Calque de remplissage"); MaskFromSelection(*v, d.AddEffectLayer(LayerKind::Fill, id).id, false); e.Commit(); }, true, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Réglage")) {
            EffectMenu([&](const std::string& id) { Edit e(d, v->history, "Calque de réglage"); MaskFromSelection(*v, d.AddEffectLayer(LayerKind::Adjustment, id).id, false); e.Commit(); }, false, true);
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!active);
    if (ui::IconButton("mask", ui::IconMask, false, bs, "Ajouter un masque de fusion") && active && !active->mask) {
        Edit e(d, v->history, "Ajouter un masque");
        MaskFromSelection(*v, active->id, true);
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("dup", ui::IconCopy, false, bs, "Dupliquer (Ctrl+J)") && active) {
        Edit e(d, v->history, "Dupliquer le calque");
        d.DuplicateLayer(active->id);
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("up", ui::IconUp, false, bs, "Monter (Ctrl+])") && active) {
        Edit e(d, v->history, "Monter le calque");
        d.MoveLayerSibling(active->id, +1);
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("down", ui::IconDown, false, bs, "Descendre (Ctrl+[)") && active) {
        Edit e(d, v->history, "Descendre le calque");
        d.MoveLayerSibling(active->id, -1);
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("del", ui::IconTrash, false, bs, "Supprimer le calque") && active) {
        Edit e(d, v->history, "Supprimer le calque");
        d.RemoveLayer(active->id);
        e.Commit();
    }
    ImGui::EndDisabled();
    ImGui::End();
}

// ------------------------------------------------------------------ properties

void App::DrawProperties() {
    if (!ImGui::Begin("Propriétés", PanelFlag("Propriétés"))) { ImGui::End(); return; }
    ui::FilterBox();
    DocView* v = Active();
    if (!v) { ImGui::End(); return; }
    Document& d = *v->doc;
    ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.42f);

    const Node* selNode = d.Mode() == DocMode::Layers ? d.S().graph.Find(d.S().activeNode) : nullptr;
    if (selNode && selNode->kind == NodeKind::Output) {
        // Output node of the layer graph: the final image (shown in Aperçu du calque) and the document.
        ImGui::SeparatorText("Sortie : image finale");
        ImGui::TextWrapped("Composition de tous les calques visibles, telle qu'elle est affichée et exportée.");
        if (ImGui::Button("Exporter l'image..."))
            ExportImage(platform::SaveFileDialog(hwnd, L"PNG\0*.png\0JPEG\0*.jpg\0TIFF 16 bits\0*.tif\0TGA\0*.tga\0BMP\0*.bmp\0", L"png"));
        ImGui::SameLine();
        if (ImGui::Button("Retour au calque actif")) d.S().activeNode = 0;
    } else if (d.Mode() == DocMode::Layers) {
        Layer* l = d.ActiveLayer();
        if (!l) {
            ImGui::TextDisabled("Aucun calque sélectionné.");
        } else {
            bool changed = false;
            // A filter node selected in the graph: its parameters first, the layer's properties folded below.
            int selFilter = -1;
            u32 selRole = 0;  // generated node of this layer: 16 fill / adjustment, 17 blend, 18 mask
            if (const Node* gn = d.S().graph.Find(d.S().activeNode); gn && gn->layer == l->id && (gn->id & 0x40000000u)) {
                const u32 role = gn->id & 31u;
                if (const int f = LayerFilterOfRole(role); f >= 0 && f < (int)l->filters.size() && l->filters[f].effectId == gn->fx.effectId) selFilter = f;
                if (role >= 16 && role <= 18) selRole = role;
                if (gn->kind == NodeKind::Blend) selRole = 17;
            }
            if (selRole == 17) {
                // Blend node: how the layer is laid over what is below (its result in Aperçu du calque).
                ImGui::PushID("selblend");
                ImGui::SeparatorText(("Fusion : " + l->name).c_str());
                if (ImGui::BeginCombo("Mode", BlendModeName(l->blend))) {
                    for (int i = 0; i < kBlendMenuCount; ++i) {
                        int m = kBlendMenuOrder[i];
                        if (m < 0) { ImGui::Separator(); continue; }
                        if (ImGui::Selectable(BlendModeName((BlendMode)m), (int)l->blend == m)) {
                            DocState before = d.S();
                            l->blend = (BlendMode)m;
                            d.SyncGraph();
                            v->history.PushState("Mode de fusion", before, d.S());
                        }
                    }
                    ImGui::EndCombo();
                }
                float op = l->opacity * 100.0f;
                if (ImGui::SliderFloat("Opacité", &op, 0, 100, "%.0f %%")) { l->opacity = op / 100.0f; changed = true; }
                TrackUndo("Opacité");
                if (ImGui::Checkbox("Masque d'écrêtage", &l->clipped)) changed = true;
                TrackUndo("Masque d'écrêtage");
                ImGui::TextDisabled("Entrée A : calques du dessous   Entrée B : ce calque (filtres compris)");
                ImGui::PopID();
                ImGui::Spacing();
            } else if (selRole == 18 && l->mask) {
                ImGui::PushID("selmask");
                ImGui::SeparatorText(("Masque : " + l->name).c_str());
                if (ImGui::Checkbox("Activé", &l->maskEnabled)) changed = true;
                TrackUndo("Masque activé");
                ImGui::SameLine();
                bool edit = d.S().editMask;
                if (ImGui::Checkbox("Peindre le masque", &edit)) d.S().editMask = edit;
                ImGui::TextDisabled("Blanc : le calque est visible, noir : masqué.");
                ImGui::PopID();
                ImGui::Spacing();
            } else if (selRole == 16 && (l->kind == LayerKind::Fill || l->kind == LayerKind::Adjustment)) {
                ImGui::PushID("seleffect");
                auto def = l->effect.Def();
                ImGui::SeparatorText(((l->kind == LayerKind::Fill ? "Remplissage : " : "Réglage : ") + (def ? def->name : l->name)).c_str());
                changed |= EffectParamsUI(l->effect, "selmain");
                if (ImGui::SmallButton("Éditer (code / graphe)")) { efEd.filterIndex = -1; OpenGraphEditor(0); }
                ImGui::PopID();
                ImGui::Spacing();
            }
            const bool nodeFirst = selFilter >= 0 || selRole == 17 || (selRole == 18 && l->mask) || (selRole == 16 && (l->kind == LayerKind::Fill || l->kind == LayerKind::Adjustment));
            if (selFilter >= 0) {
                EffectInstance& f = l->filters[selFilter];
                auto def = f.Def();
                ImGui::PushID("selfilter");
                ImGui::SeparatorText(("Filtre : " + (def ? def->name : f.effectId)).c_str());
                if (ImGui::Checkbox("Actif", &f.enabled)) changed = true;
                TrackUndo("Activer le filtre");
                ImGui::SameLine();
                ImGui::TextDisabled("calque « %s »", l->name.c_str());
                changed |= EffectParamsUI(f, "sel");
                if (ImGui::SmallButton("Éditer (code / graphe)")) { efEd.filterIndex = selFilter; OpenGraphEditor(0); }
                if (f.effectId == "camera_raw" || f.effectId == "lens_profile" || f.effectId == "photo_raw_local" || f.effectId == "photo_raw_geometry") {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Ouvrir dans Photo RAW...")) OpenCameraRaw(*v);
                }
                ImGui::PopID();
                ImGui::Spacing();
            }
            if (!nodeFirst || ImGui::CollapsingHeader(("Calque : " + l->name).c_str())) {
            ImGui::InputText("Nom", &l->name);
            TrackUndo("Renommer");
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
            const char* kinds[] = {"Calque de pixels", "Calque de remplissage", "Calque de réglage", "Groupe", "Calque de texte", "Calque de forme", "Objet dynamique"};
            ImGui::TextDisabled("%s", kinds[(int)l->kind]);
            if (ImGui::Checkbox("Visible", &l->visible)) changed = true;
            TrackUndo("Visibilité");
            ImGui::SameLine();
            if (ImGui::Checkbox("Verrouillé", &l->locked)) changed = true;
            TrackUndo("Verrouillage");
            if (l->kind == LayerKind::Text) TextPropertiesUI(*v, *l);
            if (l->kind == LayerKind::Shape) ShapePropertiesUI(*v, *l);
            if (l->kind == LayerKind::Smart) {
                // "Nouvel objet par copie" inserts a layer (the vector may reallocate): re-fetch this one by id.
                const Id lid = l->id;
                SmartPropertiesUI(*v, *l);
                l = d.FindLayer(lid);  // a duplicate never removes the source layer
            }
            if (l->kind == LayerKind::Smart || l->kind == LayerKind::Pixel || l->kind == LayerKind::Fill || l->kind == LayerKind::Text ||
                l->kind == LayerKind::Shape)
                LayerImagePropertiesUI(*v, *l);
            if (l->kind == LayerKind::Group && l->artboard) ArtboardPropertiesUI(*v, *l);
            if (l->kind == LayerKind::Fill || l->kind == LayerKind::Adjustment) {
                ImGui::SeparatorText(l->kind == LayerKind::Fill ? "Remplissage" : "Réglage");
                changed |= EffectParamsUI(l->effect, "main");
                if (ImGui::SmallButton("Éditer (code / graphe)")) { efEd.filterIndex = -1; OpenGraphEditor(0); }
            }
            if (l->mask) {
                ImGui::SeparatorText("Masque de fusion");
                if (ImGui::Checkbox("Activé", &l->maskEnabled)) changed = true;
                TrackUndo("Masque activé");
                ImGui::SameLine();
                bool edit = d.S().editMask;
                if (ImGui::Checkbox("Peindre le masque", &edit)) d.S().editMask = edit;
                ImGui::SameLine();
                if (ImGui::SmallButton("Supprimer")) {
                    DocState before = d.S();
                    l->mask = 0;
                    d.S().editMask = false;
                    d.SyncGraph();
                    v->history.PushState("Supprimer le masque", before, d.S());
                }
            }
            if (l->kind != LayerKind::Group || !l->passThrough) {
                // Channel mask (Photoshop "Canaux" in the blending options, plus alpha).
                ImGui::TextUnformatted("Canaux");
                static const char* kCh[] = {"R", "V", "B", "A"};
                for (int c = 0; c < 4; ++c) {
                    ImGui::SameLine();
                    bool on = !(l->channelOff & (1 << c));
                    if (ImGui::Checkbox(kCh[c], &on)) {
                        DocState before = d.S();
                        l->channelOff = (u8)(on ? l->channelOff & ~(1 << c) : l->channelOff | (1 << c));
                        d.SyncGraph();
                        v->history.PushState("Canaux du calque", before, d.S());
                    }
                }
                ui::HelpTooltip("Décochez un canal pour que le calque ne le modifie pas (il reste celui des calques du dessous).");
            }
            if (l->HasCustomBlend()) {
                ImGui::SeparatorText(l->blendFx.customCode.empty() ? "Fusion d'effet" : "Fusion personnalisée");
                ImGui::TextDisabled("A = dessous, B = ce calque, masque = masque du calque ; opacité = opacité du calque.");
                if (EffectParamsUI(l->blendFx, "blendfx")) changed = true;
                if (ImGui::SmallButton("Éditer la fusion (code / graphe)")) OpenGraphEditor(1);
                if (ImGui::SmallButton("Revenir au mode Normal")) {
                    DocState before = d.S();
                    l->blendFx = {};
                    l->blend = BlendMode::Normal;
                    d.SyncGraph();
                    v->history.PushState("Mode de fusion", before, d.S());
                }
            }
            if (l->blendIf.Active()) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (!ui::FilterSkip("Options de fusion (Blend If) calque sous-jacent") && ImGui::CollapsingHeader("Options de fusion (Blend If)")) {
                static int bifChannel = 0;
                static const char* kBifNames[] = {"Gris", "Rouge", "Vert", "Bleu"};
                static const ImU32 kBifColors[] = {IM_COL32(255, 255, 255, 255), IM_COL32(255, 60, 60, 255), IM_COL32(60, 230, 90, 255),
                                                   IM_COL32(70, 120, 255, 255)};
                for (int c = 0; c < 4; ++c) {
                    if (c) ImGui::SameLine();
                    // A dot marks channels with active ranges.
                    std::string label = std::string(kBifNames[c]) + (l->blendIf.Active(c) ? " •" : "") + "##bifch" + std::to_string(c);
                    if (ImGui::RadioButton(label.c_str(), bifChannel == c)) bifChannel = c;
                }
                ImGui::TextDisabled("Ce calque");
                if (BlendIfSlider("##bifSelf", l->blendIf.self[bifChannel], kBifColors[bifChannel])) changed = true;
                TrackUndo("Options de fusion");
                ImGui::TextDisabled("Calque sous-jacent");
                if (BlendIfSlider("##bifUnder", l->blendIf.under[bifChannel], kBifColors[bifChannel])) changed = true;
                TrackUndo("Options de fusion");
                ImGui::TextDisabled("Alt + glisser : séparer une poignée");
                if (l->blendIf.Active() && ImGui::SmallButton("Réinitialiser")) {
                    DocState before = d.S();
                    l->blendIf = {};
                    d.SyncGraph();
                    v->history.PushState("Options de fusion", before, d.S());
                }
            }
            ImGui::SeparatorText("Filtres");
            int removeAt = -1;
            for (size_t i = 0; i < l->filters.size(); ++i) {
                EffectInstance& f = l->filters[i];
                ImGui::PushID((int)i);
                auto def = f.Def();
                if (ImGui::Checkbox("##en", &f.enabled)) changed = true;
                TrackUndo("Activer le filtre");
                ImGui::SameLine();
                bool open = ImGui::TreeNodeEx("##f", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", def ? def->name.c_str() : f.effectId.c_str());
                ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 24 * dpi);
                if (ui::IconButton("rm", ui::IconTrash, false, 20 * dpi, "Supprimer le filtre")) removeAt = (int)i;
                if (open) {
                    changed |= EffectParamsUI(f, "f");
                    if (ImGui::SmallButton("Éditer (code / graphe)")) { efEd.filterIndex = (int)i; OpenGraphEditor(0); }
                    if (f.effectId == "camera_raw") {
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Ouvrir dans Photo RAW...")) OpenCameraRaw(*v);
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (removeAt >= 0) {
                DocState before = d.S();
                l->filters.erase(l->filters.begin() + removeAt);
                d.SyncGraph();
                v->history.PushState("Supprimer le filtre", before, d.S());
            }
            if (ImGui::Button("+ Ajouter un filtre")) ImGui::OpenPopup("##addfilter");
            if (ImGui::BeginPopup("##addfilter")) {
                if (ImGui::MenuItem("Filtre HLSL personnalisé")) {
                    DocState before = d.S();
                    EffectInstance fx;
                    fx.customCode = kCustomEffectTemplate;
                    if (auto def = fx.Def()) fx.params = def->DefaultParams();
                    if ((int)l->filters.size() >= kMaxLayerFilters) { Status("Ce calque a déjà " + std::to_string(kMaxLayerFilters) + " filtres (le maximum)"); return; }
                    l->filters.push_back(fx);
                    d.SyncGraph();
                    v->history.PushState("Ajouter un filtre", before, d.S());
                }
                ImGui::Separator();
                EffectMenu([&](const std::string& id) {
                    DocState before = d.S();
                    EffectInstance fx;
                    fx.effectId = id;
                    if (auto def = Effects().Find(id)) fx.params = def->DefaultParams();
                    if ((int)l->filters.size() >= kMaxLayerFilters) { Status("Ce calque a déjà " + std::to_string(kMaxLayerFilters) + " filtres (le maximum)"); return; }
                    l->filters.push_back(fx);
                    d.SyncGraph();
                    v->history.PushState("Ajouter un filtre", before, d.S());
                }, false, true);
                ImGui::EndPopup();
            }
            }  // layer properties
            if (changed) d.SyncGraph();
        }
    } else {
        Node* n = d.S().graph.Find(d.S().activeNode);
        if (!n) {
            ImGui::TextDisabled("Aucun nœud sélectionné.");
            ImGui::TextWrapped("Clic droit dans le graphe pour ajouter des nœuds.");
            GraphVariablesUI(*v, d.S().graph);
        } else {
            ImGui::InputText("Nom", &n->name);
            TrackUndo("Renommer");
            switch (n->kind) {
                case NodeKind::Output: ImGui::TextDisabled("Sortie du document"); break;
                case NodeKind::Surface: ImGui::TextDisabled("Calque de pixels - peignez dessus avec le pinceau"); break;
                case NodeKind::Tool: ToolNodeUI(*v, *n); break;
                case NodeKind::Function: FunctionNodeUI(*v, *n); break;
                case NodeKind::SubInput:
                case NodeKind::SubOutput:
                case NodeKind::Value:
                case NodeKind::Math:
                case NodeKind::VarGet:
                case NodeKind::VarSet:
                case NodeKind::Event:
                case NodeKind::Action: TypedNodeUI(*v, *n); break;
                case NodeKind::Blend: {
                    ImGui::Checkbox("Désactivé (bypass)", &n->bypass);
                    TrackUndo("Bypass");
                    ImGui::TextUnformatted("Canaux");
                    for (int c = 0; c < 4; ++c) {
                        static const char* kCh[] = {"R", "V", "B", "A"};
                        ImGui::SameLine();
                        bool on = !(n->channelOff & (1 << c));
                        if (ImGui::Checkbox(kCh[c], &on)) {
                            DocState before = d.S();
                            n->channelOff = (u8)(on ? n->channelOff & ~(1 << c) : n->channelOff | (1 << c));
                            v->history.PushState("Canaux", before, d.S());
                        }
                    }
                    if (ImGui::BeginCombo("Mode", BlendModeName(n->blend))) {
                        for (int i = 0; i < kBlendMenuCount; ++i) {
                            int m = kBlendMenuOrder[i];
                            if (m < 0) { ImGui::Separator(); continue; }
                            if (ImGui::Selectable(BlendModeName((BlendMode)m), (int)n->blend == m)) {
                                n->blend = (BlendMode)m;
                                v->history.PushState("Mode de fusion", frameBefore, d.S());
                            }
                        }
                        ImGui::EndCombo();
                    }
                    float op = n->opacity * 100.0f;
                    if (ImGui::SliderFloat("Opacité", &op, 0, 100, "%.0f %%")) n->opacity = op / 100.0f;
                    TrackUndo("Opacité");
                    break;
                }
                case NodeKind::Effect: {
                    ImGui::Checkbox("Désactivé (bypass)", &n->bypass);
                    TrackUndo("Bypass");
                    bool custom = !n->fx.customCode.empty();
                    auto def = n->fx.Def();
                    if (def) ImGui::TextDisabled("%s  -  %s", def->category.c_str(), custom ? "HLSL personnalisé" : n->fx.effectId.c_str());
                    ImGui::SeparatorText("Paramètres");
                    int oldInputs = NodeInputCount(*n);
                    EffectParamsUI(n->fx, "p");
                    EffectPinsUI(*v, *n);
                    if (ImGui::Button("Éditer (code / graphe)")) OpenGraphEditor(0);
                    if (NodeInputCount(*n) != oldInputs) n->inputs.resize(NodeInputCount(*n), 0);
                    break;
                }
            }
        }
    }

    ImGui::SeparatorText("Document");
    ImGui::Text("%u x %u px, %s", d.Width(), d.Height(), d.GetPrecision() == Precision::Float32 ? "32 bits flottant" : "16 bits flottant");
    if (ImGui::Checkbox("Fusion en sRGB", &d.blendInGamma)) d.dirty = true;
    ui::HelpTooltip("Mélange les calques en sRGB (comme Photoshop) au lieu de l'espace linéaire : les modes Produit, Superposition... donnent alors les mêmes résultats que Photoshop.");
    ui::HelpTooltip("Activé : les modes de fusion sont calculés en sRGB comme Photoshop.\nDésactivé : fusion physiquement linéaire (rendu, PBR).");
    ImGui::PopItemWidth();
    ImGui::End();
}

// ------------------------------------------------------------------ color & brush

void App::PushRecentColor(float4 c) {
    c.w = 1;
    std::erase_if(recentColors, [&](const float4& r) { return std::abs(r.x - c.x) + std::abs(r.y - c.y) + std::abs(r.z - c.z) < 1.5f / 255.0f; });
    recentColors.insert(recentColors.begin(), c);
    if (recentColors.size() > 24) recentColors.resize(24);
}

// Compact Photoshop-like color panel: fg/bg swatches, saturation/value square +
// hue bar sized to the space available, hex field, RGB/HSV sliders, recent colors.
void App::DrawColor() {
    if (!ImGui::Begin("Couleur", PanelFlag("Couleur"))) { ImGui::End(); return; }
    ui::FilterBox();
    ColorPickerBody();
    ImGui::End();
}

void App::ColorPickerBody() {
    float4& c = editBg ? bg : fg;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImGuiStyle& st = ImGui::GetStyle();
    const float fh = ImGui::GetFrameHeight();

    // HSV kept across frames so the hue survives gray / black colors.
    static float hue = 0, sat = 0, val = 0;
    static float4 last = {-1, -1, -1, -1};
    if (std::abs(last.x - c.x) + std::abs(last.y - c.y) + std::abs(last.z - c.z) > 1e-5f) {
        float h, s, v;
        ImGui::ColorConvertRGBtoHSV(c.x, c.y, c.z, h, s, v);
        if (s > 1e-4f && v > 1e-4f) hue = h;
        if (v > 1e-4f) sat = s;
        val = v;
    }
    bool edited = false, released = false;

    // Header: swatches, swap / default, hex.
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float sw = fh * 1.15f, off = sw * 0.45f;
        ImVec2 bgMin(p.x + off, p.y + off);
        auto swatch = [&](ImVec2 a, const float4& col, bool active) {
            dl->AddRectFilled(a, ImVec2(a.x + sw, a.y + sw), ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, 1)), 3);
            dl->AddRect(a, ImVec2(a.x + sw, a.y + sw), active ? ui::col::Accent : IM_COL32(120, 122, 135, 255), 3, 0, active ? 2.0f : 1.0f);
        };
        swatch(bgMin, bg, editBg);
        swatch(p, fg, !editBg);
        ImGui::SetCursorScreenPos(bgMin);
        if (ImGui::InvisibleButton("##bgsw", ImVec2(sw, sw))) editBg = true;
        ui::HelpTooltip("Arrière-plan");
        ImGui::SetCursorScreenPos(p);
        if (ImGui::InvisibleButton("##fgsw", ImVec2(sw, sw))) editBg = false;
        ui::HelpTooltip("Premier plan");
        ImGui::SetCursorScreenPos(ImVec2(p.x + sw + off + st.ItemSpacing.x * 2, p.y));
        if (ui::IconButton("swap", ui::IconSwap, false, fh, "Permuter (X)")) std::swap(fg, bg);
        ImGui::SameLine();
        if (ImGui::Button("D", ImVec2(fh, fh))) { fg = {0, 0, 0, 1}; bg = {1, 1, 1, 1}; }
        ui::HelpTooltip("Couleurs par défaut (D)");
        ImGui::SameLine();
        ImGui::TextDisabled("#");
        ImGui::SameLine(0, 2);
        char hex[16];
        snprintf(hex, sizeof(hex), "%02X%02X%02X", (int)std::lround(c.x * 255), (int)std::lround(c.y * 255), (int)std::lround(c.z * 255));
        ImGui::SetNextItemWidth(std::min(fh * 3.6f, ImGui::GetContentRegionAvail().x));
        if (ImGui::InputText("##hex", hex, sizeof(hex), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue)) {
            unsigned v = 0;
            if (sscanf_s(hex, "%x", &v) == 1) {
                c = {((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f, 1};
                PushRecentColor(c);
            }
        }
        // Back under the swatches through an item (a bare SetCursorPos must not grow the window).
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + sw + off));
        ImGui::Dummy(ImVec2(0, 0));
    }

    // Square + hue bar sized to fit, keeping room for the sliders and recent colors when possible.
    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float barW = std::round(fh * 0.7f), gap = st.ItemSpacing.x;
    const float sideW = 170 * dpi;  // sliders column when placed beside the square
    // Wide panel: sliders beside the square (square uses the full height); otherwise stacked.
    float sqSide = std::min(avail.y - st.ItemSpacing.y, 420 * dpi);
    bool side = avail.x - sqSide - barW - gap * 3 >= sideW;
    float below = side ? 0 : (fh + st.ItemSpacing.y) * (colorSliderMode == 2 ? 6 : 5) + st.ItemSpacing.y;
    float sq = side ? sqSide : std::min(avail.x - barW - gap, avail.y - below);
    if (!side && sq < 90 * dpi) {  // short panel: the square gets priority over the sliders
        below = 0;
        sq = std::min(avail.x - barW - gap, avail.y - st.ItemSpacing.y);
    }
    sq = std::clamp(sq, 40 * dpi, 420 * dpi);
    bool showSliders = side || below > 0;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    {
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(hue, 1, 1, r, g, b);
        ImU32 hueCol = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1));
        ImVec2 p1(p0.x + sq, p0.y + sq);
        dl->AddRectFilledMultiColor(p0, p1, IM_COL32_WHITE, hueCol, hueCol, IM_COL32_WHITE);
        dl->AddRectFilledMultiColor(p0, p1, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32_BLACK, IM_COL32_BLACK);
        dl->AddRect(p0, p1, ui::col::Border);
        ImGui::InvisibleButton("##sv", ImVec2(sq, sq));
        if (ImGui::IsItemActive()) {
            ImVec2 m = ImGui::GetIO().MousePos;
            sat = std::clamp((m.x - p0.x) / sq, 0.0f, 1.0f);
            val = 1.0f - std::clamp((m.y - p0.y) / sq, 0.0f, 1.0f);
            edited = true;
        }
        if (ImGui::IsItemDeactivated()) released = true;
        ImVec2 mk(p0.x + sat * sq, p0.y + (1 - val) * sq);
        dl->AddCircle(mk, 6.0f * dpi, IM_COL32_BLACK, 0, 2.5f);
        dl->AddCircle(mk, 6.0f * dpi, IM_COL32_WHITE, 0, 1.5f);
        // Hue bar (6 gradient segments).
        ImVec2 h0(p1.x + gap, p0.y);
        for (int i = 0; i < 6; ++i) {
            float ra, ga, ba, rb, gb, bb;
            ImGui::ColorConvertHSVtoRGB(i / 6.0f, 1, 1, ra, ga, ba);
            ImGui::ColorConvertHSVtoRGB((i + 1) / 6.0f, 1, 1, rb, gb, bb);
            ImU32 ca = ImGui::ColorConvertFloat4ToU32(ImVec4(ra, ga, ba, 1)), cb = ImGui::ColorConvertFloat4ToU32(ImVec4(rb, gb, bb, 1));
            dl->AddRectFilledMultiColor(ImVec2(h0.x, h0.y + sq * i / 6), ImVec2(h0.x + barW, h0.y + sq * (i + 1) / 6), ca, ca, cb, cb);
        }
        dl->AddRect(h0, ImVec2(h0.x + barW, h0.y + sq), ui::col::Border);
        ImGui::SetCursorScreenPos(h0);
        ImGui::InvisibleButton("##hue", ImVec2(barW, sq));
        if (ImGui::IsItemActive()) {
            hue = std::clamp((ImGui::GetIO().MousePos.y - h0.y) / sq, 0.0f, 0.9999f);
            edited = true;
        }
        if (ImGui::IsItemDeactivated()) released = true;
        float hy = h0.y + hue * sq;
        dl->AddRect(ImVec2(h0.x - 2, hy - 3), ImVec2(h0.x + barW + 2, hy + 3), IM_COL32_WHITE, 2, 0, 1.5f);
        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + sq));
        ImGui::Dummy(ImVec2(0, 0));
    }
    if (edited) {
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(hue, sat, val, r, g, b);
        c = {r, g, b, 1};
    }

    float colX = p0.x + sq + barW + gap * 3;
    if (side) {
        ImGui::SetCursorScreenPos(ImVec2(colX, p0.y));
        ImGui::BeginGroup();
    }
    if (showSliders) {
        // Sliders: RGB 0-255 or HSV.
        ImGui::SetNextItemWidth(fh * 3.4f);
        ImGui::Combo("##smode", &colorSliderMode, "RVB\0TSV\0CMJN\0Lab\0");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", editBg ? "Arrière-plan" : "Premier plan");
        float w = side ? std::min(ImGui::GetContentRegionAvail().x, 260 * dpi) : std::min(sq + barW + gap, ImGui::GetContentRegionAvail().x);
        ImGui::PushItemWidth(w - ImGui::CalcTextSize("M ").x);
        if (colorSliderMode == 2) {
            // CMYK through the document's CMYK profile (Windows color system).
            static float cmyk[4] = {0, 0, 0, 0};
            static float4 cmykFor = {-1, -1, -1, -1};
            const std::string prof = CmykProfileFor(Active());
            if (std::abs(cmykFor.x - c.x) + std::abs(cmykFor.y - c.y) + std::abs(cmykFor.z - c.z) > 1e-5f) {
                u8 rgb8[3] = {(u8)std::lround(c.x * 255), (u8)std::lround(c.y * 255), (u8)std::lround(c.z * 255)}, k8[4] = {};
                if (!prof.empty() && color::RgbToCmyk(DocProfile(Active()), prof, 1, rgb8, 1, k8)) for (int i = 0; i < 4; ++i) cmyk[i] = k8[i] / 2.55f;
                else {
                    float k = 1 - std::max({c.x, c.y, c.z});
                    cmyk[3] = k * 100;
                    for (int i = 0; i < 3; ++i) cmyk[i] = k < 1 ? (1 - (&c.x)[i] - k) / (1 - k) * 100 : 0;
                }
                cmykFor = c;
            }
            const char* names[] = {"C", "M", "J", "N"};
            bool ch = false;
            for (int i = 0; i < 4; ++i) {
                ch |= ImGui::SliderFloat(names[i], &cmyk[i], 0, 100, "%.0f %%");
                released |= ImGui::IsItemDeactivatedAfterEdit();
            }
            if (ch) {
                u8 k8[4], rgb8[3];
                for (int i = 0; i < 4; ++i) k8[i] = (u8)std::lround(std::clamp(cmyk[i], 0.0f, 100.0f) * 2.55f);
                if (!prof.empty() && color::CmykToRgb(prof, DocProfile(Active()), 1, k8, 1, rgb8)) c = {rgb8[0] / 255.0f, rgb8[1] / 255.0f, rgb8[2] / 255.0f, 1};
                else {
                    float k = cmyk[3] / 100;
                    c = {(1 - cmyk[0] / 100) * (1 - k), (1 - cmyk[1] / 100) * (1 - k), (1 - cmyk[2] / 100) * (1 - k), 1};
                }
                cmykFor = c;
            }
            ImGui::TextDisabled("%s", prof.empty() ? "Sans profil CMJN (formule simple)" : color::ProfileName(prof).c_str());
        } else if (colorSliderMode == 3) {
            float4 lab = color::SrgbToLab({c.x, c.y, c.z, 1});
            bool ch = ImGui::SliderFloat("L", &lab.x, 0, 100, "%.0f");
            released |= ImGui::IsItemDeactivatedAfterEdit();
            ch |= ImGui::SliderFloat("a", &lab.y, -128, 127, "%.0f");
            released |= ImGui::IsItemDeactivatedAfterEdit();
            ch |= ImGui::SliderFloat("b", &lab.z, -128, 127, "%.0f");
            released |= ImGui::IsItemDeactivatedAfterEdit();
            if (ch) {
                float4 s = color::LabToSrgb(lab);
                c = {s.x, s.y, s.z, 1};
            }
        } else if (colorSliderMode == 0) {
            int rgb[3] = {(int)std::lround(c.x * 255), (int)std::lround(c.y * 255), (int)std::lround(c.z * 255)};
            const char* names[] = {"R", "V", "B"};
            for (int i = 0; i < 3; ++i) {
                if (ImGui::SliderInt(names[i], &rgb[i], 0, 255)) c = {rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f, 1};
                if (ImGui::IsItemDeactivatedAfterEdit()) released = true;
            }
        } else {
            float hsv[3] = {hue * 360.0f, sat * 100.0f, val * 100.0f};
            bool ch = ImGui::SliderFloat("T", &hsv[0], 0, 359.9f, "%.0f°");
            released |= ImGui::IsItemDeactivatedAfterEdit();
            ch |= ImGui::SliderFloat("S", &hsv[1], 0, 100, "%.0f %%");
            released |= ImGui::IsItemDeactivatedAfterEdit();
            ch |= ImGui::SliderFloat("V", &hsv[2], 0, 100, "%.0f %%");
            released |= ImGui::IsItemDeactivatedAfterEdit();
            if (ch) {
                hue = std::min(hsv[0] / 360.0f, 0.9999f);
                sat = hsv[1] / 100.0f;
                val = hsv[2] / 100.0f;
                float r, g, b;
                ImGui::ColorConvertHSVtoRGB(hue, sat, val, r, g, b);
                c = {r, g, b, 1};
            }
        }
        ImGui::PopItemWidth();
    }
    if (released) PushRecentColor(c);
    last = c;

    // Recent colors (click to use).
    if (!recentColors.empty() && (side || ImGui::GetContentRegionAvail().y > fh * 0.6f)) {
        float s = std::round(fh * 0.75f);
        float xmax = side ? colX + std::min(ImGui::GetContentRegionAvail().x, 260 * dpi) : ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        if (side) ImGui::Spacing();
        for (size_t i = 0; i < recentColors.size(); ++i) {
            ImVec2 q = ImGui::GetCursorScreenPos();
            if (q.x + s > xmax) break;
            const float4 rc = recentColors[i];
            dl->AddRectFilled(q, ImVec2(q.x + s, q.y + s), ImGui::ColorConvertFloat4ToU32(ImVec4(rc.x, rc.y, rc.z, 1)), 2);
            dl->AddRect(q, ImVec2(q.x + s, q.y + s), ui::col::Border, 2);
            ImGui::PushID((int)i);
            if (ImGui::InvisibleButton("##rc", ImVec2(s, s))) c = rc;
            ui::HelpTooltip("Couleur récente");
            ImGui::PopID();
            ImGui::SameLine(0, 3);
        }
        ImGui::NewLine();
    }
    if (side) ImGui::EndGroup();
}

void App::DrawBrush() {
    if (!ImGui::Begin("Options", PanelFlag("Options"))) { ImGui::End(); return; }
    ui::FilterBox();
    ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.38f);
    DocView* v = Active();

    if (xf.active) {
        ImGui::SeparatorText(xf.moveOnly ? "Déplacement"
                             : xf.warp   ? (xf.warpKind == 1 ? "Marionnette" : xf.warpKind == 2 ? "Déformation de la perspective" : "Déformation")
                                         : "Transformation manuelle");
        if (xf.boundsRb) {
            ImGui::TextDisabled("Calcul des limites...");
        } else if (xf.warp && xf.warpKind) {
            MeshWarpOptions();
        } else if (xf.warp) {
            ImGui::TextDisabled("Glisser un point, une poignée ou la surface.");
            ImGui::TextDisabled("Les coins entraînent leurs poignées.");
            if (ImGui::Button("Réinitialiser la grille")) {
                for (int j = 0; j < 4; ++j)
                    for (int i = 0; i < 4; ++i) xf.grid[j * 4 + i] = {xf.src.x0 + xf.src.Width() * i / 3.0f, xf.src.y0 + xf.src.Height() * j / 3.0f};
                xf.dirty = true;
            }
        } else if (!xf.moveOnly) {
            bool changed = false;
            if (!xf.freeCorners) {
                float wp = xf.w / xf.src.Width() * 100.0f, hp = xf.h / xf.src.Height() * 100.0f;
                if (ImGui::DragFloat("Largeur %", &wp, 0.25f, -1000, 1000, "%.1f")) { xf.w = wp / 100.0f * xf.src.Width(); changed = true; }
                if (ImGui::DragFloat("Hauteur %", &hp, 0.25f, -1000, 1000, "%.1f")) { xf.h = hp / 100.0f * xf.src.Height(); changed = true; }
                float deg = xf.angle * 180.0f / 3.14159265f;
                if (ImGui::DragFloat("Angle", &deg, 0.25f, -360, 360, "%.1f°")) { xf.angle = deg * 3.14159265f / 180.0f; changed = true; }
                if (ImGui::DragFloat2("Centre", &xf.center.x, 1.0f, -100000, 100000, "%.1f")) changed = true;
            } else {
                ImGui::TextDisabled("Mode déformation (coins libres)");
            }
            if (ImGui::Combo("Interpolation", &xf.interp, "Au plus proche\0Bilinéaire\0Bicubique\0")) changed = true;
            if (changed) {
                TransformUpdateCorners();
                xf.dirty = true;
            }
            ImGui::TextDisabled("Coin : échelle  (Maj : proportionnel, Alt : depuis le centre)");
            ImGui::TextDisabled("Ctrl+coin : perspective / déformation");
            ImGui::TextDisabled("Hors du cadre : rotation (Maj : 15°)");
            if (ImGui::Button("Passer en déformation (grille)")) WarpFromCorners();
        }
        if (ImGui::Button("Valider (Entrée)") && xf.view) TransformCommit(*xf.view);
        ImGui::SameLine();
        if (ImGui::Button("Annuler (Échap)") && xf.view) TransformCancel(*xf.view);
    } else if (tool == Tool::Shape) {
        ImGui::SeparatorText("Forme");
        ImGui::Combo("Type", &shapeDefaults.type, "Rectangle\0Ellipse\0Ligne\0Polygone\0Étoile\0Personnalisée\0");
        if (shapeDefaults.type == 5) ShapePicker(shapeDefaults, Active());
        if (shapeDefaults.type == 0) ImGui::SliderFloat("Rayon des coins", &shapeDefaults.radius, 0, 500, "%.0f px");
        if (shapeDefaults.type == 3 || shapeDefaults.type == 4) ImGui::SliderInt("Côtés / branches", &shapeDefaults.sides, 3, 32);
        if (shapeDefaults.type == 4) ImGui::SliderFloat("Creux", &shapeDefaults.radius, 0.05f, 0.95f, "%.2f");
        ImGui::Checkbox("Fond (premier plan)", &shapeDefaults.fillOn);
        ImGui::Checkbox("Contour (arrière-plan)", &shapeDefaults.strokeOn);
        ImGui::SliderFloat("Épaisseur", &shapeDefaults.strokeWidth, 0.5f, 200, "%.1f px", ImGuiSliderFlags_Logarithmic);
        ImGui::TextDisabled("Glisser : tracer (Maj : proportions). Déplacement (V) pour bouger.");
    } else if (tool == Tool::Text) {
        ImGui::SeparatorText("Texte");
        ImGui::TextWrapped("Clic : nouveau calque de texte. Le texte se modifie dans Propriétés ; outil Déplacement pour le placer.");
        ImGui::SliderFloat("Taille", &textDefaults.size, 4, 1000, "%.0f px", ImGuiSliderFlags_Logarithmic);
        ImGui::Combo("Alignement", &textDefaults.align, "Gauche\0Centre\0Droite\0");
        std::string fname = textDefaults.font.empty() ? "segoeui" : Narrow(Utf8Path(textDefaults.font).stem().wstring());
        if (ImGui::BeginCombo("Police", fname.c_str(), ImGuiComboFlags_HeightLarge)) {
            ImGui::InputTextWithHint("##ff", "Filtrer...", fontFilter, sizeof(fontFilter));
            for (auto& f : SystemFonts()) {
                if (fontFilter[0] && f.name.find(fontFilter) == std::string::npos) continue;
                if (ImGui::Selectable(f.name.c_str(), f.path == textDefaults.font)) textDefaults.font = f.path;
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Couleur : premier plan.");
    } else if (tool == Tool::Crop) {
        ImGui::SeparatorText("Recadrage");
        ImGui::TextWrapped("Tracez le cadre (Maj : carré), puis Entrée ou double-clic pour recadrer. Échap : annuler. Le cadre peut dépasser l'image pour agrandir la zone de travail.");
        if (ImGui::Checkbox("Perspective", &cropPersp)) cropActive = cropQuadSet = false;
        ui::HelpTooltip("Recadrage en perspective : tracez le cadre puis placez ses quatre coins sur les bords d'un plan\n"
                        "(façade, document, tableau) ; l'image est redressée en rectangle.");
        if (cropActive && v && cropPersp && cropQuadSet) {
            if (ImGui::Button("Redresser (Entrée)")) ApplyPerspectiveCrop(*v);
            ImGui::SameLine();
            if (ImGui::Button("Annuler")) cropActive = cropQuadSet = false;
        } else if (cropActive && v && !cropPersp) {
            Rect r = CropRect();
            ImGui::Text("%d x %d px", r.Width(), r.Height());
            if (ImGui::Button("Recadrer (Entrée)")) { ApplyCrop(*v, r); cropActive = false; }
            ImGui::SameLine();
            if (ImGui::Button("Annuler")) cropActive = false;
        }
    } else if (tool == Tool::Ruler) {
        RulerOptions(v);
    } else if (tool == Tool::SpotHeal) {
        ImGui::SeparatorText("Correcteur localisé");
        ImGui::SliderFloat("Taille", &spotHeal.size, 2, 400, "%.0f px", ImGuiSliderFlags_Logarithmic);
        ImGui::TextWrapped("Peignez sur un défaut (poussière, bouton, fil, objet) : au relâchement, la zone est reconstruite d'après ce qui l'entoure, sans source à choisir. [ et ] : taille.");
        if (fillJob.active) ImGui::TextDisabled("Correction en cours...");
    } else if (tool == Tool::Move) {
        ImGui::SeparatorText("Déplacement");
        ImGui::TextWrapped("Glissez pour déplacer le calque actif (ou la sélection). Flèches : 1 px, Maj+flèches : 10 px. Ctrl+T : transformation manuelle.");
    } else if (tool == Tool::MagneticLasso) {
        ImGui::SeparatorText("Lasso magnétique");
        ImGui::SliderFloat("Fréquence des points", &magLasso.anchorEvery, 30, 300, "%.0f px");
        ImGui::TextDisabled("Clic : point d'ancrage. Double-clic / Entrée / clic sur le départ : fermer.\nRetour arrière : retirer le dernier point. Échap : annuler.");
    } else if (tool == Tool::RectSelect || tool == Tool::EllipseSelect || tool == Tool::Lasso || tool == Tool::PolyLasso || tool == Tool::Wand) {
        ImGui::SeparatorText("Sélection");
        const char* modes[] = {"Nouvelle", "Ajouter", "Soustraire", "Intersection"};
        for (int i = 0; i < 4; ++i) {
            if (i) ImGui::SameLine();
            if (ImGui::RadioButton(modes[i], selModeOption == i)) selModeOption = i;
        }
        ImGui::TextDisabled("Maj : ajouter, Alt : soustraire, Maj+Alt : intersection");
        ImGui::SliderFloat("Contour progressif", &selFeather, 0, 250, "%.1f px", ImGuiSliderFlags_Logarithmic);
        ImGui::Checkbox("Lissage (anti-crénelage)", &selAntialias);
        if (tool == Tool::Wand) {
            ImGui::SeparatorText("Baguette magique");
            ImGui::SliderFloat("Tolérance", &wandTolerance, 0, 255, "%.0f");
            ImGui::Checkbox("Pixels contigus", &wandContiguous);
            ImGui::Checkbox("Échantillonner tous les calques", &wandSampleMerged);
        }
        if (tool == Tool::PolyLasso) ImGui::TextDisabled("Clic : point. Double-clic / Entrée : fermer. Retour arrière : annuler un point.");
        if (v && v->doc->S().selection) {
            ImGui::Spacing();
            if (ImGui::Button("Désélectionner (Ctrl+D)")) v->renderer.Deselect(*v->doc, v->history);
            ImGui::SameLine();
            if (ImGui::Button("Intervertir")) v->renderer.SelectionFilter(*v->doc, *ctx, v->history, 3, 0, "Intervertir");
        }
        ImGui::Checkbox("Mode masque (Q)", &quickMask);
    } else if (tool == Tool::QuickSelect) {
        ImGui::SeparatorText("Sélection rapide");
        ImGui::SliderFloat("Taille", &qselSize, 2, 500, "%.0f px", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Tolérance", &qselTolerance, 5, 150, "%.0f");
        ImGui::Checkbox("Échantillonner tous les calques", &qselMerged);
        ImGui::TextDisabled("Peignez sur la zone : la sélection s'étend jusqu'aux bords. Alt : soustraire. [ ] : taille.");
        if (v && v->doc->S().selection) {
            if (ImGui::Button("Désélectionner (Ctrl+D)")) v->renderer.Deselect(*v->doc, v->history);
        }
    } else if (tool == Tool::Patch || tool == Tool::ContentMove) {
        ImGui::SeparatorText(tool == Tool::Patch ? "Pièce" : "Déplacement basé sur le contenu");
        ImGui::SliderFloat("Fondu des bords", &patchBlend, 0.05f, 1, "%.2f");
        ImGui::TextDisabled(tool == Tool::Patch ? "Entourez la zone à corriger, puis faites-la glisser\nsur la zone qui doit la remplacer."
                                                : "Entourez l'objet, puis faites-le glisser :\nl'ancienne place est remplie d'après le contenu.");
        if (v && v->doc->S().selection && ImGui::Button("Remplir d'après le contenu (Maj+F5)")) StartContentAwareFill(*v);
    } else if (tool == Tool::RedEye) {
        ImGui::SeparatorText("Yeux rouges");
        ImGui::SliderFloat("Taille de la pupille", &redEyeSize, 4, 300, "%.0f px");
        ImGui::SliderFloat("Obscurcissement", &redEyeDarken, 0, 1, "%.2f");
        ImGui::TextDisabled("Cliquez sur chaque pupille.");
    } else if (tool == Tool::MagicEraser) {
        ImGui::SeparatorText("Gomme magique");
        ImGui::SliderFloat("Tolérance", &wandTolerance, 0, 255, "%.0f");
        ImGui::TextDisabled("Clic : efface les pixels voisins de couleur proche.");
    } else if (tool == Tool::Fluid) {
        ImGui::SeparatorText("Peinture liquide");
        DrawFluidOptions(false);
    } else if (tool == Tool::Gradient) {
        ImGui::SeparatorText("Dégradé");
        ImGui::Combo("Style", &gradientShape, "Linéaire\0" "Radial\0" "Angle\0" "Réfléchi\0" "Losange\0");
        ImGui::SliderFloat("Opacité", &gradientOpacity, 0, 1);
        ImGui::TextDisabled("Premier plan -> arrière-plan. Maj : 45°.");
    } else if (IsPaintTool(tool)) {
        BrushSettings& b = CurrentBrush();
        static const char* titles[] = {"", "", "", "", "", "", "Pinceau", "Gomme", "Tampon de duplication", "Correcteur", "", "",
                                       "Goutte", "Netteté", "Doigt", "Densité -", "Densité +", "Éponge", "Fluidité"};
        ImGui::SeparatorText(tool == Tool::HistoryBrush ? "Forme d'historique" : tool == Tool::BackgroundEraser ? "Gomme d'arrière-plan"
                             : tool == Tool::PatternStamp ? "Tampon de motif"
                             : tool == Tool::Mixer        ? "Pinceau mélangeur"
                             : tool == Tool::ColorReplace ? "Remplacement de couleur"
                             : (int)tool < (int)std::size(titles) ? titles[(int)tool]
                                                          : "Pinceau");
        if (tool == Tool::PatternStamp) {
            if (!patternsScanned) ScanPatterns();
            const char* pn = patternSel < (int)patterns.size() ? patterns[patternSel].name.c_str() : "(aucun)";
            if (ImGui::BeginCombo("Motif", pn)) {
                for (int i = 0; i < (int)patterns.size(); ++i) if (ImGui::Selectable(patterns[i].name.c_str(), i == patternSel)) patternSel = i;
                ImGui::EndCombo();
            }
            ImGui::SliderFloat("Échelle du motif", &patternScale, 0.1f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
            ImGui::TextDisabled("Motif aligné sur le document (panneau Motifs).");
        }
        if (tool == Tool::ColorReplace) {
            ImGui::Combo("Mode##cr", &colorReplaceMode, "Teinte\0Saturation\0Couleur\0Luminosité\0");
            ImGui::SliderFloat("Tolérance##cr", &colorReplaceTolerance, 0.01f, 1, "%.2f");
            ImGui::TextDisabled("Remplace les tons proches de celui sous le centre du pinceau par la couleur de premier plan\n(Couleur : teinte et saturation, la luminosité est gardée).");
        }
        if (tool == Tool::BackgroundEraser) {
            ImGui::SliderFloat("Tolérance##bg", &bgEraserTolerance, 0.01f, 1, "%.2f");
            ImGui::TextDisabled("Efface les couleurs proches de celle sous le centre du pinceau.");
        }
        if (tool == Tool::HistoryBrush) ImGui::TextDisabled("Repeint l'instantané du calque (panneau Historique).");
        if (tool == Tool::Mixer) {
            ImGui::SeparatorText("Pinceau mélangeur");
            ImGui::SliderFloat("Humidité", &mixerSet.wet, 0, 1, "%.2f");
            ui::HelpTooltip("Part de la toile reprise par le pinceau à chaque touche : élevée, les couleurs se mêlent et s'étirent.");
            ImGui::SliderFloat("Charge", &mixerSet.load, 0, 1, "%.2f");
            ui::HelpTooltip("Couleur de premier plan rechargée à chaque touche : faible, le pinceau se vide et ne fait plus qu'étaler.");
            ImGui::SliderFloat("Débit", &mixerSet.flow, 0.01f, 1, "%.2f");
            ImGui::Checkbox("Nettoyer le pinceau après chaque trait", &mixerSet.clean);
            ImGui::TextDisabled("Préréglages :");
            ImGui::SameLine();
            if (ImGui::SmallButton("Sec")) mixerSet = {0.05f, 0.5f, 0.9f, true};
            ImGui::SameLine();
            if (ImGui::SmallButton("Humide")) mixerSet = {0.5f, 0.15f, 0.8f, true};
            ImGui::SameLine();
            if (ImGui::SmallButton("Très humide")) mixerSet = {0.85f, 0.05f, 0.7f, true};
            ImGui::SameLine();
            if (ImGui::SmallButton("Mélange seul")) mixerSet = {0.6f, 0.0f, 0.8f, false};
        }
        if (IsRetouchTool(tool) && tool != Tool::Mixer) {
            if (tool != Tool::ColorReplace)
                ImGui::SliderFloat(tool == Tool::Clone || tool == Tool::Heal ? "Intensité (inutilisée)" : "Intensité", &retouchStrength, 0.01f, 1, "%.2f");
            if (tool == Tool::Dodge || tool == Tool::Burn) ImGui::Combo("Plage", &dodgeRange, "Tons foncés\0Tons moyens\0Tons clairs\0");
            if (tool == Tool::Liquify) {
                ImGui::Combo("Mode", &liquifyMode, "Déformation avant\0Tourbillon horaire\0Tourbillon antihoraire\0Contraction\0Dilatation\0Reconstruction\0");
                ImGui::TextDisabled("Chaque trait se reconstruit depuis l'image d'origine (pas de flou cumulé).");
            }
            if (tool == Tool::Sponge) {
                int m = spongeSaturate ? 1 : 0;
                if (ImGui::Combo("Mode", &m, "Désaturer\0Saturer\0")) spongeSaturate = m == 1;
            }
            if (tool == Tool::Clone || tool == Tool::Heal) {
                ImGui::Checkbox("Aligné", &cloneAligned);
                ImGui::TextDisabled(cloneSourceSet ? "Source définie (Alt+clic pour changer)" : "Alt+clic pour définir la source");
            }
            ImGui::Separator();
        }
        // Brush: stroke preview (white on transparent), tip, its settings - everything in one place.
        auto td = (tool == Tool::Brush || tool == Tool::Eraser) ? TipDef(b) : nullptr;
        BrushStrokePreview(b, td.get(), 84 * dpi);
        if (tool == Tool::Brush || tool == Tool::Eraser) {
            BrushTipPicker(b);
            td = TipDef(b);
            if (v) SymmetryOptions(*v->doc);
        }
        if (td) {
            EffectInstance fx;
            fx.customCode = td->text;
            fx.params = b.tipParams;
            if (EffectParamsUI(fx, "tip")) b.tipParams = fx.params;
            TipTexturesUI(b, *td);
            if (td->airbrush > 0) ImGui::TextDisabled("Aérographe : %.0f touches/s", td->airbrush);
        }
        if (tool == Tool::Brush || tool == Tool::Eraser) {
            if (ImGui::SmallButton(td ? "Éditer le pinceau" : "Nouveau pinceau (code)")) {
                if (!td) {
                    b.tipId.clear();
                    b.tipGraph.clear();
                    b.tipCode = kBrushTemplate;
                    b.tipParams = ParseBrush("edit", b.tipCode)->def->DefaultParams();
                }
                OpenGraphEditor(2);
            }
            ui::HelpTooltip("Ouvre le pinceau dans l'Éditeur d'effets (une copie : la bibliothèque reste intacte).");
            ImGui::SameLine();
            if (ImGui::SmallButton("Nouveau (graphe)")) {
                b.tipId.clear();
                b.tipCode.clear();
                b.tipGraph = EffectGraph::DefaultBrush().Serialize();
                if (auto nd = TipDef(b); nd && nd->def) b.tipParams = nd->def->DefaultParams();
                OpenGraphEditor(2);
            }
        }
        ImGui::SeparatorText("Réglages");
        ImGui::SliderFloat("Taille", &b.size, 1, 2000, "%.0f px", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Dureté", &b.hardness, 0, 1, "%.2f");
        ImGui::SliderFloat("Opacité", &b.opacity, 0, 1, "%.2f");
        ImGui::SliderFloat("Flux", &b.flow, 0.01f, 1, "%.2f");
        ImGui::SliderFloat("Espacement", &b.spacing, 0.01f, 2.0f, "%.2f");
        ImGui::SliderFloat("Lissage", &b.smoothing, 0, 0.95f, "%.2f");
        ImGui::SeparatorText("Tablette");
        ImGui::Checkbox("Pression -> taille", &b.pressureSize);
        ImGui::Checkbox("Pression -> flux", &b.pressureOpacity);
        {
            // Driver API: Windows Ink (default) or Wintab (Wacom-style drivers with Windows Ink disabled).
            int api = (int)platform::GetTabletApi();
            if (ImGui::Combo("Pilote", &api, "Windows Ink\0Wintab\0")) {
                if (!platform::SetTabletApi((platform::TabletApi)api)) Status("Wintab indisponible (wintab32.dll absent) : Windows Ink conservé");
                SaveAiSettings();
            }
            ui::HelpTooltip("Windows Ink : pilote standard de Windows.\nWintab : pilotes Wacom / Huion / XP-Pen avec « Windows Ink » désactivé.");
            if (ImGui::SliderFloat("Courbe de pression", &penPressureGamma, 0.3f, 3.0f, "%.2f", ImGuiSliderFlags_Logarithmic)) {}
            if (ImGui::IsItemDeactivatedAfterEdit()) SaveAiSettings();
            ui::HelpTooltip("< 1 : plus sensible (peu de force suffit), > 1 : plus ferme.");
            // Curve preview.
            ImVec2 p = ImGui::GetCursorScreenPos();
            float w = std::min(ImGui::GetContentRegionAvail().x, 140 * dpi), hh = 50 * dpi;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p, ImVec2(p.x + w, p.y + hh), IM_COL32(20, 20, 24, 255), 3);
            ImVec2 prev(p.x, p.y + hh);
            for (int i = 1; i <= 24; ++i) {
                float x = i / 24.0f;
                ImVec2 q(p.x + x * w, p.y + hh * (1 - PenPressure(x)));
                dl->AddLine(prev, q, ui::col::Accent, 1.5f);
                prev = q;
            }
            if (platform::PenActive()) {
                float x = platform::LastPressure();
                dl->AddCircleFilled(ImVec2(p.x + x * w, p.y + hh * (1 - PenPressure(x))), 3.5f, IM_COL32_WHITE);
            }
            ImGui::Dummy(ImVec2(w, hh));
            ImGui::TextDisabled("%s", platform::PenActive() ? platform::TabletStatus().c_str() : "Souris (aucun stylet détecté)");
        }
    } else if (tool == Tool::Bucket) {
        ImGui::SeparatorText("Pot de peinture");
        ImGui::SliderFloat("Tolérance", &bucketTolerance, 0, 255, "%.0f");
        ImGui::Checkbox("Pixels contigus", &bucketContiguous);
        ImGui::Checkbox("Tous les calques", &bucketSampleMerged);
        ImGui::TextDisabled("Respecte la sélection. Alt+Retour : remplir toute la sélection.");
    } else {
        ImGui::TextDisabled("Aucune option pour cet outil.");
    }
    ImGui::PopItemWidth();
    ImGui::End();
}


}  // namespace sx
