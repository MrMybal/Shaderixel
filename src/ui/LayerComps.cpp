// Layer comps (snapshots of visibility / appearance / smart object placement,
// applied and exported in one go) and artboards (groups clipped to a rectangle
// by their mask, laid side by side on the canvas, exported one image each).
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <filesystem>

namespace sx {

// ------------------------------------------------------------------ layer comps

void App::CaptureComp(Document& d, LayerComp& c) {
    c.entries.clear();
    for (auto& l : d.S().layers) {
        LayerComp::Entry e;
        e.layer = l.id;
        e.visible = l.visible;
        e.opacity = l.opacity;
        e.blend = l.blend;
        for (auto& f : l.filters) e.filterOn.push_back(f.enabled ? 1 : 0);
        for (int k = 0; k < 4; ++k) e.corners[k] = l.smartCorners[k];
        c.entries.push_back(std::move(e));
    }
}

void App::ApplyComp(DocView& v, Id compId, bool undoable) {
    Document& d = *v.doc;
    const LayerComp* c = nullptr;
    for (auto& x : d.S().comps) if (x.id == compId) c = &x;
    if (!c) return;
    std::optional<Edit> e;
    if (undoable) e.emplace(d, v.history, "Composition : " + c->name);
    for (auto& en : c->entries) {
        Layer* l = d.FindLayer(en.layer);
        if (!l) continue;  // deleted since (the comp keeps it, harmless)
        if (c->useVisibility) l->visible = en.visible;
        if (c->useAppearance) {
            l->opacity = en.opacity;
            l->blend = en.blend;
            for (size_t i = 0; i < l->filters.size() && i < en.filterOn.size(); ++i) l->filters[i].enabled = en.filterOn[i] != 0;
        }
        if (c->usePosition && l->kind == LayerKind::Smart)
            for (int k = 0; k < 4; ++k) l->smartCorners[k] = en.corners[k];
    }
    d.S().activeComp = compId;
    d.SyncGraph();
    if (e) e->Commit();
}

void App::DrawLayerCompsPanel() {
    if (!ImGui::Begin("Compositions de calques", PanelFlag("Compositions de calques"))) { ImGui::End(); return; }
    DocView* v = Active();
    if (!v || v->doc->Mode() != DocMode::Layers) {
        ImGui::TextDisabled(v ? "Disponible en mode calques" : "Aucun document");
        ImGui::End();
        return;
    }
    Document& d = *v->doc;
    auto& comps = d.S().comps;
    int cur = -1;
    for (int i = 0; i < (int)comps.size(); ++i) if (comps[i].id == d.S().activeComp) cur = i;
    if (ImGui::Button("Nouvelle")) {
        Edit e(d, v->history, "Nouvelle composition");
        LayerComp c;
        c.id = d.NewId();
        c.name = "Composition " + std::to_string(comps.size() + 1);
        CaptureComp(d, c);
        comps.push_back(std::move(c));
        d.S().activeComp = comps.back().id;
        compRename = comps.back().id;
        e.Commit();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mémorise l'état actuel des calques (visibilité, apparence, position des objets dynamiques).");
    ImGui::SameLine();
    ImGui::BeginDisabled(cur < 0);
    if (ImGui::Button("Mettre à jour") && cur >= 0) {
        Edit e(d, v->history, "Mettre à jour la composition");
        CaptureComp(d, comps[cur]);
        e.Commit();
    }
    ImGui::SameLine();
    if (ImGui::Button("Supprimer") && cur >= 0) {
        Edit e(d, v->history, "Supprimer la composition");
        comps.erase(comps.begin() + cur);
        d.S().activeComp = 0;
        e.Commit();
        cur = -1;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(comps.empty());
    if (ImGui::ArrowButton("##prev", ImGuiDir_Left)) ApplyComp(*v, comps[(cur <= 0 ? (int)comps.size() : cur) - 1].id, true);
    ImGui::SameLine();
    if (ImGui::ArrowButton("##next", ImGuiDir_Right)) ApplyComp(*v, comps[(cur + 1) % comps.size()].id, true);
    ImGui::EndDisabled();
    ImGui::Separator();
    for (int i = 0; i < (int)comps.size(); ++i) {
        LayerComp& c = comps[i];
        ImGui::PushID((int)c.id);
        if (compRename == c.id) {
            ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1);
            std::string n = c.name;
            if (ImGui::InputText("##n", &n, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) || ImGui::IsItemDeactivated()) {
                c.name = n;
                compRename = 0;
            } else {
                c.name = n;
            }
        } else {
            if (ImGui::Selectable(c.name.c_str(), i == cur, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) compRename = c.id;
                else ApplyComp(*v, c.id, true);
            }
            if (ImGui::BeginPopupContextItem()) {
                ImGui::Checkbox("Visibilité", &c.useVisibility);
                ImGui::Checkbox("Apparence (opacité, fusion, filtres)", &c.useAppearance);
                ImGui::Checkbox("Position (objets dynamiques)", &c.usePosition);
                ImGui::SetNextItemWidth(220 * dpi);
                ImGui::InputText("Commentaire", &c.comment);
                if (ImGui::MenuItem("Renommer")) compRename = c.id;
                ImGui::EndPopup();
            }
            if (!c.comment.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", c.comment.c_str());
        }
        ImGui::PopID();
    }
    if (comps.empty()) ImGui::TextDisabled("Aucune composition. « Nouvelle » mémorise\nl'état des calques ; clic : l'appliquer.");
    ImGui::Separator();
    ImGui::BeginDisabled(comps.empty());
    if (ImGui::Button("Exporter les compositions...", ImVec2(-1, 0))) {
        std::string f = platform::SaveFileDialog(hwnd, L"PNG\0*.png\0JPEG\0*.jpg\0", L"png");
        if (!f.empty()) ExportComps(*v, f);
    }
    ImGui::EndDisabled();
    ImGui::End();
}

void App::ExportComps(DocView& v, const std::string& basePath) {
    // One image per comp: <base>_<comp name>.<ext>; the current comp is restored after.
    Document& d = *v.doc;
    auto base = Utf8Path(basePath);
    std::string ext = Narrow(base.extension().wstring());
    if (ext.empty()) ext = ".png";
    std::string stem = Narrow(base.parent_path().wstring()) + "/" + Narrow(base.stem().wstring());
    DocState keep = d.S();
    int n = 0;
    for (auto& c : keep.comps) {
        ApplyComp(v, c.id, false);
        std::string name = c.name;
        for (auto& ch : name) if (strchr("\\/:*?\"<>|", ch)) ch = '_';
        DocView* act = Active();
        if (act != &v) continue;
        ExportImage(stem + "_" + name + ext);
        ++n;
    }
    // Restore visibility / appearance without an undo entry.
    for (auto& l : d.S().layers)
        if (const Layer* k = [&]() -> const Layer* { for (auto& x : keep.layers) if (x.id == l.id) return &x; return nullptr; }()) {
            l.visible = k->visible;
            l.opacity = k->opacity;
            l.blend = k->blend;
            l.filters = k->filters;
            for (int i = 0; i < 4; ++i) l.smartCorners[i] = k->smartCorners[i];
        }
    d.S().activeComp = keep.activeComp;
    d.SyncGraph();
    Status(std::to_string(n) + " composition(s) exportée(s)");
}

// ------------------------------------------------------------------ artboards

void App::WriteRectMask(Document& d, Id surface, const Rect& r) {
    Surface* s = d.GetSurface(surface);
    if (!s) return;
    const u32 W = s->tex->Width(), H = s->tex->Height();
    const auto fmt = s->tex->GetFormat();
    Rect c = r.Clipped((int)W, (int)H);
    if (fmt == gpu::Format::RGBA16F) {
        std::vector<u16> px((size_t)W * H * 4, 0);
        for (int y = c.y0; y < c.y1; ++y)
            for (int x = c.x0; x < c.x1; ++x) for (int k = 0; k < 4; ++k) px[((size_t)y * W + x) * 4 + k] = 0x3C00;  // 1.0 in half
        ctx->Upload(s->tex.get(), px.data(), W * 8);
    } else if (fmt == gpu::Format::RGBA32F) {
        std::vector<float> px((size_t)W * H * 4, 0.0f);
        for (int y = c.y0; y < c.y1; ++y)
            for (int x = c.x0; x < c.x1; ++x) for (int k = 0; k < 4; ++k) px[((size_t)y * W + x) * 4 + k] = 1.0f;
        ctx->Upload(s->tex.get(), px.data(), W * 16);
    } else {
        std::vector<u8> px((size_t)W * H * 4, 0);
        for (int y = c.y0; y < c.y1; ++y)
            for (int x = c.x0; x < c.x1; ++x) for (int k = 0; k < 4; ++k) px[((size_t)y * W + x) * 4 + k] = 255;
        ctx->Upload(s->tex.get(), px.data(), W * 4);
    }
    d.CancelPendingClear(surface);
    d.TouchSurface(surface);
}

void App::NewArtboard(DocView& v) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) { Status("Plans de travail : mode calques uniquement"); return; }
    bool any = false;
    Rect last;
    for (auto& l : d.S().layers)
        if (l.artboard) { any = true; if (l.board.x1 > last.x1) last = l.board; }
    v.history.BeginGroup();
    if (!any) {
        // First artboard: the whole canvas, holding every root layer.
        Edit e(d, v.history, "Plan de travail");
        Layer g;
        g.id = d.NewId();
        g.kind = LayerKind::Group;
        g.name = "Plan de travail 1";
        g.artboard = true;
        g.passThrough = false;
        g.board = {0, 0, (int)d.Width(), (int)d.Height()};
        for (auto& l : d.S().layers) if (l.parent == 0) l.parent = g.id;
        d.S().layers.push_back(g);
        d.S().activeLayer = g.id;
        d.AddMask(g.id);
        d.S().editMask = false;
        Prepare(v);
        WriteRectMask(d, d.FindLayer(g.id)->mask, g.board);
        d.SyncGraph();
        e.Commit();
    } else {
        // Next one on the right, same size, with a white background; the canvas grows.
        const int gap = std::max(40, last.Width() / 10);
        Rect b = {last.x1 + gap, last.y0, last.x1 + gap + last.Width(), last.y1};
        if (b.x1 > (int)d.Width() || b.y1 > (int)d.Height()) {
            u32 nw = (u32)std::max<int>((int)d.Width(), b.x1), nh = (u32)std::max<int>((int)d.Height(), b.y1);
            float m[6] = {1, 0, 0, 0, 1, 0};
            ApplyCanvasOp(v, nw, nh, m, 0, "Taille de la zone de travail");
        }
        Edit e(d, v.history, "Plan de travail");
        int count = 0;
        for (auto& l : d.S().layers) count += l.artboard;
        Layer g;
        g.id = d.NewId();
        g.kind = LayerKind::Group;
        g.name = "Plan de travail " + std::to_string(count + 1);
        g.artboard = true;
        g.passThrough = false;
        g.board = b;
        d.S().layers.push_back(g);
        Layer& bgL = d.AddPixelLayer("Fond");
        Id bgId = bgL.id, bgSurf = bgL.surface;
        if (Layer* bl = d.FindLayer(bgId)) bl->parent = g.id;
        // AddPixelLayer inserts above the active layer: keep the background inside, under the group.
        d.MoveLayerTo(bgId, g.id, true);
        d.AddMask(g.id);
        d.S().editMask = false;
        d.S().activeLayer = g.id;
        Prepare(v);
        WriteRectMask(d, d.FindLayer(g.id)->mask, b);
        // White background on the board.
        Surface* bs = d.GetSurface(bgSurf);
        if (bs) {
            DocState keepSel = d.S();
            (void)keepSel;
            WriteRectMask(d, bgSurf, b);
        }
        d.SyncGraph();
        e.Commit();
    }
    v.history.EndGroup();
    // Existing artboards: their masks were padded with transparency, rewrite them all (cheap, exact).
    for (auto& l : d.S().layers) if (l.artboard && l.mask) WriteRectMask(d, l.mask, l.board);
    v.fitPending = true;
}

void App::Prepare(DocView& v) { v.renderer.Prepare(*v.doc, *ctx); }

void App::DrawArtboards(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    auto toScreen = [&](float x, float y) { return ImVec2(canvasMin.x + v.pan.x + x * v.zoom, canvasMin.y + v.pan.y + y * v.zoom); };
    for (auto& l : v.doc->S().layers) {
        if (!l.artboard) continue;
        ImVec2 a = toScreen((float)l.board.x0, (float)l.board.y0), b = toScreen((float)l.board.x1, (float)l.board.y1);
        bool act = l.id == v.doc->S().activeLayer;
        dl->AddRect(a, b, act ? ui::col::Accent2 : IM_COL32(150, 150, 160, 200), 0, 0, act ? 2.0f : 1.0f);
        dl->AddText(ImVec2(a.x, a.y - ImGui::GetFontSize() - 3), act ? IM_COL32_WHITE : IM_COL32(190, 190, 200, 255), l.name.c_str());
    }
}

void App::ExportArtboards(DocView& v, const std::string& basePath) {
    Document& d = *v.doc;
    gpu::Texture* img = v.renderer.EvaluateComplete(d, *ctx);
    if (!img) return;
    auto base = Utf8Path(basePath);
    std::string ext = Narrow(base.extension().wstring());
    if (ext.empty()) ext = ".png";
    std::string stem = Narrow(base.parent_path().wstring()) + "/" + Narrow(base.stem().wstring());
    auto rb = v.renderer.ExportRGBA8(*ctx, img);
    int n = 0;
    for (auto& l : d.S().layers) {
        if (!l.artboard) continue;
        std::string name = l.name;
        for (auto& ch : name) if (strchr("\\/:*?\"<>|", ch)) ch = '_';
        exports.push_back({stem + "_" + name + ext, rb, l.board.Clipped((int)d.Width(), (int)d.Height())});
        ++n;
    }
    if (!n) Status("Aucun plan de travail (Calque > Nouveau plan de travail)");
}

void App::ArtboardPropertiesUI(DocView& v, Layer& l) {
    Document& d = *v.doc;
    ImGui::SeparatorText("Plan de travail");
    int r[4] = {l.board.x0, l.board.y0, l.board.Width(), l.board.Height()};
    bool changed = ImGui::DragInt2("Position", r, 1, -100000, 100000);
    changed |= ImGui::DragInt2("Taille", r + 2, 1, 1, 16384);
    static const struct { const char* name; int w, h; } presets[] = {
        {"Full HD 1920 x 1080", 1920, 1080}, {"Téléphone 1080 x 1920", 1080, 1920}, {"Carré 1080 x 1080", 1080, 1080},
        {"A4 300 ppp 2480 x 3508", 2480, 3508}, {"Icône 512 x 512", 512, 512}};
    if (ImGui::BeginCombo("Préréglage", "Choisir...")) {
        for (auto& p : presets)
            if (ImGui::Selectable(p.name)) { r[2] = p.w; r[3] = p.h; changed = true; }
        ImGui::EndCombo();
    }
    if (changed) {
        Edit e(d, v.history, "Plan de travail");
        l.board = {r[0], r[1], r[0] + std::max(1, r[2]), r[1] + std::max(1, r[3])};
        e.Commit();
        Layer* lp = d.FindLayer(l.id);
        if (lp && (lp->board.x1 > (int)d.Width() || lp->board.y1 > (int)d.Height())) {
            float m[6] = {1, 0, 0, 0, 1, 0};
            ApplyCanvasOp(v, (u32)std::max<int>((int)d.Width(), lp->board.x1), (u32)std::max<int>((int)d.Height(), lp->board.y1), m, 0,
                          "Taille de la zone de travail");
            for (auto& x : d.S().layers) if (x.artboard && x.mask) WriteRectMask(d, x.mask, x.board);
        } else if (lp && lp->mask) {
            WriteRectMask(d, lp->mask, lp->board);
        }
        d.SyncGraph();
    }
    ImGui::TextDisabled("Le masque du groupe délimite le plan de travail.");
}

}  // namespace sx
