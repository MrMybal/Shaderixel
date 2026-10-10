// Trim sheets: the texture is cut into trims (strips tiling horizontally, and tiles) that meshes
// map their UVs to. Layout (presets, custom heights, from guides), the trims drawn over the canvas
// and edited with the Trim tool (drag an edge, click to pick, double-click to select its pixels),
// tiling preview, making a trim seamless, an ID layer (one color per trim), a 3D preview mesh and
// the layout exported as JSON for engines / modeling tools.
#include "ui/App.h"
#include "ui/Theme.h"
#include "mesh/Mesh.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <cmath>
#include <fstream>
#include <sstream>

namespace sx {

namespace {

ImU32 TrimColor(const Trim& t, int alpha) { return t.tileX || t.tileY ? IM_COL32(60, 210, 240, alpha) : IM_COL32(250, 170, 60, alpha); }

std::vector<int> ParseHeights(const std::string& s) {
    std::vector<int> v;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
        if (tok.empty()) continue;
        v.push_back(std::max(0, atoi(tok.c_str())));
    }
    return v;
}

void SplitTrim(TrimSheet& ts, int idx, int cols, int rows) {
    if (idx < 0 || idx >= (int)ts.trims.size() || cols < 1 || rows < 1) return;
    const Trim src = ts.trims[idx];
    std::vector<Trim> parts;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            Trim t;
            t.name = src.name + " " + std::to_string(r * cols + c + 1);
            t.r = {src.r.x0 + src.r.Width() * c / cols, src.r.y0 + src.r.Height() * r / rows, src.r.x0 + src.r.Width() * (c + 1) / cols,
                   src.r.y0 + src.r.Height() * (r + 1) / rows};
            t.tileX = cols == 1 && src.tileX;
            t.tileY = false;
            parts.push_back(t);
        }
    ts.trims.erase(ts.trims.begin() + idx);
    ts.trims.insert(ts.trims.begin() + idx, parts.begin(), parts.end());
}

// Preview mesh: each trim as panels (strips repeated three times to show the tiling), stacked.
MeshData TrimPreviewMesh(const TrimSheet& ts, u32 W, u32 H) {
    MeshData m;
    m.name = "Aperçu des trims";
    const float S = 2.0f, gap = 0.06f;
    float y = 0;
    auto quad = [&](float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
        const u32 b = (u32)m.vertices.size();
        // v = 0 at the top of the image.
        m.vertices.push_back({{x0, y0, 0}, {0, 0, 1}, {u0, v0}});
        m.vertices.push_back({{x1, y0, 0}, {0, 0, 1}, {u1, v0}});
        m.vertices.push_back({{x1, y1, 0}, {0, 0, 1}, {u1, v1}});
        m.vertices.push_back({{x0, y1, 0}, {0, 0, 1}, {u0, v1}});
        for (u32 k : {0u, 2u, 1u, 0u, 3u, 2u}) m.indices.push_back(b + k);
    };
    for (auto& t : ts.trims) {
        const float hh = (float)t.r.Height() / H * S, ww = (float)t.r.Width() / H * S;
        const float u0 = (float)t.r.x0 / W, u1 = (float)t.r.x1 / W, v0 = (float)t.r.y0 / H, v1 = (float)t.r.y1 / H;
        const int reps = t.tileX ? 3 : 1;
        const float x0 = -ww * reps * 0.5f;
        for (int k = 0; k < reps; ++k) quad(x0 + ww * k, y, x0 + ww * (k + 1), y - hh, u0, v0, u1, v1);
        y -= hh + gap;
    }
    NormalizeMesh(m);
    return m;
}

}  // namespace

void App::TrimSelectPixels(DocView& v, int idx) {
    Document& d = *v.doc;
    auto& ts = d.S().trimSheet;
    if (idx < 0 || idx >= (int)ts.trims.size()) return;
    const Rect r = ts.trims[idx].r;
    v.renderer.SelectShape(d, *ctx, v.history, SelShape::Rect, {(float)r.x0, (float)r.y0, (float)r.x1, (float)r.y1}, SelMode::Replace, false, 0,
                           "Sélectionner : " + ts.trims[idx].name);
}

void App::TrimMakeSeamless(DocView& v, int idx) {
    Document& d = *v.doc;
    auto& ts = d.S().trimSheet;
    if (idx < 0 || idx >= (int)ts.trims.size()) return;
    const Trim& t = ts.trims[idx];
    const Id target = d.PaintTarget();
    if (!d.GetSurface(target)) { Status("Raccord : choisissez un calque de pixels (ou un nœud image) à modifier"); return; }
    EffectInstance fx;
    fx.effectId = "trim_seamless";
    auto def = fx.Def();
    if (!def) { Status("Effet trim_seamless introuvable"); return; }
    fx.params = def->DefaultParams();
    fx.params[0] = {(float)t.r.x0, (float)t.r.y0, (float)t.r.Width(), (float)t.r.Height()};
    fx.params[1].x = t.tileX ? trimUi.seamBlend : 0.0f;
    fx.params[2].x = t.tileY ? trimUi.seamBlend : 0.0f;
    if (!t.tileX && !t.tileY) fx.params[1].x = trimUi.seamBlend;
    if (v.renderer.ApplyEffectToSurface(d, *ctx, v.history, target, fx, "Raccord : " + t.name)) Status(t.name + " : bords raccordés (tuile sans couture)");
}

void App::TrimIdLayer(DocView& v) {
    Document& d = *v.doc;
    auto& ts = d.S().trimSheet;
    if (ts.trims.empty()) return;
    const u32 W = d.Width(), H = d.Height();
    std::vector<u8> px((size_t)W * H * 4, 0);
    for (size_t i = 0; i < ts.trims.size(); ++i) {
        // Distinct colors (golden angle hues), full opacity inside each trim.
        float h = std::fmod(i * 0.61803398875f, 1.0f), s = 0.65f, val = 0.95f;
        float r = std::clamp(std::abs(h * 6 - 3) - 1, 0.0f, 1.0f), g = std::clamp(2 - std::abs(h * 6 - 2), 0.0f, 1.0f), b = std::clamp(2 - std::abs(h * 6 - 4), 0.0f, 1.0f);
        const u8 c[4] = {(u8)(((r - 1) * s + 1) * val * 255), (u8)(((g - 1) * s + 1) * val * 255), (u8)(((b - 1) * s + 1) * val * 255), 255};
        const Rect rc = ts.trims[i].r.Clipped((int)W, (int)H);
        for (int y = rc.y0; y < rc.y1; ++y)
            for (int x = rc.x0; x < rc.x1; ++x) memcpy(&px[((size_t)y * W + x) * 4], c, 4);
    }
    Id surface = 0;
    {
        Edit e(d, v.history, "Calque ID des trims");
        if (d.Mode() == DocMode::Layers) {
            Layer& l = d.AddPixelLayer("ID des trims");
            l.visible = false;  // a helper: shown when needed (masks, select by color)
            surface = l.surface;
            d.SyncGraph();
        } else {
            Node& n = d.AddNode(NodeKind::Surface, {-400, -300});
            n.name = "ID des trims";
            surface = n.surface;
        }
        e.Commit();
    }
    v.renderer.Prepare(d, *ctx);
    d.CancelPendingClear(surface);
    v.renderer.Import(d, *ctx, surface, px.data(), W, H, false, {0, 0});
    Status("Calque « ID des trims » (masqué) : une couleur par trim, pour les masques et la sélection par couleur");
}

bool App::ExportTrimLayout(DocView& v, const std::string& path) {
    Document& d = *v.doc;
    const auto& ts = d.S().trimSheet;
    const float W = (float)d.Width(), H = (float)d.Height();
    Json j = Json::Object();
    j["format"] = "shaderixel-trimsheet";
    j["document"] = d.Name();
    j["width"] = d.Width();
    j["height"] = d.Height();
    j["padding"] = ts.padding;
    Json arr = Json::Array();
    for (auto& t : ts.trims) {
        Json o = Json::Object();
        o["name"] = t.name;
        o["x"] = t.r.x0;
        o["y"] = t.r.y0;
        o["width"] = t.r.Width();
        o["height"] = t.r.Height();
        o["tileX"] = t.tileX;
        o["tileY"] = t.tileY;
        // UVs with the origin at the top-left (DirectX, Unreal) and at the bottom-left (OpenGL, Blender).
        Json uv = Json::Array();
        uv.Push(t.r.x0 / W); uv.Push(t.r.y0 / H); uv.Push(t.r.x1 / W); uv.Push(t.r.y1 / H);
        o["uvTopLeft"] = uv;
        Json uvb = Json::Array();
        uvb.Push(t.r.x0 / W); uvb.Push(1.0f - t.r.y1 / H); uvb.Push(t.r.x1 / W); uvb.Push(1.0f - t.r.y0 / H);
        o["uvBottomLeft"] = uvb;
        arr.Push(o);
    }
    j["trims"] = arr;
    std::ofstream f(Utf8Path(path), std::ios::binary);
    if (!f) return false;
    f << j.Dump(1);
    return true;
}

void App::TrimPreview3D(DocView& v) {
    AttachMesh(v, TrimPreviewMesh(v.doc->S().trimSheet, v.doc->Width(), v.doc->Height()));
    if (bool* open = PanelFlag("Vue 3D")) *open = true;
    Status("Vue 3D : chaque trim en panneaux (les bandes répétées trois fois pour voir le raccord)");
}

void App::TrimApplyLayout(DocView& v, std::vector<Trim> trims, const std::string& label) {
    Document& d = *v.doc;
    DocState before = d.S();
    d.S().trimSheet.trims = std::move(trims);
    d.S().trimSheet.show = true;
    d.S().activeTrim = d.S().trimSheet.trims.empty() ? -1 : 0;
    d.dirty = true;
    v.history.PushState(label, before, d.S());
}

// ------------------------------------------------------------------ canvas

void App::TrimOverlay(DocView& v, ImVec2 canvasMin, ImVec2 avail, ImDrawList* dl) {
    Document& d = *v.doc;
    const auto& ts = d.S().trimSheet;
    if (!ts.show || ts.trims.empty()) return;
    auto S = [&](float x, float y) { return ImVec2(canvasMin.x + v.pan.x + x * v.zoom, canvasMin.y + v.pan.y + y * v.zoom); };
    dl->PushClipRect(canvasMin, ImVec2(canvasMin.x + avail.x, canvasMin.y + avail.y), true);
    const bool editing = tool == Tool::Trim;
    for (int i = 0; i < (int)ts.trims.size(); ++i) {
        const Trim& t = ts.trims[i];
        const ImVec2 a = S((float)t.r.x0, (float)t.r.y0), b = S((float)t.r.x1, (float)t.r.y1);
        const bool active = i == d.S().activeTrim;
        if (active) dl->AddRectFilled(a, b, TrimColor(t, 40));
        dl->AddRect(a, b, TrimColor(t, active ? 255 : editing ? 200 : 120), 0, 0, active ? 2.0f : 1.0f);
        if (b.y - a.y > 14 && b.x - a.x > 60) {
            std::string label = std::to_string(i + 1) + "  " + t.name + "  " + std::to_string(t.r.Height()) + " px" + (t.tileX ? "  <->" : "") + (t.tileY ? "  ^v" : "");
            const ImVec2 ts2 = ImGui::CalcTextSize(label.c_str());
            dl->AddRectFilled(ImVec2(a.x + 3, a.y + 2), ImVec2(a.x + 9 + ts2.x, a.y + 4 + ts2.y), IM_COL32(10, 10, 14, 170), 3);
            dl->AddText(ImVec2(a.x + 6, a.y + 3), TrimColor(t, 255), label.c_str());
        }
    }
    if (editing && trimUi.hoverTrim >= 0 && trimUi.hoverEdge >= 0 && trimUi.hoverTrim < (int)ts.trims.size()) {
        const Rect& r = ts.trims[trimUi.hoverTrim].r;
        ImVec2 p0, p1;
        switch (trimUi.hoverEdge) {
            case 0: p0 = S((float)r.x0, (float)r.y0); p1 = S((float)r.x0, (float)r.y1); break;
            case 1: p0 = S((float)r.x0, (float)r.y0); p1 = S((float)r.x1, (float)r.y0); break;
            case 2: p0 = S((float)r.x1, (float)r.y0); p1 = S((float)r.x1, (float)r.y1); break;
            default: p0 = S((float)r.x0, (float)r.y1); p1 = S((float)r.x1, (float)r.y1); break;
        }
        dl->AddLine(p0, p1, IM_COL32(255, 255, 255, 255), 3.0f);
    }
    dl->PopClipRect();
}

void App::TrimInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    Document& d = *v.doc;
    auto& ts = d.S().trimSheet;
    ImGuiIO& io = ImGui::GetIO();
    const float2 dp = ScreenToDoc(v, canvasMin, io.MousePos);
    const float tol = 6.0f / std::max(v.zoom, 0.01f);
    if (!trimUi.dragging) {
        trimUi.hoverTrim = trimUi.hoverEdge = -1;
        float best = tol;
        for (int i = 0; i < (int)ts.trims.size() && hovered; ++i) {
            const Rect& r = ts.trims[i].r;
            const bool inX = dp.x >= r.x0 - tol && dp.x <= r.x1 + tol, inY = dp.y >= r.y0 - tol && dp.y <= r.y1 + tol;
            const float dists[4] = {inY ? std::abs(dp.x - r.x0) : 1e9f, inX ? std::abs(dp.y - r.y0) : 1e9f, inY ? std::abs(dp.x - r.x1) : 1e9f,
                                    inX ? std::abs(dp.y - r.y1) : 1e9f};
            for (int e = 0; e < 4; ++e)
                if (dists[e] < best) {
                    best = dists[e];
                    trimUi.hoverTrim = i;
                    trimUi.hoverEdge = e;
                }
        }
    }
    if (trimUi.hoverEdge >= 0) ImGui::SetMouseCursor(trimUi.hoverEdge % 2 == 0 ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (trimUi.hoverEdge >= 0) {
            trimUi.dragging = true;
            trimUi.before = d.S();
            d.S().activeTrim = trimUi.hoverTrim;
        } else {
            const int i = TrimAt(ts, dp);
            if (i != d.S().activeTrim) d.S().activeTrim = i;
        }
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && trimUi.hoverEdge < 0) TrimSelectPixels(v, TrimAt(ts, dp));
    if (trimUi.dragging) {
        Trim* t = trimUi.hoverTrim < (int)ts.trims.size() ? &ts.trims[trimUi.hoverTrim] : nullptr;
        if (t) {
            // Snapped to 8 px (Shift: 1 px), kept inside the document; the neighbor sharing the edge follows.
            const float step = io.KeyShift ? 1.0f : 8.0f;
            const int e = trimUi.hoverEdge;
            const bool vertical = e % 2 == 0;
            float val = std::round((vertical ? dp.x : dp.y) / step) * step;
            val = std::clamp(val, 0.0f, (float)(vertical ? d.Width() : d.Height()));
            int* edge = e == 0 ? &t->r.x0 : e == 1 ? &t->r.y0 : e == 2 ? &t->r.x1 : &t->r.y1;
            const int old = *edge;
            const int nv = (int)val;
            const bool ok = vertical ? (e == 0 ? nv < t->r.x1 : nv > t->r.x0) : (e == 1 ? nv < t->r.y1 : nv > t->r.y0);
            if (ok && nv != old) {
                for (int i = 0; i < (int)ts.trims.size(); ++i) {
                    if (i == trimUi.hoverTrim) continue;
                    Rect& o = ts.trims[i].r;
                    if (vertical && o.y0 < t->r.y1 && o.y1 > t->r.y0) {
                        if (e == 2 && o.x0 == old && nv < o.x1) o.x0 = nv;
                        if (e == 0 && o.x1 == old && nv > o.x0) o.x1 = nv;
                    }
                    if (!vertical && o.x0 < t->r.x1 && o.x1 > t->r.x0) {
                        if (e == 3 && o.y0 == old && nv < o.y1) o.y0 = nv;
                        if (e == 1 && o.y1 == old && nv > o.y0) o.y1 = nv;
                    }
                }
                *edge = nv;
                d.dirty = true;
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            trimUi.dragging = false;
            v.history.PushState("Déplacer le bord du trim", trimUi.before, d.S());
        }
    }
}

// ------------------------------------------------------------------ panel

void App::DrawTrimPanel() {
    if (!ImGui::Begin("Trim sheet", PanelFlag("Trim sheet"))) { ImGui::End(); return; }
    PopOutCorner("Trim sheet");
    DocView* v = Active();
    if (!v) { ImGui::TextDisabled("Aucun document"); ImGui::End(); return; }
    Document& d = *v->doc;
    auto& ts = d.S().trimSheet;
    const u32 W = d.Width(), H = d.Height();
    ImGui::PushItemWidth(-90 * ImGui::GetFontSize() / 13.0f);

    // Layout
    ImGui::SeparatorText("Disposition");
    const auto& presets = TrimPresets();
    trimUi.preset = std::clamp(trimUi.preset, 0, (int)presets.size() - 1);
    if (ImGui::BeginCombo("Modèle", presets[trimUi.preset].name)) {
        for (int i = 0; i < (int)presets.size(); ++i)
            if (ImGui::Selectable(presets[i].name, i == trimUi.preset)) trimUi.preset = i;
        ImGui::EndCombo();
    }
    ImGui::InputInt("Marge (px)", &ts.padding);
    ts.padding = std::clamp(ts.padding, 0, 64);
    if (ImGui::Button("Appliquer le modèle")) {
        std::vector<int> hs;
        for (float f : presets[trimUi.preset].fractions) hs.push_back(std::max(1, (int)std::lround(f * H)) - ts.padding);
        auto trims = TrimStrips(W, H, hs, ts.padding);
        if (std::string(presets[trimUi.preset].name).find("tuiles") != std::string::npos && !trims.empty()) {
            TrimSheet tmp;
            tmp.trims = trims;
            tmp.trims.back().name = "Tuile";
            SplitTrim(tmp, (int)tmp.trims.size() - 1, 4, 2);
            trims = tmp.trims;
        }
        TrimApplyLayout(*v, trims, "Disposition des trims");
    }
    ui::HelpTooltip("Remplace les trims par le modèle (hauteurs relatives à la taille du document).");
    ImGui::InputText("Hauteurs (px)", &trimUi.custom);
    ui::HelpTooltip("Hauteurs des bandes de haut en bas, séparées par des virgules ; 0 = le reste. Ex. 256, 128, 128, 64, 64, 0");
    if (ImGui::Button("Créer les bandes")) TrimApplyLayout(*v, TrimStrips(W, H, ParseHeights(trimUi.custom), ts.padding), "Bandes sur mesure");
    ImGui::SameLine();
    ImGui::BeginDisabled(d.S().guidesY.empty());
    if (ImGui::Button("Depuis les repères")) {
        std::vector<float> ys = d.S().guidesY;
        std::sort(ys.begin(), ys.end());
        std::vector<int> hs;
        float prev = 0;
        for (float y : ys)
            if (y > prev && y < H) { hs.push_back((int)std::lround(y - prev)); prev = y; }
        hs.push_back(0);
        TrimApplyLayout(*v, TrimStrips(W, H, hs, 0), "Bandes depuis les repères");
    }
    ImGui::EndDisabled();
    ui::HelpTooltip("Une bande entre chaque repère horizontal.");
    ImGui::Checkbox("Afficher sur le canevas", &ts.show);
    ImGui::SameLine();
    if (ImGui::Button(tool == Tool::Trim ? "Outil trim actif" : "Outil trim")) tool = Tool::Trim;
    ui::HelpTooltip("Glisser un bord : redimensionner (8 px, Maj : 1 px), clic : choisir, double-clic : sélectionner ses pixels.");

    // Trims
    ImGui::SeparatorText("Trims");
    int remove = -1;
    if (ImGui::BeginTable("##trims", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
                          ImVec2(0, std::min(10.0f, (float)ts.trims.size() + 1.5f) * ImGui::GetFrameHeightWithSpacing()))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 22);
        ImGui::TableSetupColumn("Nom");
        ImGui::TableSetupColumn("Taille");
        ImGui::TableSetupColumn("Raccord", ImGuiTableColumnFlags_WidthFixed, 96 * ImGui::GetFontSize() / 13.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 22);
        ImGui::TableHeadersRow();
        for (int i = 0; i < (int)ts.trims.size(); ++i) {
            Trim& t = ts.trims[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::to_string(i + 1).c_str(), i == d.S().activeTrim, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                d.S().activeTrim = i;
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##n", &t.name);
            ImGui::TableNextColumn();
            ImGui::Text("%d x %d", t.r.Width(), t.r.Height());
            ImGui::TableNextColumn();
            ImGui::Checkbox("H##tx", &t.tileX);
            ImGui::SameLine();
            ImGui::Checkbox("V##ty", &t.tileY);
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x")) remove = i;
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (remove >= 0) {
        DocState before = d.S();
        ts.trims.erase(ts.trims.begin() + remove);
        d.S().activeTrim = std::min(d.S().activeTrim, (int)ts.trims.size() - 1);
        v->history.PushState("Supprimer le trim", before, d.S());
    }
    if (ImGui::Button("+ Trim")) {
        DocState before = d.S();
        Trim t;
        t.name = "Trim " + std::to_string(ts.trims.size() + 1);
        int y = 0;
        for (auto& o : ts.trims) y = std::max(y, o.r.y1 + ts.padding);
        t.r = {0, std::min(y, (int)H - 32), (int)W, std::min(y + 64, (int)H)};
        ts.trims.push_back(t);
        d.S().activeTrim = (int)ts.trims.size() - 1;
        v->history.PushState("Nouveau trim", before, d.S());
    }
    ImGui::SameLine();
    if (ImGui::Button("Tout effacer") && !ts.trims.empty()) TrimApplyLayout(*v, {}, "Effacer les trims");

    const int a = d.S().activeTrim;
    if (a >= 0 && a < (int)ts.trims.size()) {
        Trim& t = ts.trims[a];
        ImGui::SeparatorText(("Trim " + std::to_string(a + 1) + " : " + t.name).c_str());
        int r4[4] = {t.r.x0, t.r.y0, t.r.Width(), t.r.Height()};
        if (ImGui::InputInt4("X, Y, L, H", r4)) {
            t.r = Rect{r4[0], r4[1], r4[0] + std::max(r4[2], 1), r4[1] + std::max(r4[3], 1)}.Clipped((int)W, (int)H);
            d.dirty = true;
        }
        if (ImGui::Button("Sélectionner ses pixels")) TrimSelectPixels(*v, a);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::InputInt("##cols", &trimUi.splitCols, 0);
        trimUi.splitCols = std::clamp(trimUi.splitCols, 2, 32);
        ImGui::SameLine();
        if (ImGui::Button("Découper en colonnes")) {
            DocState before = d.S();
            SplitTrim(ts, a, trimUi.splitCols, 1);
            v->history.PushState("Découper le trim", before, d.S());
        }
        ui::HelpTooltip("Coupe le trim en N tuiles (sans raccord horizontal).");
        ImGui::SliderFloat("Fondu du raccord", &trimUi.seamBlend, 0.05f, 0.5f, "%.2f");
        if (ImGui::Button("Rendre raccordable (calque actif)")) TrimMakeSeamless(*v, a);
        ui::HelpTooltip("Mélange les bords du trim avec son contenu décalé d'une demi-largeur : il se répète sans couture (sur le calque de pixels actif).");

        // Tiling preview: the trim repeated, shifted by the offset.
        if (v->lastImage && t.r.Width() > 0 && t.r.Height() > 0) {
            ImGui::TextDisabled("Aperçu de répétition");
            ImGui::SliderFloat("Décalage", &trimUi.previewOffset, 0.0f, 1.0f, "%.2f");
            const float aw = ImGui::GetContentRegionAvail().x;
            const float tw = aw / 2.5f, th = std::clamp(tw * t.r.Height() / t.r.Width(), 8.0f, 160.0f);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImTextureID tex = (ImTextureID)gpu::Dev().UiTextureId(v->lastImage);
            const ImVec2 uv0((float)t.r.x0 / W, (float)t.r.y0 / H), uv1((float)t.r.x1 / W, (float)t.r.y1 / H);
            dl->PushClipRect(p, ImVec2(p.x + aw, p.y + th), true);
            dl->AddRectFilled(p, ImVec2(p.x + aw, p.y + th), IM_COL32(30, 30, 34, 255));
            for (int k = -1; k < 4; ++k) {
                const float x = p.x + (k - trimUi.previewOffset) * tw;
                dl->AddImage(tex, ImVec2(x, p.y), ImVec2(x + tw, p.y + th), uv0, uv1);
            }
            for (int k = 0; k < 4; ++k) {
                const float x = p.x + (k - trimUi.previewOffset) * tw;
                dl->AddLine(ImVec2(x, p.y), ImVec2(x, p.y + 6), IM_COL32(255, 80, 80, 200), 1.0f);
            }
            dl->PopClipRect();
            ImGui::Dummy(ImVec2(aw, th));
        }
    }

    // Outputs
    ImGui::SeparatorText("Sorties");
    ImGui::BeginDisabled(ts.trims.empty());
    if (ImGui::Button("Calque ID des trims")) TrimIdLayer(*v);
    ui::HelpTooltip("Nouveau calque : une couleur unie par trim (masques, sélection par couleur, export pour les outils de modélisation).");
    ImGui::SameLine();
    if (ImGui::Button("Aperçu 3D")) TrimPreview3D(*v);
    if (ImGui::Button("Exporter la disposition (JSON)...")) {
        std::string path = platform::SaveFileDialog(hwnd, L"Disposition de trim sheet (*.json)\0*.json\0", L"json");
        if (!path.empty()) Status(ExportTrimLayout(*v, path) ? "Disposition exportée : " + path : "Impossible d'écrire " + path);
    }
    ui::HelpTooltip("Noms, rectangles en pixels et UV (origine en haut à gauche et en bas à gauche) de chaque trim.");
    ImGui::EndDisabled();
    ImGui::PopItemWidth();
    ImGui::End();
}

}  // namespace sx
