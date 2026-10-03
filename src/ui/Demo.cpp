// Scripted demo scene used for automated visual checks (--demo [nodes]).
#include "ui/App.h"
#include "gpu/ShaderSystem.h"
#include "doc/EffectGraph.h"
#include "doc/Brushes.h"
#include <imgui_internal.h>

#include <fstream>
#include <stb_image_write.h>

#include <cmath>

namespace sx {

namespace {
// Compare two RGBA8 readbacks: max channel difference and pixels differing by more than `tol`.
void CompareReadbacks(const char* what, const gpu::Readback& a, const gpu::Readback& b, int tol) {
    if (a.width != b.width || a.height != b.height) { Log("[tiles] %s: size mismatch", what); return; }
    int maxd = 0;
    size_t bad = 0;
    int bx = -1, by = -1;
    for (size_t i = 0; i < a.data.size(); ++i) {
        int dd = std::abs((int)a.data[i] - (int)b.data[i]);
        if (dd > maxd) { maxd = dd; bx = (int)((i / 4) % a.width); by = (int)((i / 4) / a.width); }
        if (dd > tol) ++bad;
    }
    Log("[compare] %s: max diff %d at (%d,%d), %zu channels > %d", what, maxd, bx, by, bad, tol);
}
}  // namespace

void App::DemoTick() {
    DocView* v = Active();
    if (demoMode == 15 && v) {
        // Tiled vs full rendering must match (halo correctness, no seams), including after an incremental stroke.
        static gpu::ReadbackPtr rb[4];
        Document& d = *v->doc;
        auto grab = [&](int i, bool tiledMode) {
            d.forceTiled = tiledMode;
            rb[i] = v->renderer.ExportRGBA8(*ctx, v->renderer.EvaluateComplete(d, *ctx));
        };
        if (frameCounter == 8) grab(0, false);
        if (frameCounter == 12) grab(1, true);
        if (frameCounter == 16 && rb[0] && rb[1] && rb[0]->ready && rb[1]->ready) {
            CompareReadbacks("full vs tiled", *rb[0], *rb[1], 1);
            std::string pre = std::string("cmp_") + gpu::BackendName();
            stbi_write_png((pre + "_full.png").c_str(), rb[0]->width, rb[0]->height, 4, rb[0]->data.data(), rb[0]->width * 4);
            stbi_write_png((pre + "_tiled.png").c_str(), rb[1]->width, rb[1]->height, 4, rb[1]->data.data(), rb[1]->width * 4);
            Hasher h;
            h.Bytes(rb[0]->data.data(), rb[0]->data.size());
            Log("[tiles] image checksum %016llx (sparse memory %llu KB)", (unsigned long long)h.h, (unsigned long long)(gpu::Dev().SparseBytes() >> 10));
        }
        if (frameCounter == 18) {
            // Small stroke while tiled: only the touched tiles (+ halo) are recomputed.
            for (auto& l : d.S().layers) if (l.name == "Peinture") d.S().activeLayer = l.id;
            Id target = d.PaintTarget();
            if (target && v->renderer.BeginStroke(d, *ctx, target)) {
                std::vector<Dab> dabs;
                for (int i = 0; i < 60; ++i) dabs.push_back({500.0f + i * 4, 480.0f + std::sin(i * 0.2f) * 30, 14, 1});
                v->renderer.StrokeDabs(d, *ctx, dabs, brush, LinearColor({0.1f, 1, 0.3f, 1}), false);
                v->renderer.EndStroke(d, *ctx, v->history, "Pinceau");
            }
        }
        if (frameCounter == 20) Log("[tiles] tiles pending after stroke: %d / %d", v->renderer.TilesPending(), v->renderer.TilesTotal());
        if (frameCounter == 22) grab(2, true);
        if (frameCounter == 26) grab(3, false);
        if (frameCounter == 30 && rb[2] && rb[3] && rb[2]->ready && rb[3]->ready) {
            CompareReadbacks("incremental tiled vs full", *rb[2], *rb[3], 1);
            Hasher h;
            h.Bytes(rb[3]->data.data(), rb[3]->data.size());
            Log("[tiles] checksum after stroke %016llx", (unsigned long long)h.h);
        }
        if (frameCounter == 34) Undo();
        if (frameCounter == 37) grab(0, false);
        if (frameCounter == 41 && rb[0] && rb[0]->ready) {
            Hasher h;
            h.Bytes(rb[0]->data.data(), rb[0]->data.size());
            Log("[tiles] checksum after undo %016llx", (unsigned long long)h.h);
        }
        if (frameCounter == 42) d.forceTiled = true;
        return;
    }
    if (demoMode == 26 && v) {
        // Edit > Stroke: outside (white), center (yellow), inside (red) on a new layer.
        Document& d = *v->doc;
        if (frameCounter == 6) {
            d.AddPixelLayer("Contours");
            v->renderer.SelectShape(d, *ctx, v->history, SelShape::Ellipse, {500, 250, 1100, 750}, SelMode::Replace, true, 0, "Ellipse");
        }
        if (frameCounter == 9) v->renderer.StrokeSelection(d, *ctx, v->history, d.PaintTarget(), 14, 2, LinearColor({1, 1, 1, 1}), 1);
        if (frameCounter == 10) v->renderer.StrokeSelection(d, *ctx, v->history, d.PaintTarget(), 8, 1, LinearColor({1, 0.9f, 0.1f, 1}), 1);
        if (frameCounter == 11) v->renderer.StrokeSelection(d, *ctx, v->history, d.PaintTarget(), 10, 0, LinearColor({0.9f, 0.1f, 0.15f, 1}), 1);
        return;
    }
    if (demoMode == 25 && v) {
        // Guides, grid and snapping.
        Document& d = *v->doc;
        if (frameCounter == 6) {
            DocState before = d.S();
            d.S().guidesX = {400, 800, 1200};
            d.S().guidesY = {250, 500};
            v->history.PushState("Repères", before, d.S());
            showGrid = true;
            gridStep = 200;
            gridSubdiv = 4;
            float2 a = Snap(*v, {403.5f, 497.0f}), b = Snap(*v, {651.0f, 333.0f}), c = Snap(*v, {1597.0f, 2.0f});
            Log("[guides] snap (403.5,497)->(%.1f,%.1f)  (651,333)->(%.1f,%.1f)  (1597,2)->(%.1f,%.1f)", a.x, a.y, b.x, b.y, c.x, c.y);
            v->renderer.SelectShape(d, *ctx, v->history, SelShape::Rect, {a.x, a.y, 1200, 750}, SelMode::Replace, true, 0, "Rectangle");
        }
        if (frameCounter == 10) SaveDocument(*v, "demo_guides.sxl");
        if (frameCounter == 18) LoadSxl("demo_guides.sxl");
        if (frameCounter == 24 && Active()) Log("[guides] reloaded %zu vertical, %zu horizontal", Active()->doc->S().guidesX.size(), Active()->doc->S().guidesY.size());
        return;
    }
    if (demoMode == 24 && v) {
        // Merging must not change the image (normal over normal is associative), undo restores it.
        static gpu::ReadbackPtr rb[6];
        Document& d = *v->doc;
        auto grab = [&](int i) { rb[i] = v->renderer.ExportRGBA8(*ctx, v->renderer.EvaluateComplete(d, *ctx)); };
        auto cmp = [&](const char* what, int a, int b) {
            if (rb[a] && rb[b] && rb[a]->ready && rb[b]->ready) CompareReadbacks(what, *rb[a], *rb[b], 1);
        };
        if (frameCounter == 6) {
            Layer& top = d.AddPixelLayer("Haut");
            top.opacity = 0.6f;
            Id surf = top.surface, id = top.id;
            v->renderer.Prepare(d, *ctx);
            if (v->renderer.BeginStroke(d, *ctx, surf)) {
                std::vector<Dab> dabs;
                for (int i = 0; i < 200; ++i) dabs.push_back({300.0f + i * 5, 300.0f + std::sin(i * 0.05f) * 200, 60, 1});
                v->renderer.StrokeDabs(d, *ctx, dabs, brush, LinearColor({0.2f, 1, 0.4f, 1}), false);
                v->renderer.EndStroke(d, *ctx, v->history, "Pinceau");
            }
            d.AddMask(id);
            v->renderer.Prepare(d, *ctx);
            v->renderer.ApplyGradient(d, *ctx, v->history, d.FindLayer(id)->mask, {0, 0}, {1600, 0}, 0, {1, 1, 1, 1}, {0, 0, 0, 1}, 1.0f, "Masque");
            d.S().editMask = false;
            // Put it right above "Peinture" so it merges onto a normal pixel layer.
            for (auto& l : d.S().layers) if (l.name == "Peinture") d.MoveLayerTo(id, l.id, false);
            d.S().activeLayer = id;
            d.SyncGraph();
        }
        if (frameCounter == 10) grab(0);
        if (frameCounter == 12) MergeDown(*v);
        if (frameCounter == 16) grab(1);
        if (frameCounter == 18) MergeVisible(*v, false);
        if (frameCounter == 22) grab(2);
        if (frameCounter == 24) { Undo(); Undo(); }
        if (frameCounter == 28) grab(3);
        if (frameCounter == 30) MergeVisible(*v, true);
        if (frameCounter == 34) grab(4);
        if (frameCounter == 38) {
            cmp("merge down", 0, 1);
            cmp("merge visible", 0, 2);
            cmp("undo x2", 0, 3);
            cmp("flatten", 0, 4);
            Log("[merge] layers now %zu", d.S().layers.size());
        }
        return;
    }
    if (demoMode == 23 && v) {
        // Pen paths: a heart made of Bézier curves, filled, stroked, made into a selection, saved and reloaded.
        Document& d = *v->doc;
        tool = Tool::Pen;
        if (frameCounter == 6) {
            VectorPath p;
            p.id = d.NewId();
            p.name = "Cœur";
            SubPath sp;
            sp.closed = true;
            float2 c = {800, 520};
            sp.pts.push_back({c + float2{0, 260}, c + float2{0, 260}, c + float2{0, 260}});                        // bottom tip (corner)
            sp.pts.push_back({c + float2{-260, -60}, c + float2{-200, 120}, c + float2{-320, -230}});            // left lobe
            sp.pts.push_back({c + float2{0, -150}, c + float2{-90, -330}, c + float2{90, -330}});                // top notch
            sp.pts.push_back({c + float2{260, -60}, c + float2{320, -230}, c + float2{200, 120}});               // right lobe
            p.subpaths.push_back(sp);
            DocState before = d.S();
            d.S().paths.push_back(p);
            d.S().activePath = p.id;
            v->history.PushState("Plume", before, d.S());
        }
        if (frameCounter == 8) {
            Layer& l = d.AddPixelLayer("Remplissage du tracé");
            (void)l;
            fg = {0.95f, 0.25f, 0.4f, 1};
        }
        if (frameCounter == 10) FillPath(*v);
        if (frameCounter == 12) {
            fg = {1, 1, 1, 1};
            brush.size = 14;
            brush.hardness = 0.9f;
            StrokePath(*v);
        }
        if (frameCounter == 14) PathToSelection(*v, SelMode::Replace);
        if (frameCounter == 16) SaveDocument(*v, "demo_pen.sxl");
        if (frameCounter == 24) LoadSxl("demo_pen.sxl");
        if (frameCounter == 30 && Active()) {
            Document& r = *Active()->doc;
            size_t anchors = 0;
            for (auto& p : r.S().paths) for (auto& sp : p.subpaths) anchors += sp.pts.size();
            Log("[pen] reloaded %zu path(s), %zu anchors, active %u, selection %u", r.S().paths.size(), anchors, r.S().activePath, r.S().selection);
        }
        return;
    }
    if (demoMode == 22 && v) {
        // Quick selection: "paint" along the middle (orange) stroke, then along part of the top one (add).
        Document& d = *v->doc;
        tool = Tool::QuickSelect;
        auto stroke = [&](float y0, float amp, float xa, float xb, SelMode mode) {
            qsel.rb = v->renderer.ExportRGBA8(*ctx, v->renderer.EvaluateComplete(d, *ctx));
            qsel.view = v;
            qsel.pts.clear();
            for (float x = xa; x <= xb; x += 15) {
                float t = (x - 120) / 1360.0f;
                qsel.pts.push_back({x, y0 + std::sin(t * 9.0f) * amp});
            }
            qsel.mode = mode;
            qsel.pending = true;
        };
        qselSize = 30;
        if (frameCounter == 6) stroke(500, 90, 300, 1300, SelMode::Replace);
        if (frameCounter == 14) stroke(300, 120, 400, 800, SelMode::Add);
        return;
    }
    if (demoMode == 21 && v) {
        // Selection outline move: ellipse, then offset (drag result) and undo/redo.
        Document& d = *v->doc;
        if (frameCounter == 6) v->renderer.SelectShape(d, *ctx, v->history, SelShape::Ellipse, {200, 200, 700, 600}, SelMode::Replace, true, 0, "Ellipse");
        if (frameCounter == 8) v->renderer.OffsetSelection(d, *ctx, v->history, {450, 120}, "Déplacer la sélection");
        if (frameCounter == 10) Undo();
        if (frameCounter == 12) Redo();
        if (frameCounter == 14) Log("[selmove] history %zu, selection %u", v->history.Cursor(), d.S().selection);
        // Transform Selection: rotate and widen the moved ellipse, then commit.
        if (frameCounter == 16) TransformSelectionBegin(*v);
        if (frameCounter == 22 && xf.active && !xf.boundsRb) {
            xf.angle = 0.5f;
            xf.w *= 1.4f;
            TransformUpdateCorners();
            xf.dirty = true;
        }
        if (frameCounter == 26) TransformCommit(*v);
        if (frameCounter == 28) Log("[selmove] after transform: history %zu, selection %u", v->history.Cursor(), d.S().selection);
        tool = Tool::EllipseSelect;
        return;
    }
    if (demoMode == 20 && v) {
        // .sxl round trip with an embedded LUT.
        static gpu::ReadbackPtr a, b;
        if (frameCounter == 6) ImGui::SetWindowFocus("Propriétés");
        if (frameCounter == 8) a = v->renderer.ExportRGBA8(*ctx, v->renderer.EvaluateComplete(*v->doc, *ctx));
        if (frameCounter == 10) SaveDocument(*v, "demo_lut.sxl");
        if (frameCounter == 20) LoadSxl("demo_lut.sxl");
        if (frameCounter == 30) b = Active()->renderer.ExportRGBA8(*ctx, Active()->renderer.EvaluateComplete(*Active()->doc, *ctx));
        if (frameCounter == 34 && a && b && a->ready && b->ready) {
            CompareReadbacks("LUT before save vs after load", *a, *b, 0);
            Hasher h;
            h.Bytes(a->data.data(), a->data.size());
            Log("[lut] image checksum %016llx", (unsigned long long)h.h);
        }
        return;
    }
    if (demoMode == 42 && frameCounter == 8 && !extraWindows.empty()) {
        // Half outside the main window: becomes its own OS window (viewport).
        ImGuiViewport* mv = ImGui::GetMainViewport();
        ImGui::SetWindowPos("Fenêtre 2", ImVec2(mv->Pos.x + mv->Size.x - 300, mv->Pos.y + 200));
        ImGui::DockBuilderDockWindow("Histogramme", ImHashStr("Fenêtre 2##dock"));
    }
    if (demoMode == 42 && GetEnvironmentVariableA("SX_LAYOUT", nullptr, 0) > 0) {
        // Saved layout round trip: save on tab 3 with the graph hidden, change, load back.
        if (frameCounter == 6) { ShowSideTab(3); hideBottom = true; }
        if (frameCounter == 8) SaveLayout("_test");
        if (frameCounter == 9) { ShowSideTab(0); hideBottom = false; }
        if (frameCounter == 11) LoadLayout("_test");
        if (frameCounter == 14) {
            Log("[demo] layout reloaded: tab %d, hideBottom %d", sideTab, hideBottom ? 1 : 0);
            DeleteLayout("_test");
        }
    }
    if (demoMode == 42 && frameCounter == 14) {
        Log("[demo] viewports %d, Fenêtre 2 viewport %s", ImGui::GetPlatformIO().Viewports.Size,
            ImGui::FindWindowByName("Fenêtre 2") && ImGui::FindWindowByName("Fenêtre 2")->Viewport != ImGui::GetMainViewport() ? "own" : "main");
    }
    if (demoMode == 42 && frameCounter == 5) {
        // Workspace: side tab, hidden zones, extra window (env SX_TAB, SX_HIDE, SX_EXTRA).
        char b[16] = "";
        if (GetEnvironmentVariableA("SX_TAB", b, sizeof(b)) > 0) ShowSideTab(atoi(b));
        if (GetEnvironmentVariableA("SX_HIDE", b, sizeof(b)) > 0) { hideBottom = strchr(b, 'b') != nullptr; hideRight = strchr(b, 'r') != nullptr; }
        if (GetEnvironmentVariableA("SX_EXTRA", b, sizeof(b)) > 0) NewExtraWindow();
        char closeList[256] = "";
        if (GetEnvironmentVariableA("SX_CLOSE", closeList, sizeof(closeList)) > 0) {
            // Comma-separated panels closed (like their tab cross).
            std::string l = closeList;
            for (size_t p0 = 0; p0 <= l.size();) {
                size_t e = l.find(',', p0);
                if (e == std::string::npos) e = l.size();
                if (e > p0) panelOpen[l.substr(p0, e - p0)].open = false;
                p0 = e + 1;
            }
        }
        if (GetEnvironmentVariableA("SX_COLORPOP", b, sizeof(b)) > 0) colorPopupRequest = true;
        return;
    }
    if (demoMode == 45 && frameCounter == 4) {
        // Substance-like material painting: PBR channels, three material layers whose
        // shared masks are painted in UV space, 3D view next to the texture.
        NewDocument(1024, 1024, Precision::Half16, DocMode::Layers, 0, "Matériaux");
        DocView& m = *Active();
        Document& d = *m.doc;
        AttachMesh(m, MakeSphere());
        CreatePbrChannels(m);
        m.renderer.Prepare(d, *ctx);
        auto paintMask = [&](int preset, auto&& path) {
            AddMaterialLayer(m, preset);
            m.renderer.Prepare(d, *ctx);
            Id target = d.PaintTarget();
            if (!target || !m.renderer.BeginStroke(d, *ctx, target)) { Log("[demo] material %d: no mask target", preset); return; }
            std::vector<Dab> dabs;
            path(dabs);
            BrushSettings b = brush;
            b.hardness = 0.6f;
            m.renderer.StrokeDabs(d, *ctx, dabs, b, PaintColor(d), false);
            m.renderer.EndStroke(d, *ctx, m.history, "Peinture du masque");
        };
        // Base: red paint everywhere.
        AddMaterialLayer(m, 0);
        m.renderer.Prepare(d, *ctx);  // creates the (black) mask before filling it
        if (Layer* l = d.ActiveLayer(); l && l->mask)
            m.renderer.FillSelection(d, *ctx, &m.history, l->mask, 0, {1, 1, 1, 1}, 1.0f, "Remplir le masque", nullptr, nullptr, false);
        // Gold band around the equator.
        paintMask(4, [](std::vector<Dab>& dabs) {
            for (float x = 0; x <= 1024; x += 6) dabs.push_back({x, 512 + std::sin(x * 0.03f) * 40, 60, 1});
        });
        // Rust patches.
        paintMask(8, [](std::vector<Dab>& dabs) {
            for (int i = 0; i < 160; ++i) {
                float a = i * 2.399963f, r = 30 + i * 2.6f;
                dabs.push_back({512 + std::cos(a) * r * 1.6f, 260 + std::sin(a) * r * 0.5f, 18.0f + (i % 7) * 4.0f, 0.8f});
            }
        });
        Log("[demo] materials: %zu", MaterialIds(d).size());
        paint3DLayoutRequest = true;
        return;
    }
    if (demoMode == 44 && v) {
        // Effect drag & drop on the layer graph, injected through ImGui (never the real mouse):
        // on the "Lueur" layer node -> filter; on empty space -> node mode + new node.
        Document& d = *v->doc;
        ImGuiIO& io = ImGui::GetIO();
        ImGuiWindow* gw = ImGui::FindWindowByName("Graphe");
        auto screenOf = [&](float2 p) {
            ImVec2 o = gw->InnerRect.Min;
            return ImVec2(o.x + (p.x + nodeEd.pan.x) * nodeEd.zoom, o.y + (p.y + nodeEd.pan.y) * nodeEd.zoom);
        };
        static ImVec2 target;
        static const char* fx = "invert";
        auto drag = [&](int f0) {
            int f = (int)frameCounter - f0;
            if (f == 0) io.AddMousePosEvent(target.x - 60, target.y - 60);
            if (f == 1) io.AddMouseButtonEvent(0, true);
            if (f >= 1 && f <= 6) {
                if (f >= 2) io.AddMousePosEvent(target.x - 60 + f * 10, target.y - 60 + f * 10);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern)) {
                    ImGui::SetDragDropPayload(kEffectPayload, fx, strlen(fx) + 1);
                    ImGui::EndDragDropSource();
                }
            }
            if (f == 6) io.AddMouseButtonEvent(0, false);
        };
        if (frameCounter == 5 && gw) {
            nodeEd.zoom = 0.6f;
            for (auto& n : d.S().graph.nodes)
                if (n.layer && n.name == "Lueur" && n.kind == NodeKind::Surface) {
                    nodeEd.pan = float2{200, 120} - n.pos;
                    target = screenOf(n.pos + float2{60, 40});
                }
        }
        if (frameCounter >= 6 && frameCounter <= 12) drag(6);
        if (frameCounter == 15) {
            for (auto& l : d.S().layers)
                if (l.name == "Lueur") Log("[demo] dnd on layer node: Lueur has %zu filters (last %s), mode %s", l.filters.size(),
                                           l.filters.empty() ? "-" : l.filters.back().effectId.c_str(), d.Mode() == DocMode::Layers ? "calques" : "nodal");
            target = ImVec2(gw->InnerRect.Max.x - 80, gw->InnerRect.Max.y - 60);
            fx = "posterize";
        }
        if (frameCounter >= 16 && frameCounter <= 22) drag(16);
        if (frameCounter == 25) {
            int found = 0;
            for (auto& n : d.S().graph.nodes) if (n.kind == NodeKind::Effect && n.fx.effectId == "posterize") ++found;
            Log("[demo] dnd on empty space: mode %s, posterize nodes %d", d.Mode() == DocMode::Layers ? "calques" : "nodal", found);
        }
        return;
    }
    if (demoMode == 43 && v && frameCounter == 4) {
        // Graphs: a brush built in nodes (scatter, random size, follow the stroke) painted
        // on a new layer, and a blend built in nodes on the glow layer.
        Document& d = *v->doc;
        EffectGraph bg = EffectGraph::DefaultBrush();
        bg.scatter = 0.8f;
        bg.sizeJitter = 0.6f;
        bg.follow = 1.0f;
        bg.spacing = 0.35f;
        // Square-ish tip: roundtip on a scaled local position, tinted by a random hue.
        for (auto& n : bg.nodes)
            if (n.op == "roundtip") n.inputs[1] = {};  // hardness from the default (brush hardness)
        brush.tipId.clear();
        brush.tipCode.clear();
        brush.tipGraph = bg.Serialize();
        auto td = TipDef(brush);
        if (!td) { Log("[demo] brush graph failed"); return; }
        brush.tipParams = td->def ? td->def->DefaultParams() : std::vector<float4>{};
        std::string err;
        for (int w = 0; w < 6000 && !v->renderer.BrushPipeline(*td, &err) && err.empty(); ++w) Sleep(10);
        if (!err.empty()) Log("[demo] brush graph: %s", err.c_str());
        Edit e(d, v->history, "Nouveau calque");
        Id surf = d.AddPixelLayer("Pinceau en graphe").surface;
        e.Commit();
        v->renderer.Prepare(d, *ctx);
        BrushSettings b = brush;
        b.size = 50;
        b.spacing = bg.spacing;
        std::vector<Dab> dabs;
        std::vector<float4> extra;
        float prevX = 150, prevY = 500, dist = 0;
        for (float x = 150; x < 1450; x += b.size * b.spacing) {
            float t = (x - 150) / 1300.0f, y = 500 + std::sin(t * 9.0f) * 150;
            float ang = std::atan2(y - prevY, std::max(x - prevX, 0.001f));
            dist += std::hypot(x - prevX, y - prevY);
            dabs.push_back({x, y, b.size * 0.5f, 1.0f});
            extra.push_back({1.0f, ang, t, dist});
            prevX = x;
            prevY = y;
        }
        EffectInstance tip;
        tip.params = brush.tipParams;
        if (v->renderer.BeginStroke(d, *ctx, surf)) {
            if (!v->renderer.StrokeProgram(d, *ctx, dabs, extra, *td, tip, b, LinearColor({1, 0.85f, 0.2f, 1}), false, 0)) Log("[demo] brush graph not ready");
            v->renderer.EndStroke(d, *ctx, v->history, "Pinceau");
        }
        for (auto& l : d.S().layers)
            if (l.name == "Lueur") {
                l.blendFx = {};
                l.blendFx.graph = EffectGraph::DefaultBlend().Serialize();
                if (auto def = l.blendFx.Def()) l.blendFx.params = def->DefaultParams();
                d.S().activeLayer = l.id;
            }
        d.SyncGraph();
        tool = Tool::Brush;
        char lib[8] = "";
        if (GetEnvironmentVariableA("SX_LIBVIEW", lib, sizeof(lib)) > 0) {
            // Library items in the editor: protected, edited as copies.
            SelectTip(brush, "charcoal");
            for (auto& l : d.S().layers)
                if (l.name == "Lueur") {
                    l.blendFx = {};
                    l.blendFx.effectId = "cy_blend_multiply";
                    if (auto def = l.blendFx.Def()) l.blendFx.params = def->DefaultParams();
                    if (GetEnvironmentVariableA("SX_GRAPHCOPY", nullptr, 0) > 0)
                        if (auto def = l.blendFx.Def(); def && !def->graph.empty()) {
                            // "Modifier une copie (graphe)": the imported code in a custom node with its pins wired.
                            l.blendFx.graph = def->graph;
                            l.blendFx.effectId.clear();
                        }
                }
            d.SyncGraph();
        }
        char tgt[8] = "1";
        GetEnvironmentVariableA("SX_TARGET", tgt, sizeof(tgt));
        OpenGraphEditor(atoi(tgt));
        efEd.zoom = 0.8f;
        return;
    }
    if (demoMode == 41 && v && frameCounter == 4) {
        // Programmable brushes: one stroke per brush on a new layer (direction,
        // pressure and time extras like a real stroke), then the star brush selected.
        Document& d = *v->doc;
        Edit e(d, v->history, "Nouveau calque");
        Id surf = d.AddPixelLayer("Pinceaux programmables").surface;
        e.Commit();
        v->renderer.Prepare(d, *ctx);
        const char* ids[] = {"calligraphy", "star_scatter", "charcoal", "airbrush", "cy_bristle", "cy_watercolor", "cy_grass", "cy_chain", "cy_sparkle"};
        float4 colors[] = {{1, 0.95f, 0.85f, 1}, {1, 0.8f, 0.2f, 1}, {0.05f, 0.05f, 0.08f, 1}, {0.3f, 0.9f, 1, 1}, {0.9f, 0.3f, 0.2f, 1},
                           {0.2f, 0.4f, 1, 1}, {0.3f, 0.85f, 0.25f, 1}, {0.85f, 0.85f, 0.9f, 1}, {1, 1, 0.7f, 1}};
        int row = 0;
        for (const char* id : ids) {
            auto td = Brushes().Find(id);
            if (!td) { Log("[demo] brush %s missing", id); continue; }
            BrushSettings b = brush;
            b.size = std::string(id) == "star_scatter" ? 40.0f : 56.0f;
            b.hardness = 0.7f;
            SelectTip(b, id);
            if (std::string(id) == "calligraphy") b.tipParams[2].x = 1.0f;  // follow the stroke
            EffectInstance tip;
            tip.params = b.tipParams;
            std::string err;
            for (int w = 0; w < 6000 && !v->renderer.BrushPipeline(*td, &err) && err.empty(); ++w) Sleep(10);
            if (!err.empty()) Log("[demo] brush %s: %s", id, err.c_str());
            float y0 = 90.0f + row * 100.0f;
            float sp = std::max(1.0f, b.size * std::max(b.spacing, 0.02f));
            std::vector<Dab> dabs;
            std::vector<float4> extra;
            float prevX = 120, prevY = y0, dist = 0;
            for (float x = 120; x < 1480; x += sp) {
                float t = (x - 120) / 1360.0f;
                float y = y0 + std::sin(t * 12.0f) * 30.0f;
                float pr = 0.3f + 0.7f * std::sin(t * 3.14159f);
                float ang = std::atan2(y - prevY, std::max(x - prevX, 0.001f));
                dist += std::sqrt((x - prevX) * (x - prevX) + (y - prevY) * (y - prevY));
                dabs.push_back({x, y, b.size * 0.5f * (0.4f + 0.6f * pr), pr});
                extra.push_back({pr, ang, t * 2.0f, dist});
                prevX = x;
                prevY = y;
            }
            if (v->renderer.BeginStroke(d, *ctx, surf)) {
                for (size_t i = 0; i < dabs.size(); i += 256) {
                    size_t n = std::min<size_t>(256, dabs.size() - i);
                    std::vector<Dab> part(dabs.begin() + i, dabs.begin() + i + n);
                    std::vector<float4> ex(extra.begin() + i, extra.begin() + i + n);
                    if (!v->renderer.StrokeProgram(d, *ctx, part, ex, *td, tip, b, LinearColor(colors[row]), false, (u32)i))
                        Log("[demo] brush %s not ready", id);
                }
                v->renderer.EndStroke(d, *ctx, v->history, "Pinceau");
            }
            ++row;
        }
        tool = Tool::Brush;
        SelectTip(brush, "star_scatter");
        return;
    }
    if (demoMode == 40 && v && frameCounter == 4) {
        // Imported CyTexture presets in the effects gallery (previews on the paint layer).
        for (auto& l : v->doc->S().layers) if (l.name == "Peinture") v->doc->S().activeLayer = l.id;
        char cat[64] = "CyTexture Effets";
        GetEnvironmentVariableA("SX_FX_CATEGORY", cat, sizeof(cat));
        fxBrowser.category = cat;
        char search[64] = "";
        if (GetEnvironmentVariableA("SX_FX_SEARCH", search, sizeof(search)) > 0) fxBrowser.search = search;
        fxBrowser.thumb = 96;
        fxBrowser.source = 1;
        return;
    }
    if (demoMode == 39 && v) {
        // Graph channel links: Combine RGBA fed with R, G, B, A of a node gives that
        // node back (full and tiled rendering).
        Document& d = *v->doc;
        static gpu::ReadbackPtr rb[4];
        static Id src = 0, comb = 0;
        auto grab = [&] { return v->renderer.ExportRGBA8(*ctx, v->renderer.EvaluateComplete(d, *ctx)); };
        Graph& g = d.S().graph;
        if (frameCounter == 4) {
            Edit e(d, v->history, "Nœuds");
            d.ConvertToNodes();
            e.Commit();
            src = g.Find(g.output) ? g.Find(g.output)->inputs[0] : 0;
            float2 mn{1e9f, 1e9f};
            for (auto& n : g.nodes) mn = {std::min(mn.x, n.pos.x), std::min(mn.y, n.pos.y)};
            nodeEd.zoom = 0.3f;
            nodeEd.pan = float2{40, 120} - mn;
        }
        if (frameCounter == 8) rb[0] = grab();
        if (frameCounter == 10 && src) {
            Node* out = g.Find(g.output);
            float2 p = out->pos + float2{-30, 260};
            Id split = d.AddNode(NodeKind::Split, p + float2{-260, 0}).id;
            d.Link(src, split, 0);
            comb = d.AddEffectNode("combine_rgba", "", p).id;
            for (int c = 0; c < 4; ++c) d.Link(split, comb, c, (u8)(c + 1));
            d.Link(comb, g.output, 0);
            nodeEd.selection = {comb};
            nodeEd.zoom = 0.85f;
            nodeEd.pan = float2{760, 30} - p;  // Split / Combine in view
        }
        if (frameCounter == 40) rb[1] = grab();
        if (frameCounter == 42) d.forceTiled = true;
        if (frameCounter == 70) rb[2] = grab();
        if (frameCounter == 80 && rb[2] && rb[2]->ready) {
            CompareReadbacks("node vs Combine(R,G,B,A of node)", *rb[0], *rb[1], 1);
            CompareReadbacks("full vs tiled channel links", *rb[1], *rb[2], 1);
            d.forceTiled = false;
        }
        return;
    }
    if (demoMode == 38 && v) {
        // Channel mask: all channels off == layer hidden. Custom blend (template =
        // multiply) == standard Multiply in linear blending.
        Document& d = *v->doc;
        static gpu::ReadbackPtr rb[4];
        auto grab = [&] { return v->renderer.ExportRGBA8(*ctx, v->renderer.EvaluateComplete(d, *ctx)); };
        Layer* glow = nullptr;
        for (auto& l : d.S().layers) if (l.name == "Lueur") glow = &l;
        if (!glow) return;
        if (frameCounter == 4) { d.blendInGamma = false; glow->visible = false; d.SyncGraph(); }
        if (frameCounter == 6) rb[0] = grab();
        if (frameCounter == 8) { glow->visible = true; glow->channelOff = 15; d.SyncGraph(); }
        if (frameCounter == 10) rb[1] = grab();
        if (frameCounter == 12) { glow->channelOff = 0; glow->blend = BlendMode::Multiply; d.SyncGraph(); }
        if (frameCounter == 14) rb[2] = grab();
        if (frameCounter == 16) {
            glow->blendFx.customCode = kCustomBlendTemplate;
            if (auto def = glow->blendFx.Def()) glow->blendFx.params = def->DefaultParams();
            d.S().activeLayer = glow->id;
            d.SyncGraph();
        }
        if (frameCounter == 40) rb[3] = grab();
        if (frameCounter == 42) {
            glow->blendFx = {};
            glow->blendFx.effectId = "cy_blend_multiply";
            if (auto def = glow->blendFx.Def()) glow->blendFx.params = def->DefaultParams();
            d.SyncGraph();
        }
        static gpu::ReadbackPtr cy;
        if (frameCounter == 70) cy = grab();
        if (frameCounter == 80 && rb[3] && rb[3]->ready && cy && cy->ready) {
            CompareReadbacks("hidden vs all channels off", *rb[0], *rb[1], 0);
            CompareReadbacks("Multiply vs custom blend template", *rb[2], *rb[3], 2);
            CompareReadbacks("Multiply vs CyTexture Multiply blend", *rb[2], *cy, 2);
        }
        return;
    }
    if (demoMode == 37 && v && frameCounter == 4) {
        // Custom HLSL adjustment layer with two inputs-free params, shown in Propriétés.
        Document& d = *v->doc;
        Edit e(d, v->history, "Calque HLSL");
        Layer& l = d.AddEffectLayer(LayerKind::Adjustment, "");
        l.name = "HLSL personnalisé";
        l.effect.customCode = kCustomEffectTemplate;
        if (auto def = l.effect.Def()) l.effect.params = def->DefaultParams();
        d.SyncGraph();
        e.Commit();
        return;
    }
    if (demoMode == 36 && v) {
        // Effects gallery + layer preview; destructive apply (and its undo); a moved
        // node keeps its place when the layer graph is rebuilt.
        Document& d = *v->doc;
        static gpu::ReadbackPtr orig, applied, undone;
        auto grab = [&] { return v->renderer.ExportRGBA8(*ctx, v->renderer.EvaluateComplete(d, *ctx)); };
        if (frameCounter == 4) {
            for (auto& l : d.S().layers) if (l.name == "Peinture") d.S().activeLayer = l.id;
            fxBrowser.selected = "invert";
        }
        if (frameCounter == 6) orig = grab();
        if (frameCounter == 8) ApplyLibraryEffect(*v, "invert", FxApply::Destructive);
        if (frameCounter == 10) applied = grab();
        if (frameCounter == 12) Undo();
        if (frameCounter == 14) undone = grab();
        if (frameCounter == 16) Redo();
        if (frameCounter == 18) {
            Id moved = 0;
            for (auto& n : d.S().graph.nodes) if (n.kind == NodeKind::Surface && n.layer == d.S().activeLayer) moved = n.id;
            d.S().layerNodePos[moved] = {-300, 900};
            d.SyncGraph();
            const Node* n = d.S().graph.Find(moved);
            Log("[fxbrowser] moved node %u at %.0f,%.0f after rebuild", moved, n ? n->pos.x : 0.0f, n ? n->pos.y : 0.0f);
        }
        if (frameCounter == 24 && orig && applied && undone && orig->ready && applied->ready && undone->ready) {
            CompareReadbacks("original vs invert applied", *orig, *applied, 1);
            CompareReadbacks("original vs undo", *orig, *undone, 0);
        }
        return;
    }
    if (demoMode == 35) {
        // Relative sliders/drags, driven through ImGui's input queue only (the real
        // cursor and keyboard are never touched): 100 px plain, with Shift, with Ctrl.
        static float slider = 50, drag = 0;
        ImGui::SetNextWindowPos(ImVec2(100, 100));
        ImGui::SetNextWindowSize(ImVec2(460, 120));
        ImGui::Begin("##slidertest", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetNextItemWidth(400);
        ImGui::SliderFloat("##s", &slider, 0, 100);
        ImVec2 sp = ImGui::GetItemRectMin();
        ImGui::SetNextItemWidth(400);
        ImGui::DragFloat("##d", &drag, 1.0f);
        ImVec2 dp = ImGui::GetItemRectMin();
        ImGui::End();
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDoubleClickTime = 0.0f;  // successive scripted clicks are not double-clicks
        static float before = 0;
        auto stroke = [&](int f0, ImVec2 at, ImGuiKey mod, float* value, const char* what) {
            int f = (int)frameCounter - f0;
            if (f == 0) io.AddMousePosEvent(at.x + 200, at.y + 8);
            if (f == 1) before = *value;
            // Ctrl at click time is ImGui's text entry: it is pressed once the drag has started (as a user would).
            if (mod != ImGuiKey_None && f == (mod == ImGuiMod_Ctrl ? 3 : 1)) io.AddKeyEvent(mod, true);
            if (f == 2) io.AddMouseButtonEvent(0, true);
            if (f >= 3 && f < 13) io.AddMousePosEvent(at.x + 200 + (f - 2) * 10.0f, at.y + 8);
            if (f == 14) io.AddMouseButtonEvent(0, false);
            if (f == 15) {
                if (mod != ImGuiKey_None) io.AddKeyEvent(mod, false);
                Log("[sliders] %s: %.2f -> %.2f (%+.2f for 100 px)", what, before, *value, *value - before);
            }
        };
        if (frameCounter >= 10 && frameCounter < 30) stroke(10, sp, ImGuiKey_None, &slider, "slider");
        if (frameCounter >= 30 && frameCounter < 50) { if (frameCounter == 30) slider = 10; stroke(30, sp, ImGuiMod_Shift, &slider, "slider + Maj"); }
        if (frameCounter >= 50 && frameCounter < 70) stroke(50, sp, ImGuiMod_Ctrl, &slider, "slider + Ctrl");
        if (frameCounter >= 70 && frameCounter < 90) stroke(70, dp, ImGuiKey_None, &drag, "drag");
        if (frameCounter >= 90 && frameCounter < 110) stroke(90, dp, ImGuiMod_Shift, &drag, "drag + Maj");
        if (frameCounter >= 110 && frameCounter < 130) stroke(110, dp, ImGuiMod_Ctrl, &drag, "drag + Ctrl");
        return;
    }
    if (demoMode == 34 && v) {
        // ComfyUI (run tools/mock_comfyui.py or a real server; SX_COMFY_URL overrides the endpoint):
        // image-to-image from the current composite, result imported as a layer.
        static size_t layersBefore = 0;
        if (frameCounter == 5) {
            aiSettings.imageProvider = "comfyui";
            char url[512] = {};
            if (GetEnvironmentVariableA("SX_COMFY_URL", url, sizeof(url)) > 0) aiSettings.comfyEndpoint = url;
            aiSettings.comfyWorkflow.clear();
            genPrompt = "aquarelle, vagues lumineuses";
            genUseImage = true;
            genNewDoc = false;
            layersBefore = v->doc->S().layers.size();
            StartImageGeneration();
        }
        if (frameCounter > 5 && frameCounter % 10 == 0 && !genBusy) {
            static bool logged = false;
            if (!logged) {
                logged = true;
                Log("[comfy] layers %zu -> %zu, active \"%s\"", layersBefore, v->doc->S().layers.size(),
                    v->doc->ActiveLayer() ? v->doc->ActiveLayer()->name.c_str() : "-");
            }
        }
        return;
    }
    if (demoMode == 33 && v) {
        // Warp: bulge the paint strokes, commit, then undo back to the original.
        Document& d = *v->doc;
        static gpu::ReadbackPtr orig, warpedRb, undone;
        auto grab = [&] { return v->renderer.ExportRGBA8(*ctx, v->renderer.EvaluateComplete(d, *ctx)); };
        if (frameCounter == 5) {
            for (auto& l : d.S().layers) if (l.name == "Peinture") d.S().activeLayer = l.id;
            orig = grab();
        }
        if (frameCounter == 6) WarpBegin(*v);
        if (frameCounter == 10 && xf.warp) {
            for (int k : {5, 6}) xf.grid[k] = xf.grid[k] + float2{0, -260};
            for (int k : {9, 10}) xf.grid[k] = xf.grid[k] + float2{0, 160};
            xf.grid[15] = xf.grid[15] + float2{80, 60};
            xf.dirty = true;
        }
        if (frameCounter == 30) TransformCommit(*v);
        if (frameCounter == 32) warpedRb = grab();
        if (frameCounter == 34) Undo();
        if (frameCounter == 37) undone = grab();
        if (frameCounter == 42 && orig && warpedRb && undone && orig->ready && warpedRb->ready && undone->ready) {
            CompareReadbacks("original vs warped", *orig, *warpedRb, 1);
            CompareReadbacks("original vs undo", *orig, *undone, 0);
            Log("[warp] history: %s", v->history.Count() ? "ok" : "empty");
        }
        return;
    }
    if (demoMode == 32 && v) {
        // Selection -> path: ellipse with a rectangular hole plus a separate disc,
        // traced into a path, turned back into a selection and compared.
        Document& d = *v->doc;
        static gpu::ReadbackPtr before, after;
        tool = Tool::Pen;
        if (frameCounter == 6) v->renderer.SelectShape(d, *ctx, v->history, SelShape::Ellipse, {250, 200, 950, 800}, SelMode::Replace, true, 0, "Ellipse");
        if (frameCounter == 7) v->renderer.SelectShape(d, *ctx, v->history, SelShape::Rect, {450, 420, 750, 580}, SelMode::Subtract, true, 0, "Rectangle");
        if (frameCounter == 8) v->renderer.SelectShape(d, *ctx, v->history, SelShape::Ellipse, {1050, 300, 1400, 650}, SelMode::Add, true, 0, "Disque");
        if (frameCounter == 10) before = ctx->ReadbackTexture(d.Selection()->tex.get());
        if (frameCounter == 12) MakePathFromSelection(*v);
        if (frameCounter == 20) {
            if (VectorPath* p = ActivePath(d)) {
                size_t n = 0;
                for (auto& sp : p->subpaths) n += sp.pts.size();
                Log("[selpath] %zu subpaths, %zu anchors", p->subpaths.size(), n);
            }
            PathToSelection(*v, SelMode::Replace);
        }
        if (frameCounter == 24) after = ctx->ReadbackTexture(d.Selection()->tex.get());
        if (frameCounter == 30 && before && after && before->ready && after->ready) {
            size_t diff = 0, inside = 0;
            for (size_t i = 0; i < before->data.size(); ++i) {
                inside += before->data[i] > 127;
                diff += (before->data[i] > 127) != (after->data[i] > 127);
            }
            Log("[selpath] selection pixels %zu, differing after round trip %zu (%.3f%%)", inside, diff, 100.0 * diff / std::max<size_t>(inside, 1));
        }
        if (frameCounter == 32) ExportPsd("demo_paths.psd", 8);
        return;
    }
    if (demoMode == 31 && v) {
        // PSD 16-bit and PSB exports, re-imported and compared with the original composite.
        static gpu::ReadbackPtr orig, back[2];
        static int stage = 0;
        static u64 at = 0;
        static DocView* source = nullptr;
        if (frameCounter == 8) {
            source = v;
            orig = v->renderer.ExportRGBA8(*ctx, v->renderer.EvaluateComplete(*v->doc, *ctx));
            ExportPsd("demo_export16.psd", 16);
        }
        if (stage == 0 && frameCounter > 9 && !psdExport) {
            ExportPsd("demo_export.psb", 8);
            stage = 1;
        }
        if (stage == 1 && !psdExport) {
            std::error_code ec;
            Log("[psd16] sizes: 16-bit %llu, psb %llu", (unsigned long long)std::filesystem::file_size("demo_export16.psd", ec),
                (unsigned long long)std::filesystem::file_size("demo_export.psb", ec));
            OpenImage("demo_export16.psd", false);
            OpenImage("demo_export.psb", false);
            stage = 2;
            at = frameCounter;
        }
        if (stage == 2 && frameCounter == at + 10 && docs.size() >= 3) {
            for (int k = 0; k < 2; ++k) {
                DocView& dv = *docs[docs.size() - 2 + k];
                back[k] = dv.renderer.ExportRGBA8(*ctx, dv.renderer.EvaluateComplete(*dv.doc, *ctx));
            }
            stage = 3;
        }
        if (stage == 3 && back[0] && back[1] && back[0]->ready && back[1]->ready && orig && orig->ready) {
            CompareReadbacks("original vs PSD 16-bit", *orig, *back[0], 3);
            CompareReadbacks("original vs PSB", *orig, *back[1], 3);
            stage = 4;
        }
        return;
    }
    if (demoMode == 17) {
        // PSD round trip: export with layers, re-import, compare composites.
        static gpu::ReadbackPtr orig, back;
        static int stage = 0;
        static u64 at = 0;
        if (frameCounter == 8 && v) {
            orig = v->renderer.ExportRGBA8(*ctx, v->renderer.EvaluateComplete(*v->doc, *ctx));
            ExportPsd("demo_export.psd");
        }
        if (stage == 0 && frameCounter > 9 && !psdExport) {
            std::error_code ec;
            Log("[psd] file size %llu bytes", (unsigned long long)std::filesystem::file_size("demo_export.psd", ec));
            OpenImage("demo_export.psd", false);
            stage = 1;
            at = frameCounter;
        }
        if (stage == 1 && frameCounter == at + 8 && Active()) {
            Log("[psd] reimported %zu layers", Active()->doc->S().layers.size());
            back = Active()->renderer.ExportRGBA8(*ctx, Active()->renderer.EvaluateComplete(*Active()->doc, *ctx));
            stage = 2;
        }
        if (stage == 2 && back && back->ready && orig && orig->ready) {
            CompareReadbacks("original vs PSD round trip", *orig, *back, 3);
            stage = 3;
        }
        return;
    }
    if (demoMode == 16 && v) {
        if (frameCounter == 90) SaveDocument(*v, "demo_16k.sxl");
        if (frameCounter % 10 == 0)
            Log("[16k] frame %llu: tiles pending %d / %d, VRAM %.0f MB", (unsigned long long)frameCounter, v->renderer.TilesPending(),
                v->renderer.TilesTotal(), gpu::Dev().VideoMemoryUsage() / 1048576.0);
        return;
    }
    if (demoMode == 8 && v) {
        if (frameCounter == 10) SaveDocument(*v, "demo_save.sxl");
        if (frameCounter == 24) {
            std::error_code ec;
            Log("[demo] saved file size: %llu bytes", (unsigned long long)std::filesystem::file_size("demo_save.sxl", ec));
            LoadSxl("demo_save.sxl");
        }
        return;
    }
    if (demoMode == 10 && v && frameCounter == 5) ImGui::SetWindowFocus("Propriétés");
    if (demoMode == 10 && v && frameCounter == 6) ImGui::SetWindowFocus("Histogramme");
    if (demoMode == 11 && v && frameCounter == 5) ImGui::SetWindowFocus("Propriétés");
    if (demoMode == 12 && v && frameCounter == 5) ImGui::SetWindowFocus("Propriétés");
    if (demoMode == 18 && v && frameCounter == 5) ImGui::SetWindowFocus("Propriétés");
    if (demoMode == 11 && v && frameCounter == 7) ImGui::SetWindowFocus("Éditeur d'effets");
    if (demoMode == 27 && v && frameCounter == 5) ImGui::SetWindowFocus("Vue 3D");
    if (demoMode == 30 && v) {
        // Generative fill with the local stand-in (SX_AI_MOCK=1): ellipse selection -> masked AI layer.
        Document& d = *v->doc;
        if (frameCounter == 6) {
            v->renderer.SelectShape(d, *ctx, v->history, SelShape::Ellipse, {550, 300, 1050, 700}, SelMode::Replace, true, 0, "Ellipse");
            ImGui::SetWindowFocus("Génération IA");
        }
        if (frameCounter == 9) StartGenerativeFill(*v, "un vitrail lumineux");
        if (frameCounter == 40) {
            const Layer* top = d.ActiveLayer();
            Log("[genfill] busy=%d active layer '%s' mask=%u layers=%zu", (int)genFill.busy, top ? top->name.c_str() : "-", top ? top->mask : 0, d.S().layers.size());
        }
        return;
    }
    if (demoMode == 29 && v) {
        // HDRI lighting: glossy metal band on a sphere under a generated environment.
        Document& d = *v->doc;
        if (frameCounter == 5) ImGui::SetWindowFocus("Vue 3D");
        if (frameCounter == 6) CreatePbrChannels(*v);
        auto fillChannel = [&](PbrChannel c, float gray, float y0, float y1) {
            Id g = FindChannelGroup(d, c);
            for (auto& l : d.S().layers)
                if (l.parent == g && l.kind == LayerKind::Pixel) {
                    if (!v->renderer.BeginStroke(d, *ctx, l.surface)) return;
                    std::vector<Dab> dabs;
                    for (float y = y0; y <= y1; y += 20)
                        for (float x = 0; x <= (float)d.Width(); x += 20) dabs.push_back({x, y, 30, 1});
                    BrushSettings b = brush;
                    b.hardness = 1;
                    v->renderer.StrokeDabs(d, *ctx, dabs, b, LinearColor({gray, gray, gray, 1}), false);
                    v->renderer.EndStroke(d, *ctx, v->history, "Pinceau");
                    return;
                }
        };
        if (frameCounter == 8) fillChannel(PbrChannel::Metallic, 1.0f, 0, 1000);
        if (frameCounter == 9) fillChannel(PbrChannel::Roughness, 0.08f, 0, 520);
        if (frameCounter == 10) {
            // Equirect sky: blue gradient, warm sun, gray ground.
            const int W = 512, H = 256;
            std::vector<float> px((size_t)W * H * 3);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x) {
                    float th = (y + 0.5f) / H * 3.14159f, ph = ((x + 0.5f) / W - 0.5f) * 6.28318f;
                    float dy = std::cos(th);
                    float3 c = dy > 0 ? float3{0.25f + 0.3f * (1 - dy), 0.45f + 0.25f * (1 - dy), 0.9f} : float3{0.18f, 0.16f, 0.14f};
                    float3 sun = {std::sin(0.8f) * std::cos(1.0f), std::cos(0.8f), std::sin(0.8f) * std::sin(1.0f)};
                    float3 dir = {std::sin(th) * std::sin(ph), dy, -std::sin(th) * std::cos(ph)};
                    float dd = Dot(dir, sun);
                    if (dd > 0.995f) c = c + float3{40, 36, 30};
                    float* p = &px[((size_t)y * W + x) * 3];
                    p[0] = c.x; p[1] = c.y; p[2] = c.z;
                }
            stbi_write_hdr("demo_sky.hdr", W, H, 3, px.data());
            std::string err;
            if (!LoadEnvironment(*v->mesh, "demo_sky.hdr", &err)) Log("[hdri] %s", err.c_str());
            else Log("[hdri] environment loaded (%ux%u atlas)", v->mesh->envW, v->mesh->envH);
        }
        return;
    }
    if (demoMode == 28 && v) {
        // PBR channels: glossy band in roughness, metallic patch, then texture + Unreal script export.
        Document& d = *v->doc;
        if (frameCounter == 5) ImGui::SetWindowFocus("Vue 3D");
        if (frameCounter == 6) CreatePbrChannels(*v);
        auto paintIn = [&](PbrChannel c, float gray, float y0, float y1) {
            Id g = FindChannelGroup(d, c);
            for (auto& l : d.S().layers)
                if (l.parent == g && l.kind == LayerKind::Pixel) {
                    if (!v->renderer.BeginStroke(d, *ctx, l.surface)) return;
                    std::vector<Dab> dabs;
                    for (float y = y0; y <= y1; y += 30)
                        for (float x = 0; x <= (float)d.Width(); x += 30) dabs.push_back({x, y, 40, 1});
                    BrushSettings b = brush;
                    b.hardness = 1;
                    v->renderer.StrokeDabs(d, *ctx, dabs, b, LinearColor({gray, gray, gray, 1}), false);
                    v->renderer.EndStroke(d, *ctx, v->history, "Pinceau");
                    return;
                }
        };
        if (frameCounter == 8) paintIn(PbrChannel::Roughness, 0.08f, 380, 620);
        if (frameCounter == 9) paintIn(PbrChannel::Metallic, 1.0f, 650, 1000);
        if (frameCounter == 10) paintIn(PbrChannel::Height, 1.0f, 120, 300);  // raised band (bump)
        if (frameCounter == 14) ExportPbrTextures(*v, "pbr_test.png");
        if (frameCounter == 20 && v->mesh) v->mesh->viewChannel = 2;  // roughness alone
        if (frameCounter == 24) {
            std::error_code ec;
            for (const char* f : {"pbr_test_BaseColor.png", "pbr_test_Roughness.png", "pbr_test_Metallic.png", "pbr_test_Normal.png", "pbr_test_AO.png",
                                  "pbr_test_Emissive.png", "pbr_test_Height.png", "pbr_test_unreal_material.py"})
                Log("[pbr] %s: %llu bytes", f, (unsigned long long)std::filesystem::file_size(f, ec));
        }
        return;
    }
    if (demoMode == 14 && v && v->mesh) {
        if (frameCounter == 5) ImGui::SetWindowFocus("Vue 3D");
        if (frameCounter == 8) {
            // Paint a ring around the sphere as seen from the camera.
            Document& d = *v->doc;
            Id target = d.PaintTarget();
            MeshScene& s = *v->mesh;
            if (target && s.depth && v->renderer.BeginStroke(d, *ctx, target)) {
                std::vector<Dab> dabs;
                float cx = s.depth->Width() * 0.5f, cy = s.depth->Height() * 0.5f, r = std::min(cx, cy) * 0.45f;
                for (int i = 0; i <= 400; ++i) {
                    float a = i / 400.0f * 6.2831853f;
                    dabs.push_back({cx + std::cos(a) * r, cy + std::sin(a) * r * 0.6f, 18, 1});
                }
                BrushSettings b = brush;
                b.hardness = 0.7f;
                v->renderer.MeshStrokeDabs(d, *ctx, s, dabs, b, LinearColor({1, 0.85f, 0.1f, 1}), false);
                v->renderer.EndStroke(d, *ctx, v->history, "Peinture 3D");
            }
        }
        if (frameCounter == 12) v->mesh->yaw += 0.9f;
        if (frameCounter == 16 && meshDebugCanvas) ImGui::SetWindowFocus("Canevas");
    }
    if (demoMode == 9 && v) {
        Document& d = *v->doc;
        if (frameCounter == 6) RotateCanvas(*v, 0);
        if (frameCounter == 9) {
            float m[6] = {1, 0, -100, 0, 1, -100};
            ApplyCanvasOp(*v, d.Width() + 200, d.Height() + 200, m, 0, "Taille de la zone de travail");
        }
        if (frameCounter == 12) ApplyCrop(*v, {50, 300, (int)d.Width() - 50, (int)d.Height() - 300});
        if (frameCounter == 15) {
            float m[6] = {2, 0, 0, 0, 2, 0};
            ApplyCanvasOp(*v, d.Width() / 2, d.Height() / 2, m, 2, "Taille de l'image");
        }
        if (frameCounter == 18) Undo();
        if (frameCounter == 20) Log("[demo] size after undo %ux%u", d.Width(), d.Height());
        return;
    }
    if (!v || (demoMode != 4 && demoMode != 5)) return;
    Document& d = *v->doc;
    if (frameCounter == 6) {
        // Select the paint layer and start a free transform on part of it.
        for (auto& l : d.S().layers) if (l.name == "Peinture") d.S().activeLayer = l.id;
        v->renderer.SelectShape(d, *ctx, v->history, SelShape::Rect, {100, 150, 900, 900}, SelMode::Replace, true, 0, "Rectangle");
        TransformBegin(*v, false);
    }
    if (frameCounter == 14 && xf.active && !xf.boundsRb) {
        xf.angle = 0.3f;
        xf.w *= 0.8f;
        xf.h *= 1.1f;
        xf.center = xf.center + float2{420, -40};
        TransformUpdateCorners();
        xf.corners[2] = xf.corners[2] + float2{0, 0};
        xf.dirty = true;
        tool = Tool::Move;
    }
    if (demoMode == 5) {
        if (frameCounter == 18) TransformCommit(*v);
        if (frameCounter == 22) Undo();
        if (frameCounter == 26) Redo();
        if (frameCounter == 30) Log("[demo] history %zu/%zu, selection %u", v->history.Cursor(), v->history.Count(), d.S().selection);
    }
}

void App::RunDemo() {
    for (const char* s : {"composite.slang", "display.slang", "brush.slang", "clear.slang", "clear_r.slang", "gradient.slang"})
        gpu::Shaders().CompileNow(s, gpu::Shaders().File(s));

    if (demoMode == 16) {
        RunDemo16k();
        return;
    }
    NewDocument(1600, 1000, Precision::Half16, DocMode::Layers, 0, "Démo");
    DocView& v = *Active();
    Document& d = *v.doc;

    // Background: radial gradient fill layer.
    Layer& fill = d.AddEffectLayer(LayerKind::Fill, "gradient_fill");
    fill.effect.params[0] = {0.02f, 0.01f, 0.06f, 1};   // colorA (linear)
    fill.effect.params[1] = {0.20f, 0.08f, 0.55f, 1};   // colorB
    fill.effect.params[2] = {1, 0, 0, 0};               // radial
    fill.effect.params[4] = {1.2f, 0, 0, 0};            // scale
    d.SyncGraph();

    // Paint layer with strokes.
    Layer& paint = d.AddPixelLayer("Peinture");
    Id paintSurface = paint.surface;
    Id paintId = paint.id;
    v.renderer.Prepare(d, *ctx);
    auto stroke = [&](float y0, float amp, float size, float hardness, float4 color) {
        BrushSettings b = brush;
        b.size = size;
        b.hardness = hardness;
        if (!v.renderer.BeginStroke(d, *ctx, paintSurface)) return;
        std::vector<Dab> dabs;
        for (float x = 120; x < 1480; x += size * 0.1f) {
            float t = (x - 120) / 1360.0f;
            float pressure = 0.35f + 0.65f * std::sin(t * 3.14159f);
            dabs.push_back({x, y0 + std::sin(t * 9.0f) * amp, size * 0.5f * pressure, 1.0f});
        }
        v.renderer.StrokeDabs(d, *ctx, dabs, b, color, false);
        v.renderer.EndStroke(d, *ctx, v.history, "Pinceau");
    };
    stroke(300, 120, 60, 0.9f, LinearColor({0.18f, 0.85f, 0.95f, 1}));
    stroke(500, 90, 110, 0.2f, LinearColor({1.0f, 0.45f, 0.25f, 1}));
    stroke(720, 140, 34, 1.0f, LinearColor({1.0f, 0.95f, 0.85f, 1}));

    // Glow copy: duplicate, blur filter, screen blend.
    Layer& glow = d.DuplicateLayer(paintId);
    glow.name = "Lueur";
    glow.blend = BlendMode::Screen;
    EffectInstance blur;
    blur.effectId = "gaussian_blur";
    blur.params = {{18, 0, 0, 0}};
    glow.filters.push_back(blur);
    d.SyncGraph();

    // Adjustment on top.
    Layer& adj = d.AddEffectLayer(LayerKind::Adjustment, "vibrance");
    adj.effect.params[0] = {0.6f, 0, 0, 0};
    adj.opacity = 0.8f;
    d.SyncGraph();

    // Noise texture in overlay, masked by a gradient.
    Layer& noise = d.AddEffectLayer(LayerKind::Fill, "noise");
    noise.blend = BlendMode::Overlay;
    noise.opacity = 0.35f;
    d.SyncGraph();
    d.AddMask(noise.id);
    Id maskSurface = d.ActiveLayer()->mask;
    v.renderer.Prepare(d, *ctx);
    v.renderer.ApplyGradient(d, *ctx, v.history, maskSurface, {0, 0}, {1600, 0}, 0, {0, 0, 0, 1}, {1, 1, 1, 1}, 1.0f, "Masque");
    d.S().editMask = false;
    d.S().activeLayer = paintId;

    if (demoMode == 3) {
        // Feathered ellipse + polygon (add) + rectangle (intersect), then fill on a new layer.
        tool = Tool::EllipseSelect;
        v.renderer.SelectShape(d, *ctx, v.history, SelShape::Ellipse, {250, 180, 900, 820}, SelMode::Replace, true, 25, "Ellipse");
        tool = Tool::PolyLasso;
        selDragMode = SelMode::Add;
        lassoPts.clear();
        for (int i = 0; i < 10; ++i) {
            float a = i * 3.14159265f / 5 - 1.5708f;
            float r = (i % 2) ? 120.0f : 300.0f;
            lassoPts.push_back({1150 + std::cos(a) * r, 500 + std::sin(a) * r});
        }
        FinishPolygon(v);
        v.renderer.SelectShape(d, *ctx, v.history, SelShape::Rect, {0, 120, 1600, 880}, SelMode::Intersect, true, 0, "Intersection");
        Edit e(d, v.history, "Nouveau calque");
        Id fillSurface = d.AddPixelLayer("Remplissage sélection").surface;
        e.Commit();
        v.renderer.Prepare(d, *ctx);
        v.renderer.FillSelection(d, *ctx, &v.history, fillSurface, 0, LinearColor({1.0f, 0.75f, 0.2f, 1}), 0.55f, "Remplir");
        tool = Tool::RectSelect;
    }
    if (demoMode == 6) {
        // Retouch tools on the paint layer: clone, blur, smudge, dodge, burn, sponge.
        auto line = [&](RetouchMode mode, bool smudge, float x0, float x1, float y, float size, float strength, int2 off) {
            if (!v.renderer.BeginStroke(d, *ctx, paintSurface)) return;
            BrushSettings b = brush;
            b.size = size;
            b.hardness = 0.5f;
            std::vector<Dab> dabs;
            for (float x = x0; x <= x1; x += size * 0.1f) dabs.push_back({x, y + std::sin(x * 0.02f) * 30, size * 0.5f, 1.0f});
            if (smudge) v.renderer.SmudgeDabs(d, *ctx, dabs, b, strength);
            else {
                RetouchParams rp;
                rp.mode = mode;
                rp.strength = strength;
                rp.srcOffset = off;
                v.renderer.RetouchDabs(d, *ctx, dabs, b, rp);
            }
            v.renderer.EndStroke(d, *ctx, v.history, "Retouche");
        };
        line(RetouchMode::Clone, false, 200, 700, 880, 120, 1, {0, -580});
        line(RetouchMode::Blur, false, 900, 1400, 300, 140, 1, {});
        line(RetouchMode::Burn, false, 200, 600, 500, 160, 0.8f, {});
        line(RetouchMode::Dodge, false, 900, 1400, 720, 160, 0.8f, {});
        line(RetouchMode::Desaturate, false, 650, 900, 500, 200, 1, {});
        line(RetouchMode::Clone, true, 1050, 1450, 500, 90, 0.9f, {});
        // Bucket fill (contiguous, all layers) on a new layer.
        Edit e(d, v.history, "Nouveau calque");
        Id bucketSurface = d.AddPixelLayer("Pot de peinture").surface;
        e.Commit();
        v.renderer.Prepare(d, *ctx);
        fg = {1.0f, 0.9f, 0.2f, 1};
        bucketTolerance = 18;
        wand.rb = v.renderer.ExportRGBA8(*ctx, v.lastImage ? v.lastImage : v.renderer.Evaluate(d, *ctx));
        wand.x = 800;
        wand.y = 520;
        wand.view = &v;
        wand.fill = true;
        wand.surface = bucketSurface;
        tool = Tool::Smudge;
    }
    if (demoMode == 7 || demoMode == 8 || demoMode == 9 || demoMode == 17 || demoMode == 31) {
        // Clipping: a rainbow gradient clipped to the paint strokes; then group paint+glow
        // and apply a group opacity; a hidden noise layer inside a collapsed group.
        d.S().activeLayer = paintId;
        Layer& rainbow = d.AddEffectLayer(LayerKind::Fill, "gradient_fill");
        rainbow.name = "Dégradé écrêté";
        rainbow.effect.params[0] = {1.0f, 0.1f, 0.05f, 1};
        rainbow.effect.params[1] = {0.1f, 0.3f, 1.0f, 1};
        rainbow.effect.params[3] = {0, 0, 0, 0};
        rainbow.clipped = true;
        d.SyncGraph();
        Layer& grp = d.GroupLayer(paintId);
        Id gid = grp.id;
        // Move the clipped gradient into the group, above the paint layer.
        Id rid = 0;
        for (auto& l : d.S().layers) if (l.name == "Dégradé écrêté") rid = l.id;
        d.MoveLayerTo(rid, paintId, false);
        if (Layer* g = d.FindLayer(gid)) { g->name = "Groupe peinture"; g->opacity = 0.85f; }
        d.SyncGraph();
        d.S().activeLayer = gid;
    }
    if (demoMode == 17 || demoMode == 18) {
        // Blend If: the glow only over dark areas, the paint fades out in its darks.
        for (auto& l : d.S().layers) {
            if (l.name == "Lueur") { l.blendIf.under[0][2] = 70; l.blendIf.under[0][3] = 150; d.S().activeLayer = l.id; }
            if (l.name == "Peinture") { l.blendIf.self[0][0] = 30; l.blendIf.self[0][1] = 110; }
            if (l.name == "Vibrance") { l.blendIf.under[1][2] = 120; l.blendIf.under[1][3] = 200; }  // red channel: not over reds
        }
        d.SyncGraph();
    }
    if (demoMode == 10) {
        Layer& c = d.AddEffectLayer(LayerKind::Adjustment, "curves");
        c.effect.curves.resize(c.effect.params.size());
        c.effect.curves[0] = {{0, 0}, {0.25f, 0.12f}, {0.7f, 0.85f}, {1, 1}};
        c.effect.curves[1] = {{0, 0.1f}, {0.5f, 0.6f}, {1, 1}};
        d.SyncGraph();
    }
    if (demoMode == 11) {
        // Visual effect graph: tint the image through a noise mask, plus Unreal export.
        EffectGraph g = EffectGraph::Default();
        g.name = "Teinte bruitée";
        Id in = g.nodes[0].id, out = g.nodes[1].id;
        auto add = [&](const char* op, float2 pos) { EGNode n; n.id = g.nextId++; n.op = op; n.pos = pos; n.inputs.resize(FindOp(op)->inputs.size()); g.nodes.push_back(n); return n.id; };
        Id nz = add("noise", {40, 330});
        Id scale = add("const", {-160, 370});
        g.Find(scale)->value = {6, 0, 0, 0};
        Id tint = add("paramcolor", {40, 450});
        g.Find(tint)->name = "Teinte";
        g.Find(tint)->value = {0.2f, 0.7f, 1.0f, 1};
        Id mul = add("mul", {260, 200});
        Id lerp = add("lerp", {460, 120});
        g.Find(nz)->inputs[1] = {scale, 0};
        g.Find(mul)->inputs = {{in, 0}, {tint, 0}};
        g.Find(lerp)->inputs = {{in, 0}, {mul, 0}, {nz, 0}};
        g.Find(out)->inputs[0] = {lerp, 0};
        g.Find(out)->pos = {680, 120};
        Edit e(d, v.history, "Effet visuel");
        Layer& l = d.AddEffectLayer(LayerKind::Adjustment, "");
        l.name = "Teinte bruitée";
        l.effect.graph = g.Serialize();
        l.effect.SyncParams(nullptr);
        d.SyncGraph();
        e.Commit();
        EGUnrealExport ex = ExportEffectGraphToUnreal(g, "M_SX_TeinteBruitee", "/Game/Shaderixel");
        std::ofstream("demo_unreal_export.py", std::ios::binary) << ex.python;
        Log("[demo] graph compile: %s", CompileEffectGraph(g).error.empty() ? "ok" : CompileEffectGraph(g).error.c_str());
    }
    if (demoMode == 12) {
        TextStyle t;
        t.content = "Shaderixel\nGPU · Calques · Nœuds";
        t.size = 120;
        t.color = {1, 1, 1, 1};
        t.align = 1;
        t.pos = {800, 330};
        for (auto& f : SystemFonts()) if (f.name == "segoeuib") t.font = f.path;
        Edit e(d, v.history, "Texte");
        Layer& tl = d.AddTextLayer(t);
        tl.blend = BlendMode::Overlay;
        Id tid = tl.id;
        d.SyncGraph();
        e.Commit();
        // Second text layer, clipped gradient on top of it.
        TextStyle t2 = t;
        t2.content = "Texte éditable";
        t2.size = 64;
        t2.pos = {800, 700};
        t2.faux = true;
        d.AddTextLayer(t2);
        Layer& grad = d.AddEffectLayer(LayerKind::Fill, "gradient_fill");
        grad.clipped = true;
        grad.effect.params[0] = {1, 0.4f, 0.1f, 1};
        grad.effect.params[1] = {0.2f, 0.9f, 1, 1};
        grad.effect.params[3] = {0, 0, 0, 0};
        d.SyncGraph();
        d.S().activeLayer = tid;
        tool = Tool::Text;
    }
    if (demoMode == 13 || demoMode == 15) {
        // Shape layers + layer styles (filters).
        auto shape = [&](int type, float4 r, float4 fill, bool stroke, float radius, int sides) -> Layer& {
            ShapeStyle s;
            s.type = type; s.rect = r; s.fill = fill; s.strokeOn = stroke; s.stroke = {1, 1, 1, 1}; s.strokeWidth = 6;
            s.radius = radius; s.sides = sides;
            return d.AddShapeLayer(s);
        };
        auto addStyle = [&](Layer& l, const char* id) -> EffectInstance& {
            EffectInstance fx;
            fx.effectId = id;
            if (auto def = Effects().Find(id)) fx.params = def->DefaultParams();
            l.filters.push_back(fx);
            return l.filters.back();
        };
        Layer& r1 = shape(0, {150, 600, 550, 900}, {0.95f, 0.3f, 0.35f, 1}, false, 40, 5);
        addStyle(r1, "style_drop_shadow");
        addStyle(r1, "style_bevel");
        Layer& e1 = shape(1, {650, 560, 950, 940}, {0.2f, 0.75f, 0.95f, 1}, true, 0, 5);
        addStyle(e1, "style_outer_glow");
        Layer& st = shape(4, {1050, 560, 1450, 960}, {1, 0.85f, 0.2f, 1}, false, 0.45f, 5);
        addStyle(st, "style_stroke").params[1] = {8, 0, 0, 0};
        addStyle(st, "style_drop_shadow");
        Layer& pg = shape(3, {80, 80, 330, 330}, {0.6f, 0.95f, 0.5f, 1}, true, 0, 6);
        addStyle(pg, "style_inner_shadow");
        Layer& ln = shape(2, {400, 120, 1500, 300}, {1, 1, 1, 1}, true, 0, 5);
        ln.shape.stroke = {1, 1, 1, 1};
        ln.shape.strokeWidth = 10;
        addStyle(ln, "style_outer_glow");
        d.SyncGraph();
        tool = Tool::Shape;
    }
    if (demoMode == 14 || demoMode == 28 || demoMode == 29) {
        AttachMesh(v, MakeSphere());
        tool = Tool::Brush;
    }
    if (demoMode == 27) {
        // glTF: embedded (.gltf, base64) and binary (.glb) versions of the same scene.
        for (const char* f : {"test.gltf", "test.glb"}) {
            MeshData m;
            std::string err;
            bool ok = LoadMesh(f, m, &err);
            Log("[gltf] %s: %s %zu vertices, %zu triangles, bounds (%.2f %.2f %.2f)-(%.2f %.2f %.2f)", f, ok ? "ok" : err.c_str(), m.vertices.size(),
                m.indices.size() / 3, m.boundsMin.x, m.boundsMin.y, m.boundsMin.z, m.boundsMax.x, m.boundsMax.y, m.boundsMax.z);
            if (ok && std::string(f) == "test.glb") AttachMesh(v, std::move(m));
        }
        tool = Tool::Brush;
    }
    if (demoMode == 20) {
        // Write a teal & orange grading LUT, then apply it with a Color Lookup adjustment.
        {
            const int n = 17;
            std::ofstream f("demo_grade.cube");
            f << "TITLE \"Sarcelle et orange\"\nLUT_3D_SIZE " << n << "\n";
            for (int b = 0; b < n; ++b)
                for (int g = 0; g < n; ++g)
                    for (int r = 0; r < n; ++r) {
                        float R = r / (n - 1.0f), G = g / (n - 1.0f), B = b / (n - 1.0f);
                        float y = 0.299f * R + 0.587f * G + 0.114f * B;
                        float t = std::clamp((y - 0.5f) * 2.0f, -1.0f, 1.0f);  // -1 shadows .. 1 highlights
                        float3 tint = t < 0 ? float3{-0.10f, 0.05f, 0.10f} * -t : float3{0.12f, 0.03f, -0.10f} * t;
                        float s = 1.15f;
                        float3 c = {y + (R - y) * s + tint.x, y + (G - y) * s + tint.y, y + (B - y) * s + tint.z};
                        f << std::clamp(c.x, 0.0f, 1.0f) << " " << std::clamp(c.y, 0.0f, 1.0f) << " " << std::clamp(c.z, 0.0f, 1.0f) << "\n";
                    }
        }
        Layer& lk = d.AddEffectLayer(LayerKind::Adjustment, "color_lookup");
        std::string err;
        if (auto def = Effects().Find("color_lookup")) {
            lk.effect.luts.resize(def->params.size());
            lk.effect.luts[0] = LoadCubeLut("demo_grade.cube", &err);
        }
        if (!err.empty()) Log("[lut] %s", err.c_str());
        d.SyncGraph();
    }
    if (demoMode == 19) {
        // Liquify: push across the strokes, twirl, bloat.
        d.S().activeLayer = paintId;
        v.renderer.Prepare(d, *ctx);
        auto liq = [&](int mode, std::vector<Dab> dabs, float size) {
            BrushSettings b = retouch;
            b.size = size;
            b.hardness = 0.3f;
            if (!v.renderer.BeginStroke(d, *ctx, paintSurface)) return;
            v.renderer.LiquifyDabs(d, *ctx, dabs, b, mode, 0.8f);
            v.renderer.EndStroke(d, *ctx, v.history, "Fluidité");
        };
        std::vector<Dab> push;
        for (int i = 0; i <= 40; ++i) push.push_back({350.0f + i * 4.0f, 250.0f + i * 7.0f, 90, 1});
        liq(0, push, 180);
        liq(1, std::vector<Dab>(40, Dab{800, 500, 130, 1}), 260);
        liq(4, std::vector<Dab>(25, Dab{1250, 330, 110, 1}), 220);
        tool = Tool::Liquify;
    }
    if (demoMode == 2) {
        d.ConvertToNodes();
        // Add a custom HLSL node between the output and its input.
        Graph& g = d.S().graph;
        Node* out = g.Find(g.output);
        Id last = out->inputs[0];
        float2 pos = out->pos;
        out->pos = pos + float2{240, 0};
        Id custom = d.AddEffectNode("", kCustomEffectTemplate, pos).id;
        d.Link(last, custom, 0);
        d.Link(custom, g.output, 0);
        d.S().activeNode = custom;
        nodeEd.zoom = 0.55f;
        nodeEd.pan = {40, 30};
    }
    Status("Scène de démonstration créée");
}

void App::RunDemo16k() {
    // 16384 x 16384 half float: 2 GB per surface, tiled evaluation keeps only the output.
    NewDocument(16384, 16384, Precision::Half16, DocMode::Layers, 0, "Démo 16K");
    DocView& v = *Active();
    Document& d = *v.doc;
    Layer& fill = d.AddEffectLayer(LayerKind::Fill, "gradient_fill");
    fill.effect.params[0] = {0.02f, 0.01f, 0.06f, 1};
    fill.effect.params[1] = {0.20f, 0.08f, 0.55f, 1};
    fill.effect.params[2] = {1, 0, 0, 0};
    d.SyncGraph();
    Layer& paint = d.AddPixelLayer("Peinture");
    Id surf = paint.surface;
    v.renderer.Prepare(d, *ctx);
    BrushSettings b = brush;
    b.size = 600;
    b.hardness = 0.6f;
    if (v.renderer.BeginStroke(d, *ctx, surf)) {
        std::vector<Dab> dabs;
        for (float x = 1000; x < 15400; x += 40) dabs.push_back({x, 8192 + std::sin(x * 0.0007f) * 4000, 300, 1});
        v.renderer.StrokeDabs(d, *ctx, dabs, b, LinearColor({1.0f, 0.45f, 0.25f, 1}), false);
        v.renderer.EndStroke(d, *ctx, v.history, "Pinceau");
    }
    EffectInstance blur;
    blur.effectId = "gaussian_blur";
    blur.params = {{40, 0, 0, 0}};
    d.ActiveLayer()->filters.push_back(blur);
    if (auto def = Effects().Find("style_drop_shadow")) {
        EffectInstance fx;
        fx.effectId = "style_drop_shadow";
        fx.params = def->DefaultParams();
        d.ActiveLayer()->filters.push_back(fx);
    }
    d.SyncGraph();
    FitView(v, ImVec2(1200, 800));
    Status("Démo 16K créée");
}

}  // namespace sx
