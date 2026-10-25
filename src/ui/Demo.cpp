// Scripted demo scene used for automated visual checks (--demo [nodes]).
#include "io/PsdImport.h"
#include "doc/ShapePaths.h"
#include "ui/App.h"
#include "ui/Filter.h"
#include "ui/Search.h"
#include "doc/GraphTypes.h"
#include "doc/DocFile.h"
#include <filesystem>
#include <unordered_set>
#include "io/UAsset.h"
#include "io/ColorMgmt.h"
#include "gpu/ShaderSystem.h"
#include "doc/EffectGraph.h"
#include "doc/Brushes.h"
#include <imgui_internal.h>
#include <limits>

#include <fstream>
#include <stb_image_write.h>

#include <cmath>

namespace sx {

void GraphFrozenNodes(const Graph& g, std::unordered_set<Id>& frozen);  // GraphTools.cpp

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
        if (frameCounter == 6) FocusPanel("Propriétés");
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
    if (demoMode == 91) {
        // Projected material: SX_IMAGE projected on SX_MESHFILE, then a dirt smart mask on top (SX_SMART index optional), SX_RT.
        char img[512] = "";
        GetEnvironmentVariableA("SX_IMAGE", img, sizeof(img));
        if (frameCounter == 4) {
            p3dNew = {};
            p3dNew.basePreset = 1;
            p3dNew.mesh = 0;
            p3dNew.skipPaintLayer = true;
            char mf[512] = "";
            if (GetEnvironmentVariableA("SX_MESHFILE", mf, sizeof(mf)) > 0) { p3dNew.mesh = 3; p3dNew.meshPath = mf; }
            NewPaint3DProject();
        }
        DocView* a = Active();
        if (!a || !a->mesh) return;
        if (frameCounter == 8) FocusPanel("Vue 3D");
        if (frameCounter == 10) {
            Id m = AddProjectedMaterial(*a, img, "Briques");
            Log("[demo] projection: material %u", (unsigned)m);
            if (GetEnvironmentVariableA("SX_RT", nullptr, 0) > 0) a->mesh->quality = 2;
            a->mesh->yaw = 0.5f;
            a->mesh->pitch = 0.3f;
        }
        if (frameCounter == 12) {
            ShowSideTab(5);
            if (GetEnvironmentVariableA("SX_FILTER", nullptr, 0) > 0)
                if (Layer* l = MaterialChannelLayer(*a->doc, materialSel, PbrChannel::BaseColor)) {
                    // Hue shift on the projected color (3D layer filter).
                    EffectInstance fx;
                    fx.effectId = "hue_saturation";
                    fx.params = Effects().Find("hue_saturation")->DefaultParams();
                    fx.params[0].x = 120.0f;
                    l->filters.push_back(fx);
                    a->doc->SyncGraph();
                    Log("[demo] filter added on %s", l->name.c_str());
                }
        }
        if (frameCounter >= 13 && frameCounter <= 16) FocusPanel("Calques 3D");
        char exp[512] = "", pre[8] = "1";
        if (frameCounter == 40 && GetEnvironmentVariableA("SX_EXPORTDIR", exp, sizeof(exp)) > 0) {
            // Texture export: preset SX_EXPORTPRESET, 16-bit PNG, padding.
            GetEnvironmentVariableA("SX_EXPORTPRESET", pre, sizeof(pre));
            texExport.preset = atoi(pre);
            texExport.depth = GetEnvironmentVariableA("SX_EXPORT8", nullptr, 0) > 0 ? 0 : 1;
            texExport.padding = 8;
            texExport.folder = exp;
            texExport.name = "Vase";
            StartTextureExport(*a);
        }
        if (frameCounter == 200) Log("[demo] export: %s (jobs %zu)", statusMsg.c_str(), texExportJobs.size());
        return;
    }
    if (demoMode == 90) {
        // Paint simulation SX_SIM (preset) on SX_MESHFILE: the button held on the upper part of the model.
        char preset[8] = "0";
        GetEnvironmentVariableA("SX_SIM", preset, sizeof(preset));
        if (frameCounter == 4) {
            p3dNew = {};
            p3dNew.basePreset = 1;
            p3dNew.mesh = 0;
            char mf[512] = "";
            if (GetEnvironmentVariableA("SX_MESHFILE", mf, sizeof(mf)) > 0) { p3dNew.mesh = 3; p3dNew.meshPath = mf; }
            NewPaint3DProject();
        }
        DocView* a = Active();
        if (!a || !a->mesh) return;
        if (frameCounter == 8) FocusPanel("Vue 3D");
        ImGuiWindow* w = ImGui::FindWindowByName("Vue 3D");
        if (!w) return;
        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 c((w->InnerRect.Min.x + w->InnerRect.Max.x) * 0.5f, (w->InnerRect.Min.y + w->InnerRect.Max.y) * 0.5f + 20);
        const float span = (w->InnerRect.Max.x - w->InnerRect.Min.x) * 0.1f;
        if (frameCounter == 10) {
            tool = Tool::Particles;
            viewTool[0] = viewTool[1] = Tool::Particles;  // each view keeps its tool
            brush3D.simPreset = atoi(preset);
            brush3D.simRate = 120;
            brush.size = 120;
            fg = {0.35f, 0.12f, 0.04f, 1};
            brush3D.use[(int)PbrChannel::Roughness] = true;
            brush3D.rough = 0.9f;
            a->mesh->yaw = 0.3f;
            a->mesh->pitch = 0.25f;
        }
        if (frameCounter >= 16 && frameCounter <= 70) {
            const float t = (frameCounter - 16) / 54.0f;
            io.AddMousePosEvent(c.x - span + 2 * span * t, c.y - span * 1.6f);
            if (frameCounter == 17) io.AddMouseButtonEvent(0, true);
        }
        if (frameCounter == 71) io.AddMouseButtonEvent(0, false);
        if (frameCounter == 40 || frameCounter == 120) Log("[demo] simulation: %zu particles, stroking %d", sim.parts.size(), sim.stroking ? 1 : 0);
        if (frameCounter == 400) Log("[demo] simulation end: %zu particles, stroking %d, history %s", sim.parts.size(), sim.stroking ? 1 : 0, statusMsg.c_str());
        return;
    }
    if (demoMode == 89) {
        // Baker: project from SX_MESHFILE (low poly), bake from SX_HIGH (optional) at 1024 px, PNGs in SX_OUT, view SX_VIEWCH.
        char high[512] = "", outDir[512] = "", view[8] = "0";
        GetEnvironmentVariableA("SX_HIGH", high, sizeof(high));
        GetEnvironmentVariableA("SX_OUT", outDir, sizeof(outDir));
        GetEnvironmentVariableA("SX_VIEWCH", view, sizeof(view));
        if (frameCounter == 4) {
            p3dNew = {};
            p3dNew.basePreset = 7;  // raw iron
            p3dNew.mesh = 1;
            char mf[512] = "";
            if (GetEnvironmentVariableA("SX_MESHFILE", mf, sizeof(mf)) > 0) { p3dNew.mesh = 3; p3dNew.meshPath = mf; }
            p3dNew.skipPaintLayer = true;
            NewPaint3DProject();
        }
        DocView* a = Active();
        if (!a || !a->mesh) return;
        if (frameCounter == 8) FocusPanel("Vue 3D");
        static bool started = false, reported = false;
        if (frameCounter == 10) {
            baker.open = true;
            baker.useHigh = high[0] != 0;
            baker.highPath = high;
            baker.sizeIndex = 1;
            baker.opt.position = baker.opt.worldNormal = true;
            char idm[8] = "";
            if (GetEnvironmentVariableA("SX_ID", idm, sizeof(idm)) > 0) baker.opt.id = baker.idMode = atoi(idm);
            baker.exportPng = outDir[0] != 0;
            baker.exportDir = outDir;
            StartBake(*a);
            started = true;
            a->mesh->yaw = 0.6f;
            a->mesh->pitch = 0.5f;
        }
        if (started && !reported && !baker.running && frameCounter > 12) {
            reported = true;
            Log("[demo] bake: %s", statusMsg.c_str());
            a->mesh->viewChannel = atoi(view);
            baker.open = false;
        }
        return;
    }
    if (demoMode == 109) {
        // Perspective crop and trim: SX_IMAGE opened; SX_TRIM=1: Image > Trim (transparent pixels) first; then the crop
        // tool in perspective mode with the frame's corners SX_QUAD (x0,y0,...: TL TR BR BL), straightened; SX_OUT.
        char img[512] = "", quad[128] = "", out[512] = "";
        GetEnvironmentVariableA("SX_IMAGE", img, sizeof(img));
        GetEnvironmentVariableA("SX_QUAD", quad, sizeof(quad));
        GetEnvironmentVariableA("SX_OUT", out, sizeof(out));
        const bool trim = GetEnvironmentVariableA("SX_TRIM", nullptr, 0) > 0;
        static gpu::ReadbackPtr outRb;
        if (frameCounter == 4) OpenImage(img, false);
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        // SX_FREQ=<radius>: frequency separation, the composite compared with the original (should match).
        char freq[16] = "";
        if (GetEnvironmentVariableA("SX_FREQ", freq, sizeof(freq)) > 0) {
            static gpu::ReadbackPtr before, after;
            if (frameCounter == 10) before = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            if (frameCounter == 12) {
                FrequencySeparation(*a, (float)atof(freq));
                std::string names;
                for (auto& l : d.S().layers) names += l.name + (l.visible ? "" : " (masqué)") + (l.blend == BlendMode::LinearLight ? " [lumière linéaire]" : "") + "; ";
                Log("[freq] %s | %s | history %zu", statusMsg.c_str(), names.c_str(), a->history.Count());
            }
            if (frameCounter == 24) after = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            if (frameCounter == 34 && before && after && before->ready && after->ready) CompareReadbacks("original vs frequency separation", *before, *after, 2);
            if (frameCounter == 36 && out[0]) {
                // The high frequency layer alone (shown as the final image of the active layer).
                for (auto& l : d.S().layers) l.visible = l.blend == BlendMode::LinearLight;
                for (auto& l : d.S().layers) if (l.visible) l.blend = BlendMode::Normal;
                d.SyncGraph();
                outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            }
            if (outRb && outRb->ready) {
                stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
                outRb.reset();
            }
            return;
        }
        // SX_PICK: Iris Blur added as a filter, its center placed on the image by a click (synthetic events) on the
        // canvas, then the result exported (SX_OUT).
        if (GetEnvironmentVariableA("SX_PICK", nullptr, 0) > 0) {
            ImGuiIO& io = ImGui::GetIO();
            if (frameCounter == 10) {
                ApplyLibraryEffect(*a, "blur_iris", FxApply::Filter);
                FocusPanel("Propriétés");
                if (Layer* l = d.ActiveLayer(); l && !l->filters.empty()) {
                    auto def = l->filters.back().Def();
                    for (size_t i = 0; def && i < def->params.size(); ++i)
                        if (def->params[i].id == "center") paramPick = {true, a, l->id, (int)l->filters.size() - 1, i, false, {}};
                }
            }
            ImGuiWindow* w = ImGui::FindWindowByName("Canevas");
            // The house (doc 590, 250): the click lands there in screen space.
            const ImVec2 target = w ? ImVec2(w->InnerRect.Min.x + a->pan.x + 590 * a->zoom, w->InnerRect.Min.y + a->pan.y + 250 * a->zoom) : ImVec2(0, 0);
            if (frameCounter == 20 && w) io.AddMousePosEvent(target.x, target.y);
            if (frameCounter == 22 && w) io.AddMouseButtonEvent(0, true);
            if (frameCounter == 24 && w) io.AddMouseButtonEvent(0, false);
            if (frameCounter == 30) {
                const EffectInstance* fx = ParamPickTarget();
                if (fx) Log("[pick] center %.3f %.3f (expected ~%.3f %.3f), history %zu: %s", fx->params[paramPick.param].x, fx->params[paramPick.param].y,
                            590.0f / d.Width(), 250.0f / d.Height(), a->history.Count(), a->history.Count() ? a->history.Label(a->history.Count() - 1).c_str() : "");
                else Log("[pick] no target");
            }
            if (frameCounter == 34 && out[0]) outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            if (outRb && outRb->ready) {
                stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
                outRb.reset();
            }
            return;
        }
        // SX_VPASTE: a layer with a checker patch pasted in perspective on the ground (SX_TILE=1: as tiles), applied,
        // then undone / redone; result exported (SX_OUT).
        if (GetEnvironmentVariableA("SX_VPASTE", nullptr, 0) > 0) {
            static Id patchLayer = 0;
            if (frameCounter == 8) {
                Layer& l = d.AddPixelLayer("Motif");
                patchLayer = l.id;
                d.S().activeLayer = l.id;
                a->renderer.SelectShape(d, *ctx, a->history, SelShape::Rect, float4{100, 100, 260, 220}, SelMode::Replace, false, 0, "Sélection");
            }
            if (frameCounter == 10) {
                // A 2 x 2 checker patch (red / white) with a blue border.
                Layer* pl = d.FindLayer(patchLayer);
                auto fillRect = [&](float4 r, float4 c) {
                    a->renderer.SelectShape(d, *ctx, a->history, SelShape::Rect, r, SelMode::Replace, false, 0, "Sélection");
                    a->renderer.FillSelection(d, *ctx, &a->history, pl->surface, 0, c, 1, "Remplir");
                };
                fillRect({96, 96, 264, 224}, {0.05f, 0.1f, 0.8f, 1});
                fillRect({100, 100, 180, 160}, {0.9f, 0.05f, 0.05f, 1});
                fillRect({180, 100, 260, 160}, {1, 1, 1, 1});
                fillRect({100, 160, 180, 220}, {1, 1, 1, 1});
                fillRect({180, 160, 260, 220}, {0.9f, 0.05f, 0.05f, 1});
            }
            if (frameCounter == 12) a->renderer.Deselect(d, a->history);
            if (frameCounter == 14) {
                d.S().activeLayer = patchLayer;
                StartPerspectivePaste(*a);
            }
            if (frameCounter == 20) {
                persp.quad[0] = {260, 300};
                persp.quad[1] = {640, 300};
                persp.quad[2] = {890, 445};
                persp.quad[3] = {10, 445};
                persp.tile = GetEnvironmentVariableA("SX_TILE", nullptr, 0) > 0;
                persp.tileSize = 0.12f;
                persp.dirty = true;
                if (Layer* l = d.FindLayer(patchLayer)) l->visible = false;
                Log("[vpaste] source rect %.0f %.0f %.0f %.0f", persp.srcRect.x, persp.srcRect.y, persp.srcRect.z, persp.srcRect.w);
            }
            if (frameCounter == 26) {
                FinishPerspectivePaste(GetEnvironmentVariableA("SX_VCANCEL", nullptr, 0) == 0);
                Log("[vpaste] %s, layers %zu, history %zu (%s)", statusMsg.c_str(), d.S().layers.size(), a->history.Count(), a->history.Label(a->history.Count() - 1).c_str());
            }
            if (frameCounter == 28) { Undo(); Log("[vpaste] undo: layers %zu", d.S().layers.size()); }
            if (frameCounter == 30) { Redo(); Log("[vpaste] redo: layers %zu", d.S().layers.size()); }
            if (frameCounter == 36 && out[0]) outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            if (outRb && outRb->ready) {
                stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
                outRb.reset();
            }
            return;
        }
        // SX_FOCUS=<threshold>: Selection > Focus Area through its dialog, OK, the selection exported as an image (SX_OUT).
        char focus[16] = "";
        if (GetEnvironmentVariableA("SX_FOCUS", focus, sizeof(focus)) > 0) {
            if (frameCounter == 10) {
                focusArea = {};
                focusArea.open = true;
                focusArea.view = a;
                focusArea.threshold = (float)atof(focus);
                char fill[16] = "";
                if (GetEnvironmentVariableA("SX_FILL", fill, sizeof(fill)) > 0) focusArea.fill = (float)atof(fill);
                focusArea.live = true;
            }
            if (frameCounter == 60 && focusArea.open) {
                focusArea = {};  // (OK: the live result kept)
                Log("[focus] selection %s, history %zu (%s)", d.S().selection ? "made" : "NONE", a->history.Count(),
                    a->history.Count() ? a->history.Label(a->history.Count() - 1).c_str() : "");
            }
            if (frameCounter == 66 && out[0])
                if (Surface* sel = d.Selection()) outRb = a->renderer.ExportRGBA8(*ctx, sel->tex.get(), false);
            if (outRb && outRb->ready) {
                stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
                outRb.reset();
            }
            return;
        }
        // SX_MIXER=<preset 0..3>: mixer brush strokes (synthetic events) across the ball, the ground and the sky, then
        // exported (SX_OUT).
        char mixer[8] = "";
        if (GetEnvironmentVariableA("SX_MIXER", mixer, sizeof(mixer)) > 0) {
            ImGuiIO& io = ImGui::GetIO();
            if (frameCounter == 8) {
                tool = Tool::Mixer;
                viewTool[0] = Tool::Mixer;
                fg = {0.15f, 0.3f, 0.9f, 1};
                retouch.size = 40;
                retouch.hardness = 0.6f;
                const int pr = atoi(mixer);
                const decltype(mixerSet) presets[] = {{0.05f, 0.5f, 0.9f, true}, {0.5f, 0.15f, 0.8f, true}, {0.85f, 0.05f, 0.7f, true}, {0.6f, 0.0f, 0.8f, false}};
                mixerSet = presets[std::clamp(pr, 0, 3)];
                char sym[8] = "";
                if (GetEnvironmentVariableA("SX_SYM", sym, sizeof(sym)) > 0) {
                    // The same strokes with the brush and painting symmetry (SX_SYM = mode, 6 segments).
                    tool = viewTool[0] = Tool::Brush;
                    brush.size = 16;
                    brush.hardness = 0.8f;
                    paintSym.mode = atoi(sym);
                    paintSym.segments = 6;
                    paintSym.center = {450, 225};
                }
                if (GetEnvironmentVariableA("SX_CREPLACE", nullptr, 0) > 0) {
                    // The same strokes with the color replacement tool (blue, Color mode).
                    tool = viewTool[0] = Tool::ColorReplace;
                    colorReplaceMode = 2;
                    colorReplaceTolerance = 0.3f;
                }
            }
            ImGuiWindow* w = ImGui::FindWindowByName("Canevas");
            auto toScreen = [&](float x, float y) { return ImVec2(w->InnerRect.Min.x + a->pan.x + x * a->zoom, w->InnerRect.Min.y + a->pan.y + y * a->zoom); };
            // Two strokes: through the ball to the right, then down from the sky into the ground.
            const struct { float x0, y0, x1, y1; int f0; } strokes[] = {{60, 260, 420, 270, 20}, {700, 150, 720, 420, 70}};
            for (auto& st : strokes)
                if (w && frameCounter >= st.f0 && frameCounter <= st.f0 + 40) {
                    const float t = (frameCounter - st.f0) / 40.0f;
                    const ImVec2 sp = toScreen(st.x0 + (st.x1 - st.x0) * t, st.y0 + (st.y1 - st.y0) * t);
                    io.AddMousePosEvent(sp.x, sp.y);
                    if (frameCounter == st.f0 + 1) io.AddMouseButtonEvent(0, true);
                    if (frameCounter == st.f0 + 40) io.AddMouseButtonEvent(0, false);
                }
            if (frameCounter == 116) Log("[mixer] history %zu (%s)", a->history.Count(), a->history.Count() ? a->history.Label(a->history.Count() - 1).c_str() : "");
            if (frameCounter == 118 && out[0]) outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            if (outRb && outRb->ready) {
                stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
                outRb.reset();
            }
            return;
        }
        // SX_RULER: the image turned 8° (keep size), the ruler laid on the tilted horizon, then Straighten (crop mode);
        // the horizon must come back level. Exported (SX_OUT).
        if (GetEnvironmentVariableA("SX_RULER", nullptr, 0) > 0) {
            if (frameCounter == 10) {
                RotateCanvasArbitrary(*a, 8, 0);
                Log("[ruler] tilted: %u x %u", d.Width(), d.Height());
            }
            if (frameCounter == 14) {
                // Where the horizon (y = 300, x 100..800) went: turned 8° clockwise around the center.
                const float ang = 8 * 3.14159265f / 180, c = std::cos(ang), sn = std::sin(ang), cx = 450, cy = 225;
                auto turn = [&](float x, float y) { return float2{c * (x - cx) - sn * (y - cy) + cx, sn * (x - cx) + c * (y - cy) + cy}; };
                tool = Tool::Ruler;
                ruler.active = true;
                ruler.a = turn(100, 300);
                ruler.b = turn(800, 300);
                ruler.edges = 1;
                Log("[ruler] measured angle %.2f, length %.1f", RulerAngle(), Length(ruler.b - ruler.a));
            }
            if (frameCounter == 18) {
                const float ang = RulerAngle();
                RotateCanvasArbitrary(*a, ang, ruler.edges);  // (what Straighten does for a line under 45°)
                ruler.active = false;
                Log("[ruler] straightened: %u x %u, history %zu", d.Width(), d.Height(), a->history.Count());
            }
            if (frameCounter == 24 && out[0]) outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            if (outRb && outRb->ready) {
                stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
                outRb.reset();
            }
            return;
        }
        // SX_SPOT: dark blobs painted on the grass and the sky, then removed with the spot healing brush (strokes over
        // them); exported (SX_OUT).
        if (GetEnvironmentVariableA("SX_SPOT", nullptr, 0) > 0) {
            static int step = 0;
            if (frameCounter == 8) {
                tool = Tool::SpotHeal;
                const float4 blobs[] = {{300, 360, 330, 385}, {420, 400, 470, 415}, {380, 90, 405, 115}};
                for (auto& bl : blobs) {
                    a->renderer.SelectShape(d, *ctx, a->history, SelShape::Ellipse, bl, SelMode::Replace, true, 0, "Sélection");
                    a->renderer.FillSelection(d, *ctx, &a->history, d.PaintTarget(), 0, float4{0.02f, 0.02f, 0.02f, 1}, 1, "Remplir");
                }
                a->renderer.Deselect(d, a->history);
            }
            if (frameCounter == 12 && out[0]) outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            // One stroke per blob, each once the previous correction is done.
            const float4 strokes[][2] = {{{300, 372, 0, 0}, {330, 372, 0, 0}}, {{420, 407, 0, 0}, {470, 407, 0, 0}}, {{392, 102, 0, 0}, {393, 103, 0, 0}}};
            if (frameCounter > 16 && step < 3 && !fillJob.active) {
                spotHeal.size = step == 1 ? 26 : 36;
                spotHeal.dabs.clear();
                const float4 s0 = strokes[step][0], s1 = strokes[step][1];
                for (int k = 0; k <= 10; ++k) spotHeal.dabs.push_back({s0.x + (s1.x - s0.x) * k / 10, s0.y + (s1.y - s0.y) * k / 10, spotHeal.size * 0.5f, 0});
                SpotHealApply(*a);
                ++step;
            }
            if (step == 3 && !fillJob.active && !outRb) {
                Log("[spot] %s, history %zu (%s)", statusMsg.c_str(), a->history.Count(), a->history.Label(a->history.Count() - 1).c_str());
                std::string o = std::string(out).substr(0, strlen(out) - 4) + "_after.png";
                static std::string after;
                after = o;
                outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
                step = 4;
            }
            if (outRb && outRb->ready) {
                const std::string o = step == 4 ? std::string(out).substr(0, strlen(out) - 4) + "_after.png" : std::string(out);
                stbi_write_png(o.c_str(), outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
                outRb.reset();
            }
            return;
        }
        // SX_TWARP=<bend>: four text layers with warp styles (arc, flag, fish, twist), exported (SX_OUT).
        char twarp[16] = "";
        if (GetEnvironmentVariableA("SX_TWARP", twarp, sizeof(twarp)) > 0) {
            if (frameCounter == 8) {
                const int styles[] = {1, 6, 8, 12};
                const char* words[] = {"Arc", "Drapeau", "Poisson", "Torsion"};
                for (int i = 0; i < 4; ++i) {
                    TextStyle ts;
                    ts.content = words[i];
                    ts.size = 64;
                    ts.color = {0.1f, 0.1f, 0.4f, 1};
                    ts.pos = {40.0f + (i % 2) * 440.0f, 30.0f + (i / 2) * 220.0f};
                    ts.warp = styles[i];
                    ts.warpBend = (float)atof(twarp);
                    d.AddTextLayer(ts);
                }
                d.SyncGraph();
            }
            if (frameCounter == 24 && out[0]) outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            if (outRb && outRb->ready) {
                stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
                outRb.reset();
            }
            return;
        }
        // SX_EXPORTAS=<folder>: a text layer added, then Export As: every layer at 1x and @0.5x trimmed (PNG), and the
        // whole image as JPEG at 2x.
        char exdir[512] = "";
        if (GetEnvironmentVariableA("SX_EXPORTAS", exdir, sizeof(exdir)) > 0) {
            if (frameCounter == 8) {
                TextStyle ts;
                ts.content = "Export";
                ts.size = 80;
                ts.color = {0.9f, 0.2f, 0.1f, 1};
                ts.pos = {300, 40};
                d.AddTextLayer(ts);
                d.SyncGraph();
            }
            if (frameCounter == 14) {
                exportAs.format = 0;
                exportAs.area = 2;
                exportAs.trim = true;
                exportAs.transparent = true;
                exportAs.scales = {{1.0f, ""}, {0.5f, "@0.5x"}};
                d.dirty = false;
                ExportAsRun(*a, std::string(exdir) + "\\layers.png");
            }
            if (frameCounter > 14 && frameCounter < 60 && !exportAsJob && exportAs.area == 2) {
                Log("[exportas] %s, dirty %d", statusMsg.c_str(), (int)d.dirty);
                exportAs.format = 2;  // TIFF 16 bits, flattened on white, trimmed
                exportAs.area = 0;
                exportAs.transparent = false;
                exportAs.scales = {{2.0f, "@2x"}, {2.0f, "@2x"}};  // (twice: numbered, not overwritten)
                ExportAsRun(*a, std::string(exdir) + "\\whole.tif");
            }
            if (frameCounter > 20 && !exportAsJob && exportAs.area == 0 && exports.empty()) {
                Log("[exportas] %s, layers visible after: %d", statusMsg.c_str(), (int)std::count_if(d.S().layers.begin(), d.S().layers.end(), [](const Layer& l) { return l.visible; }));
                exportAs.area = -1;
            }
            return;
        }
        // SX_GRAD: the gradient tool with the editor: a rainbow (linear) on a new layer, then foreground -> transparent
        // (radial) on another; exported (SX_OUT).
        if (GetEnvironmentVariableA("SX_GRAD", nullptr, 0) > 0) {
            auto stopsOf = [&]() {
                std::vector<std::pair<float, float4>> st;
                for (auto& g : gradStops) st.push_back({g.pos, g.follow == 1 ? float4{fg.x, fg.y, fg.z, g.color.w} : g.follow == 2 ? float4{bg.x, bg.y, bg.z, g.color.w} : g.color});
                std::stable_sort(st.begin(), st.end(), [](auto& x, auto& y) { return x.first < y.first; });
                return st;
            };
            if (frameCounter == 8) {
                tool = viewTool[0] = Tool::Gradient;
                Layer& l = d.AddPixelLayer("Arc-en-ciel");
                d.S().activeLayer = l.id;
                gradStops = {{0, {1, 0, 0, 1}}, {0.17f, {1, 0.5f, 0, 1}}, {0.33f, {1, 1, 0, 1}}, {0.5f, {0, 0.8f, 0, 1}}, {0.67f, {0, 0.6f, 1, 1}}, {0.83f, {0.3f, 0, 0.8f, 1}}, {1, {0.6f, 0, 0.6f, 1}}};
                FocusPanel("Options");
            }
            if (frameCounter == 12) {
                Prepare(*a);
                a->renderer.ApplyGradientStops(d, *ctx, a->history, d.PaintTarget(), {0, 0}, {900, 0}, 0, stopsOf(), 0x800, 0.6f, "Dégradé");
            }
            if (frameCounter == 16) {
                Layer& l = d.AddPixelLayer("Halo");
                d.S().activeLayer = l.id;
                fg = {1, 1, 0.6f, 1};
                gradStops = {{0, {0, 0, 0, 1}, 1}, {1, {0, 0, 0, 0}, 1}};
            }
            if (frameCounter == 20) {
                Prepare(*a);
                a->renderer.ApplyGradientStops(d, *ctx, a->history, d.PaintTarget(), {760, 80}, {900, 80}, 1, stopsOf(), 0x800, 1, "Dégradé");
                gradStops = {{0, {1, 0, 0, 1}}, {0.5f, {0, 0.8f, 0, 1}}, {1, {0, 0.6f, 1, 1}}};  // (shown in the editor)
            }
            if (frameCounter == 28 && out[0]) outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            if (outRb && outRb->ready) {
                stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
                outRb.reset();
            }
            return;
        }
        // SX_ALIGN: a red rectangle on its own layer aligned to the right of the canvas, then to the bottom of a selection;
        // its bounds logged after each.
        if (GetEnvironmentVariableA("SX_ALIGN", nullptr, 0) > 0) {
            static Id red = 0;
            static int step = 0;
            if (frameCounter == 8) {
                red = d.AddPixelLayer("Rouge").id;
                d.SyncGraph();
                Prepare(*a);
                a->renderer.SelectShape(d, *ctx, a->history, SelShape::Rect, float4{100, 100, 200, 180}, SelMode::Replace, false, 0, "Sélection");
                if (Layer* l = d.FindLayer(red)) a->renderer.FillSelection(d, *ctx, &a->history, l->surface, 0, float4{0.9f, 0.1f, 0.1f, 1}, 1, "Remplir");
                a->renderer.Deselect(d, a->history);
                d.S().activeLayer = red;
            }
            auto logBounds = [&](const char* what) {
                if (Layer* l = d.FindLayer(red)) outRb = a->renderer.ComputeBounds(*ctx, d.GetSurface(l->surface)->tex.get(), nullptr, false);
                Log("[align] %s (%s)", what, statusMsg.c_str());
            };
            if (frameCounter == 14) StartAlignLayer(*a, 2);  // right of the canvas
            if (frameCounter == 20) logBounds("right of the canvas, expected x1 = 900");
            if (frameCounter == 26) {
                a->renderer.SelectShape(d, *ctx, a->history, SelShape::Rect, float4{300, 50, 700, 300}, SelMode::Replace, false, 0, "Sélection");
                StartAlignLayer(*a, 5);  // bottom of the selection
                step = 1;
            }
            if (frameCounter == 34) logBounds("bottom of the selection, expected y1 = 300");
            if (frameCounter == 40) { Undo(); Undo(); }
            if (frameCounter == 46) logBounds("two undos: back at the canvas' right");
            if (outRb && outRb->ready) {
                const u32* b = (const u32*)outRb->data.data();
                Log("[align]   red box x %u..%u y %u..%u, selection %s", b[0], b[2], b[1], b[3], d.S().selection ? "kept" : "none");
                outRb.reset();
            }
            return;
        }
        // SX_DEFBRUSH: Define Brush Preset from a selection around the ball, then a stroke with it on a new layer;
        // exported (SX_OUT). The brush files written are removed at the end (tests leave no brush behind).
        if (GetEnvironmentVariableA("SX_DEFBRUSH", nullptr, 0) > 0) {
            ImGuiIO& io = ImGui::GetIO();
            if (frameCounter == 8) {
                a->renderer.SelectShape(d, *ctx, a->history, SelShape::Rect, float4{110, 200, 230, 320}, SelMode::Replace, false, 0, "Sélection");
                DefineBrush(*a);
            }
            if (frameCounter == 14) {
                Log("[defbrush] %s, tip %s, found %d", statusMsg.c_str(), brush.tipId.c_str(), Brushes().Find(brush.tipId) ? 1 : 0);
                a->renderer.Deselect(d, a->history);
                d.S().activeLayer = d.AddPixelLayer("Trait").id;
                d.SyncGraph();
                tool = viewTool[0] = Tool::Brush;
                brush.size = 60;
                brush.spacing = 0.6f;
                fg = {0.1f, 0.3f, 0.9f, 1};
            }
            auto S = [&](float x, float y) { return ImVec2(lxf.canvasMin.x + a->pan.x + x * a->zoom, lxf.canvasMin.y + a->pan.y + y * a->zoom); };
            if (frameCounter >= 20 && frameCounter <= 50) {
                const float t = (frameCounter - 20) / 30.0f;
                const ImVec2 p = S(300 + 500 * t, 380 - 40 * t);
                io.AddMousePosEvent(p.x, p.y);
                if (frameCounter == 21) io.AddMouseButtonEvent(0, true);
                if (frameCounter == 50) io.AddMouseButtonEvent(0, false);
            }
            if (frameCounter == 56 && out[0]) outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            if (outRb && outRb->ready) {
                stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
                outRb.reset();
                std::error_code ec;
                const std::string id = brush.tipId;
                std::filesystem::remove(Utf8Path(Brushes().Dir() + "/" + id + ".slang"), ec);
                std::filesystem::remove(Utf8Path(Brushes().Dir() + "/textures/user/" + id.substr(5) + ".png"), ec);
                Log("[defbrush] test brush removed (%s)", id.c_str());
            }
            return;
        }
        // SX_SMARTG2: a shape layer (rectangle, no stroke) dragged with the move tool to 2.5 px from a blue pixel box's
        // left edge: it snaps onto it (shape rect logged).
        if (GetEnvironmentVariableA("SX_SMARTG2", nullptr, 0) > 0) {
            ImGuiIO& io = ImGui::GetIO();
            static Id shapeL = 0;
            if (frameCounter == 8) {
                Id blue = d.AddPixelLayer("Bleu").id;
                d.SyncGraph();
                Prepare(*a);
                a->renderer.SelectShape(d, *ctx, a->history, SelShape::Rect, float4{500, 300, 600, 380}, SelMode::Replace, false, 0, "Sélection");
                if (Layer* l = d.FindLayer(blue)) a->renderer.FillSelection(d, *ctx, &a->history, l->surface, 0, float4{0.1f, 0.2f, 0.9f, 1}, 1, "Remplir");
                a->renderer.Deselect(d, a->history);
                ShapeStyle st;
                st.rect = {100, 100, 200, 180};
                st.strokeOn = false;
                shapeL = d.AddShapeLayer(st).id;
                d.S().activeLayer = shapeL;
                d.SyncGraph();
                tool = viewTool[0] = Tool::Move;
            }
            auto S = [&](float x, float y) { return ImVec2(lxf.canvasMin.x + a->pan.x + x * a->zoom, lxf.canvasMin.y + a->pan.y + y * a->zoom); };
            const float tx = 150 + 397.5f, ty = 140 + 30;
            if (frameCounter == 20) { const ImVec2 p = S(150, 140); io.AddMousePosEvent(p.x, p.y); }
            if (frameCounter == 22) io.AddMouseButtonEvent(0, true);
            if (frameCounter > 22 && frameCounter <= 42) {
                const float t = (frameCounter - 22) / 20.0f;
                const ImVec2 p = S(150 + (tx - 150) * t, 140 + (ty - 140) * t);
                io.AddMousePosEvent(p.x, p.y);
            }
            if (frameCounter == 44) io.AddMouseButtonEvent(0, false);
            if (frameCounter == 50)
                if (Layer* l = d.FindLayer(shapeL))
                    Log("[smartg2] shape rect after the move: %.1f..%.1f, %.1f..%.1f (blue starts at x 500)", l->shape.rect.x, l->shape.rect.z, l->shape.rect.y, l->shape.rect.w);
            return;
        }
        // SX_SMARTG: two rectangles on their own layers; the first dragged with the move tool (synthetic events) to
        // 2.5 px from the second's left edge: the smart guides snap it onto it. Exported (SX_OUT).
        if (GetEnvironmentVariableA("SX_SMARTG", nullptr, 0) > 0) {
            ImGuiIO& io = ImGui::GetIO();
            static Id moving = 0;
            auto fillRect = [&](Id layer, float4 r, float4 c) {
                a->renderer.SelectShape(d, *ctx, a->history, SelShape::Rect, r, SelMode::Replace, false, 0, "Sélection");
                if (Layer* l = d.FindLayer(layer)) a->renderer.FillSelection(d, *ctx, &a->history, l->surface, 0, c, 1, "Remplir");
                a->renderer.Deselect(d, a->history);
            };
            if (frameCounter == 8) {
                Id other = d.AddPixelLayer("Bleu").id;
                moving = d.AddPixelLayer("Rouge").id;
                d.SyncGraph();
                Prepare(*a);
                fillRect(other, {500, 300, 600, 380}, {0.1f, 0.2f, 0.9f, 1});
                fillRect(moving, {100, 100, 200, 180}, {0.9f, 0.1f, 0.1f, 1});
                d.S().activeLayer = moving;
                tool = viewTool[0] = Tool::Move;
            }
            auto S = [&](float x, float y) { return ImVec2(lxf.canvasMin.x + a->pan.x + x * a->zoom, lxf.canvasMin.y + a->pan.y + y * a->zoom); };
            // Grab the red box at (150, 140), drop it so its left edge is at 497.5 (the blue box starts at 500).
            const float tx = 150 + 397.5f, ty = 140 + 30;
            if (frameCounter == 20) { const ImVec2 p = S(150, 140); io.AddMousePosEvent(p.x, p.y); }
            if (frameCounter == 22) io.AddMouseButtonEvent(0, true);
            if (frameCounter > 22 && frameCounter <= 42) {
                const float t = (frameCounter - 22) / 20.0f;
                const ImVec2 p = S(150 + (tx - 150) * t, 140 + (ty - 140) * t);
                io.AddMousePosEvent(p.x, p.y);
            }
            if (frameCounter == 43)
                Log("[smartg] while dragging: guide x %.1f y %.1f, box %d..%d", sg.lineX, sg.lineY, sg.box.x0, sg.box.x1);
            if (frameCounter == 44) io.AddMouseButtonEvent(0, false);
            if (frameCounter == 50)
                if (Layer* l = d.FindLayer(moving); l && d.GetSurface(l->surface))
                    outRb = a->renderer.ComputeBounds(*ctx, d.GetSurface(l->surface)->tex.get(), nullptr, false);
            if (outRb && outRb->ready && frameCounter > 50) {
                const u32* b = (const u32*)outRb->data.data();
                Log("[smartg] red box after the move: x %u..%u, y %u..%u (blue: 500..600, 300..380)", b[0], b[2], b[1], b[3]);
                outRb.reset();
            }
            return;
        }
        // SX_FX=<effect id>: that effect applied (destructive) to the image instead, then exported (SX_OUT).
        char fx[96] = "";
        if (GetEnvironmentVariableA("SX_FX", fx, sizeof(fx)) > 0) {
            if (frameCounter == 10) {
                ApplyLibraryEffect(*a, fx, FxApply::Destructive);
                Log("[persp] effect %s: %s", fx, statusMsg.c_str());
            }
            if (frameCounter == 16 && out[0]) outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
            if (outRb && outRb->ready) {
                stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
                Log("[persp] exported %ux%u", outRb->width, outRb->height);
                outRb.reset();
            }
            return;
        }
        if (frameCounter == 10 && trim) {
            trimMode = 0;
            for (bool& b : trimSides) b = true;
            trimRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx), false);
            trimView = a;
        }
        if (frameCounter == 20) {
            Log("[persp] after trim: %u x %u (%s)", d.Width(), d.Height(), statusMsg.c_str());
            tool = Tool::Crop;
            cropPersp = true;
            float q[8] = {};
            sscanf(quad, "%f,%f,%f,%f,%f,%f,%f,%f", &q[0], &q[1], &q[2], &q[3], &q[4], &q[5], &q[6], &q[7]);
            const float off = trim ? 40.0f : 0.0f;  // (the trimmed margin)
            for (int k = 0; k < 4; ++k) cropQuad[k] = {q[k * 2] - off, q[k * 2 + 1] - off};
            cropActive = cropQuadSet = true;
        }
        if (frameCounter == 30) {
            ApplyPerspectiveCrop(*a);
            Log("[persp] %s", statusMsg.c_str());
        }
        if (frameCounter == 36 && out[0]) outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
        if (outRb && outRb->ready) {
            stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
            Log("[persp] exported %ux%u", outRb->width, outRb->height);
            outRb.reset();
        }
        return;
    }
    if (demoMode == 108) {
        // Frame animation: SX_IMAGE opened, a red ball (smart object) added, frame 1, frame 2 with the ball moved
        // right, 6 frames tweened between them, played, then exported to SX_OUT (.gif) and the same path in .mp4.
        char img[512] = "", out[512] = "";
        GetEnvironmentVariableA("SX_IMAGE", img, sizeof(img));
        GetEnvironmentVariableA("SX_OUT", out, sizeof(out));
        if (frameCounter == 4) {
            OpenImage(img, false);
            panelOpen["Animation"].open = true;
        }
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        static Id ball = 0;
        if (frameCounter == 8) {
            Layer& l = d.AddPixelLayer("Balle");
            ball = l.id;
            d.S().activeLayer = l.id;
            a->renderer.SelectShape(d, *ctx, a->history, SelShape::Ellipse, float4{60, 60, 160, 160}, SelMode::Replace, true, 0, "Sélection");
        }
        if (frameCounter == 10) {
            if (Layer* l = d.FindLayer(ball)) a->renderer.FillSelection(d, *ctx, &a->history, l->surface, 0, float4{0.9f, 0.05f, 0.3f, 1}, 1, "Remplir");
            a->renderer.Deselect(d, a->history);
        }
        if (frameCounter == 12) {
            d.S().activeLayer = ball;
            ConvertToSmartObject(*a);
            FocusPanel("Animation");
        }
        if (frameCounter == 20) {
            Edit e(d, a->history, "Créer l'animation");
            AnimFrame f;
            f.id = d.NewId();
            AnimCaptureEntries(d, f.entries);
            d.S().animFrames.push_back(f);
            f.id = d.NewId();
            d.S().animFrames.push_back(f);
            d.S().activeFrame = d.S().animFrames[1].id;
            e.Commit();
        }
        if (frameCounter == 24) {
            // Frame 2: the ball moved right and half transparent (recorded into the selected frame).
            for (auto& l : d.S().layers)
                if (l.kind == LayerKind::Smart) {
                    for (auto& c : l.smartCorners) c.x += 600;
                    l.opacity = 0.5f;
                }
            Prepare(*a);
            SyncSmartLayers(*a);
        }
        if (frameCounter == 28) {
            const auto& f = d.S().animFrames;
            float x0 = 0, x1 = 0;
            for (auto& en : f[0].entries) if (d.FindLayer(en.layer) && d.FindLayer(en.layer)->kind == LayerKind::Smart) x0 = en.corners[0].x;
            for (auto& en : f[1].entries) if (d.FindLayer(en.layer) && d.FindLayer(en.layer)->kind == LayerKind::Smart) x1 = en.corners[0].x;
            Log("[anim] frames %zu, ball x %.0f -> %.0f", f.size(), x0, x1);
            AnimTween(*a, 0, 6);
            Log("[anim] after tween: %zu frames", d.S().animFrames.size());
            ApplyAnimFrame(*a, d.S().animFrames[0].id);
            anim.playing = true;
            anim.view = a;
            anim.acc = 0;
            anim.played = 0;
        }
        if (frameCounter == 60) {
            anim.playing = false;
            Log("[anim] playing: on frame %d", [&] { int i = 0; for (auto& f : d.S().animFrames) { if (f.id == d.S().activeFrame) return i; ++i; } return -1; }() + 1);
            if (out[0]) StartAnimExport(*a, out, false);
        }
        static bool videoStarted = false;
        if (frameCounter > 60 && !animExport && !videoStarted && out[0]) {
            videoStarted = true;
            std::error_code ec;
            Log("[anim] gif: %llu bytes; %s", (unsigned long long)std::filesystem::file_size(out, ec), statusMsg.c_str());
            std::string mp4 = std::string(out).substr(0, strlen(out) - 4) + ".mp4";
            StartAnimExport(*a, mp4, true);
        }
        static bool reported = false;
        if (videoStarted && !animExport && !reported) {
            reported = true;
            std::error_code ec;
            std::string mp4 = std::string(out).substr(0, strlen(out) - 4) + ".mp4";
            Log("[anim] mp4: %llu bytes; %s", (unsigned long long)std::filesystem::file_size(mp4, ec), statusMsg.c_str());
            ApplyAnimFrame(*a, d.S().animFrames[3].id);
            anim.onion = true;  // onion skins of frames 3 and 5 over the canvas
        }
        return;
    }
    if (demoMode == 107) {
        // Content-aware scale: SX_IMAGE opened, then scaled to SX_CARVE (width %, default 60; "h" suffix: height),
        // SX_CARVESEL=x0,y0,x1,y1 protects (or with SX_CARVEMODE=2 removes) a rectangle; result exported to SX_OUT.
        char img[512] = "", pct[32] = "60", selR[64] = "", mode[8] = "1", out[512] = "";
        GetEnvironmentVariableA("SX_IMAGE", img, sizeof(img));
        GetEnvironmentVariableA("SX_CARVE", pct, sizeof(pct));
        GetEnvironmentVariableA("SX_CARVESEL", selR, sizeof(selR));
        GetEnvironmentVariableA("SX_CARVEMODE", mode, sizeof(mode));
        GetEnvironmentVariableA("SX_OUT", out, sizeof(out));
        static bool started = false, reported = false;
        static double t0 = 0;
        static gpu::ReadbackPtr outRb;
        if (frameCounter == 4) OpenImage(img, false);
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        if (frameCounter == 10 && selR[0]) {
            float r[4] = {};
            sscanf(selR, "%f,%f,%f,%f", &r[0], &r[1], &r[2], &r[3]);
            a->renderer.SelectShape(d, *ctx, a->history, SelShape::Rect, float4{r[0], r[1], r[2], r[3]}, SelMode::Replace, false, 0, "Sélection");
        }
        if (frameCounter == 14) {
            const bool vert = strchr(pct, 'h') != nullptr;
            const float f = (float)atof(pct) / 100.0f;
            const u32 w = vert ? d.Width() : (u32)std::lround(d.Width() * f), h = vert ? (u32)std::lround(d.Height() * f) : d.Height();
            Log("[carve] %u x %u -> %u x %u, selection %s mode %s", d.Width(), d.Height(), w, h, selR[0] ? selR : "-", mode);
            t0 = ImGui::GetTime();
            StartContentScale(*a, w, h, selR[0] ? atoi(mode) : 0);
            started = true;
        }
        if (started && !reported && !carveJob) {
            reported = true;
            Log("[carve] done in %.2f s: %u x %u, history %s; %s", ImGui::GetTime() - t0, d.Width(), d.Height(),
                a->history.CanUndo() ? "undoable" : "none", statusMsg.c_str());
            if (out[0]) outRb = a->renderer.ExportRGBA8(*ctx, a->renderer.EvaluateComplete(d, *ctx));
        }
        if (outRb && outRb->ready) {
            stbi_write_png(out, outRb->width, outRb->height, 4, outRb->data.data(), outRb->width * 4);
            Log("[carve] exported %s", out);
            outRb.reset();
        }
        return;
    }
    if (demoMode == 106) {
        // Several materials over the same UVs: SX_MESHFILE (two materials), a UV tile each, the first material's triangle
        // filled red, then the textures exported (SX_OUT folder) as separate maps and packed (Unreal), and the mesh
        // saved / read back (tiles and materials kept).
        char mf[512] = "", outDir[512] = "";
        GetEnvironmentVariableA("SX_MESHFILE", mf, sizeof(mf));
        GetEnvironmentVariableA("SX_OUT", outDir, sizeof(outDir));
        if (frameCounter == 4) {
            p3dNew = {};
            p3dNew.basePreset = 1;
            p3dNew.mesh = 3;
            p3dNew.meshPath = mf;
            p3dNew.resolution = 0;
            // SX_RETILE: the project keeps the overlapping UVs; the tiles come later (Une tuile par matériau).
            if (GetEnvironmentVariableA("SX_RETILE", nullptr, 0) > 0) p3dNew.tileMode = 2;
            NewPaint3DProject();
        }
        DocView* a = Active();
        if (!a || !a->mesh) return;
        if (frameCounter == 16 && GetEnvironmentVariableA("SX_RETILE", nullptr, 0) > 0) {
            RetileMaterials(*a);
            Log("[multimat] retiled: %zu tiles, %zu vertices, history %zu, %s", a->mesh->data.uvTiles.size(), a->mesh->data.vertices.size(),
                a->history.Count(), statusMsg.c_str());
        }
        const MeshData& m = a->mesh->data;
        if (frameCounter == 8) {
            FocusPanel("Vue 3D");
            Log("[multimat] %zu materials, %zu tiles, %zu vertices", m.materials.size(), m.uvTiles.size(), m.vertices.size());
            for (size_t i = 0; i < m.uvTiles.size(); ++i)
                Log("[multimat] %s tile %.3f %.3f %.3f %.3f", m.materials[i].name.c_str(), m.uvTiles[i].x, m.uvTiles[i].y, m.uvTiles[i].z, m.uvTiles[i].w);
            for (size_t i = 0; i < m.vertices.size(); ++i) Log("[multimat] v%zu uv %.3f %.3f", i, m.vertices[i].uv.x, m.vertices[i].uv.y);
            MeshData back;
            const bool ok = MeshFromJson(MeshToJson(m), back);
            bool same = ok && back.uvTiles.size() == m.uvTiles.size() && back.materials.size() == m.materials.size() && back.triMaterial == m.triMaterial;
            for (size_t i = 0; same && i < m.uvTiles.size(); ++i) same = std::fabs(back.uvTiles[i].x - m.uvTiles[i].x) < 1e-6f && back.materials[i].name == m.materials[i].name;
            Log("[multimat] mesh saved / read back: %s", same ? "same tiles and materials" : "DIFFERENT");
            if (!m.udims.empty()) {
                // UDIM: tiles, cells, the project's copy and the OBJ export (UVs back in UDIM space, read back).
                std::string tiles;
                for (u16 t : m.udims) tiles += std::to_string(t) + " ";
                Log("[udim] tiles %s(grid %d x %d), project copy %s", tiles.c_str(), m.udimCols, m.udimCols,
                    ok && back.udims == m.udims && back.udimCols == m.udimCols ? "same" : "DIFFERENT");
                const std::string obj = std::string(outDir[0] ? outDir : ".") + "\\udim_back.obj";
                MeshData re;
                std::string err;
                if (SaveObj(obj, m, true, &err) && LoadMesh(obj, re, &err)) {
                    float lo = 1e9f, hi = -1e9f;
                    for (auto& vx : re.vertices) { lo = std::min(lo, vx.uv.x); hi = std::max(hi, vx.uv.x); }
                    MeshData packed = re;
                    const bool again = PackUdims(packed);
                    Log("[udim] OBJ export read back: u %.2f..%.2f, packs again %s (%zu tiles)", lo, hi, again ? "yes" : "NO", packed.udims.size());
                } else Log("[udim] OBJ export: %s", err.c_str());
            }
        }
        if (frameCounter == 12) {
            fg = {1, 0, 0, 1};
            PolygonFill(*a, 0, false);  // first triangle: material 1
        }
        // SX_LIMIT: a green stroke started on the first material, across onto the second, limited to the first.
        const bool limit = GetEnvironmentVariableA("SX_LIMIT", nullptr, 0) > 0;
        if (limit) {
            ImGuiIO& io = ImGui::GetIO();
            if (frameCounter == 14) {
                tool = Tool::Brush;
                viewTool[1] = Tool::Brush;
                brush3D.limitMaterial = true;
                fg = {0, 1, 0, 1};
                brush.size = 60;
                a->mesh->yaw = 0;
                a->mesh->pitch = 0;
            }
            if (ImGuiWindow* w = ImGui::FindWindowByName("Vue 3D"); w && frameCounter >= 30 && frameCounter <= 60) {
                const float t = (frameCounter - 30) / 30.0f;
                const float x = w->InnerRect.Min.x + (w->InnerRect.Max.x - w->InnerRect.Min.x) * (0.3f + 0.4f * t);
                io.AddMousePosEvent(x, (w->InnerRect.Min.y + w->InnerRect.Max.y) * 0.5f);
                if (frameCounter == 31) io.AddMouseButtonEvent(0, true);
            }
            if (frameCounter == 61) io.AddMouseButtonEvent(0, false);
        }
        if (frameCounter == (limit ? 80 : 20) && outDir[0]) {
            a->doc->exportTemplate = 0;
            ExportProjectTextures(*a, std::string(outDir) + "\\sep.png");
            a->doc->exportTemplate = 1;
            ExportProjectTextures(*a, std::string(outDir) + "\\ue.png");
            Log("[multimat] export: %s", statusMsg.c_str());
        }
        return;
    }
    if (demoMode == 105) {
        // Liquid paint: SX_FLUID = 2d (a tilted image), 3d (a sphere), plane (a flat plane, gravity tilted), cube.
        // SX_FLPRESET preset, SX_TILT support tilt (image), SX_FLSTILL frames the brush stays still (a pool).
        char mode[16] = "2d", pre[8] = "0", tilt[16] = "90", still[8] = "12", view[8] = "";
        GetEnvironmentVariableA("SX_FLUID", mode, sizeof(mode));
        GetEnvironmentVariableA("SX_FLPRESET", pre, sizeof(pre));
        GetEnvironmentVariableA("SX_TILT", tilt, sizeof(tilt));
        GetEnvironmentVariableA("SX_FLSTILL", still, sizeof(still));
        GetEnvironmentVariableA("SX_VIEWCH", view, sizeof(view));
        const std::string m = mode;
        const bool is3D = m != "2d";
        ImGuiIO& io = ImGui::GetIO();
        if (frameCounter == 4) {
            if (is3D) {
                p3dNew = {};
                p3dNew.basePreset = m == "cube" ? 7 : 1;
                p3dNew.mesh = m == "plane" ? 2 : (m == "cube" || m == "cubetop") ? 1 : 0;
                char mf[512] = "";
                if (m == "table" && GetEnvironmentVariableA("SX_MESHFILE", mf, sizeof(mf)) > 0) { p3dNew.mesh = 3; p3dNew.meshPath = mf; }
                NewPaint3DProject();
            } else {
                NewDocument(1024, 1024, Precision::Half16, DocMode::Layers, 0, "Liquide");
                DocView* dv = Active();
                Document& d = *dv->doc;
                Layer& l = d.AddPixelLayer("Peinture");
                d.S().activeLayer = l.id;
                d.SyncGraph();
                // SX_PICKUP: the layer already holds a blue paint (the red liquid runs over it and takes some of its color).
                if (GetEnvironmentVariableA("SX_PICKUP", nullptr, 0) > 0) {
                    dv->renderer.Prepare(d, *ctx);
                    dv->renderer.FillSelection(d, *ctx, nullptr, l.surface, 0, {0.05f, 0.15f, 0.8f, 1}, 1.0f, "Bleu", nullptr, nullptr, false);
                }
            }
        }
        DocView* a = Active();
        if (!a) return;
        if (frameCounter == 8 && is3D) FocusPanel("Vue 3D");
        if (frameCounter == 12) FocusPanel(is3D ? "Matériau du pinceau" : "Options");
        if (frameCounter >= 10 && frameCounter <= 15) {
            tool = Tool::Fluid;  // each view keeps its tool: set it once the 3D view is the one worked in
            viewTool[0] = viewTool[1] = Tool::Fluid;
        }
        if (frameCounter == 10) {
            ApplyFluidPreset(atoi(pre));
            fluidSet.tilt2D = (float)atof(tilt);
            brush.size = is3D ? 70.0f : 46.0f;
            if (m == "plane") {
                fluidSet.gravMode = 2;  // the plane tilted up as a wall
                fluidSet.tilt = 90;
                fluidSet.azimuth = -90;
                a->mesh->yaw = 0.0f;
                a->mesh->pitch = 1.2f;
            } else if (m == "table") {
                fluidSet.dripOff = 1.5f;  // the paint running under the top falls on the base
                fluidSet.flow = 6.0f;
                a->mesh->yaw = 0.25f;
                a->mesh->pitch = 0.15f;
            } else if (m == "cubetop") {
                fluidSet.gravMode = 1;  // the bottom of the view: the top face's paint runs over the front edge
                a->mesh->yaw = 0.0f;
                a->mesh->pitch = 0.75f;
            } else if (is3D) {
                a->mesh->yaw = 0.35f;
                a->mesh->pitch = 0.2f;
            }
            if (is3D && view[0]) a->mesh->viewChannel = atoi(view);
        }
        ImVec2 p0, p1;
        bool have = false;
        if (is3D) {
            if (ImGuiWindow* w = ImGui::FindWindowByName("Vue 3D")) {
                const ImVec2 c((w->InnerRect.Min.x + w->InnerRect.Max.x) * 0.5f, (w->InnerRect.Min.y + w->InnerRect.Max.y) * 0.5f);
                const float span = std::min(w->InnerRect.Max.x - w->InnerRect.Min.x, w->InnerRect.Max.y - w->InnerRect.Min.y) * 0.22f;
                p0 = ImVec2(c.x - span, c.y - span * (m == "plane" ? 0.3f : m == "cubetop" ? 0.35f : m == "table" ? 0.62f : 0.85f));
                p1 = ImVec2(c.x + span, p0.y);
                have = true;
            }
        } else if (ImGui::FindWindowByName("Canevas")) {
            auto screen = [&](float2 p) { return ImVec2(lxf.canvasMin.x + a->pan.x + p.x * a->zoom, lxf.canvasMin.y + a->pan.y + p.y * a->zoom); };
            p0 = screen({160, 230});
            p1 = screen({860, 230});
            have = true;
        }
        // A stroke across, the brush held still halfway (a pool that runs further).
        const int hold = atoi(still);
        // SX_RUNLAYER: no stroke; the blue layer (SX_PICKUP) or a painted band becomes liquid and runs.
        if (GetEnvironmentVariableA("SX_RUNLAYER", nullptr, 0) > 0) {
            if (frameCounter == 16 && !is3D) {
                Document& d = *a->doc;
                d.S().selection = 0;
                // A band of paint (rectangle selection filled), then the selection dropped.
                Id target = d.PaintTarget();
                a->renderer.SelectShape(d, *ctx, a->history, SelShape::Rect, {200, 180, 824, 260}, SelMode::Replace, false, 0, "Bande");
                a->renderer.FillSelection(d, *ctx, nullptr, target, 0, {0.75f, 0.1f, 0.05f, 1}, 1.0f, "Bande", nullptr, nullptr, true);
                a->renderer.Deselect(d, a->history);
            }
            if (frameCounter == 20) FluidRunLayer(*a);
            if (frameCounter % 60 == 0 && frameCounter >= 60)
                Log("[fluid] frame %d: run layer, active %d, wet %.2f s, history %zu", frameCounter, fluidRun.active ? 1 : 0, a->renderer.FluidTime(), a->history.Count());
            return;
        }
        if (have && frameCounter >= 16 && frameCounter <= 70 + hold) {
            const int f = frameCounter - 16;
            float t = f < 27 ? f / 54.0f : f < 27 + hold ? 0.5f : (f - hold) / 54.0f;
            t = std::min(t, 1.0f);
            io.AddMousePosEvent(p0.x + (p1.x - p0.x) * t, p0.y + (p1.y - p0.y) * t + 6 * std::sin(t * 9.0f));
            if (frameCounter == 17) io.AddMouseButtonEvent(0, true);
        }
        if (frameCounter == 71 + hold) io.AddMouseButtonEvent(0, false);
        if (frameCounter == 72 + hold && have) io.AddMousePosEvent(p1.x + 300, p1.y + 300);
        if (frameCounter % 60 == 0 && frameCounter >= 60)
            Log("[fluid] frame %d: active %d, wet %.2f s, %d substeps, sim %ux%u, portals %d, drops falling %zu landed %d, history %zu, status %s", frameCounter, fluidRun.active ? 1 : 0,
                a->renderer.FluidTime(), fluidRun.substeps, fluidRun.simW, fluidRun.simH, a->renderer.FluidPortalCount(), fluidRun.drops.size(), fluidRun.landed, a->history.Count(), statusMsg.c_str());
        return;
    }
    if (demoMode == 104) {
        // Undo pixels in system memory: a 4096 x 4096 document, 12 destructive steps (256 MB of undo data each),
        // the older ones moved off the GPU; everything undone then redone, compared with the original / final image.
        static gpu::ReadbackPtr original, final_, undone, redone;
        static int step = 0;
        DocView* dv = Active();
        if (frameCounter == 4) {
            if (char mb[16] = ""; GetEnvironmentVariableA("SX_HISTGPU", mb, sizeof(mb)) > 0) {
                histGpuMB = atoi(mb);  // as set in History > Memory settings
                ApplyHistoryLimits();
            }
            NewDocument(4096, 4096, Precision::Half16, DocMode::Layers, 0, "Historique");
            dv = Active();
            Document& d = *dv->doc;
            Layer& l = d.AddPixelLayer("Peinture");
            d.S().activeLayer = l.id;
            d.SyncGraph();
        }
        if (!dv) return;
        Document& d = *dv->doc;
        auto grab = [&] { return dv->renderer.ExportRGBA8(*ctx, dv->renderer.EvaluateComplete(d, *ctx)); };
        if (frameCounter == 10) {
            ApplyLibraryEffect(*dv, "checker", FxApply::Destructive);  // some content
            dv->history.Clear();
        }
        if (frameCounter == 14) original = grab();
        if (frameCounter >= 20 && frameCounter < 20 + 12 * 12 && (frameCounter - 20) % 12 == 0) {
            ApplyLibraryEffect(*dv, (step % 2) ? "invert" : "posterize", FxApply::Destructive);
            ++step;
            Log("[undoram] step %d: GPU %.0f MB, system %.0f MB, VRAM %.0f MB", step, dv->history.MemoryBytes() / 1048576.0,
                dv->history.SystemBytes() / 1048576.0, gpu::Dev().VideoMemoryUsage() / 1048576.0);
        }
        if (frameCounter == 200 || frameCounter == 320) {
            Log("[undoram] after the steps: %zu entries, GPU %.0f MB, system %.0f MB, VRAM %.0f MB", dv->history.Count(),
                dv->history.MemoryBytes() / 1048576.0, dv->history.SystemBytes() / 1048576.0, gpu::Dev().VideoMemoryUsage() / 1048576.0);
            if (frameCounter == 320) final_ = grab();
        }
        if (frameCounter == 324) {
            while (dv->history.CanUndo()) Undo();
        }
        if (frameCounter == 328) undone = grab();
        if (frameCounter == 332) {
            while (dv->history.CanRedo()) Redo();
        }
        if (frameCounter == 336) redone = grab();
        if (frameCounter == 340 && GetEnvironmentVariableA("SX_HISTUI", nullptr, 0) > 0) {
            ShowSideTab(0);
            FocusPanel("Historique");
        }
        if (frameCounter == 346 && GetEnvironmentVariableA("SX_HISTUI", nullptr, 0) > 0) demoOpenHistMem = true;
        if (frameCounter == 350 && original && undone && final_ && redone) {
            CompareReadbacks("undo all vs original", *original, *undone, 0);
            CompareReadbacks("redo all vs final", *final_, *redone, 0);
        }
        return;
    }
    if (demoMode == 103 && v) {
        // Native PSD adjustment layers: 7 adjustments with known settings, exported, imported back, compared.
        Document& d = *v->doc;
        static std::string psdPath;
        struct Case {
            const char* id;
            std::vector<float> params;
        };
        static const Case cases[] = {
            {"brightness_contrast", {0.2f, 0.3f}},
            {"levels", {20 / 255.0f, 230 / 255.0f, 1.4f, 10 / 255.0f, 245 / 255.0f}},
            {"hue_saturation", {30, -20, 10, 0}},
            {"invert", {}},
            {"posterize", {6}},
            {"threshold", {100 / 255.0f, 0}},
            {"exposure", {0.75f, 0.05f, 1.2f}},
            {"curves", {}},
        };
        if (frameCounter == 6) {
            for (const Case& c : cases) {
                Layer& l = d.AddEffectLayer(LayerKind::Adjustment, c.id);
                for (size_t k = 0; k < c.params.size() && k < l.effect.params.size(); ++k) l.effect.params[k].x = c.params[k];
                if (std::string(c.id) == "curves") {
                    // An S curve on the composite, a lift on the red.
                    l.effect.curves.resize(4);
                    l.effect.curves[0] = {{0, 0}, {64 / 255.0f, 40 / 255.0f}, {192 / 255.0f, 215 / 255.0f}, {1, 1}};
                    l.effect.curves[1] = {{0, 30 / 255.0f}, {1, 1}};
                }
                l.name = std::string("Réglage ") + c.id;
                l.visible = false;  // the export stays readable
            }
            d.SyncGraph();
            char dir[512] = "";
            GetEnvironmentVariableA("SX_OUTDIR", dir, sizeof(dir));
            psdPath = std::string(dir[0] ? dir : ".") + "\\adjustments.psd";
            ExportPsd(psdPath, 8);
        }
        if (frameCounter > 8 && !psdExport && !psdPath.empty()) {
            PsdImportReport rep;
            std::string err;
            auto back = ImportPsd(psdPath, v->renderer, *ctx, rep, &err);
            int same = 0, total = 0;
            if (back)
                for (const Case& c : cases) {
                    ++total;
                    for (auto& l : back->S().layers) {
                        if (l.kind != LayerKind::Adjustment || l.effect.effectId != c.id) continue;
                        bool ok = true;
                        for (size_t k = 0; k < c.params.size() && k < l.effect.params.size(); ++k)
                            if (std::abs(l.effect.params[k].x - c.params[k]) > 0.01f) {
                                ok = false;
                                Log("[psdadj] %s param %zu: %.4f -> %.4f", c.id, k, c.params[k], l.effect.params[k].x);
                            }
                        if (std::string(c.id) == "curves") {
                            // Same points on the composite and red curves.
                            for (int ch : {0, 1}) {
                                const CurvePoints& want = ch == 0 ? CurvePoints{{0, 0}, {64 / 255.0f, 40 / 255.0f}, {192 / 255.0f, 215 / 255.0f}, {1, 1}}
                                                                 : CurvePoints{{0, 30 / 255.0f}, {1, 1}};
                                const CurvePoints got = ch < (int)l.effect.curves.size() ? l.effect.curves[ch] : CurvePoints{};
                                bool eq = got.size() == want.size();
                                for (size_t q = 0; eq && q < got.size(); ++q)
                                    eq = std::abs(got[q].x - want[q].x) < 0.003f && std::abs(got[q].y - want[q].y) < 0.003f;
                                if (!eq) {
                                    ok = false;
                                    Log("[psdadj] curves channel %d: %zu points read back", ch, got.size());
                                }
                            }
                        }
                        same += ok;
                        break;
                    }
                }
            Log("[psdadj] %s: %d adjustments read back, %d / %d identical %s", psdPath.c_str(), rep.adjustments, same, total, err.c_str());
            psdPath.clear();
        }
        return;
    }
    if (demoMode == 102 && v) {
        // Custom shapes: every built-in shape as a shape layer on a grid (fill + stroke), one rotated, then the tool options.
        Document& d = *v->doc;
        if (frameCounter == 6) {
            const auto& shapes = BuiltinShapes();
            const int cols = 5;
            const float cw = d.Width() / (float)cols, ch = d.Height() / (float)((shapes.size() + cols - 1) / cols);
            for (size_t i = 0; i < shapes.size(); ++i) {
                ShapeStyle st;
                st.type = 5;
                st.custom = shapes[i].path;
                st.customName = shapes[i].name;
                const float x = (i % cols) * cw, y = (i / cols) * ch;
                st.rect = {x + cw * 0.15f, y + ch * 0.12f, x + cw * 0.85f, y + ch * 0.88f};
                st.fill = {0.25f + 0.05f * (i % 4), 0.5f, 0.9f - 0.04f * i, 1};
                st.strokeOn = (i % 3) == 0;
                st.stroke = {0.05f, 0.05f, 0.1f, 1};
                st.strokeWidth = 6;
                if (i == 1) st.rotation = -20;
                d.AddShapeLayer(st);
            }
            d.SyncGraph();
            tool = Tool::Shape;
            shapeDefaults.type = 5;
            Log("[shapes] %zu shape layers", shapes.size());
            // SVG reading (not saved): a path with relative commands and an arc, and a polygon.
            auto svg = ParseSvgFile("<svg><path fill='red' d='m10 10 h80 v30 q0 10 -10 10 a5 5 0 0 1 -5 5 H10 z'/><polygon points='0,0 50,0 25,40'/></svg>");
            size_t pts = 0;
            for (auto& sp : svg) pts += sp.pts.size();
            {
                // Arcs with compact flags (svgo output): a circle of radius 10 around (10, 10) from two half arcs.
                auto circle = ParseSvgFile("<svg><path d='M0 10a10 10 0 1120 0a10 10 0 11-20 0z'/></svg>");
                float worst = 0;
                size_t n = 0;
                for (auto& sp : circle)
                    for (auto& q : sp.pts) {
                        ++n;
                        worst = std::max(worst, std::abs(Length(q.p - float2{10, 10}) - 10.0f));
                    }
                Log("[shapes] svg arc circle: %zu subpaths, %zu points, worst radius error %.4f", circle.size(), n, worst);
            }
            {
                // The custom outlines through the document format.
                Json j = SerializeDocument(d);
                std::string err;
                auto back = DeserializeDocument(j, {}, &err);
                int same = 0, total = 0;
                if (back)
                    for (auto& l : d.S().layers)
                        if (l.kind == LayerKind::Shape)
                            for (auto& l2 : back->S().layers)
                                if (l2.id == l.id) { ++total; same += l2.shape.CustomHash() == l.shape.CustomHash() && l2.shape.customName == l.shape.customName && l2.shape.type == 5; }
                Log("[shapes] document round trip: %d / %d custom shapes identical %s", same, total, err.c_str());
            }
            Log("[shapes] svg: %zu subpaths, %zu points, closed %d %d", svg.size(), pts, svg.size() > 0 && svg[0].closed, svg.size() > 1 && svg[1].closed);
        }
        if (frameCounter == 10) ShowSideTab(1);
        if (frameCounter == 16) FocusPanel("Options");
        return;
    }
    if (demoMode == 101) {
        // Image analysis: SX_FILE opened, the panel on tab SX_ANTAB (or the 4 views with SX_ANQUAD), source SX_ANSRC.
        char file[512] = "", tab[8] = "0";
        GetEnvironmentVariableA("SX_FILE", file, sizeof(file));
        GetEnvironmentVariableA("SX_ANTAB", tab, sizeof(tab));
        if (frameCounter == 4 && file[0]) OpenImage(file, false);
        if (frameCounter == 30) {
            panelOpen["Analyse d'image"].open = true;
            analysis.quad = GetEnvironmentVariableA("SX_ANQUAD", nullptr, 0) > 0;
            analysis.waveMode = GetEnvironmentVariableA("SX_ANWAVE", nullptr, 0) > 0 ? 2 : 0;
            analysis.source = GetEnvironmentVariableA("SX_ANSRC", nullptr, 0) > 0 ? 1 : 0;
            char ov[8] = "0";
            GetEnvironmentVariableA("SX_ANOV", ov, sizeof(ov));
            analysis.overlay = atoi(ov);
        }
        if (frameCounter == 34) {
            ShowSideTab(0);
            FocusPanel("Analyse d'image");
            if (GetEnvironmentVariableA("SX_ANSEL", nullptr, 0) > 0 && v) {
                // Selection on the face; two samplers (face, shirt).
                Document& dd = *v->doc;
                const float W = (float)dd.Width(), H = (float)dd.Height();
                v->renderer.SelectShape(dd, *ctx, v->history, SelShape::Ellipse, {W * 0.38f, H * 0.18f, W * 0.62f, H * 0.62f}, SelMode::Replace, true, 0, "Ellipse");
                analysis.selectionOnly = true;
                analysis.samplers.clear();
                AnalysisSampler a, b;
                a.pos = {std::floor(W * 0.47f), std::floor(H * 0.45f)};
                b.pos = {std::floor(W * 0.30f), std::floor(H * 0.85f)};
                analysis.samplers.push_back(a);
                analysis.samplers.push_back(b);
            }
            if (GetEnvironmentVariableA("SX_ANCMP", nullptr, 0) > 0 && v)
                for (auto& l : v->doc->S().layers)
                    if (l.id != v->doc->S().activeLayer && l.name != "Arrière-plan") { analysis.compareLayer = l.id; break; }
            analysis.demoTab = atoi(tab);
        }
        if (char ab[8] = ""; GetEnvironmentVariableA("SX_AB", ab, sizeof(ab)) > 0 && v) {
            // Before / after: snapshot, then an effect on the layer, compared (1 split, 2 before, 3 difference).
            if (frameCounter == 40) TakeSnapshot(*v);
            if (frameCounter == 44) ApplyLibraryEffect(*v, Effects().Find("sepia") ? "sepia" : "invert", FxApply::Filter);
            if (frameCounter == 46) {
                abCompare.mode = atoi(ab);
                abCompare.split = 0.45f;
                abCompare.on = true;
            }
        }
        if (frameCounter == 80) {
            Log("[analysis] valid %d, %u x %u, luma mean %.3f sd %.3f, palette %zu", (int)analysis.valid, analysis.w, analysis.h,
                analysis.stats[3].mean, analysis.stats[3].sd, analysis.palette.size());
            Log("[analysis] sharpness %.2f noise %.2f; compare %u: mean %.2f max %.1f psnr %.1f changed %.3f", analysis.sharpness, analysis.noise,
                analysis.compareLayer, analysis.diffMean, analysis.diffMax, analysis.psnr, analysis.changedShare);
        }
        return;
    }
    if (demoMode == 96) {
        // Camera Raw: a RAW (SX_RAW) decoded by LibRaw, the Camera Raw window with a development and the
        // lens corrections preselected from the shot (lensfun base SX_LENSFUN). Without SX_RAW: a checker
        // with the "Correction d'objectif" effect simulating the lens SX_LENS (SX_SIMMODE 1) on it.
        char raw[512] = "", lf[512] = "", lensName[128] = "", bdir[512] = "";
        if (GetEnvironmentVariableA("SX_BATCHDIR", bdir, sizeof(bdir)) > 0) {
            // Batch development: an action "photo_raw" (settings + auto + lens) on a folder of RAW, exported as JPG.
            if (frameCounter == 4) {
                if (GetEnvironmentVariableA("SX_LENSFUN", lf, sizeof(lf)) > 0) lensfunDir = lf;
                EnsureLensfun(true);
                Json p = Json::Object(), st = Json::Object();
                st["vibrance"] = 30;
                st["clarity"] = 20;
                p["settings"] = st;
                p["auto"] = true;
                p["lens"] = true;
                actions.push_back({"Demo Photo RAW", {ActionStep{"photo_raw", p, true}}, ""});
                batch.sourceKind = 0;
                batch.source = bdir;
                batch.dest = std::string(bdir) + "\\out";
                batch.chain = {"u:Demo Photo RAW"};
                char bf[8] = "2";
                GetEnvironmentVariableA("SX_BATCHFMT", bf, sizeof(bf));
                batch.format = atoi(bf);
                StartBatch();
            }
            static bool reported = false;
            if (!batch.running && frameCounter > 10 && !reported) {
                reported = true;
                for (auto& e : batch.log) Log("[demo] batch: %s -> %s (%s)", e.file.c_str(), e.out.c_str(), e.msg.c_str());
            }
            return;
        }
        const bool hasRaw = GetEnvironmentVariableA("SX_RAW", raw, sizeof(raw)) > 0;
        GetEnvironmentVariableA("SX_LENS", lensName, sizeof(lensName));
        if (frameCounter == 4) {
            if (GetEnvironmentVariableA("SX_LENSFUN", lf, sizeof(lf)) > 0) lensfunDir = lf;
            EnsureLensfun(true);
            Log("[demo] lensfun: %zu lenses, %zu cameras (%s), LibRaw %d", Lensfun().lenses.size(), Lensfun().cameras.size(), Lensfun().Dir().c_str(), (int)HaveLibRaw());
            if (hasRaw) {
                RawInfo info;
                Log("[demo] raw info: %d %s", (int)ReadRawInfo(raw, info), info.Summary().c_str());
                OpenImage(raw, false);
            } else {
                NewDocument(1800, 1200, Precision::Half16, DocMode::Layers, 0, "Objectif");
                Document& d = *Active()->doc;
                Layer& f = d.AddEffectLayer(LayerKind::Fill, "checker");
                d.S().activeLayer = f.id;
                d.SyncGraph();
            }
        }
        if (frameCounter == 12 && Active()) {
            DocView& v = *Active();
            Document& d = *v.doc;
            Log("[demo] document %ux%u, shot: %s", d.Width(), d.Height(), d.shot.summary.c_str());
            Layer* l = d.ActiveLayer();
            if (hasRaw) {
                OpenCameraRaw(v);
                l = d.ActiveLayer();
                for (auto& f : l->filters)
                    if (f.effectId == "camera_raw") {
                        auto def = f.Def();
                        for (size_t i = 0; i < def->params.size(); ++i) {
                            const std::string& id = def->params[i].id;
                            if (id == "exposure") f.params[i].x = 0.6f;
                            if (id == "shadows") f.params[i].x = 35;
                            if (id == "highlights") f.params[i].x = -30;
                            if (id == "clarity") f.params[i].x = 20;
                            if (id == "vibrance") f.params[i].x = 25;
                            if (id == "nr_luma") f.params[i].x = 30;
                            if (id == "nr_color") f.params[i].x = 40;
                            if (id == "mix_s_blue") f.params[i].x = 30;
                            if (id == "cg_sh_sat") f.params[i].x = 20;
                        }
                    }
                EffectInstance fx;
                fx.effectId = "lens_profile";
                fx.params = Effects().Find("lens_profile")->DefaultParams();
                l->filters.insert(l->filters.begin(), fx);
                lensPick.autoFor = nullptr;
                d.SyncGraph();
            } else if (l) {
                EffectInstance fx;
                fx.effectId = "lens_profile";
                fx.params = Effects().Find("lens_profile")->DefaultParams();
                char sm[8] = "1";
                GetEnvironmentVariableA("SX_SIMMODE", sm, sizeof(sm));
                fx.params[0].x = (float)atoi(sm);
                lensPick.lens = -1;
                if (lensName[0]) {
                    const LensfunLens* L = Lensfun().FindLens(lensName, "");
                    if (L) {
                        lensPick.lens = (int)(L - Lensfun().lenses.data());
                        lensPick.focal = L->minFocal;
                        lensPick.aperture = 2.8f;
                        lensPick.crop = L->crop;
                        ApplyLensPick(fx);
                    }
                }
                Log("[demo] simulated lens: %s (%s)", fx.note.c_str(), lensPick.status.c_str());
                l->filters.push_back(fx);
                if (GetEnvironmentVariableA("SX_ROUNDTRIP", nullptr, 0) > 0) {
                    // Then corrected with the same profile (no fill zoom): the checker should come back straight.
                    fx.params[0].x = 0;
                    for (size_t i = 0; i < fx.params.size(); ++i)
                        if (fx.Def()->params[i].id == "autoscale") fx.params[i].x = 0;
                    l->filters.push_back(fx);
                }
                d.SyncGraph();
                FocusPanel("Propriétés");
            }
        }
        if (frameCounter == 40 && hasRaw && lensName[0] && Active()) {
            // No lens in the metadata (manual lens): chosen by name, as in the list.
            Layer* l = Active()->doc->ActiveLayer();
            if (const LensfunLens* L = Lensfun().FindLens(lensName, ""))
                for (auto& f : l->filters)
                    if (f.effectId == "lens_profile") {
                        lensPick.lens = (int)(L - Lensfun().lenses.data());
                        lensPick.focal = L->minFocal;
                        lensPick.aperture = 4;
                        lensPick.crop = 1.613f;  // EOS 550D
                        ApplyLensPick(f);
                        Active()->doc->SyncGraph();
                    }
        }
        if (frameCounter == 30 && hasRaw && Active() && GetEnvironmentVariableA("SX_LOCAL", nullptr, 0) > 0) {
            // Local adjustments: a graduated filter darkening / cooling the top, a radial one brightening the face.
            Layer* l = Active()->doc->ActiveLayer();
            auto def = Effects().Find("photo_raw_local");
            auto set = [&](EffectInstance& f, const char* id, float4 v4) {
                for (size_t i = 0; i < def->params.size(); ++i)
                    if (def->params[i].id == id) f.params[i] = v4;
            };
            EffectInstance g;
            g.effectId = "photo_raw_local";
            g.params = def->DefaultParams();
            set(g, "p0", {0.5f, 0.0f, 0, 0});
            set(g, "p1", {0.5f, 0.45f, 0, 0});
            set(g, "exposure", {-1.2f, 0, 0, 0});
            set(g, "temperature", {-25, 0, 0, 0});
            l->filters.push_back(g);
            EffectInstance r = g;
            r.params = def->DefaultParams();
            set(r, "kind", {1, 0, 0, 0});
            set(r, "p0", {0.46f, 0.4f, 0, 0});
            set(r, "radius", {0.1f, 0.22f, 0, 0});
            set(r, "angle", {-10, 0, 0, 0});
            set(r, "exposure", {0.8f, 0, 0, 0});
            set(r, "shadows", {30, 0, 0, 0});
            l->filters.push_back(r);
            camRaw.localSel = GetEnvironmentVariableA("SX_LOCALSEL", nullptr, 0) > 0 ? 0 : 1;
            Active()->doc->SyncGraph();
        }
        if (hasRaw && Active() && GetEnvironmentVariableA("SX_LOCALDRAG", nullptr, 0) > 0 && frameCounter >= 60 && frameCounter <= 72) {
            // Synthetic drag of the gradient's point B, 120 px down (ImGui events only).
            Document& d = *Active()->doc;
            static ImVec2 b;
            ImGuiIO& io = ImGui::GetIO();
            if (frameCounter == 60) {
                for (auto& f : d.ActiveLayer()->filters)
                    if (f.effectId == "photo_raw_local") {
                        auto def = f.Def();
                        for (size_t i = 0; i < def->params.size(); ++i)
                            if (def->params[i].id == "p1")
                                b = ImVec2(camRaw.viewPos.x + (f.params[i].x * d.Width() - camRaw.viewOrigin.x) / camRaw.viewInv,
                                           camRaw.viewPos.y + (f.params[i].y * d.Height() - camRaw.viewOrigin.y) / camRaw.viewInv);
                        break;
                    }
                io.AddMousePosEvent(b.x, b.y);
            }
            if (frameCounter == 62) io.AddMouseButtonEvent(0, true);
            if (frameCounter == 64) io.AddMousePosEvent(b.x, b.y + 60);
            if (frameCounter == 66) io.AddMousePosEvent(b.x, b.y + 120);
            if (frameCounter == 68) io.AddMouseButtonEvent(0, false);
            if (frameCounter == 72)
                for (auto& f : d.ActiveLayer()->filters)
                    if (f.effectId == "photo_raw_local") {
                        auto def = f.Def();
                        for (size_t i = 0; i < def->params.size(); ++i)
                            if (def->params[i].id == "p1") Log("[demo] local drag: point B now %.3f %.3f (pan %.3f %.3f)", f.params[i].x, f.params[i].y, camRaw.pan.x, camRaw.pan.y);
                        break;
                    }
        }
        if (frameCounter == 50 && Active() && GetEnvironmentVariableA("SX_SELFILTER", nullptr, 0) > 0) {
            // The camera_raw filter node of the active layer selected in the graph (as a click on it does).
            camRaw.open = false;
            Document& d = *Active()->doc;
            for (auto& n : d.S().graph.nodes)
                if (n.layer == d.S().activeLayer && n.fx.effectId == "camera_raw") d.S().activeNode = n.id;
            FocusPanel("Propriétés");
            Log("[demo] selected filter node %u", (unsigned)d.S().activeNode);
        }
        if (frameCounter == 30 && Active() && GetEnvironmentVariableA("SX_GEOM", nullptr, 0) > 0) {
            // Geometry: vertical perspective 40, rotation 5 degrees, grid, frame filled.
            Layer* l = Active()->doc->ActiveLayer();
            EffectInstance g;
            g.effectId = "photo_raw_geometry";
            auto def = Effects().Find("photo_raw_geometry");
            g.params = def->DefaultParams();
            for (size_t i = 0; i < def->params.size(); ++i) {
                if (def->params[i].id == "vertical") g.params[i].x = 40;
                if (def->params[i].id == "rotate") g.params[i].x = 5;
                if (def->params[i].id == "grid") g.params[i].x = 1;
            }
            l->filters.insert(l->filters.begin(), g);
            Active()->doc->SyncGraph();
        }
        if (frameCounter == 45 && hasRaw && Active() && GetEnvironmentVariableA("SX_AUTO", nullptr, 0) > 0)
            PhotoRawAuto(*Active(), *Active()->doc->ActiveLayer(), 3);  // white balance + tones
        if (frameCounter == 45 && hasRaw && Active() && GetEnvironmentVariableA("SX_WBPICK", nullptr, 0) > 0) {
            // Eyedropper on the white letters of the shirt (doc coordinates of the test photo).
            Document& d = *Active()->doc;
            Surface* s = d.GetSurface(d.ActiveLayer()->surface);
            Rect r{3202, 2881, 3207, 2886};
            if (s && s->tex) camRaw.wbRb = ctx->ReadbackTexture(s->tex.get(), &r);
        }
        if (frameCounter == 70 && Active()) {
            Layer* l = Active()->doc->ActiveLayer();
            for (auto& f : l->filters) {
                std::string ps;
                for (size_t i = 0; i < f.params.size() && i < 24; ++i) {
                    char b[32];
                    snprintf(b, sizeof(b), "%.4g ", f.params[i].x);
                    ps += b;
                }
                Log("[demo] filter %s enabled %d note '%s' params %s", f.effectId.c_str(), (int)f.enabled, f.note.c_str(), ps.c_str());
            }
            Log("[demo] lens pick %d (%s), focal %.1f f/%.1f crop %.2f, camera raw open %d", lensPick.lens,
                lensPick.lens >= 0 ? Lensfun().lenses[lensPick.lens].Name().c_str() : "-", lensPick.focal, lensPick.aperture, lensPick.crop, (int)camRaw.open);
        }
        return;
    }
    if (demoMode == 95) {
        // Search everywhere: index built (menus opened invisibly, tabs visited), queries, a menu result
        // reached (Plugins > Gestionnaire de plugins...), then the results list of SX_QUERY shown.
        auto& S = GlobalSearch::Get();
        if (frameCounter == 4) NewDocument(1200, 800, Precision::Half16, DocMode::Layers, 0, "Recherche");
        static int phase = 0;
        static u64 at = 0;
        if (phase == 0 && frameCounter > 50 && !S.Crawling() && searchCrawlChecked) {
            int counts[3] = {};
            for (auto* e : S.Query(" ", 100000)) (void)e;
            phase = 1;
            at = frameCounter;
            for (const char* q : {"flou", "export", "opacite", "gestionnaire plugins", "calques", "rayon"}) {
                auto r = S.Query(q, 5);
                std::string list;
                for (auto* e : r) list += "[" + std::to_string(e->kind) + "] " + e->label + " (" + e->context + ") | ";
                Log("[demo] search '%s': %s", q, list.c_str());
            }
            (void)counts;
            Log("[demo] search index: %zu entries", S.Size());
            auto r = S.Query("gestionnaire de plugins", 1);
            if (!r.empty()) ActivateSearchResult(*r[0]);
        }
        if (phase == 1 && frameCounter == at + 40) {
            Log("[demo] plugin manager open after the menu result: %d", (int)pluginManager);
            pluginManager = false;
            char q[64] = "flou";
            GetEnvironmentVariableA("SX_QUERY", q, sizeof(q));
            if (GetEnvironmentVariableA("SX_CONTENT", nullptr, 0) > 0) {
                // A content result run: the gaussian blur effect added to the active layer.
                BuildSearchRows("flou gaussien");
                for (auto& r : searchRows)
                    if (r.group == 3) {
                        RunSearchRow(r);
                        break;
                    }
                const Layer* l = Active() ? Active()->doc->ActiveLayer() : nullptr;
                Log("[demo] content result: filters on the active layer %zu, recent %zu", l ? l->filters.size() : 0, recentSearches.size());
            }
            char go[64] = "";
            if (GetEnvironmentVariableA("SX_GOTO", go, sizeof(go)) > 0) {
                // A property reached: its window forward, scrolled to it, blinking.
                auto r = S.Query(go, 1);
                if (!r.empty()) {
                    Log("[demo] goto %s -> %s (%s)", go, r[0]->label.c_str(), r[0]->context.c_str());
                    ActivateSearchResult(*r[0]);
                }
            } else {
                searchText = q;
                searchOpen = true;
            }
            phase = 2;
        }
        return;
    }
    if (demoMode == 94) {
        // Plugins: the example plugin's command, window, tool (synthetic clicks on the canvas) and operation,
        // then the example script plugin's two commands (Python through the bridge).
        char palImg[512] = "";
        const bool palette = GetEnvironmentVariableA("SX_PALIMG", palImg, sizeof(palImg)) > 0;
        if (frameCounter == 4) {
            if (palette) OpenImage(palImg, false);
            else NewDocument(1200, 800, Precision::Half16, DocMode::Layers, 1, "Plugins");
        }
        if (palette) {
            // Palette plugin: its window, an extraction through its operation, the colors logged.
            if (frameCounter == 10) {
                for (auto& pp : pluginPanels) pp.open = pp.title == "Palette";
                if (const AiOperation* op = bridge.Find("palette.extraire")) op->run(Json::Object(), PluginWorkspace("demo"));
            }
            if (frameCounter == 20)
                if (const AiOperation* op = bridge.Find("palette.extraire")) {
                    AiResult r = op->run(Json::Object(), PluginWorkspace("demo"));
                    Log("[demo] palette: %s", r.result.Dump(0).c_str());
                }
            return;
        }
        DocView* a = Active();
        if (!a) return;
        if (frameCounter == 8) {
            std::string list;
            for (auto& p : plugins) list += p.name + (p.loaded ? " (chargé)" : " (" + p.error + ")") + ", ";
            Log("[demo] plugins: %s commands %zu panels %zu tools %zu ops %zu", list.c_str(), pluginCommands.size(), pluginPanels.size(), pluginTools.size(), pluginOps.size());
            for (auto& c : pluginCommands)
                if (c.path == "Exemple/Dire bonjour") CallPlugin(c.plugin, [&] { c.fn(c.user); });
            Log("[demo] after the command: %s", statusMsg.c_str());
            for (auto& pp : pluginPanels) pp.open = true;
            // New pixel layer to paint on.
            if (const AiOperation* op = bridge.Find("shaderixel.layer_add")) {
                Json prm = Json::Object();
                prm["kind"] = "pixel";
                prm["name"] = "Étoiles";
                op->run(prm, PluginWorkspace("demo"));
            }
        }
        ImGuiIO& io = ImGui::GetIO();
        ImGuiWindow* cw = ImGui::FindWindowByName("Canevas");
        if (frameCounter == 12) {
            for (size_t i = 0; i < pluginTools.size(); ++i)
                if (pluginTools[i].name == "\xC3\x89toile") { tool = Tool::Plugin; pluginTool = (int)i; }
            if (cw) io.AddMousePosEvent(cw->InnerRect.Min.x + (cw->InnerRect.Max.x - cw->InnerRect.Min.x) * 0.35f, (cw->InnerRect.Min.y + cw->InnerRect.Max.y) * 0.5f);
        }
        if (frameCounter == 14) io.AddMouseButtonEvent(0, true);
        if (frameCounter == 16) io.AddMouseButtonEvent(0, false);
        if (frameCounter == 20) {
            // The plugin's operation, as the assistant would call it.
            if (const AiOperation* op = bridge.Find("exemple.etoile")) {
                Json prm = Json::Object();
                prm["x"] = 800;
                prm["y"] = 400;
                prm["radius"] = 150;
                AiResult r = op->run(prm, PluginWorkspace("demo"));
                Log("[demo] exemple.etoile: %s %s", r.errorCode.c_str(), r.result.Dump(0).c_str());
            }
            if (cw) io.AddMousePosEvent(cw->InnerRect.Min.x + (cw->InnerRect.Max.x - cw->InnerRect.Min.x) * 0.55f, cw->InnerRect.Min.y + (cw->InnerRect.Max.y - cw->InnerRect.Min.y) * 0.3f);
        }
        if (frameCounter == 24) {
            for (auto& p : plugins)
                for (auto& sc : p.scripts) RunPluginScript(p, sc);
        }
        if (frameCounter == 22 && GetEnvironmentVariableA("SX_PIXELS", nullptr, 0) > 0) {
            // Pixel API (plugin command): the active layer (stars) read and written back inverted.
            for (auto& c : pluginCommands)
                if (c.path == "Exemple/Inverser les couleurs du calque") CallPlugin(c.plugin, [&] { c.fn(c.user); });
        }
        if (frameCounter == 25 && GetEnvironmentVariableA("SX_PIXELS", nullptr, 0) > 0)
            Log("[demo] pixels: %s, undo step %s", statusMsg.c_str(), a->history.Count() ? a->history.Label(a->history.Count() - 1).c_str() : "-");
        if (GetEnvironmentVariableA("SX_PLUGCREATE", nullptr, 0) > 0) {
            // A Python window plugin created from the skeleton, then a plugin installed from a .zip (SX_PLUGZIP).
            if (frameCounter == 10) {
                std::string dir, err;
                const bool ok = CreatePluginSkeleton("Mon essai", 0, &dir, &err);
                Log("[demo] skeleton: %d %s %s", (int)ok, dir.c_str(), err.c_str());
                std::string dir2;
                Log("[demo] native skeleton: %d", (int)CreatePluginSkeleton("Mon natif", 2, &dir2, &err));
                for (auto& sp : scriptPanels) sp.open = sp.title == "Mon essai";
                char zip[512] = "";
                if (GetEnvironmentVariableA("SX_PLUGZIP", zip, sizeof(zip)) > 0) {
                    const bool zok = InstallPluginZip(zip, &err);
                    Log("[demo] zip install: %d %s", (int)zok, err.c_str());
                }
                std::string list;
                for (auto& p : plugins) list += p.name + (p.loaded ? "+ " : "- ");
                Log("[demo] plugins now: %s", list.c_str());
            }
            if (frameCounter == 80)
                for (auto& sp : scriptPanels)
                    if (sp.title == "Mon essai") Log("[demo] skeleton window: %zu widgets %s", sp.ui.Size(), sp.error.c_str());
            return;
        }
        if (GetEnvironmentVariableA("SX_SCRIPTPANEL", nullptr, 0) > 0) {
            // Script window (Python): opened, then its "Dessiner la grille" button pressed (event sent as the window does).
            for (auto& sp : scriptPanels) {
                if (frameCounter == 10) sp.open = true;
                if (frameCounter == 60 && sp.process) {
                    Json m = Json::Object();
                    m["event"] = "click";
                    m["id"] = "draw";
                    SendScriptPanel(sp, m);
                    Log("[demo] script panel %s: %zu widgets", sp.title.c_str(), sp.ui.Size());
                }
                if (frameCounter == 200) Log("[demo] script panel after: %s / %s", statusMsg.c_str(), sp.error.c_str());
                auto grids = [&] {
                    int n = 0;
                    for (auto& l : a->doc->S().layers) n += l.name == "Grille" ? 1 : 0;
                    return n;
                };
                if (frameCounter == 205 && sp.process && GetEnvironmentVariableA("SX_SCRIPTUNDO", nullptr, 0) > 0) {
                    Log("[demo] grid layers before undo: %d", grids());
                    Json m = Json::Object();
                    m["event"] = "click";
                    m["id"] = "undo";
                    SendScriptPanel(sp, m);
                }
                if (frameCounter == 240 && GetEnvironmentVariableA("SX_SCRIPTUNDO", nullptr, 0) > 0) Log("[demo] grid layers after one undo: %d", grids());
            }
            return;
        }
        if (frameCounter == 26) {
            // Hot reload of the native plugin (as the manager's "Recharger" button).
            for (auto& p : plugins)
                if (!p.dll.empty()) {
                    UnloadPlugin(p);
                    const bool ok = LoadPlugin(p);
                    Log("[demo] reload %s: %d %s, tools %zu panels %zu", p.name.c_str(), (int)ok, p.error.c_str(), pluginTools.size(), pluginPanels.size());
                }
            for (auto& pp : pluginPanels) pp.open = true;
            pluginManager = true;
        }
        static bool logged = false;
        if (frameCounter > 30 && pluginProcs.empty() && !logged) {
            logged = true;
            Log("[demo] scripts done at frame %llu: %s", (unsigned long long)frameCounter, statusMsg.c_str());
            int layers = 0;
            for (auto& l : a->doc->S().layers) layers += l.kind == LayerKind::Pixel ? 1 : 0;
            Log("[demo] pixel layers: %d", layers);
        }
        return;
    }
    if (demoMode == 93) {
        // Previews of the smart materials (thumbs/<name>.png in SX_THUMBDIR, the repository library by default):
        // each one applied to a sphere, the 3D view captured and cropped, its layers removed, the next one.
        static size_t idx = 0;
        static u64 applied = 0;
        static gpu::ReadbackPtr rb;
        static ImVec4 rect;
        static std::vector<Id> before;
        if (frameCounter == 4) {
            p3dNew = {};
            p3dNew.basePreset = 1;
            p3dNew.mesh = 1;  // cube: its edges show the wear
            p3dNew.skipPaintLayer = true;
            NewPaint3DProject();
        }
        DocView* a = Active();
        if (!a || !a->mesh) return;
        const auto& list = SmartMaterials();
        MeshScene& s = *a->mesh;
        if (frameCounter == 8) FocusPanel("Vue 3D");
        if (frameCounter < 12) return;
        if (idx >= list.size()) {
            if (!autoQuit) { Log("[demo] smartthumbs: %zu previews", list.size()); autoQuit = true; }
            return;
        }
        if (!applied) {
            s.yaw = 0.62f;
            s.pitch = 0.5f;
            s.distance = 3.0f;
            s.target = {};
            s.quality = 1;
            s.groundShadow = false;
            ApplyLightingPreset(s, 1);  // studio 3 points
            before = MaterialIds(*a->doc);
            ApplySmartMaterial(*a, list[idx]);
            applied = frameCounter;
            return;
        }
        if (!rb && s.maps && frameCounter > applied + 30) {
            if (ImGuiWindow* w = ImGui::FindWindowByName("Vue 3D")) rect = ImVec4(w->InnerRect.Min.x, w->InnerRect.Min.y, w->InnerRect.Max.x, w->InnerRect.Max.y);
            rb = gpu::Dev().CaptureNextFrame();
            return;
        }
        if (rb && rb->ready) {
            char dir[512] = "";
            std::string out = GetEnvironmentVariableA("SX_THUMBDIR", dir, sizeof(dir)) > 0 ? std::string(dir) : ExeDir() + "/../../library/smartmaterials/thumbs";
            const std::string name = list[idx]["name"].S("");
            const std::string file = out + "/" + name + ".png";
            SaveViewCrop(*rb, rect, file);
            Log("[demo] smartthumbs: %s", file.c_str());
            // Next: the layers of this material removed.
            std::vector<Id> ids;
            for (auto& l : a->doc->S().layers)
                if (l.material && std::find(before.begin(), before.end(), l.material) == before.end()) ids.push_back(l.id);
            for (Id id : ids) a->doc->RemoveLayer(id);
            a->doc->SyncGraph();
            rb.reset();
            applied = 0;
            ++idx;
        }
        return;
    }
    if (demoMode == 88) {
        // Smart material SX_SMART (index, 0 by default) on SX_MESHFILE (cube otherwise): mesh maps baked, masks generated.
        char sm[8] = "0", view[8] = "0";
        GetEnvironmentVariableA("SX_SMART", sm, sizeof(sm));
        GetEnvironmentVariableA("SX_VIEWCH", view, sizeof(view));
        if (frameCounter == 4) {
            p3dNew = {};
            p3dNew.basePreset = 1;
            p3dNew.mesh = 1;
            p3dNew.skipPaintLayer = true;
            char mf[512] = "";
            if (GetEnvironmentVariableA("SX_MESHFILE", mf, sizeof(mf)) > 0) { p3dNew.mesh = 3; p3dNew.meshPath = mf; }
            NewPaint3DProject();
        }
        DocView* a = Active();
        if (!a || !a->mesh) return;
        if (frameCounter == 8) FocusPanel("Vue 3D");
        if (frameCounter == 10) {
            const auto& list = SmartMaterials();
            const int i = std::clamp(atoi(sm), 0, (int)list.size() - 1);
            ApplySmartMaterial(*a, list[i]);
            Log("[demo] smart materials: %zu, applied %s", list.size(), list[i]["name"].S().c_str());
            a->mesh->yaw = 0.6f;
            a->mesh->pitch = 0.45f;
            if (GetEnvironmentVariableA("SX_RT", nullptr, 0) > 0) a->mesh->quality = 2;  // ray-traced view
            char rs[16] = "";
            if (GetEnvironmentVariableA("SX_RTSAMPLES", rs, sizeof(rs)) > 0) a->mesh->rtSamples = atoi(rs);
            if (GetEnvironmentVariableA("SX_DENOISE", rs, sizeof(rs)) > 0) a->mesh->rtDenoise = (float)atof(rs);
            if (GetEnvironmentVariableA("SX_POST", nullptr, 0) > 0) {
                // Cinema look, fixed strong light from the side: bloom, vignette, grain, shadow.
                PostSettings& p = a->mesh->post;
                p.tonemap = 4; p.bloom = 0.8f; p.bloomThreshold = 0.8f; p.contrast = 0.15f; p.saturation = 0.9f;
                p.vignette = 0.5f; p.grain = 0.25f; p.temperature = 0.15f; p.chroma = 0.3f;
                a->mesh->lightFollow = false;
                char ly[16] = "-0.9", lp[16] = "0.6";
                GetEnvironmentVariableA("SX_LYAW", ly, sizeof(ly));
                GetEnvironmentVariableA("SX_LPITCH", lp, sizeof(lp));
                a->mesh->lightYaw = (float)atof(ly);
                a->mesh->lightPitch = (float)atof(lp);
                a->mesh->lightIntensity = 3.5f;
            }
            // Presentation: lighting preset, depth of field, camera bookmark.
            if (GetEnvironmentVariableA("SX_LIGHTS", rs, sizeof(rs)) > 0) ApplyLightingPreset(*a->mesh, atoi(rs));
            if (GetEnvironmentVariableA("SX_DOF", rs, sizeof(rs)) > 0) {
                a->mesh->dofAperture = (float)atof(rs);
                a->mesh->distance = 1.9f;
            }
            if (GetEnvironmentVariableA("SX_FOV", rs, sizeof(rs)) > 0) a->mesh->fov = (float)atof(rs);
            if (GetEnvironmentVariableA("SX_SSAO", rs, sizeof(rs)) > 0) a->mesh->ssao = (float)atof(rs);
            if (GetEnvironmentVariableA("SX_NOAO", nullptr, 0) > 0) a->mesh->aoStrength = 0;  // without the baked occlusion
            char envId[64] = "";
            if (GetEnvironmentVariableA("SX_ENV", envId, sizeof(envId)) > 0) {
                std::string err;
                Log("[demo] environment %s: %d %s", envId, (int)LoadEnvironment(*a->mesh, envId, &err), err.c_str());
            }
        }
        if (frameCounter == 14 && GetEnvironmentVariableA("SX_DOF", nullptr, 0) > 0) {
            // Focus on the nearest corner region (left of center), as a click would.
            focusPick = true;
            if (ImGuiWindow* w = ImGui::FindWindowByName("Vue 3D")) {
                ImGuiIO& io = ImGui::GetIO();
                const ImVec2 c((w->InnerRect.Min.x + w->InnerRect.Max.x) * 0.5f, (w->InnerRect.Min.y + w->InnerRect.Max.y) * 0.5f);
                io.AddMousePosEvent(c.x - (w->InnerRect.Max.x - w->InnerRect.Min.x) * 0.06f, c.y + 30);
            }
        }
        if (frameCounter == 16 && focusPick) ImGui::GetIO().AddMouseButtonEvent(0, true);
        if (frameCounter == 17 && GetEnvironmentVariableA("SX_DOF", nullptr, 0) > 0) {
            ImGui::GetIO().AddMouseButtonEvent(0, false);
            Log("[demo] focus distance %.3f (camera distance %.3f)", a->mesh->dofFocus, a->mesh->distance);
        }
        {
            // Turntable export SX_TURNTABLE (.mp4), SX_TTFRAMES frames at 640 x 360.
            char tp[512] = "";
            if (frameCounter == 30 && GetEnvironmentVariableA("SX_TURNTABLE", tp, sizeof(tp)) > 0) {
                char nf[16] = "24";
                GetEnvironmentVariableA("SX_TTFRAMES", nf, sizeof(nf));
                turntable.path = tp;
                turntable.frames = atoi(nf);
                turntable.width = 640;
                turntable.height = 360;
                turntable.viewSize = false;
                turntable.png = true;
                turntable.gif = GetEnvironmentVariableA("SX_TTGIF", nullptr, 0) > 0;
                turntable.samples = 32;
                turntable.open = true;
                StartTurntable(*a);
            }
            static bool ttLogged = false;
            if (frameCounter > 30 && !ttLogged && GetEnvironmentVariableA("SX_TURNTABLE", nullptr, 0) > 0 && !turntable.running && turntable.pending.empty()) {
                ttLogged = true;
                Log("[demo] turntable done at frame %llu: %s", (unsigned long long)frameCounter, turntable.error.c_str());
            }
        }
        if (frameCounter == 30 && GetEnvironmentVariableA("SX_SMARTGRID", nullptr, 0) > 0) smartGridRequest = true;
        if (GetEnvironmentVariableA("SX_PLUGTOOL3D", nullptr, 0) > 0) {
            // A plugin tool in the 3D view (example Étoile): a click on the model paints on its texture.
            ImGuiIO& io = ImGui::GetIO();
            ImGuiWindow* vw = ImGui::FindWindowByName("Vue 3D");
            if (frameCounter == 40 && vw) {
                for (size_t i = 0; i < pluginTools.size(); ++i)
                    if (pluginTools[i].name == "\xC3\x89toile") { tool = Tool::Plugin; pluginTool = (int)i; }
                // The paint layer of the project as target.
                AddPaintLayer3D(*a, "Plugin");
                io.AddMousePosEvent((vw->InnerRect.Min.x + vw->InnerRect.Max.x) * 0.5f, (vw->InnerRect.Min.y + vw->InnerRect.Max.y) * 0.5f + 40);
            }
            if (frameCounter == 43) io.AddMouseButtonEvent(0, true);
            if (frameCounter == 45) io.AddMouseButtonEvent(0, false);
            if (frameCounter == 50) Log("[demo] 3D plugin tool: last undo step %s", a->history.Count() ? a->history.Label(a->history.Count() - 1).c_str() : "-");
        }
        if (GetEnvironmentVariableA("SX_FOLDER", nullptr, 0) > 0) {
            // 3D folder around the paint layer (2nd from the top), a gradient smart mask on the folder (the
            // bottom of the model loses the paint); then the dirt goes in and out (logged), the folder stays.
            static Id folder = 0;
            std::vector<Id> mats = MaterialIds(*a->doc);
            if (frameCounter == 40 && mats.size() > 2) {
                folder = CreateFolder3D(*a, mats[2]);
                for (auto& l : a->doc->S().layers)
                    if (l.material == folder) l.generator = "{\"type\":3,\"amount\":0.5,\"contrast\":3,\"noise\":0.3,\"scale\":6,\"dir\":0}";
                Log("[demo] folder %u around %u: depth of the layer %d", folder, mats[2], Folder3DDepth(*a->doc, mats[2]));
            }
            if (frameCounter == 50 && folder && !mats.empty() && mats[0] != folder) {
                MoveIntoFolder3D(*a, mats[0], folder);  // the top layer (above the folder) cannot enter: logged as it is
                Log("[demo] after entering: depth %d", Folder3DDepth(*a->doc, mats[0]));
                MoveOutOfFolder3D(*a, mats[0]);
                Log("[demo] after leaving: depth %d", Folder3DDepth(*a->doc, mats[0]));
            }
            char fm[8] = "";
            GetEnvironmentVariableA("SX_FOLDER", fm, sizeof(fm));
            if (frameCounter == 53 && folder && atoi(fm) == 2) UngroupFolder3D(*a, folder);
            if (frameCounter == 55) {
                std::string order;
                for (Id m : MaterialIds(*a->doc)) order += std::string(Folder3DDepth(*a->doc, m) ? "  " : "") + AnyMaterialLayer(*a->doc, m)->name + " | ";
                Log("[demo] 3D layers: %s", order.c_str());
            }
        }
        if (GetEnvironmentVariableA("SX_PICK", nullptr, 0) > 0 && a->mesh->depth) {
            // 3D eyedropper at the view center (frame 60), result logged at frame 70.
            const u32 w = a->mesh->depth->Width(), h = a->mesh->depth->Height();
            if (frameCounter == 60) StartMaterialPick(*a, float2{w * 0.5f, h * 0.5f}, w, h);
            if (frameCounter == 70)
                Log("[demo] picked color %.2f %.2f %.2f, rough %.2f, metal %.2f, height %.2f (%s)", fg.x, fg.y, fg.z, brush3D.rough, brush3D.metal, brush3D.height,
                    statusMsg.c_str());
        }
        char ssn[128] = "";
        if (frameCounter == 70 && GetEnvironmentVariableA("SX_SAVESMART", ssn, sizeof(ssn)) > 0) {
            Log("[demo] save smart material %s: %d", ssn, (int)SaveSmartMaterial(*a, ssn, "Enregistré par la démo"));
            Log("[demo] %s", statusMsg.c_str());
        }
        if (frameCounter == 90 && GetEnvironmentVariableA("SX_SAVESMART", ssn, sizeof(ssn)) > 0) {
            bool found = false;
            for (const Json& j : SmartMaterials()) found |= j["name"].S("") == ssn;
            Log("[demo] library reloaded: %zu materials, saved one listed %d", SmartMaterials().size(), (int)found);
            smartGridRequest = true;
        }
        if (GetEnvironmentVariableA("SX_SOLO", nullptr, 0) > 0 && (frameCounter == 40 || frameCounter == 70)) {
            // Solo of the second 3D layer (frame 40), then every layer again (frame 70).
            std::vector<Id> mats = MaterialIds(*a->doc);
            if (mats.size() > 1) {
                if (frameCounter == 40 || GetEnvironmentVariableA("SX_SOLO", nullptr, 0) > 0) SoloMaterial(*a, mats[1]);
                int shown = 0;
                for (Id m : mats) if (Layer* l = AnyMaterialLayer(*a->doc, m); l && l->visible) ++shown;
                Log("[demo] solo at frame %llu: %d of %zu 3D layers shown", (unsigned long long)frameCounter, shown, mats.size());
            }
        }
        if (GetEnvironmentVariableA("SX_LIGHTSPIN", nullptr, 0) > 0) {
            // Shift + right drag on the 3D view: the lighting turns (synthetic events).
            ImGuiIO& io = ImGui::GetIO();
            if (ImGuiWindow* w = ImGui::FindWindowByName("Vue 3D")) {
                const ImVec2 c((w->InnerRect.Min.x + w->InnerRect.Max.x) * 0.5f, (w->InnerRect.Min.y + w->InnerRect.Max.y) * 0.5f + 60);
                if (frameCounter == 60) {
                    io.AddMousePosEvent(c.x, c.y);
                    io.AddKeyEvent(ImGuiMod_Shift, true);
                }
                if (frameCounter == 62) io.AddMouseButtonEvent(1, true);
                if (frameCounter > 62 && frameCounter <= 92) io.AddMousePosEvent(c.x + (frameCounter - 62) * 8.0f, c.y);
                if (frameCounter == 94) {
                    io.AddMouseButtonEvent(1, false);
                    io.AddKeyEvent(ImGuiMod_Shift, false);
                }
                if (frameCounter == 96) Log("[demo] light spin: yaw %.1f deg, follow %d, camera yaw %.2f", a->mesh->lightYaw * 57.29578f, a->mesh->lightFollow ? 1 : 0, a->mesh->yaw);
            }
        }
        if (GetEnvironmentVariableA("SX_MASKEDIT", nullptr, 0) > 0 && a->mesh->maps) {
            // Corrections painted over a smart mask (layer SX_MASKLAYER, "Peinture" by default): a black
            // stroke removes it, then its settings change and the mask is generated again (the stroke stays).
            static int phase = 0;
            char ml[64] = "Peinture";
            GetEnvironmentVariableA("SX_MASKLAYER", ml, sizeof(ml));
            Id mat = 0;
            for (auto& l : a->doc->S().layers) if (l.material && l.name == ml) mat = l.material;
            if (phase == 0 && mat && a->mesh->depth) {
                SelectMaterial(*a->doc, mat);
                fg = {0, 0, 0, 1};
                std::vector<float4> colors;
                std::vector<Id> targets = Paint3DTargets(*a, colors);
                for (auto& c : colors) c = {0, 0, 0, 1};  // as the eraser paints corrections: removes the material
                MeshScene& s = *a->mesh;
                if (!targets.empty() && a->renderer.BeginStrokeMulti(*a->doc, *ctx, targets)) {
                    std::vector<Dab> dabs;
                    const float cx = s.depth->Width() * 0.5f, cy = s.depth->Height() * 0.5f;
                    for (int i = 0; i <= 160; ++i) dabs.push_back({cx - 200 + i * 2.5f, cy - 40 + 60 * std::sin(i * 0.05f), 26, 1});
                    a->renderer.MeshStrokeDabsMulti(*a->doc, *ctx, s, dabs, brush, colors, false);
                    a->renderer.EndStrokeMulti(*a->doc, *ctx, a->history, "Retouche du masque");
                }
                Layer* al = a->doc->ActiveLayer();
                Log("[demo] mask corrections painted into surface %u (layer mask %u, edits %u, generator %d, editMask %d)", targets.empty() ? 0u : targets[0],
                    al ? al->mask : 0, al ? al->maskEdit : 0, al && !al->generator.empty() ? 1 : 0, a->doc->S().editMask ? 1 : 0);
                phase = 1;
            } else if (phase >= 1 && phase < 30) {
                ++phase;
            } else if (phase == 30) {
                // Settings change: the mask is generated again.
                for (auto& l : a->doc->S().layers)
                    if (l.material == mat && !l.generator.empty()) {
                        Json g;
                        Json::Parse(l.generator, g);
                        g["amount"] = g["amount"].F(0.3f) + 0.1f;
                        l.generator = g.Dump(0);
                    }
                Log("[demo] smart mask settings changed");
                phase = 31;
            } else if (phase >= 31 && phase < 60) {
                if (++phase == 60)
                    for (auto& l : a->doc->S().layers)
                        if (l.material == mat && l.mask) {
                            Surface* m = a->doc->GetSurface(l.mask);
                            Surface* e = a->doc->GetSurface(l.maskEdit);
                            Log("[demo] mask version %llu, edits version %llu", m ? (unsigned long long)m->version : 0ull, e ? (unsigned long long)e->version : 0ull);
                            char dp[512] = "";
                            if (GetEnvironmentVariableA("SX_MASKDUMP", dp, sizeof(dp)) > 0 && e && m) {
                                exports.push_back({std::string(dp) + "_edits.png", a->renderer.ExportRGBA8(*ctx, e->tex.get()), {}});
                                exports.push_back({std::string(dp) + "_mask.png", a->renderer.ExportRGBA8(*ctx, m->tex.get()), {}});
                            }
                            break;
                        }
            }
        }
        static double t0 = 0;
        if (frameCounter == 11) t0 = ImGui::GetTime();
        if (frameCounter == 12) ShowSideTab(5);  // 3D tab: Calques 3D (smart mask settings)
        if (frameCounter >= 13 && frameCounter <= 16) FocusPanel("Calques 3D");
        static bool logged = false;
        if (frameCounter > 11 && !logged && a->mesh->maps) {
            logged = true;
            Log("[demo] mesh maps baked in %.2f s (%u px)", ImGui::GetTime() - t0, a->mesh->maps->Width());
            a->mesh->viewChannel = atoi(view);
        }
        return;
    }
    if (demoMode == 87) {
        // Paint 3D tools through synthetic mouse events on the 3D view: a scattered stroke with a
        // programmable tip (SX_TIP, cy_star by default), symmetry SX_SYM (1..3), then polygon fills (face, part).
        char tipId[64] = "cy_star", sym[4] = "0";
        GetEnvironmentVariableA("SX_TIP", tipId, sizeof(tipId));
        GetEnvironmentVariableA("SX_SYM", sym, sizeof(sym));
        if (frameCounter == 4) {
            p3dNew = {};
            p3dNew.basePreset = 1;
            p3dNew.mesh = 1;  // cube: flat faces
            char mf[512] = "";
            if (GetEnvironmentVariableA("SX_MESHFILE", mf, sizeof(mf)) > 0) { p3dNew.mesh = 3; p3dNew.meshPath = mf; }
            NewPaint3DProject();
        }
        DocView* a = Active();
        if (!a || !a->mesh) return;
        if (frameCounter == 8) FocusPanel("Vue 3D");
        ImGuiWindow* w = ImGui::FindWindowByName("Vue 3D");
        if (!w) return;
        const ImVec2 c((w->InnerRect.Min.x + w->InnerRect.Max.x) * 0.5f, (w->InnerRect.Min.y + w->InnerRect.Max.y) * 0.5f + 20);
        const float span = (w->InnerRect.Max.x - w->InnerRect.Min.x) * 0.18f;
        ImGuiIO& io = ImGui::GetIO();
        if (frameCounter == 12) {
            if (GetEnvironmentVariableA("SX_RT", nullptr, 0) > 0) a->mesh->quality = 2;  // painting in the ray-traced view
            char st[512] = "";
            if (GetEnvironmentVariableA("SX_STENCIL", st, sizeof(st)) > 0 && LoadDecal(st)) {
                // Painting through a stencil: big round brush, the image decides where it paints.
                decal.stencil = true;
                decal.size = 0.9f;
                decal.rotation = 20;
                brush3D.scatter = 0;
                brush3D.jitterSize = 0;
                brush3D.jitterAngle = 0;
            }
            SelectTip(brush, tipId);
            brush.size = 46;
            brush.flow = 1;
            brush3D.tip = true;
            brush3D.scatter = 0.5f;
            brush3D.jitterAngle = 1;
            brush3D.jitterSize = 0.4f;
            a->renderer.meshSymmetry = atoi(sym);
            if (decal.stencil) { brush.size = 160; brush3D.scatter = 0; brush3D.jitterSize = 0; brush3D.jitterAngle = 0; brush3D.tip = false; }
            fg = {0.85f, 0.12f, 0.1f, 1};
            tool = Tool::Brush;
        }
        if (frameCounter == 14) {
            // Compile the tip before the stroke.
            EffectInstance tip;
            tip.params = brush.tipParams;
            if (auto td = TipDef(brush)) a->renderer.BuildMeshTip(*ctx, *td, tip, brush);
        }
        if (frameCounter >= 26 && frameCounter <= 50) {
            const float t = (frameCounter - 26) / 24.0f;
            io.AddMousePosEvent(c.x - span + 2 * span * t, c.y - span * 0.6f + std::sin(t * 6.0f) * 15);
            if (frameCounter == 27) io.AddMouseButtonEvent(0, true);
        }
        if (frameCounter == 51) io.AddMouseButtonEvent(0, false);
        if (frameCounter == 56) Log("[demo] stroke: tip %s, atlas %d, %s", tipId, a->renderer.meshTipAtlas ? 1 : 0, statusMsg.c_str());
        // Polygon fills: a face in blue, then (Ctrl) erase nothing; a part in green on SX_MESHFILE models.
        if (frameCounter == 60) {
            tool = Tool::PolyFill;
            brush3D.polyMode = 1;
            fg = {0.1f, 0.35f, 0.9f, 1};
            io.AddMousePosEvent(c.x + span * 0.3f, c.y + span * 0.4f);
        }
        if (frameCounter == 63) io.AddMouseButtonEvent(0, true);
        if (frameCounter == 65) io.AddMouseButtonEvent(0, false);
        if (frameCounter == 68) Log("[demo] polyfill: %s", statusMsg.c_str());
        if (frameCounter == 70) {
            brush3D.polyMode = 0;
            fg = {0.2f, 0.8f, 0.3f, 1};
            io.AddMousePosEvent(c.x - span * 0.5f, c.y + span * 0.2f);
        }
        if (frameCounter == 73) io.AddMouseButtonEvent(0, true);
        if (frameCounter == 75) io.AddMouseButtonEvent(0, false);
        if (frameCounter == 78) Log("[demo] polyfill part: %s", statusMsg.c_str());
        if (frameCounter == 80) io.AddMousePosEvent(c.x + span * 0.2f, c.y - span * 0.2f);  // hover highlight shown in the capture
        char an[8] = "";
        if (frameCounter == 82 && GetEnvironmentVariableA("SX_ANCHOR", an, sizeof(an)) > 0) {
            // A material following the painted layer (SX_ANCHOR: 0 on, 1 border, 2 around, 3 elsewhere).
            Layer* al = a->doc->ActiveLayer();
            const Id src = al ? al->material : 0;
            const float3 col = {0.45f, 0.2f, 0.06f};
            const float rough = 0.9f;
            if (Id m = AddMaterialLayerValues(*a, "Suit la peinture", &col, &rough, nullptr, nullptr, false)) {
                Json g = Json::Object();
                g["type"] = 8;
                g["source"] = (int)src;
                g["anchor"] = atoi(an);
                g["radius"] = 40.0f;
                g["amount"] = 0.5f;
                g["contrast"] = 4.0f;
                g["noise"] = 0.25f;
                g["scale"] = 12.0f;
                for (auto& x : a->doc->S().layers) if (x.material == m) x.generator = g.Dump(0);
                Log("[demo] anchor layer %u follows %u (mode %s)", m, src, an);
            }
        }
        return;
    }
    if (demoMode == 86) {
        // A panel (SX_PANEL, Effets by default) opened large in its own window; SX_BACK=1: put back at frame 60.
        char panel[64] = "Effets", back[4] = "";
        GetEnvironmentVariableA("SX_PANEL", panel, sizeof(panel));
        GetEnvironmentVariableA("SX_BACK", back, sizeof(back));
        if (frameCounter == 6) { hideBottom = false; TogglePopOut(panel); }
        if (frameCounter == 60 && back[0] == '1') TogglePopOut(panel);
        if (frameCounter == 40 || frameCounter == 90) {
            ImGuiWindow* w = ImGui::FindWindowByName(panel);
            Log("[demo] popout %s: out %d, docked %d, size %.0f x %.0f, own viewport %d", panel, IsPoppedOut(panel) ? 1 : 0, w && w->DockIsActive ? 1 : 0,
                w ? w->Size.x : 0.f, w ? w->Size.y : 0.f, w && w->Viewport && w->Viewport != ImGui::GetMainViewport() ? 1 : 0);
        }
        return;
    }
    if (demoMode == 85) {
        // Action editor: an action of 3 steps shown in the editor, then a batch (my action + a built-in one)
        // from SX_SRC (with subfolders) to SX_DST, JPG, numbered names. Actions are saved in SX_DST, not in the user's file.
        char path[512] = "", src[512] = "", dst[512] = "";
        GetEnvironmentVariableA("SX_SRC", src, sizeof(src));
        GetEnvironmentVariableA("SX_DST", dst, sizeof(dst));
        if (frameCounter == 4) {
            settingsPath = std::string(dst) + "/demo_settings.json";
            if (GetEnvironmentVariableA("SX_IMAGE", path, sizeof(path)) > 0) OpenImage(path, false);
        }
        if (frameCounter == 6) {
            hideBottom = false;
            Action a{"Carré N&B + JPG", {}};
            Json c = Json::Object();
            c["square"] = true; c["size"] = 512; c["interp"] = 2;
            a.steps.push_back({"fit_crop", c, true});
            Json f = Json::Object();
            f["effect"] = "black_white"; f["destructive"] = true;
            a.steps.push_back({"filter", f, true});
            Json l = Json::Object();
            l["opacity"] = 0.8f;
            a.steps.push_back({"layer_props", l, false});
            Json e = Json::Object();
            e["format"] = "jpg"; e["quality"] = 80; e["folder"] = ""; e["suffix"] = "_carre";
            a.steps.push_back({"export", e, true});
            actions.push_back(a);
            actionSel = (int)actions.size() - 1;
            builtinSel = -1;
            actionStepSel = 0;
            panelOpen["Éditeur d'actions"].open = true;
            actionEditorFocus = true;
            for (auto& s : a.steps) Log("[demo] step: %s | %s", ActionStepSummary(s).c_str(), ActionStepDescription(s).c_str());
            Log("[demo] built-in actions: %zu", BuiltinActions().size());
        }
        if (frameCounter == 20) {
            batch.chain = {"u:Carré N&B + JPG", "b:Rotation 90° horaire"};
            batch.sourceKind = 0;
            batch.source = src;
            batch.subfolders = true;
            batch.keepTree = true;
            batch.dest = dst;
            batch.format = 2;
            batch.quality = 85;
            batch.pattern = "{n3}_{nom}_{l}x{h}";
            batch.open = true;
            StartBatch();
        }
        if (frameCounter > 20 && frameCounter % 30 == 0 && batch.running) Log("[demo] batch: %zu/%zu", batch.index, batch.files.size());
        static bool reported = false;
        if (frameCounter > 22 && !batch.running && !reported) {
            reported = true;
            for (auto& l : batch.log) Log("[demo] log: %s -> %s (%s)", l.file.c_str(), l.out.c_str(), l.msg.c_str());
            Log("[demo] batch finished: %s", statusMsg.c_str());
            if (ImGuiWindow* w = ImGui::FindWindowByName("Éditeur d'actions")) (void)w;
            actionEditorFocus = true;
            actionStepSel = 1;
            batch.open = false;
        }
        return;
    }
    if (demoMode == 84) {
        // Resources panel: SX_EXPTAB (0 images, 1 meshes, 2 lights) on SX_EXPDIR, shown in the bottom zone.
        char dir[512] = "", tab[8] = "0";
        GetEnvironmentVariableA("SX_EXPDIR", dir, sizeof(dir));
        GetEnvironmentVariableA("SX_EXPTAB", tab, sizeof(tab));
        if (frameCounter == 6) {
            hideBottom = false;
            if (!explorer.loaded) LoadExplorerSettings();
            *PanelFlag("Ressources") = true;
            if (dir[0]) { explorer.dirImages = dir; explorer.dirMeshes = dir; }
        }
        if (frameCounter >= 8 && frameCounter <= 12) {
            FocusPanel("Ressources");
            if (ImGuiWindow* w = ImGui::FindWindowByName("Ressources"); w && w->DockNode && w->DockNode->TabBar) w->DockNode->TabBar->NextSelectedTabId = w->TabId;
        }
        if (frameCounter == 10) {
            const int t = atoi(tab);
            if (ImGuiWindow* w = ImGui::FindWindowByName("Ressources")) (void)w;
            explorerTabRequest = t;
        }
        if (frameCounter == 300) {
            size_t ready = 0;
            for (auto& [p, t] : explorer.thumbs) ready += t.tex != nullptr;
            Log("[demo] resources: %zu files, %zu folders, %zu thumbnails ready", explorer.files.size(), explorer.folders.size(), ready);
        }
        return;
    }
    if (demoMode == 83) {
        // AI inpainting: the mask painted with a synthetic brush stroke across the middle wave, then generated
        // with the local test provider (3 variations), variation 2 shown.
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        ImGuiIO& io = ImGui::GetIO();
        auto screen = [&](float2 p) { return ImVec2(lxf.canvasMin.x + a->pan.x + p.x * a->zoom, lxf.canvasMin.y + a->pan.y + p.y * a->zoom); };
        if (frameCounter == 8) {
            hideBottom = false;
            *PanelFlag("Inpainting IA") = true;
            ShowSideTab(6);
            EnsureInpaintMask(*a);
            d.S().editInpaint = true;
            tool = Tool::Brush;
            brush.size = 90;
            inpaint.prompt = "des fleurs";
            inpaint.provider = 3;
            inpaint.variations = 3;
        }
        if (frameCounter == 12) io.AddMousePosEvent(screen({450, 520}).x, screen({450, 520}).y);
        if (frameCounter == 13) io.AddMouseButtonEvent(0, true);
        for (int k = 1; k <= 10; ++k)
            if (frameCounter == (u64)13 + k) {
                const ImVec2 p = screen({450 + 70.0f * k, 520 + 30 * std::sin(k * 0.6f)});
                io.AddMousePosEvent(p.x, p.y);
            }
        if (frameCounter == 25) io.AddMouseButtonEvent(0, false);
        if (frameCounter == 27) {
            io.AddMousePosEvent(-100, -100);
            StartInpaint(*a);
        }
        if (frameCounter == 45) {
            size_t covered = 0;
            for (u8 m : inpaint.maskFull) covered += m > 128;
            Log("[demo] inpaint: %zu result(s), crop %dx%d, mask %zu px, layers %zu", inpaint.results.size(), inpaint.crop.Width(), inpaint.crop.Height(), covered,
                d.S().layers.size());
            ShowInpaintVariant(1);
        }
        return;
    }
    if (demoMode == 82) {
        // Calling a function: Event "Lancer" -> "avant" -> call (Outil, "Dire") -> "après"; the function's event
        // "Dire" prints "dans la fonction" and sets its variable N to 7.
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        static Id ev = 0, fid = 0;
        if (frameCounter == 8 && d.Mode() == DocMode::Layers) {
            Edit e(d, a->history, "Passer en mode nodal");
            d.ConvertToNodes();
            e.Commit();
        }
        auto text = [](const char* t) { Value v = MakeValue(PinType::String); v.s = t; return v; };
        auto name = [](const char* t) { Value v = MakeValue(PinType::Name); v.s = t; return v; };
        if (frameCounter == 10) {
            Node& f = d.AddFunctionNode("tool", {0, 1600});
            f.name = "Outil";
            fid = f.id;
            auto sub = std::make_shared<Graph>(*f.sub);
            GraphVariable var;
            var.name = "N";
            var.value = MakeValue(PinType::Int);
            sub->variables.push_back(var);
            Node e;
            e.id = d.NewId();
            e.kind = NodeKind::Event;
            e.name = "Dire";
            Node p;
            p.id = d.NewId();
            p.kind = NodeKind::Action;
            p.op = "print";
            p.inputs = {e.id, 0};
            p.pinValues = {Value{}, text("dans la fonction")};
            Node s;
            s.id = d.NewId();
            s.kind = NodeKind::VarSet;
            s.op = "N";
            s.value = MakeValue(PinType::Int);
            s.inputs = {p.id, 0};
            Value seven = MakeValue(PinType::Int);
            seven.f.x = 7;
            s.pinValues = {Value{}, seven};
            sub->nodes.push_back(e);
            sub->nodes.push_back(p);
            sub->nodes.push_back(s);
            d.S().graph.Find(fid)->sub = sub;
            ev = AddTypedNode(d, NodeKind::Event, "Lancer", PinType::Exec, false, {0, 1300}).id;
            Node& p1 = AddTypedNode(d, NodeKind::Action, "print", PinType::Exec, false, {300, 1300});
            NodeSetPinValue(p1, 1, text("avant"));
            const Id a1 = p1.id;
            Node& c = AddTypedNode(d, NodeKind::Action, "call", PinType::Exec, false, {600, 1300});
            NodeSetPinValue(c, 1, name("Outil"));
            NodeSetPinValue(c, 2, name("Dire"));
            const Id cid = c.id;
            Node& p2 = AddTypedNode(d, NodeKind::Action, "print", PinType::Exec, false, {900, 1300});
            NodeSetPinValue(p2, 1, text("après"));
            const Id a2 = p2.id;
            const bool l1 = d.Link(ev, a1, 0), l2 = d.Link(a1, cid, 0), l3 = d.Link(cid, a2, 0);
            const Node* cn = d.S().graph.Find(cid);
            Log("[demo] call: links %d %d %d, call node inputs %d, op '%s'", (int)l1, (int)l2, (int)l3, cn ? NodeInputCount(*cn) : -1, cn ? cn->op.c_str() : "");
            StartExec(*a, 0, ev);
        }
        if (frameCounter == 20)
            if (const Node* f = d.S().graph.Find(fid)) {
                auto it = f->varValues.find("N");
                Log("[demo] call: function variable N = %s", it != f->varValues.end() ? ValueToText(it->second).c_str() : "(not set)");
            }
        return;
    }
    if (demoMode == 81) {
        // Joining flows: Event -> Branch; True -> "A", False -> "B"; both -> "Fin" (one execution input, two
        // flows). Run with the condition true then false; the graph saved / read back keeps the join.
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        static Id ev = 0, br = 0;
        if (frameCounter == 8 && d.Mode() == DocMode::Layers) {
            Edit e(d, a->history, "Passer en mode nodal");
            d.ConvertToNodes();
            e.Commit();
        }
        auto print = [&](const char* text, float2 pos) {
            Node& p = AddTypedNode(d, NodeKind::Action, "print", PinType::Exec, false, pos);
            Value v = MakeValue(PinType::String);
            v.s = text;
            NodeSetPinValue(p, 1, v);
            return p.id;
        };
        if (frameCounter == 10) {
            ev = AddTypedNode(d, NodeKind::Event, "Tester", PinType::Exec, false, {0, 1400}).id;
            br = AddTypedNode(d, NodeKind::Action, "branch", PinType::Exec, false, {300, 1400}).id;
            const Id pa = print("A", {600, 1300}), pb = print("B", {600, 1500}), fin = print("Fin", {900, 1400});
            Log("[demo] execjoin: links %d %d %d %d %d", (int)d.Link(ev, br, 0), (int)d.Link(br, pa, 0, 0), (int)d.Link(br, pb, 0, 8),
                (int)d.Link(pa, fin, 0), (int)d.Link(pb, fin, 0));
            if (const Node* f = d.S().graph.Find(fin)) Log("[demo] execjoin: Fin has %zu extra flow(s)", f->execLinks.size());
            StartExec(*a, 0, ev);
        }
        if (frameCounter == 20) {
            if (Node* b = d.S().graph.Find(br)) {
                Value f = MakeValue(PinType::Bool);
                NodeSetPinValue(*b, 1, f);
            }
            StartExec(*a, 0, ev);
        }
        if (frameCounter == 30) {
            Graph back;
            GraphFromJson(GraphToJson(d.S().graph), back);
            size_t extra = 0;
            for (auto& n : back.nodes) extra += n.execLinks.size();
            Log("[demo] execjoin: read back, %zu extra flow(s)", extra);
        }
        return;
    }
    if (demoMode == 80) {
        // Graph comments: "Peinture" and its blend selected, C pressed in the graph (frame around them),
        // the frame dragged by its title (+120, +80 px): the nodes inside follow.
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        Graph& g = d.S().graph;
        ImGuiIO& io = ImGui::GetIO();
        static Id paint = 0;
        static float2 before;
        static ImVec2 start;
        if (frameCounter == 8 && d.Mode() == DocMode::Layers) {
            Edit e(d, a->history, "Passer en mode nodal");
            d.ConvertToNodes();
            e.Commit();
        }
        if (frameCounter == 10) {
            nodeEd.selection.clear();
            for (auto& n : g.nodes)
                if (n.name == "Peinture" || n.name == "Fusion Peinture") {
                    nodeEd.selection.insert(n.id);
                    if (n.name == "Peinture") paint = n.id;
                }
            graphFrameOnly = nodeEd.selection;
            graphFrameRequest = true;
            FocusPanel("Graphe");
        }
        if (frameCounter == 9) hideBottom = false;
        if (frameCounter == 13) FocusPanel("Graphe");
        if (frameCounter == 14) {
            Log("[demo] comments: graph focused %d, nav window %s", (int)graphFocused, GImGui->NavWindow ? GImGui->NavWindow->Name : "-");
            io.AddKeyEvent(ImGuiKey_C, true);
        }
        if (frameCounter == 15) io.AddKeyEvent(ImGuiKey_C, false);
        if (frameCounter == 18) {
            Log("[demo] comments: %zu comment(s)", g.comments.size());
            if (!g.comments.empty()) {
                g.comments.back().text = "Peinture et sa fusion";
                if (const Node* n = g.Find(paint)) before = n->pos;
                graphFrameOnly.clear();
                for (auto& n : g.nodes) graphFrameOnly.insert(n.id);
                graphFrameRequest = true;
            }
        }
        if (frameCounter == 22 && !g.comments.empty()) {
            auto it = nodeEd.commentRects.find(g.comments.back().id);
            if (it != nodeEd.commentRects.end()) {
                start = ImVec2(it->second.x + 60, (it->second.y + it->second.w) * 0.5f);
                io.AddMousePosEvent(start.x, start.y);
            }
        }
        if (frameCounter == 24) io.AddMouseButtonEvent(0, true);
        for (int k = 1; k <= 6; ++k)
            if (frameCounter == (u64)24 + k) io.AddMousePosEvent(start.x + 120.0f * k / 6, start.y + 80.0f * k / 6);
        if (frameCounter == 32) io.AddMouseButtonEvent(0, false);
        if (frameCounter == 34) {
            io.AddMousePosEvent(-100, -100);
            if (const Node* n = g.Find(paint))
                Log("[demo] comments: node moved by %.0f, %.0f (graph units)", n->pos.x - before.x, n->pos.y - before.y);
        }
        return;
    }
    if (demoMode == 79) {
        // Scene presets: written to SX_SCENEDIR (shipped presets), then each applied to the mug project
        // (SX_SCENE_APPLY = preset name, SX_SCENE_MODE 0/1/2) and the scene saved / loaded back (SX_SCENE_FILE).
        for (int i = 0; i < (int)docs.size(); ++i)
            if (docs[i]->doc->kind == 1 && activeDoc != i && frameCounter < 20) { activeDoc = i; focusDocRequest = i; }
        DocView* a = Active();
        if (!a) return;
        char dir[512] = "", apply[128] = "", file[512] = "", mode[8] = "0";
        GetEnvironmentVariableA("SX_SCENEDIR", dir, sizeof(dir));
        GetEnvironmentVariableA("SX_SCENE_APPLY", apply, sizeof(apply));
        GetEnvironmentVariableA("SX_SCENE_FILE", file, sizeof(file));
        GetEnvironmentVariableA("SX_SCENE_MODE", mode, sizeof(mode));
        if (frameCounter == 24 && a->mesh) {
            RenderStateFor(*a);
            if (dir[0]) SaveBuiltinScenes(*a, dir);
            ScanScenePresets();
            Log("[demo] makescenes: %zu presets", scenePresets.size());
            for (auto& p : scenePresets)
                if (p.name == apply) Log("[demo] makescenes: applied %s -> %d", apply, (int)LoadScene(*a, p.path, atoi(mode)));
            if (a->render) {
                Log("[demo] makescenes: %zu objects, %zu lights, hdri '%s'", a->render->objects.size(), a->render->lights.size(), a->render->settings.hdri.c_str());
                if (file[0]) Log("[demo] makescenes: saved %d", (int)SaveScene(*a, file, "Test", ""));
            }
            *PanelFlag("Rendu") = true;
            *PanelFlag("Scène de rendu") = true;
        }
        if (frameCounter == 30) {
            FocusPanel("Rendu");
            FocusPanel("Scène de rendu");
        }
        wchar_t wgather[512] = L"", wimport[512] = L"";
        if (frameCounter == 26 && GetEnvironmentVariableW(L"SX_GATHER", wgather, 512) > 0 && GetEnvironmentVariableW(L"SX_IMPORT", wimport, 512) > 0) {
            // Gathering: a model and the HDRI from elsewhere, the project "saved" at SX_GATHER, files copied beside it.
            ImportIntoRender(*a, Narrow(wimport));
            a->render->settings.hdri = Narrow((std::filesystem::path(wimport).parent_path() / L"Ciel.hdr").wstring());
            a->doc->path = Narrow(wgather);
            GatherSceneFiles(*a);
            for (auto& o : a->render->objects) if (!o.path.empty()) Log("[demo] makescenes: gathered object path %s", o.path.c_str());
            Log("[demo] makescenes: hdri %s", a->render->settings.hdri.c_str());
            Json j = RenderToJson(*a->render, Narrow(std::filesystem::path(wgather).parent_path().wstring()));
            for (auto& o : j["objects"].Elements()) if (o.Has("path")) Log("[demo] makescenes: saved as %s", o["path"].S("").c_str());
            Log("[demo] makescenes: hdri saved as %s", j["settings"]["hdri"].S("").c_str());
        }
        if (frameCounter == 28 && file[0] && a->render) {
            const size_t nObj = a->render ? a->render->objects.size() : 0;
            a->render->objects.clear();
            const bool ok = LoadScene(*a, file, 0);
            Log("[demo] makescenes: reloaded %d, %zu objects (was %zu)", (int)ok, a->render->objects.size(), nObj);
        }
        return;
    }
    if (demoMode == 78) {
        // Shipped templates: built, each shown (rendered) in turn, then saved with its thumbnail into SX_TPLDIR.
        static std::vector<BuiltinTemplate> list;
        char dir[512] = "";
        GetEnvironmentVariableA("SX_TPLDIR", dir, sizeof(dir));
        if (frameCounter == 10) {
            list = BuildBuiltinTemplates();
            Log("[demo] maketemplates: %zu documents", list.size());
        }
        for (size_t i = 0; i < list.size(); ++i) {
            const u64 f0 = 20 + i * 12;
            if (frameCounter == f0)
                for (int k = 0; k < (int)docs.size(); ++k)
                    if (docs[k].get() == list[i].view) activeDoc = focusDocRequest = k;
            if (frameCounter == f0 + 10 && dir[0] && ViewAlive(list[i].view)) {
                SaveAsTemplate(*list[i].view, list[i].name, list[i].category, list[i].desc, dir);
                Log("[demo] maketemplates: %s", list[i].name.c_str());
            }
        }
        if (frameCounter == 20 + list.size() * 12 + 30) {
            ScanTemplates();
            Log("[demo] maketemplates: %zu templates listed", templates.size());
            for (auto& t : templates)
                if (t.name == "Logo 1024" && NewFromTemplate(t)) {
                    const Document& nd = *docs.back()->doc;
                    Log("[demo] maketemplates: new from template -> name '%s', path '%s', %zu layers, dirty %d", nd.Name().c_str(), nd.path.c_str(),
                        nd.S().layers.size(), (int)nd.dirty);
                }
            showNewDoc = true;
            tplUi.tabRequest = 0;
        }
        return;
    }
    if (demoMode == 77) {
        // Values on nodes: "Ombre colorée" (Color, Float inputs) and a Bool value node; the Distance field dragged
        // +60 px and the Bool field clicked with synthetic ImGui events; SX_STEP "edit": its graph opened (breadcrumb),
        // "ref": effects browser on the reference image.
        static DocView* parent = nullptr;
        static Id fid = 0, cmp = 0;
        char step[64] = "";
        GetEnvironmentVariableA("SX_STEP", step, sizeof(step));
        ImGuiIO& io = ImGui::GetIO();
        if (frameCounter == 8) {
            parent = Active();
            if (!parent) return;
            Edit e(*parent->doc, parent->history, "Passer en mode nodal");
            parent->doc->ConvertToNodes();
            e.Commit();
        }
        if (!parent || !ViewAlive(parent)) return;
        Document& d = *parent->doc;
        Graph& g = d.S().graph;
        auto fieldCenter = [&](Id node, int pin, ImVec2& c) {
            auto it = nodeEd.valRects.find(((u64)node << 8) | (u64)pin);
            if (it == nodeEd.valRects.end()) return false;
            c = ImVec2((it->second.x + it->second.z) * 0.5f, (it->second.y + it->second.w) * 0.5f);
            return true;
        };
        if (frameCounter == 10) {
            for (auto& f : FunctionLibrary())
                if (f.name == "Ombre colorée") fid = InsertLibraryFunction(*parent, f.path, {0, 1300});
            Node& c = AddTypedNode(d, NodeKind::Math, "greater", PinType::Float, false, {420, 1300});
            cmp = c.id;
            Node* out = g.Find(g.output);
            if (out && !out->inputs.empty() && fid) d.Link(out->inputs[0], fid, 0);
            graphFrameOnly = {fid, cmp};
            graphFrameRequest = true;
            if (strstr(step, "ref")) {
                fxBrowser.source = 2;
                fxBrowser.refKind = 0;
                *PanelFlag("Effets") = true;
                ShowSideTab(2);
            }
        }
        static ImVec2 start;
        if (frameCounter == 16 && fieldCenter(fid, 2, start)) io.AddMousePosEvent(start.x, start.y);
        if (frameCounter == 18) io.AddMouseButtonEvent(0, true);
        for (int k = 1; k <= 6; ++k)
            if (frameCounter == (u64)18 + k) io.AddMousePosEvent(start.x + 60.0f * k / 6, start.y);
        if (frameCounter == 26) io.AddMouseButtonEvent(0, false);
        if (frameCounter == 28) {
            if (const Node* f = g.Find(fid)) Log("[demo] nodeui distance = %s", ValueToText(NodePinDefault(*f, 2)).c_str());
            ImVec2 c;
            if (fieldCenter(cmp, 0, c)) io.AddMousePosEvent(c.x, c.y);
        }
        if (frameCounter == 30) io.AddMouseButtonEvent(0, true);
        if (frameCounter == 31) io.AddMouseButtonEvent(0, false);
        if (frameCounter == 34) {
            ImVec2 c;
            if (fieldCenter(cmp, 0, c)) Log("[demo] nodeui popup open: %d", (int)ImGui::IsPopupOpen("##pinval", ImGuiPopupFlags_AnyPopupId));
            io.AddKeyEvent(ImGuiKey_Escape, true);
        }
        if (frameCounter == 35) io.AddKeyEvent(ImGuiKey_Escape, false);
        if (frameCounter == 36) {
            io.AddMousePosEvent(-100, -100);
            if (const Node* f = g.Find(fid)) {
                std::vector<PinDesc> ins;
                NodeSignature(*f, &ins, nullptr);
                Log("[demo] nodeui pins: %zu, widths ok", ins.size());
            }
            if (strstr(step, "edit")) EditFunction(*parent, fid);
        }
        return;
    }
    if (demoMode == 76) {
        // Trim sheet: "Architecture" layout, the bottom edge of trim 2 dragged down 40 px with synthetic
        // ImGui mouse events, trim 1 made seamless on "Peinture", ID layer, JSON export (SX_TRIMJSON), 3D preview.
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        auto& ts = d.S().trimSheet;
        ImGuiIO& io = ImGui::GetIO();
        auto screen = [&](float2 p) { return ImVec2(lxf.canvasMin.x + a->pan.x + p.x * a->zoom, lxf.canvasMin.y + a->pan.y + p.y * a->zoom); };
        auto logTrims = [&](const char* when) {
            std::string s;
            for (auto& t : ts.trims) s += std::to_string(t.r.y0) + "-" + std::to_string(t.r.y1) + " ";
            Log("[demo] trims %s: %zu trims, active %d, y: %s", when, ts.trims.size(), d.S().activeTrim, s.c_str());
        };
        static ImVec2 start;
        if (frameCounter == 10) {
            std::vector<int> hs;
            for (float f : TrimPresets()[0].fractions) hs.push_back(std::max(1, (int)std::lround(f * d.Height())));
            TrimApplyLayout(*a, TrimStrips(d.Width(), d.Height(), hs, 0), "Disposition des trims");
            tool = Tool::Trim;
            *PanelFlag("Trim sheet") = true;
            ShowSideTab(1);
            logTrims("layout");
        }
        if (frameCounter == 14 && ts.trims.size() > 2) {
            const Rect& r = ts.trims[1].r;
            start = screen({r.x0 + r.Width() * 0.5f, (float)r.y1});
            io.AddMousePosEvent(start.x, start.y);
        }
        if (frameCounter == 16) io.AddMouseButtonEvent(0, true);
        for (int k = 1; k <= 6; ++k)
            if (frameCounter == (u64)16 + k) io.AddMousePosEvent(start.x, start.y + 40 * a->zoom * k / 6);
        if (frameCounter == 24) io.AddMouseButtonEvent(0, false);
        if (frameCounter == 26) {
            logTrims("dragged");
            io.AddMousePosEvent(-100, -100);
            for (auto& l : d.S().layers) if (l.name == "Peinture") d.S().activeLayer = l.id;
            d.S().activeTrim = 0;
            TrimMakeSeamless(*a, 0);
        }
        if (frameCounter == 30) TrimIdLayer(*a);
        if (frameCounter == 32) {
            char path[512] = "";
            if (GetEnvironmentVariableA("SX_TRIMJSON", path, sizeof(path)) > 0) Log("[demo] trims export: %d", (int)ExportTrimLayout(*a, path));
            d.S().activeTrim = 0;
            TrimPreview3D(*a);
        }
        return;
    }
    if (demoMode == 75) {
        // Function library: every entry inserted (signature logged); "Variantes de teinte" and "Bake et export PNG"
        // fed with the document image and run (PNG files written next to the executable, exports/).
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        Graph& g = d.S().graph;
        static std::vector<Id> ids;
        if (frameCounter == 8 && d.Mode() == DocMode::Layers) {
            Edit e(d, a->history, "Passer en mode nodal");
            d.ConvertToNodes();
            e.Commit();
        }
        if (frameCounter == 10) {
            Node* out = g.Find(g.output);
            const Id img = out && !out->inputs.empty() ? out->inputs[0] : 0;
            float y = 1200;
            Log("[demo] library image source %u (output %u, %zu inputs)", img, g.output, out ? out->inputs.size() : 0);
            for (auto& f : FunctionLibrary()) {
                const Id id = InsertLibraryFunction(*a, f.path, {0, y});
                y += 300;
                const Node* n = g.Find(id);
                if (!n) { Log("[demo] library %s: insert failed", f.name.c_str()); continue; }
                std::vector<PinDesc> ins, outs;
                NodeSignature(*n, &ins, &outs);
                std::string sig;
                for (auto& p : ins) sig += p.name + ":" + PinTypeLabel(p.type) + (p.array ? "[]" : "") + " ";
                sig += "-> ";
                for (auto& p : outs) sig += p.name + ":" + PinTypeLabel(p.type) + " ";
                Log("[demo] library %s (%s): %s", f.name.c_str(), f.kind.c_str(), sig.c_str());
                if (img) Log("[demo]   link image: %d", (int)d.Link(img, id, 0));
                if (f.name == "Fondu A/B" && img) d.Link(img, id, 1);
                ids.push_back(id);
            }
            graphFrameOnly = std::set<Id>(ids.begin(), ids.end());
            graphFrameRequest = true;
        }
        auto run = [&](const char* fname, const char* evName) {
            for (Id id : ids)
                if (const Node* n = g.Find(id); n && n->name == fname && n->sub)
                    for (auto& m : n->sub->nodes)
                        if (m.kind == NodeKind::Event && m.name == evName) StartExec(*a, id, m.id);
        };
        if (frameCounter == 20) run("Variantes de teinte", "Exporter les variantes");
        if (frameCounter == 60) run("Bake et export PNG", "Exporter");
        if (frameCounter == 90) {
            for (Id id : ids)
                if (const Node* n = g.Find(id)) Log("[demo] library state %s: results %zu vars %zu", n->name.c_str(), n->execResults.size(), n->varValues.size());
            std::error_code ec;
            int count = 0;
            for (auto& e : std::filesystem::directory_iterator(Utf8Path(ExeDir() + "/exports"), ec)) { ++count; Log("[demo]   exported %s", Narrow(e.path().filename().wstring()).c_str()); }
            Log("[demo] library exports: %d", count);
            for (Id id : ids) if (const Node* n = g.Find(id); n && n->name == "Variantes de teinte") d.S().activeNode = id;
        }
        return;
    }
    if (demoMode == 74) {
        // Typed pins: (A) a Float value x Multiply linked to the Vibrance parameter; (B) an Event running a Bake
        // in the document graph; (C) a tool function with a variable, a loop and messages, run from its node.
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        Graph& g = d.S().graph;
        static Id vib = 0, mulId = 0, evt = 0, bake = 0, fid = 0, fevt = 0;
        if (frameCounter == 8 && d.Mode() == DocMode::Layers) {
            Edit e(d, a->history, "Passer en mode nodal");
            d.ConvertToNodes();
            e.Commit();
        }
        if (frameCounter == 10) {
            for (auto& n : g.nodes) if (n.name == "Vibrance") vib = n.id;
            Node& val = AddTypedNode(d, NodeKind::Value, "", PinType::Float, false, {-600, 900});
            val.value.f.x = 0.8f;
            const Id valId = val.id;
            Node& mul = AddTypedNode(d, NodeKind::Math, "mul", PinType::Float, false, {-300, 900});
            mul.pinValues = {Value{}, ConvertValue(MakeValue(PinType::Float), PinType::Float, false)};
            mul.pinValues[1].f.x = 0.5f;
            mulId = mul.id;
            d.Link(valId, mulId, 0);
            if (Node* vn = g.Find(vib)) {
                vn->paramPins = {0};
                vn->inputs.resize(NodeInputCount(*vn), 0);
                Log("[demo] typed link value->param: %d", (int)d.Link(mulId, vib, NodeInputCount(*vn) - 1));
            }
            // (B) Event -> Bake of the Vibrance output, shown as the document output.
            Node& ev = AddTypedNode(d, NodeKind::Event, "Baker l'image", PinType::Exec, false, {-600, 1200});
            evt = ev.id;
            Node& bk = AddTypedNode(d, NodeKind::Action, "bake", PinType::Exec, false, {-300, 1200});
            bake = bk.id;
            Log("[demo] typed link exec: %d", (int)d.Link(evt, bake, 0));
            Node* out = g.Find(g.output);
            const Id last = out && !out->inputs.empty() ? out->inputs[0] : 0;
            Log("[demo] typed link image->bake: %d", (int)d.Link(last, bake, 1));
            Log("[demo] typed link bake->output: %d", (int)d.Link(bake, g.output, 0, NodeOutputChannel(*g.Find(bake), 1)));
            Log("[demo] typed refused value->image: %d", (int)d.Link(valId, g.output, 0));
            // (C) tool function: variable Compteur, event "Compter": for 1..3 -> set Compteur = index -> message.
            Node& f = d.AddFunctionNode("tool", {-300, 1500});
            fid = f.id;
            auto sub = std::make_shared<Graph>(*f.sub);
            GraphVariable var;
            var.name = "Compteur";
            var.value = MakeValue(PinType::Int);
            sub->variables.push_back(var);
            auto add = [&](NodeKind k, const std::string& op, PinType t) {
                Node n;
                n.id = d.NewId();
                n.kind = k;
                n.op = op;
                n.value = MakeValue(t);
                if (auto* ad = FindExecAction(op); ad && k == NodeKind::Action) n.name = ad->label;
                n.name = n.name.empty() ? op : n.name;
                n.inputs.assign(NodeInputCount(n), 0);
                sub->nodes.push_back(n);
                return n.id;
            };
            fevt = add(NodeKind::Event, "", PinType::Exec);
            sub->Find(fevt)->name = "Compter";
            const Id loop = add(NodeKind::Action, "for_range", PinType::Exec);
            sub->Find(loop)->pinValues = {Value{}, MakeValue(PinType::Int), MakeValue(PinType::Int)};
            sub->Find(loop)->pinValues[1].f.x = 1;
            sub->Find(loop)->pinValues[2].f.x = 3;
            const Id set = add(NodeKind::VarSet, "Compteur", PinType::Int);
            const Id str = add(NodeKind::Math, "to_string", PinType::Float);
            const Id cat = add(NodeKind::Math, "concat", PinType::String);
            { Value t = MakeValue(PinType::String); t.s = "Tour "; sub->Find(cat)->pinValues = {t, Value{}}; }
            const Id msg = add(NodeKind::Action, "print", PinType::Exec);
            auto link = [&](Id from, Id to, int in, int port) {
                Node* t = sub->Find(to);
                t->inputs.resize(std::max<size_t>(t->inputs.size(), in + 1), 0);
                t->inputs[in] = from;
                t->inputChannel.resize(t->inputs.size(), 0);
                t->inputChannel[in] = (u8)(port << 3);
            };
            link(fevt, loop, 0, 0);
            link(loop, set, 0, 0);    // body
            link(loop, set, 1, 1);    // index -> value
            link(set, msg, 0, 0);
            link(loop, str, 0, 1);
            link(str, cat, 1, 0);
            link(cat, msg, 1, 0);
            Node* fn = g.Find(fid);
            fn->sub = sub;
            graphFrameRequest = true;
        }
        if (frameCounter == 11 && GetEnvironmentVariableA("SX_RELAY", nullptr, 0) > 0) {
            // Relays on a value link (Multiply -> Vibrance), an execution link (Event -> Bake) and an image link (-> Bake).
            auto inputOf = [&](Id dst, Id src) {
                if (const Node* n = g.Find(dst))
                    for (int i = 0; i < (int)n->inputs.size(); ++i) if (n->inputs[i] == src) return i;
                return -1;
            };
            if (int i = inputOf(vib, mulId); i >= 0) InsertRelay(*a, mulId, vib, i, 0, {-150, 850});
            if (int i = inputOf(bake, evt); i >= 0) InsertRelay(*a, evt, bake, i, 0, {-450, 1150});
            if (const Node* bn = g.Find(bake); bn && bn->inputs.size() > 1 && bn->inputs[1]) InsertRelay(*a, bn->inputs[1], bake, 1, InputChannel(*bn, 1), {-450, 1250});
            int relays = 0;
            std::string types;
            for (auto& n : g.nodes)
                if (n.kind == NodeKind::Reroute) {
                    ++relays;
                    types += std::string(PinTypeLabel(n.value.type)) + " ";
                }
            Log("[demo] relays inserted: %d (%s)", relays, types.c_str());
        }
        if (frameCounter == 14) {
            Value m = a->renderer.EvalValue(d, mulId, 0);
            Log("[demo] typed multiply = %s", ValueToText(m).c_str());
            if (const Node* vn = g.Find(vib)) Log("[demo] typed vibrance param0 = %.3f (node %.3f)", a->renderer.EffectParams(d, *vn)[0].x, vn->fx.params.empty() ? 0.f : vn->fx.params[0].x);
            StartExec(*a, 0, evt);
        }
        if (frameCounter == 24) {
            const Node* b = g.Find(bake);
            std::unordered_set<Id> frozen;
            GraphFrozenNodes(g, frozen);
            Log("[demo] typed bake result surface %u frozen %zu", b ? b->surface : 0, frozen.size());
            StartExec(*a, fid, fevt);
        }
        if (frameCounter == 34) {
            const Node* f = g.Find(fid);
            auto it = f ? f->varValues.find("Compteur") : decltype(f->varValues)::const_iterator{};
            Log("[demo] typed function Compteur = %s", f && it != f->varValues.end() ? ValueToText(it->second).c_str() : "(none)");
            d.S().activeNode = fid;
            graphFrameOnly = {mulId, evt, bake, fid};
            for (auto& n : g.nodes) if (n.kind == NodeKind::Value) graphFrameOnly.insert(n.id);
            graphFrameRequest = true;
        }
        if (frameCounter == 40) SaveDocument(*a, "typed_test.sxl");
        if (frameCounter == 140 && LoadSxl("typed_test.sxl")) {
            Document& r = *Active()->doc;
            int counts[16] = {};
            for (auto& n : r.S().graph.nodes) counts[(int)n.kind]++;
            for (auto& n : r.S().graph.nodes)
                if (n.kind == NodeKind::Function && n.sub)
                    Log("[demo] typed reloaded function: inner %zu vars %zu Compteur=%s", n.sub->nodes.size(), n.sub->variables.size(),
                        n.varValues.count("Compteur") ? ValueToText(n.varValues.at("Compteur")).c_str() : "-");
            Log("[demo] typed reloaded: value %d math %d event %d action %d", counts[(int)NodeKind::Value], counts[(int)NodeKind::Math], counts[(int)NodeKind::Event],
                counts[(int)NodeKind::Action]);
            for (auto& n : r.S().graph.nodes)
                if (n.kind == NodeKind::Math) Log("[demo] typed reloaded multiply = %s", ValueToText(Active()->renderer.EvalValue(r, n.id, 0)).c_str());
        }
        return;
    }
    if (demoMode == 73) {
        // Functions: the "Peinture" nodes collapsed into a function (SX_FKIND subgraph / tool); SX_STEP may add
        // "apply" (Apply mode), "edit" (its tab: an Invert node before its output), "lib" (saved, inserted back).
        static DocView* parent = nullptr;
        static Id fid = 0;
        char kind[32] = "subgraph", step[64] = "";
        GetEnvironmentVariableA("SX_FKIND", kind, sizeof(kind));
        GetEnvironmentVariableA("SX_STEP", step, sizeof(step));
        if (frameCounter == 8) {
            parent = Active();
            if (!parent) return;
            Edit e(*parent->doc, parent->history, "Passer en mode nodal");
            parent->doc->ConvertToNodes();
            e.Commit();
        }
        if (!parent || !ViewAlive(parent)) return;
        Document& d = *parent->doc;
        auto logState = [&](const char* when) {
            const Node* f = d.S().graph.Find(fid);
            if (!f) { Log("[demo] functions %s: no function", when); return; }
            std::unordered_set<Id> frozen;
            GraphFrozenNodes(d.S().graph, frozen);
            Graph flat;
            FlattenGraph(d.S().graph, flat);
            Log("[demo] functions %s: kind %s in %d out %d inner %zu apply %d applied %d frozen %zu flat %zu", when, f->funcKind.c_str(), NodeInputCount(*f),
                NodeOutputCount(*f), f->sub ? f->sub->nodes.size() : 0, (int)f->applyMode, (int)FunctionApplied(*f), frozen.size(), flat.nodes.size());
        };
        if (frameCounter == 10) {
            Id paint = 0;
            for (auto& l : d.S().layers) if (l.name == "Peinture") paint = l.id;
            std::set<Id> sel;
            for (auto& n : d.S().graph.nodes)
                if (n.layer == paint && n.kind != NodeKind::Blend && (!strstr(step, "fx") || n.kind == NodeKind::Effect)) sel.insert(n.id);
            char want[64] = "";
            if (GetEnvironmentVariableA("SX_SEL", want, sizeof(want)) > 0) {
                sel.clear();
                for (auto& n : d.S().graph.nodes) if (strstr(want, n.name.c_str()) && !n.name.empty()) sel.insert(n.id);
            }
            for (auto& n : d.S().graph.nodes) Log("[demo] node %u %s kind %d inputs %zu", n.id, n.name.c_str(), (int)n.kind, n.inputs.size());
            FunctionFromSelection(*parent, sel, kind);
            fid = d.S().activeNode;
            graphFrameRequest = true;
        }
        if (frameCounter == 20) logState("created");
        if (frameCounter == 20)
            if (const Node* f = d.S().graph.Find(fid); f && f->sub)
                for (auto& m : f->sub->nodes) Log("[demo]   inner %u %s kind %d port %d inputs %zu", m.id, m.name.c_str(), (int)m.kind, m.port, m.inputs.size());
        if (frameCounter == 22 && strstr(step, "apply")) {
            if (Node* f = d.S().graph.Find(fid); f && !f->applyMode) RequestToolAction(*parent, fid, ToolAction::ToggleLive);
            RequestToolAction(*parent, fid, ToolAction::Bake);
        }
        if (frameCounter == 32) logState("after-apply");
        if (frameCounter == 34 && strstr(step, "edit")) EditFunction(*parent, fid);
        if (frameCounter == 40 && strstr(step, "edit")) {
            DocView* c = Active();
            if (c && c->funcLink.parent == parent) {
                Graph& g = c->doc->S().graph;
                Node* out = g.Find(g.output);
                const Id prev = out && !out->inputs.empty() ? out->inputs[0] : 0;
                Node& inv = c->doc->AddEffectNode("invert", "", out ? out->pos - float2{200, -200} : float2{});
                const Id iid = inv.id;
                if (prev) c->doc->Link(prev, iid, 0);
                c->doc->Link(iid, g.output, 0);
                c->history.PushState("Inverser", c->doc->S(), c->doc->S());
                Log("[demo] functions edit: child nodes %zu", g.nodes.size());
            }
        }
        if (frameCounter == 50) logState("after-edit");
        if (frameCounter == 52 && strstr(step, "edit")) {
            for (int i = 0; i < (int)docs.size(); ++i) if (docs[i].get() == parent) activeDoc = focusDocRequest = i;
        }
        if (frameCounter == 56 && strstr(step, "lib")) SaveFunctionToLibrary(*parent, fid);
        if (frameCounter == 70 && strstr(step, "lib")) {
            Log("[demo] functions library: %zu entries", FunctionLibrary().size());
            for (auto& f : FunctionLibrary()) Log("[demo]   %s (%s) %s", f.name.c_str(), f.kind.c_str(), f.path.c_str());
            if (!FunctionLibrary().empty()) {
                Id nid = InsertLibraryFunction(*parent, FunctionLibrary().front().path, {0, -400});
                if (const Node* n = d.S().graph.Find(nid))
                    Log("[demo] functions inserted: %s in %d out %d inner %zu", n->name.c_str(), NodeInputCount(*n), NodeOutputCount(*n), n->sub ? n->sub->nodes.size() : 0);
            }
        }
        return;
    }
    if (demoMode == 72) {
        // Graph tools: a tool node (SX_TOOL = bake / upscale / comfyui) inserted before the output and
        // baked; SX_STEP may add "live" (PassLive on) and "convert" (convert to image).
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        char tool[32] = "bake", step[64] = "";
        GetEnvironmentVariableA("SX_TOOL", tool, sizeof(tool));
        GetEnvironmentVariableA("SX_STEP", step, sizeof(step));
        static Id tid = 0;
        auto logState = [&](const char* when) {
            const Node* n = d.S().graph.Find(tid);
            if (!n) return;
            std::unordered_set<Id> frozen;
            GraphFrozenNodes(d.S().graph, frozen);
            SmartObject* so = n->smart ? d.GetSmart(n->smart) : nullptr;
            Log("[demo] graphtools %s: kind %d surface %llu passLive %d baked %d smart %ux%u frozen %zu", when, (int)n->kind, (unsigned long long)n->surface,
                (int)n->passLive, (int)ToolBaked(*n), so ? so->width : 0, so ? so->height : 0, frozen.size());
        };
        if (frameCounter == 8 && d.Mode() == DocMode::Layers) {
            Edit e(d, a->history, "Passer en mode nodal");
            d.ConvertToNodes();
            e.Commit();
        }
        if (frameCounter == 10) {
            Node* out = d.S().graph.Find(d.S().graph.output);
            if (!out) return;
            const Id prev = out->inputs.empty() ? 0 : out->inputs[0];
            const float2 at = out->pos;
            out->pos = at + float2{320, 0};
            Node& t = d.AddToolNode(tool, at);
            tid = t.id;
            if (prev) d.Link(prev, tid, 0);
            d.Link(tid, d.S().graph.output, 0);
            d.S().activeNode = tid;
            graphFrameRequest = true;
            RequestToolAction(*a, tid, ToolAction::Bake);
        }
        if (frameCounter == 24) logState("baked");
        if (frameCounter == 26 && strstr(step, "live")) RequestToolAction(*a, tid, ToolAction::ToggleLive);
        if (frameCounter == 32) logState("after-live");
        if (frameCounter == 34 && strstr(step, "convert")) RequestToolAction(*a, tid, ToolAction::ConvertToImage);
        if (frameCounter == 40) logState("final");
        if (frameCounter == 44 && strstr(step, "save")) SaveDocument(*a, "graphtools_test.sxl");
        if (frameCounter == 90 && strstr(step, "save") && LoadSxl("graphtools_test.sxl")) {
            for (auto& n : Active()->doc->S().graph.nodes)
                if (n.kind == NodeKind::Tool) {
                    SmartObject* so = n.smart ? Active()->doc->GetSmart(n.smart) : nullptr;
                    Log("[demo] graphtools reloaded: tool %s surface %d passLive %d smart %ux%u props %zu", n.tool.c_str(), (int)(Active()->doc->GetSurface(n.surface) != nullptr),
                        (int)n.passLive, so ? so->width : 0, so ? so->height : 0, n.props.size());
                }
        }
        return;
    }
    if (demoMode == 71) {
        // Layer transform tool with synthetic ImGui mouse events (never the real cursor): the "Peinture"
        // pixel layer is moved past the document (snapping 10 px), turned, scaled; SX_MODE picks the final mode.
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        ImGuiIO& io = ImGui::GetIO();
        auto screen = [&](float2 p) { return ImVec2(lxf.canvasMin.x + a->pan.x + p.x * a->zoom, lxf.canvasMin.y + a->pan.y + p.y * a->zoom); };
        auto pivot = [&]() {
            Layer* l = d.Mode() == DocMode::Layers ? d.ActiveLayer() : nullptr;
            return l && l->kind == LayerKind::Smart ? (l->smartCorners[0] + l->smartCorners[2]) * 0.5f : float2{d.Width() * 0.5f, d.Height() * 0.5f};
        };
        static ImVec2 start;
        if (frameCounter == 8 && GetEnvironmentVariableA("SX_NODES", nullptr, 0) > 0 && d.Mode() == DocMode::Layers) {
            Edit e(d, a->history, "Passer en mode nodal");
            d.ConvertToNodes();
            e.Commit();
        }
        if (frameCounter == 9 && GetEnvironmentVariableA("SX_FILL", nullptr, 0) > 0) {
            // A Draw layer (shader-drawn): the tool puts a Transform filter in front of its filters.
            Edit e(d, a->history, "Remplissage");
            Layer& f = d.AddEffectLayer(LayerKind::Fill, "checker");
            f.name = "Damier";
            e.Commit();
        }
        if (frameCounter == 10) {
            char name[64] = "Peinture";
            if (GetEnvironmentVariableA("SX_FILL", nullptr, 0) > 0) strcpy_s(name, "Damier");
            GetEnvironmentVariableA("SX_LAYER", name, sizeof(name));
            for (auto& l : d.S().layers)
                if (l.name == name) d.S().activeLayer = l.id;
            for (auto& n : d.S().graph.nodes)
                if (d.Mode() == DocMode::Nodes && n.name == name) d.S().activeNode = n.id;
            tool = Tool::LayerTransform;
            lxf.snap[0] = true;
            lxf.step[0] = 10;
        }
        auto drag = [&](int f0, int mode, ImVec2 from, ImVec2 to) {
            if (frameCounter == (u64)f0) {
                lxf.mode = mode;
                io.AddMousePosEvent(from.x, from.y);
            }
            if (frameCounter == (u64)f0 + 1) io.AddMouseButtonEvent(0, true);
            for (int k = 1; k <= 8; ++k)
                if (frameCounter == (u64)f0 + 1 + k) io.AddMousePosEvent(from.x + (to.x - from.x) * k / 8, from.y + (to.y - from.y) * k / 8);
            if (frameCounter == (u64)f0 + 11) io.AddMouseButtonEvent(0, false);
        };
        if (frameCounter == 20) start = screen(pivot());
        if (frameCounter >= 20 && frameCounter < 34) drag(20, 0, start, ImVec2(start.x + 260, start.y + 37));
        if (frameCounter == 36) start = screen(pivot());
        if (frameCounter >= 36 && frameCounter < 50) drag(36, 1, ImVec2(start.x - 150, start.y), ImVec2(start.x - 130, start.y - 75));
        if (frameCounter == 52) start = screen(pivot());
        if (frameCounter >= 52 && frameCounter < 66) drag(52, 2, ImVec2(start.x + 40, start.y + 40), ImVec2(start.x + 60, start.y + 60));
        if (frameCounter == 60) ShowSideTab(0);
        if (frameCounter == 70) {
            char m[8] = "";
            if (GetEnvironmentVariableA("SX_MODE", m, sizeof(m)) > 0) lxf.mode = atoi(m);
            if (Layer* l = d.Mode() == DocMode::Layers ? d.ActiveLayer() : nullptr; l && l->kind != LayerKind::Smart)
                for (auto& f : l->filters)
                    if (f.effectId == "transform_2d") Log("[demo] layerxf: transform filter pos %.1f,%.1f scale %.3f,%.3f rot %.2f", f.params[0].x, f.params[0].y, f.params[2].x, f.params[2].y, f.params[3].x);
            if (d.Mode() == DocMode::Nodes)
                for (auto& n : d.S().graph.nodes)
                    if (n.fx.effectId == "transform_2d") Log("[demo] layerxf: transform node pos %.1f,%.1f scale %.3f rot %.2f", n.fx.params[0].x, n.fx.params[0].y, n.fx.params[2].x, n.fx.params[3].x);
            if (Layer* l = d.Mode() == DocMode::Layers ? d.ActiveLayer() : nullptr)
                Log("[demo] layerxf: kind %d corners %.1f,%.1f %.1f,%.1f %.1f,%.1f %.1f,%.1f", (int)l->kind, l->smartCorners[0].x, l->smartCorners[0].y,
                    l->smartCorners[1].x, l->smartCorners[1].y, l->smartCorners[2].x, l->smartCorners[2].y, l->smartCorners[3].x, l->smartCorners[3].y);
            io.AddMousePosEvent(-100, -100);
        }
        return;
    }
    if (demoMode == 70) {
        // Unreal mesh conversion alone (SX_UASSET), result in the log.
        char path[1024] = "";
        if (frameCounter == 5 && GetEnvironmentVariableA("SX_UASSET", path, sizeof(path)) > 0) {
            std::string glb, err;
            bool ok = ConvertUAssetMesh(path, glb, &err);
            Log("[demo] uassetconv: %s %s", ok ? "OK" : "FAIL", ok ? glb.c_str() : err.c_str());
        }
        return;
    }
    if (demoMode == 69) {
        // Simulated drops (no real mouse): SX_DROPS = files separated by ';', released at the center of the
        // window SX_DROPWIN ("Rendu", "Graphe", ...). A 3D project given on the command line is brought forward;
        // SX_NODES: the active document goes to node mode first.
        char drops[4096] = "", win[64] = "Rendu";
        GetEnvironmentVariableA("SX_DROPS", drops, sizeof(drops));
        GetEnvironmentVariableA("SX_DROPWIN", win, sizeof(win));
        for (int i = 0; i < (int)docs.size(); ++i)
            if (docs[i]->doc->kind == 1 && activeDoc != i && frameCounter < 20) { activeDoc = i; focusDocRequest = i; }
        if (frameCounter == 18 && GetEnvironmentVariableA("SX_NODES", nullptr, 0) > 0 && Active() && Active()->doc->Mode() == DocMode::Layers) {
            Edit e(*Active()->doc, Active()->history, "Passer en mode nodal");
            Active()->doc->ConvertToNodes();
            e.Commit();
        }
        if (frameCounter == 20) FocusPanel(win);
        static size_t next = 0;
        if (frameCounter >= 30 && frameCounter % 10 == 0) {
            std::vector<std::string> files;
            for (char* t = strtok(drops, ";"); t; t = strtok(nullptr, ";")) files.push_back(t);
            if (next < files.size())
                if (ImGuiWindow* w = ImGui::FindWindowByName(win)) {
                    DropRequest r;
                    r.path = files[next++];
                    r.x = w->Pos.x + w->Size.x * 0.5f;
                    r.y = w->Pos.y + w->Size.y * 0.5f;
                    pendingDrops.push_back(r);
                    Log("[demo] drop %s on %s", r.path.c_str(), win);
                }
        }
        return;
    }
    if (demoMode == 68) {
        // Linked document (--link): a red stroke on the model (or the image), then "Envoyer vers Unreal".
        static int sentAt = -1;
        for (int i = 0; i < (int)docs.size(); ++i)
            if (docs[i]->link && activeDoc != i && sentAt < 0) { activeDoc = i; focusDocRequest = i; }
        DocView* a = Active();
        if (!a || !a->link) return;
        const bool live = GetEnvironmentVariableA("SX_LIVE", nullptr, 0) > 0;  // "Direct": no explicit send
        if (live && frameCounter == 12 && !LinkLive(*a)) SetLinkLive(*a, true);
        if (sentAt < 0 && frameCounter > 20) {
            Document& d = *a->doc;
            if (a->mesh && a->mesh->depth) {
                MeshScene& s = *a->mesh;
                fg = {0.9f, 0.1f, 0.1f, 1};
                std::vector<float4> colors;
                std::vector<Id> targets = Paint3DTargets(*a, colors);
                if (a->renderer.BeginStrokeMulti(d, *ctx, targets)) {
                    std::vector<Dab> dabs;
                    float cx = s.depth->Width() * 0.5f, cy = s.depth->Height() * 0.5f;
                    for (int i = 0; i <= 200; ++i) dabs.push_back({cx - 150 + i * 1.5f, cy + 40 * std::sin(i * 0.06f), 14, 1});
                    a->renderer.MeshStrokeDabsMulti(d, *ctx, s, dabs, brush, colors, false);
                    a->renderer.EndStrokeMulti(d, *ctx, a->history, "Peinture 3D");
                }
            } else if (Id target = d.PaintTarget()) {
                fg = {0.9f, 0.1f, 0.1f, 1};
                if (a->renderer.BeginStroke(d, *ctx, target)) {
                    std::vector<Dab> dabs;
                    for (int i = 0; i <= 200; ++i) dabs.push_back({d.Width() * (0.2f + i * 0.003f), d.Height() * 0.5f, d.Width() * 0.02f, 1});
                    a->renderer.StrokeDabs(d, *ctx, dabs, brush, fg, false);
                    a->renderer.EndStroke(d, *ctx, a->history, "Pinceau");
                }
            }
            if (!live) SendToUnreal(*a);
            sentAt = (int)frameCounter;
        }
        if (sentAt > 0 && frameCounter == (u64)sentAt + 30) Log("[demo] linksend: sent (%s)", a->link ? "ok" : "-");
        return;
    }
    if (demoMode == 67) {
        DemoExample3D();
        return;
    }
    if (demoMode == 66) {
        // Render mode on the project given on the command line: SX_HDRI (file), SX_CPU (CPU renderer),
        // SX_SCENE (extra objects: glass sphere, gold cube, spot), SX_SAMPLES, SX_EXPORT (image written at frame 150).
        // The demo scene document opens too: bring the 3D project forward.
        for (int i = 0; i < (int)docs.size(); ++i)
            if (docs[i]->doc->kind == 1 && activeDoc != i && frameCounter < 20) { activeDoc = i; focusDocRequest = i; }
        DocView* a = Active();
        if (!a || !a->mesh || a->doc->kind != 1) {
            if (frameCounter == 30) Log("[demo] render: no 3D project open");
            return;
        }
        char b[512] = "";
        wchar_t wpreset[256] = L"";
        if (frameCounter == 21 && GetEnvironmentVariableW(L"SX_PRESET", wpreset, 256) > 0) {
            // A scene preset applied (whole scene); the name may have accents (read as UTF-16).
            const std::string want = Narrow(wpreset);
            ScanScenePresets();
            for (auto& p : scenePresets)
                if (p.name == want) Log("[demo] render: preset %s -> %d", want.c_str(), (int)LoadScene(*a, p.path, 0));
        }
        if (frameCounter == 20) {
            PtRenderState& r = RenderStateFor(*a);
            if (GetEnvironmentVariableA("SX_HDRI", b, sizeof(b)) > 0) r.settings.hdri = b;
            if (GetEnvironmentVariableA("SX_CPU", b, sizeof(b)) > 0) r.settings.device = 1;
            if (GetEnvironmentVariableA("SX_SAMPLES", b, sizeof(b)) > 0) r.settings.maxSamples = atoi(b);
            if (GetEnvironmentVariableA("SX_TONEMAP", b, sizeof(b)) > 0) r.settings.tonemap = atoi(b);
            if (GetEnvironmentVariableA("SX_EXPOSURE", b, sizeof(b)) > 0) r.settings.exposure = (float)atof(b);
            if (GetEnvironmentVariableA("SX_POST", b, sizeof(b)) > 0) {
                // Camera effects on the path-traced image: bloom, vignette, grain, warm grade.
                PostSettings& p = r.settings.post;
                p.bloom = 0.9f; p.bloomThreshold = 0.7f; p.vignette = 0.5f; p.grain = 0.3f; p.temperature = 0.2f; p.contrast = 0.1f; p.chroma = 0.25f;
            }
            if (GetEnvironmentVariableA("SX_COAT", b, sizeof(b)) > 0)
                for (auto& o : r.objects)
                    if (o.type == PtObjectType::Project) o.material.coat = (float)atof(b);
            if (GetEnvironmentVariableA("SX_SCENE", b, sizeof(b)) > 0) {
                // The model turned so the render camera sees it as the 3D view does (logo side).
                for (auto& o : r.objects)
                    if (o.type == PtObjectType::Project) o.rotation.x = (a->mesh->yaw - r.yaw) * 57.29578f;
                float floor = 1e30f;
                for (auto& v : a->mesh->data.vertices) floor = std::min(floor, v.pos.y);
                PtObject glass;
                glass.name = "Sphère de verre";
                glass.type = PtObjectType::Sphere;
                glass.position = {1.25f, floor + 0.35f, 0.3f};
                glass.scale = 0.35f;
                glass.material.baseColor = {1, 1, 1};
                glass.material.roughness = 0.02f;
                glass.material.transmission = 1;
                glass.material.ior = 1.5f;
                r.objects.push_back(glass);
                PtObject cube;
                cube.name = "Cube or";
                cube.type = PtObjectType::Cube;
                cube.position = {-1.3f, floor + 0.3f, 0.5f};
                cube.rotation = {30, 0, 0};
                cube.scale = 0.3f;
                cube.material.baseColor = {1.0f, 0.71f, 0.29f};
                cube.material.metallic = 1;
                cube.material.roughness = 0.25f;
                r.objects.push_back(cube);
                PtLight spot;
                spot.name = "Spot";
                spot.type = PtLightType::Spot;
                spot.position = {1.8f, 2.0f, -0.8f};
                spot.color = {1.0f, 0.75f, 0.5f};
                spot.intensity = 250;
                spot.size = 0.05f;
                spot.spotAngle = 35;
                float3 d = Normalize(float3{0, 0, 0} - spot.position);
                spot.yaw = std::atan2(d.x, d.z) * 57.29578f;
                spot.pitch = std::asin(d.y) * 57.29578f;
                r.lights.push_back(spot);
            }
        }
        if (frameCounter == 22) FocusPanel("Rendu");
        if (frameCounter % 30 == 0 && a->render) Log("[demo] render: %d samples (%s)", RenderSamples(*a->render), a->render->settings.device ? "CPU" : "GPU");
        // Export once every sample is done (SX_SIZE=WxH: fixed resolution instead of the view size).
        if (frameCounter == 20 && GetEnvironmentVariableA("SX_SIZE", b, sizeof(b)) > 0) {
            PtRenderState& r = RenderStateFor(*a);
            if (sscanf(b, "%dx%d", &r.settings.width, &r.settings.height) == 2) r.settings.viewSize = false;
        }
        static bool exported = false;
        if (!exported && frameCounter > 30 && a->render && RenderSamples(*a->render) >= a->render->settings.maxSamples &&
            GetEnvironmentVariableA("SX_EXPORT", b, sizeof(b)) > 0) {
            if (GetEnvironmentVariableA("SX_PASSES", nullptr, 0) > 0) ExportRenderPasses(*a, b);  // + albedo, normal, depth, mask
            else ExportRender(*a, b);
            exported = true;
            captureFrame = std::min(captureFrame, (int)frameCounter + 20);  // done: capture and quit soon after
            Log("[demo] render exported at frame %d", frameCounter);
        }
        return;
    }
    if (demoMode == 100) {
        // Side tabs keep their sub-tab: General, Histogramme chosen, Outils, back to General.
        if (frameCounter == 20) ShowSideTab(0);
        if (frameCounter == 30) FocusPanel("Histogramme");
        if (frameCounter == 40) ShowSideTab(1);
        if (frameCounter == 60) ShowSideTab(0);
        if (frameCounter == 90)
            if (ImGuiWindow* w = ImGui::FindWindowByName("Histogramme"); w && w->DockNode && w->DockNode->TabBar) {
                const ImGuiID sel = w->DockNode->TabBar->SelectedTabId;
                const char* name = "?";
                for (const char* n : {"Propriétés", "Historique", "Histogramme", "Calques"})
                    if (ImGuiWindow* x = ImGui::FindWindowByName(n); x && x->TabId == sel) name = n;
                Log("[demo] sidetabs: selected after coming back: %s", name);
            }
        // The user's case: Outils > Tracés, General, back to Outils (twice).
        auto toolsSelected = [&] {
            const char* name = "?";
            if (ImGuiWindow* w = ImGui::FindWindowByName("Tracés"); w && w->DockNode && w->DockNode->TabBar) {
                const ImGuiID sel = w->DockNode->TabBar->SelectedTabId;
                for (const char* n : {"Options", "Tracés", "Actions", "Compositions de calques", "Styles de texte", "Trim sheet"})
                    if (ImGuiWindow* x = ImGui::FindWindowByName(n); x && x->TabId == sel) name = n;
            }
            return name;
        };
        if (frameCounter == 100) ShowSideTab(1);
        if (frameCounter == 115) FocusPanel("Tracés");
        if (frameCounter == 125) Log("[demo] sidetabs: Outils, chosen: %s", toolsSelected());
        if (frameCounter == 130) ShowSideTab(0);
        if (frameCounter == 150) ShowSideTab(1);
        if (frameCounter == 185) Log("[demo] sidetabs: Outils after coming back: %s", toolsSelected());
        if (frameCounter == 190) ShowSideTab(0);
        if (frameCounter == 200) ShowSideTab(1);
        if (frameCounter == 240) Log("[demo] sidetabs: Outils after coming back again: %s", toolsSelected());
        return;
    }
    if (demoMode == 99) {
        // Text filters: SX_FILTER in Propriétés (layer of the demo scene); SX_MENUFILTER typed in the Fichier menu
        // (opened by a synthetic click, the menu's filter box takes the keyboard when it opens).
        char f[64] = "", mf[64] = "";
        GetEnvironmentVariableA("SX_FILTER", f, sizeof(f));
        GetEnvironmentVariableA("SX_MENUFILTER", mf, sizeof(mf));
        if (frameCounter == 20 && f[0]) {
            FocusPanel("Propriétés");
            ui::SetWindowFilter("Propriétés", f);
        }
        ImGuiIO& io = ImGui::GetIO();
        if (mf[0]) {
            if (frameCounter == 20) io.AddMousePosEvent(72 * dpi, 14 * dpi);
            if (frameCounter == 22) io.AddMouseButtonEvent(0, true);
            if (frameCounter == 23) io.AddMouseButtonEvent(0, false);
            if (frameCounter == 30) io.AddInputCharactersUTF8(mf);
            if (frameCounter == 31) io.AddMousePosEvent(72 * dpi, 300 * dpi);
        }
        return;
    }
    if (demoMode == 98) {
        // Layer graph node selection (the demo scene): SX_NODE = blend (the top layer's blend node) or output.
        char which[16] = "blend";
        GetEnvironmentVariableA("SX_NODE", which, sizeof(which));
        if (frameCounter == 20 && Active()) {
            Document& d = *Active()->doc;
            const Node* pick = nullptr;
            for (auto& n : d.S().graph.nodes) {
                if (std::string(which) == "output" && n.kind == NodeKind::Output) pick = &n;
                if (std::string(which) == "blend" && n.kind == NodeKind::Blend && n.layer) pick = &n;  // last one: the top layer
            }
            if (pick) {
                if (pick->layer) d.S().activeLayer = pick->layer;
                d.S().activeNode = pick->id;
                Log("[demo] nodesel: %s (%s)", pick->name.c_str(), which);
            }
            FocusPanel("Propriétés");
        }
        return;
    }
    if (demoMode == 97) {
        // 3D view projection: a Paint 3D cube project, SX_VIEW (-1: 3/4 view, 0..5 presets), SX_ORTHO, SX_Q (2 path tracing).
        char vw[8] = "-1", q[8] = "1";
        GetEnvironmentVariableA("SX_VIEW", vw, sizeof(vw));
        GetEnvironmentVariableA("SX_Q", q, sizeof(q));
        if (frameCounter == 4) {
            p3dNew = {};
            p3dNew.mesh = 1;
            NewPaint3DProject();
        }
        DocView* a = Active();
        if (!a || !a->mesh) return;
        MeshScene& s = *a->mesh;
        if (frameCounter == 8) FocusPanel("Vue 3D");
        if (frameCounter == 30 && GetEnvironmentVariableA("SX_CANVAS", nullptr, 0) > 0) FocusPanel("Canevas");  // image tools back
        if (frameCounter == 10) {
            const int k = atoi(vw);
            if (k >= 0) SetView3DPreset(s, k);
            else { s.yaw = 0.65f; s.pitch = 0.45f; }
            s.ortho = GetEnvironmentVariableA("SX_ORTHO", nullptr, 0) > 0 || k >= 0;
            s.quality = atoi(q);
            s.distance = 3.2f;
            char ap[16] = "", dn[16] = "", sm[16] = "";
            if (GetEnvironmentVariableA("SX_DOF", ap, sizeof(ap)) > 0) {
                s.dofAperture = (float)atof(ap);
                s.dofFocus = 2.4f;
                s.rtFloor = true;
            }
            if (GetEnvironmentVariableA("SX_DENOISE", dn, sizeof(dn)) > 0) s.rtDenoise = (float)atof(dn);
            if (GetEnvironmentVariableA("SX_SAMPLES", sm, sizeof(sm)) > 0) s.rtSamples = atoi(sm);
            Log("[demo] view3d: view %d ortho %d quality %d", k, (int)s.ortho, s.quality);
            if (GetEnvironmentVariableA("SX_SIMTOOL", nullptr, 0) > 0) {
                tool = Tool::Particles;  // as chosen in the toolbar: its panel brought forward
                *PanelFlag("Matériau du pinceau") = true;
                FocusPanel("Matériau du pinceau");
            }
        }
        return;
    }
    if (demoMode == 65) {
        // Example Paint 3D project: SX_MESHFILE (a mug without UVs -> xatlas), gold rim (material + mask),
        // blue glossy pattern (paint layer, several channels), logo decal (SX_DECAL), saved to SX_OUT.
        char meshFile[512] = "", decalPath[512] = "", out[512] = "";
        GetEnvironmentVariableA("SX_MESHFILE", meshFile, sizeof(meshFile));
        GetEnvironmentVariableA("SX_DECAL", decalPath, sizeof(decalPath));
        GetEnvironmentVariableA("SX_OUT", out, sizeof(out));
        auto presetIndex = [](const char* name) {
            for (int i = 0; i < MaterialPresetCount(); ++i) if (std::string(MaterialPresetName(i)) == name) return i;
            return 0;
        };
        if (frameCounter == 4) {
            p3dNew = {};
            p3dNew.mesh = 3;
            p3dNew.meshPath = meshFile;
            p3dNew.basePreset = presetIndex("Plastique blanc");
            NewPaint3DProject();
        }
        DocView* a = Active();
        if (!a || !a->mesh || a->doc->kind != 1) return;
        Document& d = *a->doc;
        MeshScene& s = *a->mesh;
        if (frameCounter == 8) FocusPanel("Vue 3D");
        // World points -> view pixels (the depth buffer is the view).
        auto toScreen = [&](float3 p) {
            float4 c = s.viewProj.Transform({p.x, p.y, p.z, 1});
            return float2{(c.x / c.w * 0.5f + 0.5f) * s.depth->Width(), (0.5f - c.y / c.w * 0.5f) * s.depth->Height()};
        };
        float yMin = 1e9f, yMax = -1e9f;
        for (auto& vtx : s.data.vertices) { yMin = std::min(yMin, vtx.pos.y); yMax = std::max(yMax, vtx.pos.y); }
        auto radiusAt = [&](float y) {
            float r = 0;
            for (auto& vtx : s.data.vertices)
                if (std::abs(vtx.pos.y - y) < (yMax - yMin) * 0.02f && vtx.pos.x <= 0)  // the side without the handle
                    r = std::max(r, std::sqrt(vtx.pos.x * vtx.pos.x + vtx.pos.z * vtx.pos.z));
            return r;
        };
        // Strokes around the mug: four views, the visible half each time.
        auto ring = [&](float y, float wave, float waves, float radiusPx, std::function<void()> select) {
            float r = radiusAt(y) * 1.01f;
            for (int k = 0; k < 16; ++k) {
                s.yaw = k * 0.3926991f;
                a->renderer.MeshRender(*ctx, s, GatherMaterial(*a), s.depth->Width(), s.depth->Height());
                select();
                std::vector<float4> colors;
                std::vector<Id> targets = Paint3DTargets(*a, colors);
                if (!a->renderer.BeginStrokeMulti(d, *ctx, targets)) continue;
                std::vector<Dab> dabs;
                for (int i = 0; i <= 720; ++i) {
                    float t = i / 720.0f * 6.2831853f;
                    float yy = y + std::sin(t * waves) * wave;
                    float3 p{std::cos(t) * r, yy, std::sin(t) * r};
                    // Only the points facing the camera (the back of the ring would project onto the front).
                    float3 toCam = Normalize(s.camPos - p);
                    if (std::cos(t) * toCam.x + std::sin(t) * toCam.z < 0.7f) continue;
                    if (std::cos(t) > 0.75f) continue;  // the handle side stays clean
                    float2 sp = toScreen(p);
                    dabs.push_back({sp.x, sp.y, radiusPx, 1});
                }
                if (dabs.empty()) { a->renderer.EndStrokeMulti(d, *ctx, a->history, "Peinture 3D"); continue; }
                BrushSettings b = brush;
                b.hardness = 0.75f;
                a->renderer.MeshStrokeDabsMulti(d, *ctx, s, dabs, b, colors, false);
                a->renderer.EndStrokeMulti(d, *ctx, a->history, "Peinture 3D");
            }
        };
        const float H = yMax - yMin;
        if (frameCounter == 12 && s.depth) {
            // Gold rim: a material whose mask is painted white along the top.
            AddMaterialLayer(*a, presetIndex("Or"));
            Id gold = materialSel;
            // Its mask, computed per texel from the surface: the rim (top of the outside, over the edge)
            // and a thin band near the foot. Triangles rasterized in UV space with the 3D position.
            const int W = (int)d.Width();
            std::vector<float> mask((size_t)W * W, 0.0f);
            const auto& V = s.data.vertices;
            const auto& I = s.data.indices;
            float bodyR[65];  // body radius by height (handle excluded)
            for (int k = 0; k <= 64; ++k) bodyR[k] = radiusAt(yMin + H * k / 64.0f);
            auto value = [&](float3 p, float3 n) {
                float rad = std::sqrt(p.x * p.x + p.z * p.z);
                float outward = rad > 1e-4f ? (n.x * p.x + n.z * p.z) / rad : 0.0f;
                float y = (p.y - yMin) / H;
                float br = bodyR[std::clamp((int)std::lround(y * 64), 0, 64)];
                if (rad > br * 1.08f) return 0.0f;                                    // not on the handle
                float rim = std::clamp((y - 0.93f) / 0.01f, 0.0f, 1.0f);              // top 7 %, over the edge
                if (outward < -0.2f && y < 0.97f) rim = 0;                             // outside and lip only
                float band = outward > 0.3f ? std::clamp(1.0f - std::abs(y - 0.12f) / 0.015f, 0.0f, 1.0f) : 0.0f;
                return std::max(rim, band);
            };
            for (size_t t = 0; t + 2 < I.size(); t += 3) {
                const MeshVertex &a0 = V[I[t]], &a1 = V[I[t + 1]], &a2 = V[I[t + 2]];
                float2 p0 = {a0.uv.x * W, a0.uv.y * W}, p1 = {a1.uv.x * W, a1.uv.y * W}, p2 = {a2.uv.x * W, a2.uv.y * W};
                int x0 = std::max(0, (int)std::floor(std::min({p0.x, p1.x, p2.x})) - 2), x1 = std::min(W - 1, (int)std::ceil(std::max({p0.x, p1.x, p2.x})) + 2);
                int y0 = std::max(0, (int)std::floor(std::min({p0.y, p1.y, p2.y})) - 2), y1 = std::min(W - 1, (int)std::ceil(std::max({p0.y, p1.y, p2.y})) + 2);
                float den = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
                if (std::abs(den) < 1e-9f) continue;
                for (int y = y0; y <= y1; ++y)
                    for (int x = x0; x <= x1; ++x) {
                        float px = x + 0.5f, py = y + 0.5f;
                        float l0 = ((p1.y - p2.y) * (px - p2.x) + (p2.x - p1.x) * (py - p2.y)) / den;
                        float l1 = ((p2.y - p0.y) * (px - p2.x) + (p0.x - p2.x) * (py - p2.y)) / den;
                        float l2 = 1 - l0 - l1;
                        // A little outside the triangle too: covers the island padding.
                        if (l0 < -0.08f || l1 < -0.08f || l2 < -0.08f) continue;
                        float3 pos = a0.pos * l0 + a1.pos * l1 + a2.pos * l2;
                        float3 nrm = a0.normal * l0 + a1.normal * l1 + a2.normal * l2;
                        float& m = mask[(size_t)y * W + x];
                        m = std::max(m, value(pos, nrm));
                    }
            }
            for (auto& l : d.S().layers)
                if (l.material == gold) {
                    l.name = "Or (bord)";
                    if (l.mask) {
                        a->renderer.Prepare(d, *ctx);
                        WriteMaskData(d, l.mask, mask, W, W);
                        break;
                    }
                }
        }
        if (frameCounter == 16 && s.depth) {
            // Paint layer "Vagues": two waves all around the body, written per texel into the layer's
            // channels (color, roughness: glossy, height: slight relief).
            Id paint = AddPaintLayer3D(*a, "Vagues");
            const int W = (int)d.Width();
            float bodyR[65];
            for (int k = 0; k <= 64; ++k) bodyR[k] = radiusAt(yMin + H * k / 64.0f);
            struct Px { float4 color, data; };  // data: x = roughness, y = height, w = coverage
            std::vector<Px> img((size_t)W * W, Px{{0, 0, 0, 0}, {0, 0, 0, 0}});
            auto shade = [&](float3 p, float3 n) {
                Px out{{0, 0, 0, 0}, {0, 0, 0, 0}};
                float rad = std::sqrt(p.x * p.x + p.z * p.z);
                float y = (p.y - yMin) / H;
                float br = bodyR[std::clamp((int)std::lround(y * 64), 0, 64)];
                float outward = rad > 1e-4f ? (n.x * p.x + n.z * p.z) / rad : 0.0f;
                if (rad > br * 1.08f || outward < 0.3f) return out;
                float t = std::atan2(p.z, p.x);
                auto wave = [&](float c, float amp, float k, float half) {
                    float dist = std::abs(y - (c + amp * std::sin(t * k)));
                    return std::clamp((half - dist) / 0.004f, 0.0f, 1.0f);
                };
                float blue = wave(0.42f, 0.06f, 6, 0.035f), orange = wave(0.30f, 0.035f, 9, 0.016f);
                float3 col = blue > 0 ? float3{0.12f, 0.38f, 0.85f} : float3{0.95f, 0.55f, 0.15f};
                float cov = std::max(blue, orange);
                if (orange > 0 && blue > 0) col = float3{0.95f, 0.55f, 0.15f} * orange + float3{0.12f, 0.38f, 0.85f} * (1 - orange);
                out.color = {col.x, col.y, col.z, cov};
                out.data = {0.15f, blue > 0.5f ? 0.62f : 0.57f, 0, cov};
                return out;
            };
            const auto& V = s.data.vertices;
            const auto& I = s.data.indices;
            for (size_t ti = 0; ti + 2 < I.size(); ti += 3) {
                const MeshVertex &a0 = V[I[ti]], &a1 = V[I[ti + 1]], &a2 = V[I[ti + 2]];
                float2 p0 = {a0.uv.x * W, a0.uv.y * W}, p1 = {a1.uv.x * W, a1.uv.y * W}, p2 = {a2.uv.x * W, a2.uv.y * W};
                int x0 = std::max(0, (int)std::floor(std::min({p0.x, p1.x, p2.x})) - 2), x1 = std::min(W - 1, (int)std::ceil(std::max({p0.x, p1.x, p2.x})) + 2);
                int y0 = std::max(0, (int)std::floor(std::min({p0.y, p1.y, p2.y})) - 2), y1 = std::min(W - 1, (int)std::ceil(std::max({p0.y, p1.y, p2.y})) + 2);
                float den = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
                if (std::abs(den) < 1e-9f) continue;
                for (int y = y0; y <= y1; ++y)
                    for (int x = x0; x <= x1; ++x) {
                        float px = x + 0.5f, py = y + 0.5f;
                        float l0 = ((p1.y - p2.y) * (px - p2.x) + (p2.x - p1.x) * (py - p2.y)) / den;
                        float l1 = ((p2.y - p0.y) * (px - p2.x) + (p0.x - p2.x) * (py - p2.y)) / den;
                        float l2 = 1 - l0 - l1;
                        if (l0 < -0.08f || l1 < -0.08f || l2 < -0.08f) continue;
                        Px v = shade(a0.pos * l0 + a1.pos * l1 + a2.pos * l2, a0.normal * l0 + a1.normal * l1 + a2.normal * l2);
                        Px& o = img[(size_t)y * W + x];
                        if (v.color.w > o.color.w) o = v;
                    }
            }
            // Into the channel layers (premultiplied linear; data channels hold visual gray levels).
            auto upload = [&](PbrChannel c, auto value) {
                Layer* l = MaterialChannelLayer(d, paint, c);
                Surface* sf = l ? d.GetSurface(l->surface) : nullptr;
                if (!sf) return;
                a->renderer.Prepare(d, *ctx);
                std::vector<float> f((size_t)W * W * 4);
                for (size_t i = 0; i < img.size(); ++i) {
                    float4 v = value(img[i]);
                    f[i * 4] = v.x * v.w;
                    f[i * 4 + 1] = v.y * v.w;
                    f[i * 4 + 2] = v.z * v.w;
                    f[i * 4 + 3] = v.w;
                }
                if (sf->tex->GetFormat() == gpu::Format::RGBA32F) {
                    ctx->Upload(sf->tex.get(), f.data(), W * 16);
                } else {
                    std::vector<u16> h(f.size());
                    for (size_t i = 0; i < f.size(); ++i) {
                        float x = f[i];
                        u32 b;
                        memcpy(&b, &x, 4);
                        int e = (int)((b >> 23) & 255) - 127 + 15;
                        h[i] = x <= 0 || e <= 0 ? 0 : e >= 31 ? 0x7BFF : (u16)((e << 10) | ((b >> 13) & 0x3FF));
                    }
                    ctx->Upload(sf->tex.get(), h.data(), W * 8);
                }
                d.CancelPendingClear(l->surface);
                d.TouchSurface(l->surface);
            };
            upload(PbrChannel::BaseColor, [](const Px& p) { return float4{SrgbToLinear(p.color.x), SrgbToLinear(p.color.y), SrgbToLinear(p.color.z), p.color.w}; });
            upload(PbrChannel::Roughness, [](const Px& p) { float g = SrgbToLinear(p.data.x); return float4{g, g, g, p.data.w}; });
            upload(PbrChannel::Height, [](const Px& p) { float g = SrgbToLinear(p.data.y); return float4{g, g, g, p.data.w}; });
            brush3D.use[(int)PbrChannel::Height] = false;
        }
        if (frameCounter == 20 && s.depth && decalPath[0] && LoadDecal(decalPath)) {
            // Logo on the side (+Z), a quarter turn from the handle (+X): both show in the final view.
            s.yaw = 3.1415927f;
            a->renderer.MeshRender(*ctx, s, GatherMaterial(*a), s.depth->Width(), s.depth->Height());
            Id logo = AddPaintLayer3D(*a, "Logo");
            brush3D.use[(int)PbrChannel::Height] = false;
            brush3D.rough = 0.35f;
            std::vector<float4> colors;
            std::vector<Id> targets = Paint3DTargets(*a, colors);
            (void)logo;
            if (a->renderer.BeginStrokeMulti(d, *ctx, targets)) {
                float2 c = toScreen({0, yMin + H * 0.62f, radiusAt(yMin + H * 0.62f) * 1.01f});
                float hh = s.depth->Height() * 0.11f;
                a->renderer.MeshDecalMulti(d, *ctx, s, decal.tex.get(), c, {hh * decal.w / std::max(decal.h, 1), hh}, 0, 1, colors, true);
                a->renderer.EndStrokeMulti(d, *ctx, a->history, "Projection");
            }
        }
        if (frameCounter == 24) {
            s.yaw = 2.35f;
            s.pitch = 0.3f;
            a->history.Clear();
        }
        if (frameCounter == 26) {
            // Render scene saved with the example: glazed (varnish over the painted base).
            a->render.reset();
            PtRenderState& r = RenderStateFor(*a);
            r.objects[0].material.coat = 1.0f;
            r.objects[0].material.coatRoughness = 0.04f;
            r.objects[0].rotation.x = (2.35f - r.yaw) * 57.29578f;  // logo side towards the render camera
        }
        if (frameCounter == 28 && out[0]) SaveDocument(*a, out);
        return;
    }
    if (demoMode == 64) {
        // Automatic UVs: project on a cube (SX_MESH=cube) or sphere, a stroke, preview (frame 40), apply with transfer (SX_APPLY).
        char meshName[16] = "";
        GetEnvironmentVariableA("SX_MESH", meshName, sizeof(meshName));
        if (frameCounter == 4) {
            p3dNew = {};
            p3dNew.mesh = std::string(meshName) == "cube" ? 1 : 0;
            NewPaint3DProject();
        }
        DocView* a = Active();
        if (!a || !a->mesh) return;
        Document& d = *a->doc;
        MeshScene& s = *a->mesh;
        if (frameCounter == 8) FocusPanel("Vue 3D");
        if (frameCounter == 12 && s.depth) {
            fg = {0.9f, 0.15f, 0.1f, 1};
            std::vector<float4> colors;
            std::vector<Id> targets = Paint3DTargets(*a, colors);
            if (a->renderer.BeginStrokeMulti(d, *ctx, targets)) {
                std::vector<Dab> dabs;
                float cx = s.depth->Width() * 0.5f, cy = s.depth->Height() * 0.5f, r = std::min(cx, cy) * 0.5f;
                for (int i = 0; i <= 300; ++i) {
                    float t = i / 300.0f * 6.2831853f;
                    dabs.push_back({cx + std::cos(t) * r, cy + std::sin(t) * r * 0.7f, 18, 1});
                }
                a->renderer.MeshStrokeDabsMulti(d, *ctx, s, dabs, brush, colors, false);
                a->renderer.EndStrokeMulti(d, *ctx, a->history, "Peinture 3D");
            }
        }
        if (frameCounter == 14) {
            uvTool.open = true;
            uvTool.preset = 0;
            uvTool.options = {};
            uvTool.options.resolution = (int)d.Width();
            StartUvUnwrap(*a);
        }
        if (frameCounter == 40) Log("[demo] uv: result %d, %s, charts %u", uvTool.hasResult ? 1 : 0, uvTool.message.c_str(), uvJob ? uvJob->result.charts : 0u);
        if (frameCounter == 44 && GetEnvironmentVariableA("SX_APPLY", nullptr, 0) > 0) {
            ApplyUvUnwrap(*a);
            uvTool.open = false;
            Log("[demo] uv applied: %zu vertices", a->mesh ? a->mesh->data.vertices.size() : 0);
        }
        return;
    }
    if (demoMode == 63) {
        // Paint 3D project: new project (sphere), a metallic gold stroke on the paint layer, a decal (SX_DECAL), .sx3d round trip (SX_OUT).
        char decalPath[512] = "", out[512] = "";
        if (frameCounter == 4) {
            p3dNew = {};
            p3dNew.basePreset = 1;  // white plastic
            char mf[512] = "";
            if (GetEnvironmentVariableA("SX_MESHFILE", mf, sizeof(mf)) > 0) { p3dNew.mesh = 3; p3dNew.meshPath = mf; }
            NewPaint3DProject();
        }
        DocView* a = Active();
        if (!a || !a->mesh) return;
        Document& d = *a->doc;
        MeshScene& s = *a->mesh;
        if (frameCounter == 8) FocusPanel("Vue 3D");
        if (frameCounter == 14 && s.depth) {
            fg = {1.0f, 0.78f, 0.34f, 1};
            brush3D.use[(int)PbrChannel::Roughness] = brush3D.use[(int)PbrChannel::Metallic] = true;
            brush3D.rough = 0.25f;
            brush3D.metal = 1.0f;
            std::vector<float4> colors;
            std::vector<Id> targets = Paint3DTargets(*a, colors);
            Log("[demo] paint3d: layout %d, %zu targets, %zu layers, channels 0x%X", layoutKind, targets.size(), d.S().layers.size(), d.pbrChannels);
            a->renderer.meshSymmetry = GetEnvironmentVariableA("SX_SYM", nullptr, 0) > 0 ? 1 : 0;
            if (a->renderer.meshSymmetry) s.yaw = 1.5707963f;  // facing the mirror plane: it is vertical on screen
            if (a->renderer.meshSymmetry) a->renderer.MeshRender(*ctx, s, GatherMaterial(*a), s.depth->Width(), s.depth->Height());
            if (a->renderer.BeginStrokeMulti(d, *ctx, targets)) {
                std::vector<Dab> dabs;
                float cx = s.depth->Width() * 0.5f, cy = s.depth->Height() * 0.5f, r = std::min(cx, cy) * 0.45f;
                if (a->renderer.meshSymmetry) r *= 0.4f, cx -= r * 1.4f;
                for (int i = 0; i <= 300; ++i) {
                    float t = i / 300.0f;
                    dabs.push_back({cx - r + 2 * r * t, cy + std::sin(t * 9.0f) * r * 0.25f, 22, 1});
                }
                BrushSettings b = brush;
                b.hardness = 0.6f;
                a->renderer.MeshStrokeDabsMulti(d, *ctx, s, dabs, b, colors, false);
                a->renderer.EndStrokeMulti(d, *ctx, a->history, "Peinture 3D");
            }
        }
        if (frameCounter == 18 && GetEnvironmentVariableA("SX_DECAL", decalPath, sizeof(decalPath)) > 0 && LoadDecal(decalPath) && s.depth) {
            brush3D.use[(int)PbrChannel::Metallic] = false;
            brush3D.rough = 0.8f;
            std::vector<float4> colors;
            std::vector<Id> targets = Paint3DTargets(*a, colors);
            if (a->renderer.BeginStrokeMulti(d, *ctx, targets)) {
                float cx = s.depth->Width() * 0.5f, cy = s.depth->Height() * 0.32f;
                a->renderer.MeshDecalMulti(d, *ctx, s, decal.tex.get(), {cx, cy}, {s.depth->Height() * 0.22f, s.depth->Height() * 0.15f}, 0.2f, 1.0f, colors, true);
                a->renderer.EndStrokeMulti(d, *ctx, a->history, "Projection");
            }
        }
        if (frameCounter == 20 && GetEnvironmentVariableA("SX_2D", nullptr, 0) > 0) {
            // 2D stroke on the texture (UV canvas): same channels, shared coverage.
            brush3D.use[(int)PbrChannel::Metallic] = true;
            brush3D.metal = 1;
            brush3D.rough = 0.1f;
            std::vector<float4> colors;
            std::vector<Id> targets = Paint3DTargets(*a, colors);
            if (a->renderer.BeginStrokeMulti(d, *ctx, targets)) {
                std::vector<Dab> dabs;
                for (float x = 200; x < 1850; x += 6) dabs.push_back({x, 1600 + std::sin(x / 120) * 150, 40, 1});
                a->renderer.StrokeDabs(d, *ctx, dabs, brush, colors[0], false);
                a->renderer.StrokeApplySlots(d, *ctx, brush, colors, false);
                a->renderer.EndStrokeMulti(d, *ctx, a->history, "Peinture 3D");
            }
        }
        if (frameCounter == 22) s.yaw += 0.35f;
        if (frameCounter == 24 && GetEnvironmentVariableA("SX_EXPORT", out, sizeof(out)) > 0) {
            d.exportTemplate = 1;
            ExportProjectTextures(*a, out);
        }
        if (frameCounter == 26 && GetEnvironmentVariableA("SX_OUT", out, sizeof(out)) > 0) SaveDocument(*a, out);
        if (frameCounter == 36 && GetEnvironmentVariableA("SX_OUT", out, sizeof(out)) > 0) LoadSxl(out);
        if (frameCounter == 44 && Active()) {
            DocView* r = Active();
            Log("[demo] reloaded project: kind %d, mesh %s, %zu layers, layout %d", r->doc->kind, r->mesh ? r->mesh->data.name.c_str() : "none",
                r->doc->S().layers.size(), layoutKind);
        }
        return;
    }
    if (demoMode == 62) {
        // Effect editor library page (SX_OP=browse) or an opened template (SX_OP=open) / original of a brush (SX_OP=brush).
        char op[16] = "browse";
        GetEnvironmentVariableA("SX_OP", op, sizeof(op));
        std::string o = op;
        if (frameCounter == 5) {
            hideBottom = false;
            FocusPanel("Éditeur d'effets");
            if (o == "browse") {
                efEd.browse = true;
                efEd.browseSearch = "flou";
                efEd.browseSel = "gaussian_blur";
            } else if (o == "open") {
                BeginEditSession("Calques");
                OpenLibraryItem(0, "cy_effect_bloom", false);
            } else {
                BeginEditSession("Options");
                OpenLibraryItem(2, "cy_bristle", true);
            }
        }
        if (frameCounter == 9) FocusPanel("Éditeur d'effets");
        if (frameCounter == 12 && GetEnvironmentVariableA("SX_PAL", nullptr, 0) > 0) {
            // Node palette: one node in the effect graph (center of the view) and one in the layer graph.
            ShowSideTab(2);
            FocusPanel("Palette de nœuds");
            paletteFocus = 1;
            paletteAdd.pending = true;
            paletteAdd.target = 1;
            paletteAdd.key = "noise";
            paletteAdd.hasPos = false;
        }
        if (frameCounter == 15 && GetEnvironmentVariableA("SX_PAL", nullptr, 0) > 0) {
            paletteAdd.pending = true;
            paletteAdd.target = 0;
            paletteAdd.key = std::string("Séparer RGBA (R, V, B, A)") + '\x1f' + "Canaux";
            paletteAdd.hasPos = false;
        }
        if (frameCounter == 20 && GetEnvironmentVariableA("SX_PAL", nullptr, 0) > 0 && Active()) {
            int splits = 0;
            for (auto& n : Active()->doc->S().graph.nodes) splits += n.kind == NodeKind::Split;
            Log("[demo] palette: effect graph %zu nodes, layer graph splits %d, mode %s", efEd.graph.nodes.size(), splits,
                Active()->doc->Mode() == DocMode::Nodes ? "nodal" : "calques");
        }
        if (frameCounter == 24 && o == "open" && GetEnvironmentVariableA("SX_CANCEL", nullptr, 0) > 0) {
            size_t before = Active() ? Active()->doc->S().layers.size() : 0;
            EndEditSession(true);
            Log("[demo] edit cancelled: filters back to %zu layers, focus restored", before);
        }
        return;
    }
    if (demoMode == 61) {
        // Shape booleans: four shapes, one per operation, plus a .sxl round trip (SX_OUT).
        if (frameCounter == 4) NewDocument(1200, 400, Precision::Half16, DocMode::Layers, 0, "Booléens");
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        if (frameCounter == 6) {
            for (int op = 0; op < 4; ++op) {
                ShapeStyle s;
                s.type = 0;
                s.radius = 20;
                float x = 40 + op * 290.0f;
                s.rect = {x, 80, x + 180, 260};
                s.fill = {0.2f, 0.45f, 0.9f, 1};
                s.strokeOn = true;
                s.stroke = {0.05f, 0.05f, 0.1f, 1};
                s.strokeWidth = 4;
                ShapePart p;
                p.op = op;
                p.type = 1;
                p.rect = {x + 90, 150, x + 250, 330};
                s.parts.push_back(p);
                d.AddShapeLayer(s);
            }
            d.SyncGraph();
        }
        char out[512] = "";
        if (frameCounter == 12 && GetEnvironmentVariableA("SX_OUT", out, sizeof(out)) > 0) SaveDocument(*a, out);
        if (frameCounter == 24 && GetEnvironmentVariableA("SX_OUT", out, sizeof(out)) > 0) LoadSxl(out);
        if (frameCounter == 30 && Active()) {
            size_t parts = 0;
            for (auto& l : Active()->doc->S().layers) parts += l.shape.parts.size();
            Log("[demo] shapes reloaded: %zu boolean parts", parts);
        }
        return;
    }
    if (demoMode == 60) {
        // Resources: .abr / .pat / .ase / .aco import (files in SX_DIR), pattern fill and pattern stamp.
        char dir[512] = "";
        GetEnvironmentVariableA("SX_DIR", dir, sizeof(dir));
        std::string D = dir;
        if (frameCounter == 3) {
            ImportAbrFile(D + "/test6.abr");
            Log("[demo] abr v6: %s", statusMsg.c_str());
            ImportAbrFile(D + "/test2.abr");
            Log("[demo] abr v2: %s", statusMsg.c_str());
            ImportPatFile(D + "/test.pat");
            Log("[demo] pat: %s", statusMsg.c_str());
            for (const char* f : {"/test.ase", "/test.aco"}) {
                LoadSwatchLibrary(D + f);
                std::string list;
                for (auto& s : swatches) {
                    char b[96];
                    snprintf(b, sizeof(b), " [%s %.2f %.2f %.2f]", s.name.c_str(), s.srgb.x, s.srgb.y, s.srgb.z);
                    list += b;
                }
                Log("[demo] swatches %s:%s", f, list.c_str());
            }
            swatchesLoaded = true;
            int abr = 0;
            for (auto& b : Brushes().All()) abr += b->id.rfind("abr_", 0) == 0;
            Log("[demo] %d abr brushes in the library, %zu patterns", abr, patterns.size());
            NewDocument(900, 600, Precision::Half16, DocMode::Layers, 0, "Motifs");
        }
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        if (frameCounter == 6) {
            int stripes = 0, test = 0;
            for (int i = 0; i < (int)patterns.size(); ++i) {
                if (patterns[i].name == "Briques") stripes = i;
                if (patterns[i].name == "Dégradé test") test = i;
            }
            Edit e(d, a->history, "Nouveau calque");
            d.AddPixelLayer("Motif");
            e.Commit();
            a->renderer.Prepare(d, *ctx);
            a->renderer.SelectShape(d, *ctx, a->history, SelShape::Rect, {40, 40, 460, 560}, SelMode::Replace, false, 0, "Rectangle");
            FillWithPattern(*a, stripes, 1.0f);
            a->renderer.Deselect(d, a->history);
            // Pattern stamp stroke with the imported pattern (scaled x3).
            patternSel = test;
            patternScale = 3;
            if (a->renderer.BeginStroke(d, *ctx, d.PaintTarget())) {
                RetouchParams rp;
                rp.mode = RetouchMode::History;
                rp.source = PatternTiled(*a, test, 3);
                std::vector<Dab> dabs;
                for (float x = 520; x < 860; x += 6) dabs.push_back({x, 300 + std::sin(x / 40) * 120, 50, 1.0f});
                a->renderer.RetouchDabs(d, *ctx, dabs, retouch, rp);
                a->renderer.EndStroke(d, *ctx, a->history, "Tampon de motif");
            }
        }
        if (frameCounter == 8) {
            // Strokes with two imported .abr tips.
            Id surf = d.PaintTarget();
            int row = 0;
            for (const char* id : {"abr_test6_2", "abr_test2_1"}) {
                auto td = Brushes().Find(id);
                if (!td) { Log("[demo] %s missing", id); continue; }
                BrushSettings b = brush;
                b.size = 60;
                SelectTip(b, id);
                BindTipTextures(b, *td);
                EffectInstance tip;
                tip.params = b.tipParams;
                std::string err;
                for (int w = 0; w < 6000 && !a->renderer.BrushPipeline(*td, &err) && err.empty(); ++w) Sleep(10);
                std::vector<Dab> dabs;
                std::vector<float4> extra;
                float sp = std::max(1.0f, b.size * std::max(b.spacing, 0.02f));
                for (float x = 520; x < 860; x += sp) {
                    dabs.push_back({x, 60.0f + row * 80, 30, 1});
                    extra.push_back({1, 0, 0, x - 520});
                }
                if (a->renderer.BeginStroke(d, *ctx, surf)) {
                    a->renderer.StrokeProgram(d, *ctx, dabs, extra, *td, tip, b, LinearColor({0.1f, 0.2f, 0.6f, 1}), false, 0);
                    a->renderer.EndStroke(d, *ctx, a->history, "Pinceau");
                }
                ++row;
            }
        }
        return;
    }
    if (demoMode == 59) {
        // Text: character / paragraph styles and text on a path; .sxl round trip with SX_OUT.
        if (frameCounter == 4) NewDocument(1600, 900, Precision::Half16, DocMode::Layers, 0, "Texte");
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        if (frameCounter == 6) {
            CharStyle title;
            title.id = d.NewId();
            title.name = "Titre";
            title.size = 110;
            title.color = {0.75f, 0.1f, 0.1f, 1};
            title.underline = true;
            CharStyle accent;
            accent.id = d.NewId();
            accent.name = "Accent";
            accent.color = {0.1f, 0.3f, 0.9f, 1};
            accent.faux = true;
            accent.allCaps = true;
            accent.baseline = 12;
            d.S().charStyles = {title, accent};
            ParaStyle head;
            head.id = d.NewId();
            head.name = "Titre";
            head.charStyle = title.id;
            ParaStyle body;
            body.id = d.NewId();
            body.name = "Corps";
            body.indentLeft = 40;
            body.indentFirst = 60;
            body.spaceBefore = 30;
            body.lineSpacing = 1.4f;
            d.S().paraStyles = {head, body};
            TextStyle t = textDefaults;
            t.content = "Styles de texte\nCorps avec un mot accentué au milieu\nTroisième paragraphe en retrait";
            t.size = 56;
            t.color = {0.1f, 0.1f, 0.1f, 1};
            t.pos = {80, 40};
            t.paraStyles = {head.id, body.id, body.id};
            int start = Utf8Length("Styles de texte\nCorps avec un mot ");
            t.runs = {{start, start + Utf8Length("accentué"), accent.id}};
            d.AddTextLayer(t);
            // Text on an arc (upper half circle, left to right).
            TextStyle p = textDefaults;
            p.content = "Texte qui suit un tracé courbe";
            p.size = 52;
            p.color = {0.05f, 0.45f, 0.2f, 1};
            p.align = 1;
            for (int i = 0; i <= 64; ++i) {
                float ang = 3.14159265f * (1.0f + i / 64.0f);
                p.path.push_back({1150 + std::cos(ang) * 320, 780 + std::sin(ang) * 320});
            }
            p.pathOffset = 3.14159265f * 320 * 0.5f;
            d.AddTextLayer(p);
            d.SyncGraph();
        }
        char out[512] = "";
        if (frameCounter == 12 && GetEnvironmentVariableA("SX_OUT", out, sizeof(out)) > 0) SaveDocument(*a, out);
        if (frameCounter == 24 && GetEnvironmentVariableA("SX_OUT", out, sizeof(out)) > 0) LoadSxl(out);
        if (frameCounter == 32 && Active()) {
            Document& r = *Active()->doc;
            size_t runs = 0, path = 0;
            for (auto& l : r.S().layers) { runs += l.text.runs.size(); path += l.text.path.size(); }
            Log("[demo] text reloaded: %zu char styles, %zu para styles, %zu runs, %zu path points", r.S().charStyles.size(), r.S().paraStyles.size(), runs, path);
        }
        return;
    }
    if (demoMode == 58) {
        // Photo tools: SX_OP = pano | hdr (files SX_FILES, ';' separated) | align | stack (first file opened, others as layers).
        char filesBuf[4096] = "", op[16] = "pano";
        GetEnvironmentVariableA("SX_FILES", filesBuf, sizeof(filesBuf));
        GetEnvironmentVariableA("SX_OP", op, sizeof(op));
        std::vector<std::string> files;
        for (std::string s = filesBuf; !s.empty();) {
            size_t p = s.find(';');
            files.push_back(s.substr(0, p));
            s = p == std::string::npos ? "" : s.substr(p + 1);
        }
        std::string o = op;
        if (frameCounter == 4) {
            photoDlg.files = files;
            photoDlg.layout = 0;
            photoDlg.blend = true;
            photoDlg.blendMode = o == "stack" ? 1 : 0;
            if (o == "pano") StartPhotoJob(1, nullptr);
            else if (o == "hdr") StartPhotoJob(4, nullptr);
            else {
                OpenImage(files[0], false);
                for (size_t i = 1; i < files.size(); ++i) OpenImage(files[i], true);
            }
        }
        if (frameCounter == 8 && (o == "align" || o == "stack") && Active()) StartPhotoJob(o == "align" ? 2 : 3, Active());
        if (frameCounter == 200 && o == "hdr" && Active()) {
            // Camera Raw on the merged HDR: exposure -2, highlights / shadows, clarity, vibrance.
            Document& d = *Active()->doc;
            for (auto& l : d.S().layers)
                for (auto& f : l.filters)
                    if (f.effectId == "camera_raw" && f.params.size() >= 14) {
                        f.params[2].x = -1.5f;  // exposure
                        f.params[4].x = -60;    // highlights
                        f.params[5].x = 50;     // shadows
                        f.params[9].x = 40;     // clarity
                        f.params[11].x = 30;    // vibrance
                    }
            d.SyncGraph();
        }
        if (frameCounter % 30 == 0) Log("[demo] photo %s: %s", op, statusMsg.c_str());
        return;
    }
    if (demoMode == 57) {
        // Color management: CMYK values of primaries, proofing / gamut warning on SX_IMAGE, CMYK TIFF to SX_OUT.
        char path[512] = "", out[512] = "", op[16] = "";
        GetEnvironmentVariableA("SX_OUT", out, sizeof(out));
        GetEnvironmentVariableA("SX_OP", op, sizeof(op));
        if (frameCounter == 3) {
            std::string cmyk = color::DefaultCmykPath();
            Log("[demo] color: sRGB %s, CMYK %s, monitor '%s', %zu profiles", color::SrgbPath().c_str(), cmyk.c_str(), monitorProfile.c_str(),
                color::Profiles().size());
            const u8 rgb[] = {255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 255, 0};
            u8 k[20] = {};
            color::RgbToCmyk("", cmyk, 1, rgb, 5, k);
            for (int i = 0; i < 5; ++i) Log("[demo] rgb %3d %3d %3d -> cmyk %3d %3d %3d %3d", rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2], k[i * 4], k[i * 4 + 1], k[i * 4 + 2], k[i * 4 + 3]);
            float in[6] = {0, 1, 0, 1, 0, 0}, o[6] = {};
            std::string adobe;
            for (auto& p : color::Profiles()) if (p.name.find("Adobe") != std::string::npos) adobe = p.path;
            color::TransformRgb({"", adobe}, 1, in, 2, o);
            Log("[demo] sRGB green -> Adobe RGB %.3f %.3f %.3f ; red -> %.3f %.3f %.3f", o[0], o[1], o[2], o[3], o[4], o[5]);
            {
                float pin[15] = {1, 1, 1, 1, 0, 0, 0, 1, 0, 0.5f, 0.5f, 0.5f, 0, 0, 1}, pout[15] = {};
                std::vector<u8> gam;
                color::TransformRgb({"", cmyk, ""}, 1, pin, 5, pout, &gam);
                for (int i = 0; i < 5; ++i)
                    Log("[demo] proof %.2f %.2f %.2f -> %.3f %.3f %.3f (gamut %d)", pin[i * 3], pin[i * 3 + 1], pin[i * 3 + 2], pout[i * 3], pout[i * 3 + 1], pout[i * 3 + 2], gam.empty() ? -1 : gam[i]);
                u8 k8[20] = {}, back[15] = {};
                u8 rgb8[15];
                for (int i = 0; i < 15; ++i) rgb8[i] = (u8)(pin[i] * 255);
                color::RgbToCmyk("", cmyk, 1, rgb8, 5, k8);
                color::CmykToRgb(cmyk, "", 1, k8, 5, back);
                for (int i = 0; i < 5; ++i) Log("[demo] 8-bit round trip -> %d %d %d", back[i * 3], back[i * 3 + 1], back[i * 3 + 2]);
            }
            color::TransformRgb({"", ""}, 1, in, 2, o);
            Log("[demo] sRGB -> sRGB identity %.3f %.3f %.3f", o[0], o[1], o[2]);
            float4 lab = color::SrgbToLab({1, 1, 1, 1}), lab2 = color::SrgbToLab({1, 0, 0, 1});
            float4 back = color::LabToSrgb(lab2);
            Log("[demo] Lab white %.1f %.1f %.1f, red %.1f %.1f %.1f -> %.3f %.3f %.3f", lab.x, lab.y, lab.z, lab2.x, lab2.y, lab2.z, back.x, back.y, back.z);
        }
        if (frameCounter == 4 && GetEnvironmentVariableA("SX_IMAGE", path, sizeof(path)) > 0) OpenImage(path, false);
        DocView* a = Active();
        if (!a) return;
        if (frameCounter == 6) {
            std::string o = op;
            proof.on = o == "proof" || o == "gamut";
            proof.gamutWarning = o == "gamut";
            if (o == "cmyk") SetColorMode(*a, 1);
        }
        if (frameCounter == 8 && out[0]) ExportCmykTiff(*a, out);
        return;
    }
    if (demoMode == 56) {
        // Artboards + layer comps on SX_IMAGE; exports to SX_OUT (base path); .sxl round trip.
        char path[512] = "", out[512] = "";
        GetEnvironmentVariableA("SX_OUT", out, sizeof(out));
        if (frameCounter == 4 && GetEnvironmentVariableA("SX_IMAGE", path, sizeof(path)) > 0) OpenImage(path, false);
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        if (frameCounter == 6) NewArtboard(*a);
        if (frameCounter == 8) NewArtboard(*a);
        if (frameCounter == 10) {
            // A red layer covering everything, inside the second artboard: clipped to it.
            Edit e(d, a->history, "Nouveau calque");
            Layer& l = d.AddPixelLayer("Rouge");
            Id lid = l.id, surf = l.surface;
            Id board2 = 0;
            for (auto& x : d.S().layers) if (x.artboard && x.name == "Plan de travail 2") board2 = x.id;
            d.MoveLayerTo(lid, board2, true);
            e.Commit();
            a->renderer.Prepare(d, *ctx);
            std::vector<float> px((size_t)d.Width() * d.Height() * 4, 0.0f);
            for (size_t i = 0; i < (size_t)d.Width() * d.Height(); ++i) {
                float2 p{(float)(i % d.Width()), (float)(i / d.Width())};
                float cx = d.Width() - 600.0f, cy = 400;
                if ((p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy) < 900 * 900 && (p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy) > 700 * 700) { px[i * 4] = 0.8f; px[i * 4 + 3] = 1; }
            }
            Surface* s = d.GetSurface(surf);
            if (s->tex->GetFormat() == gpu::Format::RGBA32F) ctx->Upload(s->tex.get(), px.data(), d.Width() * 16);
            else {
                std::vector<u16> h(px.size());
                for (size_t i = 0; i < px.size(); ++i) h[i] = px[i] > 0.5f ? 0x3A66 : 0;  // 0.8
                for (size_t i = 3; i < px.size(); i += 4) h[i] = px[i] > 0.5f ? 0x3C00 : 0;
                ctx->Upload(s->tex.get(), h.data(), d.Width() * 8);
            }
            d.TouchSurface(surf);
        }
        if (frameCounter == 12) {
            // Comps: "Tout" (everything), "Sans rouge" (red layer hidden, background 50 %).
            LayerComp c1;
            c1.id = d.NewId();
            c1.name = "Tout";
            CaptureComp(d, c1);
            d.S().comps.push_back(c1);
            for (auto& l : d.S().layers) {
                if (l.name == "Rouge") l.visible = false;
                if (l.name == "Arrière-plan") l.opacity = 0.5f;
            }
            d.SyncGraph();
            LayerComp c2;
            c2.id = d.NewId();
            c2.name = "Sans rouge";
            CaptureComp(d, c2);
            d.S().comps.push_back(c2);
            ApplyComp(*a, c1.id, true);
            Log("[demo] artboards: canvas %ux%u, layers %zu, comps %zu", d.Width(), d.Height(), d.S().layers.size(), d.S().comps.size());
        }
        if (frameCounter == 14 && out[0]) {
            ExportArtboards(*a, std::string(out) + ".png");
            ExportComps(*a, std::string(out) + "_comp.png");
            SaveDocument(*a, std::string(out) + ".sxl");
        }
        if (frameCounter == 30 && out[0]) LoadSxl(std::string(out) + ".sxl");
        if (frameCounter == 40 && Active()) {
            Document& r = *Active()->doc;
            int boards = 0;
            for (auto& l : r.S().layers) boards += l.artboard;
            Log("[demo] reloaded: %d artboards, %zu comps (%s / %s)", boards, r.S().comps.size(), r.S().comps.empty() ? "-" : r.S().comps[0].name.c_str(),
                r.S().comps.size() > 1 ? r.S().comps[1].name.c_str() : "-");
        }
        return;
    }
    if (demoMode == 55) {
        // Actions: record (rotate + invert filter + image size 50 %) on SX_IMAGE, then batch SX_SRC -> SX_DST.
        char path[512] = "", src[512] = "", dst[512] = "";
        GetEnvironmentVariableA("SX_SRC", src, sizeof(src));
        GetEnvironmentVariableA("SX_DST", dst, sizeof(dst));
        if (frameCounter == 4 && GetEnvironmentVariableA("SX_IMAGE", path, sizeof(path)) > 0) OpenImage(path, false);
        DocView* a = Active();
        if (frameCounter == 6 && a) {
            actions.push_back({"_test", {}});
            actionSel = (int)actions.size() - 1;
            actionRecording = true;
            actionRecordInto = actionSel;
            RotateCanvas(*a, 0);
            ApplyLibraryEffect(*a, "invert", FxApply::Filter);
            Document& d = *a->doc;
            imageDialog = 0;
            Json p = Json::Object();
            p["mode"] = "percent"; p["pct"] = 50; p["interp"] = 2;
            RecordAction("image_size", p);
            StopRecording();
            for (auto& s : actions[actionSel].steps) Log("[demo] recorded: %s %s", s.cmd.c_str(), s.params.Dump(0).c_str());
            (void)d;
        }
        if (frameCounter == 10) {
            batch.chain = {"u:_test"};
            batch.sourceKind = 0;
            batch.source = src;
            batch.dest = dst;
            batch.format = 1;
            batch.pattern = "{nom}_lot";
            batch.open = true;
            StartBatch();
        }
        if (frameCounter > 10 && frameCounter % 20 == 0) Log("[demo] batch: running %d, %zu/%zu done, %zu errors", batch.running ? 1 : 0, batch.done, batch.files.size(), batch.errors.size());
        if (frameCounter > 12 && !batch.running && actionSel < (int)actions.size() && actions[actionSel].name == "_test") {
            actions.erase(actions.begin() + actionSel);
            actionSel = 0;
            SaveActions();
            Log("[demo] batch finished: %s", statusMsg.c_str());
        }
        return;
    }
    if (demoMode == 54) {
        // Puppet warp (SX_OP=puppet, SX_MODE 0..2) or perspective warp (SX_OP=persp) on SX_IMAGE.
        char path[512] = "", op[16] = "puppet", mode[8] = "0";
        GetEnvironmentVariableA("SX_OP", op, sizeof(op));
        GetEnvironmentVariableA("SX_MODE", mode, sizeof(mode));
        bool persp = std::string(op) == "persp";
        if (frameCounter == 4 && GetEnvironmentVariableA("SX_IMAGE", path, sizeof(path)) > 0) OpenImage(path, false);
        DocView* a = Active();
        if (!a) return;
        if (frameCounter == 8) MeshWarpBegin(*a, persp ? 2 : 1);
        if (frameCounter == 14 && xf.active && xf.warp) {
            if (persp) {
                PerspectiveInitPlanes();
                xf.perspPhase = 1;
                xf.warpPts = xf.layoutPts;
                xf.warpPts[0] = {250, 120};
                xf.warpPts[1] = {950, 120};
            } else {
                xf.puppetMode = atoi(mode);
                // Pole base and ground fixed, the head pulled to the right.
                xf.pinSrc = {{580, 620}, {580, 295}, {200, 700}, {1000, 700}};
                xf.pinDst = {{580, 620}, {760, 330}, {200, 700}, {1000, 700}};
            }
            xf.dirty = true;
        }
        if (frameCounter == 20 && GetEnvironmentVariableA("SX_COMMIT", nullptr, 0) > 0) {
            TransformCommit(*a);
            Log("[demo] mesh warp committed: %s", a->history.Count() ? a->history.Label(a->history.Count() - 1).c_str() : "-");
        }
        return;
    }
    if (demoMode == 53) {
        // Retouching on SX_IMAGE: SX_OP=fill (content-aware fill of the pole), patch, move, redeye, history.
        char path[512] = "", op[16] = "fill";
        GetEnvironmentVariableA("SX_OP", op, sizeof(op));
        std::string o = op;
        if (frameCounter == 4 && GetEnvironmentVariableA("SX_IMAGE", path, sizeof(path)) > 0) OpenImage(path, false);
        DocView* a = Active();
        if (!a) return;
        Document& d = *a->doc;
        Id t = d.PaintTarget();
        if (frameCounter == 8) {
            if (o == "redeye") {
                if (a->renderer.BeginStroke(d, *ctx, t)) {
                    RetouchParams rp;
                    rp.mode = RetouchMode::RedEye;
                    rp.strength = redEyeDarken;
                    BrushSettings b = retouch;
                    b.hardness = 0.3f;
                    b.opacity = 1;
                    a->renderer.RetouchDabs(d, *ctx, {{220, 600, 40, 1.0f}}, b, rp);
                    a->renderer.EndStroke(d, *ctx, a->history, "Yeux rouges");
                }
            } else if (o == "history") {
                TakeHistorySnapshot(*a, t);
                FillOrErase(*a, false);  // whole layer filled (no selection)
            } else {
                a->renderer.SelectShape(d, *ctx, a->history, SelShape::Rect, {505, 235, 655, 635}, SelMode::Replace, true, 2, "Rectangle");
            }
        }
        if (frameCounter == 10 && o == "fill") StartContentAwareFill(*a);
        if (frameCounter == 10 && o == "magic") {
            // Magic eraser click in the sky (same request as ClickToolInput).
            d.S().selection = 0;
            tool = Tool::MagicEraser;
            wand.rb = a->renderer.ExportRGBA8(*ctx, d.GetSurface(t)->tex.get());
            wand.x = 600;
            wand.y = 100;
            wand.view = a;
            wand.fill = wand.erase = true;
            wand.surface = t;
        }
        if (frameCounter == 10 && o == "patch")
            a->renderer.HealRegion(d, *ctx, a->history, t, nullptr, d.Selection()->tex.get(), {250, 0}, std::max(6, (int)(patchBlend * 40)), "Pièce");
        if (frameCounter == 10 && o == "move") {
            // Same path as the tool: fill the original place, then heal the content at +300 px.
            tool = Tool::ContentMove;
            patchDrag.dragging = true;
            patchDrag.start = {0, 0};
            selMoving = true;
        }
        if (frameCounter == 10 && o == "history") {
            tool = Tool::HistoryBrush;
            if (a->renderer.BeginStroke(d, *ctx, t)) {
                RetouchParams rp;
                rp.mode = RetouchMode::History;
                rp.source = a->snapshots[t].get();
                std::vector<Dab> dabs;
                for (float x = 100; x < 1100; x += 10) dabs.push_back({x, 400, 60, 1.0f});
                a->renderer.RetouchDabs(d, *ctx, dabs, retouch, rp);
                a->renderer.EndStroke(d, *ctx, a->history, "Forme d'historique");
            }
        }
        if (frameCounter == 12 && o == "move") {
            // Released at +300 px: replicates PatchToolInput's release branch.
            patchDrag.dragging = false;
            selMoving = false;
            Surface* s = d.GetSurface(t);
            auto original = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "MoveSource");
            ctx->CopyTexture(original.get(), s->tex.get());
            int2 off = {300, 0};
            int radius = 16;
            StartContentAwareFill(*a, [this, a, t, original, off, radius] {
                Document& dd = *a->doc;
                a->history.BeginGroup();
                gpu::Texture* moved = a->renderer.OffsetSelectionPreview(dd, *ctx, off);
                a->renderer.HealRegion(dd, *ctx, a->history, t, original.get(), moved, {-off.x, -off.y}, radius, "Déplacement basé sur le contenu");
                a->renderer.OffsetSelection(dd, *ctx, a->history, off, "Déplacer la sélection");
                a->history.EndGroup();
            });
        }
        if (frameCounter % 30 == 0) Log("[demo] retouch %s: %s (job %d)", op, statusMsg.c_str(), fillJob.active ? 1 : 0);
        return;
    }
    if (demoMode == 52) {
        // Select > Sky on an image given by SX_IMAGE (or Subject with SX_SUBJECT: reports a missing rembg).
        char path[512] = "";
        if (frameCounter == 4 && GetEnvironmentVariableA("SX_IMAGE", path, sizeof(path)) > 0) OpenImage(path, false);
        if (frameCounter == 10 && Active()) StartSelectSubject(*Active(), GetEnvironmentVariableA("SX_SUBJECT", nullptr, 0) == 0);
        if (frameCounter == 60) Log("[demo] select sky/subject: %s, selection %s", statusMsg.c_str(), Active() && Active()->doc->Selection() ? "yes" : "no");
        return;
    }
    if (demoMode == 51 && v) {
        // Magnetic lasso: edge-following path from anchors around the top stroke, closed into a selection.
        if (frameCounter == 4 && v->lastImage) {
            tool = Tool::MagneticLasso;
            magLasso.rb = v->renderer.ExportRGBA8(*ctx, v->lastImage, true);
        }
        if (frameCounter == 10) {
            MagneticBuildCost();
            auto cost = [&](const std::vector<float2>& path) {
                float c = 0;
                for (auto& p : path) c += magLasso.cost[(size_t)std::clamp((int)p.y, 0, magLasso.h - 1) * magLasso.w + std::clamp((int)p.x, 0, magLasso.w - 1)];
                return path.empty() ? 0.0f : c / path.size();
            };
            // Anchors on the upper edge of the cyan wave (y = 300 + sin(...) * 120 - half width).
            std::vector<float2> pts;
            float2 a{150, 0};
            auto edgeY = [](float x) { float t = (x - 120) / 1360.0f; return 300 + std::sin(t * 9.0f) * 120 - 28; };
            a.y = edgeY(a.x);
            float straight = 0, magnetic = 0;
            selDragMode = SelMode::Replace;
            lassoPts = {a};
            polyActive = true;
            for (float x = 350; x <= 1450; x += 200) {
                float2 b{x, edgeY(x) + 25};  // anchors placed 25 px off the edge on purpose
                auto path = MagneticPath(lassoPts.back(), b);
                magnetic += cost(path);
                straight += cost({lassoPts.back(), (lassoPts.back() + b) * 0.5f, b});
                for (size_t i = 1; i < path.size(); ++i) lassoPts.push_back(path[i]);
            }
            Log("[demo] magnetic lasso: mean edge cost %.3f along the path, %.3f on straight segments", magnetic / 6, straight / 6);
            lassoPts.push_back({1450, 60});
            lassoPts.push_back({150, 60});
            magLasso.live.clear();
            FinishPolygon(*v);
        }
        return;
    }
    if (demoMode == 50 && v) {
        // Selection tools: Color Range (SX_RANGE: preset index, eyedropper sample on the cyan stroke)
        // or Select and Mask (rough ellipse around the top stroke, refined by the image).
        Document& d = *v->doc;
        char b[16] = "";
        bool range = GetEnvironmentVariableA("SX_RANGE", b, sizeof(b)) > 0;
        if (frameCounter == 5) {
            if (range) {
                colorRange.open = true;
                colorRange.preset = atoi(b);
                colorRange.samples.clear();
                if (colorRange.preset == 0) colorRange.pick = v->renderer.ReadPixel(*ctx, v->lastImage, 800, 210);
                colorRange.pickOp = 0;
                colorRange.fuzz = 45;
            } else {
                v->renderer.SelectShape(d, *ctx, v->history, SelShape::Ellipse, {150, 120, 1450, 470}, SelMode::Replace, true, 0, "Ellipse");
            }
        }
        if (frameCounter == 8 && !range) {
            selectMask.open = true;
            selectMask.params.radius = 40;
            selectMask.view = atoi(GetEnvironmentVariableA("SX_VIEW", b, sizeof(b)) > 0 ? b : "0");
        }
        if (frameCounter == 20 && GetEnvironmentVariableA("SX_APPLY", nullptr, 0) > 0) {
            // Apply: refined selection, then Color Range OK.
            if (selectMask.open && selectMask.mask) {
                v->renderer.SelectFrom(d, *ctx, v->history, selectMask.mask, SelSource::Mask, SelMode::Replace, {}, 0, "Sélectionner et masquer");
                selectMask.open = false;
            }
            if (colorRange.open && colorRange.mask) {
                v->renderer.SelectFrom(d, *ctx, v->history, colorRange.mask, SelSource::Mask, SelMode::Replace, {}, 0, "Plage de couleurs");
                colorRange.open = false;
            }
        }
        return;
    }
    if (demoMode == 49) {
        // Smart objects: convert, shrink to 20 % then back to 100 % (must be lossless), linked image,
        // contents edited in their tab (the layer follows), save + reload.
        static DocView* parent = nullptr;
        static Id layerId = 0;
        static gpu::ReadbackPtr before, after;
        static std::string savePath;
        if (frameCounter == 4 && v) {
            parent = v;
            Document& d = *v->doc;
            for (auto& l : d.S().layers) if (l.name == "Peinture") { d.S().activeLayer = l.id; layerId = l.id; }
            v->renderer.Prepare(d, *ctx);
            before = v->renderer.ExportRGBA8(*ctx, d.GetSurface(d.ActiveLayer()->surface)->tex.get(), false);
            ConvertToSmartObject(*v);
            // Shrink (Ctrl+T, 20 %, rotated), commit.
            TransformBegin(*v, false);
            xf.w *= 0.2f;
            xf.h *= 0.2f;
            xf.angle = 0.5f;
            TransformUpdateCorners();
            xf.dirty = true;
            TransformCommit(*v);
        }
        if (frameCounter == 6 && parent) {
            // Back to 100 %, no rotation.
            TransformBegin(*parent, false);
            xf.w *= 5.0f;
            xf.h *= 5.0f;
            xf.angle = 0.0f;
            TransformUpdateCorners();
            xf.dirty = true;
            TransformCommit(*parent);
            if (Layer* l = parent->doc->FindLayer(layerId))
                Log("[demo] smart corners after restore: %.3f,%.3f %.3f,%.3f %.3f,%.3f %.3f,%.3f", l->smartCorners[0].x, l->smartCorners[0].y, l->smartCorners[1].x,
                    l->smartCorners[1].y, l->smartCorners[2].x, l->smartCorners[2].y, l->smartCorners[3].x, l->smartCorners[3].y);
        }
        if (frameCounter == 9 && parent) {
            Document& d = *parent->doc;
            after = parent->renderer.ExportRGBA8(*ctx, d.GetSurface(d.FindLayer(layerId)->surface)->tex.get(), false);
            // A linked image, then the contents of the first object in their own tab.
            PlaceSmartFile(*parent, ExeDir() + "/../../assets/logo.png", true);
            if (Layer* l = d.FindLayer(layerId)) EditSmartContents(*parent, *l);
        }
        if (frameCounter == 12) {
            DocView* child = Active();
            if (child && child->smartLink.parent) {
                // Paint in the inner document: a big yellow stroke.
                Document& cd = *child->doc;
                child->renderer.Prepare(cd, *ctx);
                Id target = 0;
                for (auto& l : cd.S().layers) if (l.kind == LayerKind::Pixel) target = l.surface;
                if (target && child->renderer.BeginStroke(cd, *ctx, target)) {
                    std::vector<Dab> dabs;
                    for (float x = 200; x < 1400; x += 6) dabs.push_back({x, 820 + std::sin(x * 0.01f) * 60, 45, 1});
                    BrushSettings b = brush;
                    b.tipId.clear();
                    b.tipCode.clear();
                    b.tipGraph.clear();
                    child->renderer.StrokeDabs(cd, *ctx, dabs, b, LinearColor({1, 0.9f, 0.1f, 1}), false);
                    child->renderer.EndStroke(cd, *ctx, child->history, "Pinceau");
                }
            } else {
                Log("[demo] smart: no child tab");
            }
        }
        if (frameCounter == 16 && before && after && before->ready && after->ready) {
            // Premultiplied difference (colors of fully transparent pixels do not matter).
            float maxDiff = 0;
            for (size_t i = 0; i + 3 < before->data.size() && i + 3 < after->data.size(); i += 4)
                for (int c = 0; c < 4; ++c) {
                    float ka = c == 3 ? 1.0f : before->data[i + 3] / 255.0f, kb = c == 3 ? 1.0f : after->data[i + 3] / 255.0f;
                    maxDiff = std::max(maxDiff, std::abs(before->data[i + c] * ka - after->data[i + c] * kb));
                }
            Log("[demo] smart: shrink 20 %% + back to 100 %%: max difference %.2f / 255 (premultiplied)", maxDiff);
        }
        if (frameCounter == 18) {
            // Back to the parent (closing the inner tab), save, reload.
            for (int i = 0; i < (int)docs.size(); ++i)
                if (docs[i]->smartLink.parent) { CloseDocument(i, true); break; }
            for (int i = 0; i < (int)docs.size(); ++i) if (docs[i].get() == parent) activeDoc = focusDocRequest = i;
            savePath = ExeDir() + "/smart_test.sxl";
            SaveDocument(*parent, savePath);
        }
        if (frameCounter >= 30 && !savePath.empty() && saves.empty()) {
            LoadSxl(savePath);
            DocView* r = Active();
            int smartLayers = 0, withChild = 0;
            for (auto& l : r->doc->S().layers) smartLayers += l.kind == LayerKind::Smart;
            for (auto& [id, so] : r->doc->smartObjects) withChild += so->child != nullptr;
            Log("[demo] smart: reloaded %d smart layers, %zu objects, %d with inner document", smartLayers, r->doc->smartObjects.size(), withChild);
            savePath.clear();
        }
        return;
    }
    if (demoMode == 48 && v) {
        // Tablet + brush textures: simulated pen (pressure, tilt turning along the stroke, rotation)
        // with the imported TextureBrush, a graph brush reacting to tilt with a texture node, and the test pad.
        Document& d = *v->doc;
        static EffectGraph g;
        auto penStroke = [&](BrushSettings b, float y0, float4 color, Id surf) {
            auto td = TipDef(b);
            if (!td) { Log("[demo] tablet: no tip"); return; }
            std::string err;
            for (int w = 0; w < 6000 && !v->renderer.BrushPipeline(*td, &err) && err.empty(); ++w) Sleep(10);
            if (!err.empty()) { Log("[demo] tablet brush: %s", err.c_str()); return; }
            BindTipTextures(b, *td);
            EffectInstance tip;
            tip.params = b.tipParams;
            std::vector<Dab> dabs;
            std::vector<float4> extra;
            float sp = std::max(1.0f, b.size * std::max(td->spacing > 0 ? td->spacing : b.spacing, 0.02f)), dist = 0;
            float2 prev{120, y0};
            for (float x = 120; x < 1480; x += sp) {
                float t = (x - 120) / 1360.0f, y = y0 + std::sin(t * 6.28f) * 60;
                float pr = 0.25f + 0.75f * std::sin(t * 3.14159f);
                float tiltDeg = 50.0f * t;                       // the pen leans more and more
                float az = t * 6.2831853f;                       // ... and turns around
                float2 dir{x - prev.x, y - prev.y};
                dist += Length(dir);
                dabs.push_back({x, y, b.size * 0.5f * (0.3f + 0.7f * pr), pr});
                extra.push_back({pr, std::atan2(dir.y, dir.x), t, dist});
                extra.push_back({std::cos(az) * tiltDeg, std::sin(az) * tiltDeg, t * 3.0f, 800.0f});
                prev = {x, y};
            }
            if (v->renderer.BeginStroke(d, *ctx, surf)) {
                v->renderer.StrokeProgram(d, *ctx, dabs, extra, *td, tip, b, LinearColor(color), false, 0);
                v->renderer.EndStroke(d, *ctx, v->history, "Pinceau");
            }
        };
        if (frameCounter == 4) {
            Edit e(d, v->history, "Nouveau calque");
            Id surf = d.AddPixelLayer("Tablette").surface;
            e.Commit();
            v->renderer.Prepare(d, *ctx);
            // 1. Imported TextureBrush (splatter image, then bristles).
            BrushSettings b = brush;
            b.size = 70;
            SelectTip(b, "cy_texture");
            penStroke(b, 180, {1, 0.85f, 0.3f, 1}, surf);
            b.tipTextures = {Brushes().Dir() + "/textures/bristles.png"};
            penStroke(b, 360, {0.3f, 0.9f, 1, 1}, surf);
            // 2. Graph brush: round tip x brush texture, turned by the tilt and wider when the pen leans.
            g = EffectGraph::DefaultBrush();
            g.name = "Feutre incliné";
            g.tiltAngle = 1;
            g.tiltSize = 1.2f;
            g.spacing = 0.08f;
            Id br = 0, rt = 0, outId = 0;
            for (auto& n : g.nodes) {
                if (n.op == "brush") br = n.id;
                if (n.op == "roundtip") rt = n.id;
                if (n.op == "output") outId = n.id;
            }
            EGNode tx;
            tx.id = g.nextId++;
            tx.op = "brushtex";
            tx.pos = {340, 220};
            tx.text = "textures/grain.png";
            tx.name = "Grain";
            tx.inputs = {{br, 1}};
            g.nodes.push_back(tx);
            EGNode mul;
            mul.id = g.nextId++;
            mul.op = "mul";
            mul.pos = {620, 160};
            mul.inputs = {{rt, 0}, {tx.id, 5}};
            g.nodes.push_back(mul);
            // Flatten the tip: local position scaled on y (a felt nib).
            for (auto& n : g.nodes) {
                if (n.id == outId) n.inputs[2] = {mul.id, 0};
                if (n.id == rt) {
                    EGNode sc;
                    sc.id = g.nextId++;
                    sc.op = "mul";
                    sc.pos = {200, 60};
                    EGNode k;
                    k.id = g.nextId++;
                    k.op = "const2";
                    k.value = {1, 3, 0, 0};
                    k.pos = {40, 300};
                    sc.inputs = {{br, 0}, {k.id, 0}};
                    n.inputs[0] = {sc.id, 0};
                    g.nodes.push_back(k);
                    g.nodes.push_back(sc);
                    break;
                }
            }
            brush.tipId.clear();
            brush.tipCode.clear();
            brush.tipGraph = g.Serialize();
            if (auto td = TipDef(brush); td && td->def) brush.tipParams = td->def->DefaultParams();
            BrushSettings gb = brush;
            gb.size = 46;
            penStroke(gb, 560, {0.95f, 0.35f, 0.5f, 1}, surf);
            // 3. Code brush using the pen in Dynamics(): tilt -> angle, rotation, speed.
            BrushSettings cb = brush;
            cb.tipGraph.clear();
            cb.tipCode = R"(//@name Calame
//@category Essais
//@spacing 0.04
float4 Tip(Brush b)
{
    float2 p = b.local * float2(1.0, 6.0);
    return float4(b.color, 1.0 - smoothstep(0.8, 1.0, length(p)));
}
void Dynamics(inout Dab d)
{
    d.angle = TiltAngle(d) + d.twist;
    d.radius *= lerp(0.6, 1.4, TiltAmount(d));
}
)";
            cb.size = 50;
            if (auto td = TipDef(cb); td && td->def) cb.tipParams = td->def->DefaultParams();
            penStroke(cb, 760, {0.2f, 0.2f, 0.25f, 1}, surf);
            tool = Tool::Brush;
            OpenGraphEditor(2);
        }
        if (frameCounter == 10 && v->renderer.PadTexture()) {
            // Pad: one pen stroke drawn with the graph brush.
            auto td = TipDef(brush);
            if (td) {
                BrushSettings b = brush;
                b.size = 30;
                BindTipTextures(b, *td);
                EffectInstance tip;
                tip.params = b.tipParams;
                std::vector<Dab> dabs;
                std::vector<float4> extra;
                float W = (float)v->renderer.PadTexture()->Width(), H = (float)v->renderer.PadTexture()->Height();
                v->renderer.PadBeginStroke(*ctx);
                for (float x = 20; x < W - 20; x += 2.5f) {
                    float t = (x - 20) / (W - 40);
                    dabs.push_back({x, H * 0.5f + std::sin(t * 9.0f) * H * 0.25f, b.size * 0.5f, 1.0f});
                    extra.push_back({1.0f, 0.0f, t, x});
                    extra.push_back({std::cos(t * 6.28f) * 45.0f, std::sin(t * 6.28f) * 45.0f, 0.0f, 500.0f});
                }
                v->renderer.PadStroke(*ctx, *td, tip, b, LinearColor({0.1f, 0.3f, 0.8f, 1}), dabs, extra, false, 0);
            }
        }
        return;
    }
    if (demoMode == 47 && v && frameCounter == 4) {
        // HLSL import: the popup with an annotated function, or (SX_CREATE) the created effect.
        const char* src =
        R"SX(//@name Vignette douce
//@category Mes effets
//@in Amount float 0.6 0 1 "Intensité" "Réglages"
//@in Radius float 0.75 0.1 1.5 "Rayon" "Réglages"
//@in Tint color 0.05 0 0.1 1 "Couleur du bord" "Couleur"
//@out Result float4

float4 Vignette(Texture2D Image, SamplerState S, float2 UV, float Amount, float Radius, float4 Tint)
{
    float4 c = Image.Sample(S, UV);
    float d = length(UV - 0.5) * 1.41421;
    float k = smoothstep(Radius, Radius - 0.5, d);
    return float4(lerp(Tint.rgb, c.rgb, lerp(1.0, k, Amount)), c.a);
}
)SX";
        hlslImportBuffer = src;
        OpenGraphEditor(0);
        if (GetEnvironmentVariableA("SX_CREATE", nullptr, 0) > 0) {
            std::string err;
            if (!CreateEffectFromHlsl(*v, src, &err)) Log("[demo] hlsl import: %s", err.c_str());
            ShowSideTab(0);
            FocusPanel("Propriétés");
        } else {
            for (auto& l : v->doc->S().layers) if (l.name == "Peinture") v->doc->S().activeLayer = l.id;
        }
        return;
    }
    if (demoMode == 47 && frameCounter == 8 && GetEnvironmentVariableA("SX_CREATE", nullptr, 0) == 0) hlslImportRequest = true;  // editor shown by now
    if (demoMode == 46 && v && frameCounter == 4) {
        // Custom HLSL node with typed pins: texture + UV wired, the other inputs unconnected
        // become parameters (range, category) in Propriétés.
        Document& d = *v->doc;
        EffectGraph g;
        g.name = "Postérisation teintée";
        g.category = "Mes effets";
        g.inputs = 1;
        auto add = [&](const char* op, float2 pos) -> EGNode& {
            EGNode n;
            n.id = g.nextId++;
            n.op = op;
            n.pos = pos;
            n.inputs.resize(FindOp(op)->inputs.size());
            g.nodes.push_back(n);
            return g.nodes.back();
        };
        Id tex = add("texture", {40, 40}).id, uv = add("uv", {40, 170}).id;
        EGNode& c = add("custom", {340, 40});
        c.name = "Postérise";
        auto pin = [](const char* name, int type, float4 def, float mn, float mx, const char* cat, const char* label) {
            EGPinDecl d;
            d.name = name;
            d.type = type;
            d.def = def;
            d.min = mn;
            d.max = mx;
            d.category = cat;
            d.label = label;
            return d;
        };
        c.pins = {pin("Image", kPinTexture, {}, 0, 1, "", ""), pin("UV", kPinFloat2, {}, 0, 1, "", ""),
                  pin("Steps", kPinInt, {4, 0, 0, 0}, 2, 16, "Réglages", "Niveaux"), pin("Amount", kPinFloat, {1, 0, 0, 0}, 0, 1, "Réglages", "Intensité"),
                  pin("Invert", kPinBool, {0, 0, 0, 0}, 0, 1, "Options", "Inverser"), pin("Tint", kPinColor, {1, 0.75f, 0.4f, 1}, 0, 1, "Couleur", "Teinte")};
        EGPinDecl o;
        o.name = "Resultat";
        o.type = kPinFloat4;
        c.outs = {o};
        c.text = "        float4 s = Texture2DSample(Image, ImageSampler, UV);\n"
                 "        float3 q = floor(s.rgb * Steps + 0.5) / Steps;\n"
                 "        if (Invert) q = 1.0 - q;\n"
                 "        return float4(lerp(s.rgb, q * Tint.rgb, Amount), s.a);";
        c.inputs.resize(c.pins.size());
        c.inputs[0] = {tex, 0};
        c.inputs[1] = {uv, 0};
        Id cid = c.id;
        EGNode& out = add("output", {700, 40});
        out.inputs = {{cid, 0}, {}, {}};
        EGCompileResult r = CompileEffectGraph(g);
        if (!r.error.empty()) Log("[demo] custom pins: %s", r.error.c_str());
        Edit e(d, v->history, "Effet visuel");
        Layer& l = d.AddEffectLayer(LayerKind::Adjustment, "");
        l.name = "Postérisation teintée";
        l.effect.graph = g.Serialize();
        if (auto def = l.effect.Def()) {
            l.effect.params = def->DefaultParams();
            std::string list;
            for (auto& p : def->params) list += p.id + "[" + p.group + "] ";
            Log("[demo] custom pins params: %s", list.c_str());
        }
        d.SyncGraph();
        e.Commit();
        OpenGraphEditor(0);
        efEd.selected = cid;
        ShowSideTab(0);
        FocusPanel("Propriétés");
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
        // SX_BRUSHCAT: every brush of a category instead (one row each).
        std::vector<std::string> list(std::begin(ids), std::end(ids));
        char cat[64] = "";
        float rowH = 100.0f;
        if (GetEnvironmentVariableA("SX_BRUSHCAT", cat, sizeof(cat)) > 0) {
            list.clear();
            for (auto& bd : Brushes().All()) if (bd->def && bd->def->category == cat) list.push_back(bd->id);
            rowH = std::min(100.0f, (float)(d.Height() - 120) / std::max<size_t>(list.size(), 1));
            Log("[demo] brushes of %s: %zu", cat, list.size());
        }
        for (const std::string& idS : list) {
            const char* id = idS.c_str();
            auto td = Brushes().Find(id);
            if (!td) { Log("[demo] brush %s missing", id); continue; }
            BrushSettings b = brush;
            b.size = std::string(id) == "star_scatter" ? 40.0f : std::min(56.0f, rowH * 0.8f);
            b.hardness = 0.7f;
            SelectTip(b, id);
            if (std::string(id) == "calligraphy") b.tipParams[2].x = 1.0f;  // follow the stroke
            EffectInstance tip;
            tip.params = b.tipParams;
            std::string err;
            for (int w = 0; w < 6000 && !v->renderer.BrushPipeline(*td, &err) && err.empty(); ++w) Sleep(10);
            if (!err.empty()) Log("[demo] brush %s: %s", id, err.c_str());
            float y0 = 90.0f + row * rowH;
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
                    if (!v->renderer.StrokeProgram(d, *ctx, part, ex, *td, tip, b, LinearColor(colors[row % 9]), false, (u32)i))
                        Log("[demo] brush %s not ready", id);
                Log("[demo] brush %s drawn", id);
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
        static bool demoGraphOpen = false;
        auto grab = [&] { return v->renderer.ExportRGBA8(*ctx, v->renderer.EvaluateComplete(d, *ctx)); };
        static Id demoRampNode = 0;
        if (frameCounter == 4) {
            for (auto& l : d.S().layers) if (l.name == "Peinture") d.S().activeLayer = l.id;
            fxBrowser.selected = "invert";
            // SX_FXGRAPH=1: the "graph effects" list; 2: then the graph of one opened in the effect editor.
            if (const char* g = getenv("SX_FXGRAPH")) {
                fxBrowser.graphOnly = true;
                ShowSideTab(2);
                FocusPanel("Effets");
                if (g[0] == '2') demoGraphOpen = true;
            }
        }
        if (frameCounter == 40 && demoGraphOpen) {
            // The HLSL node selected (code editor), SX_FXCODE=big: its large editor window too.
            for (auto& n : efEd.graph.nodes)
                if (n.op == "custom") {
                    efEd.selected = n.id;
                    if (const char* c = getenv("SX_FXCODE"); c && std::string(c) == "big") efEd.bigCode = n.id;
                }
            if (const char* a = getenv("SX_FXADD")) efEd.demoAdd = atoi(a);
            if (getenv("SX_FXPVNODE")) efPv.node = true;
        }
        if (demoGraphOpen && getenv("SX_FXFUNC")) {
            // Functions: 3 nodes collapsed into a function (same image), opened (SX_FXFUNC=in: stays inside), back.
            static gpu::ReadbackPtr before, after;
            static Id fn = 0;
            if (frameCounter == 60) before = grab();
            if (frameCounter == 70) {
                efEd.multi.clear();
                for (auto& n : efEd.graph.nodes)
                    if (efEd.multi.size() < 3 && n.op != "output" && n.op != "input") efEd.multi.insert(n.id);
                efEd.selected = *efEd.multi.begin();
                efEd.demoCollapse = true;
            }
            if (frameCounter == 74) {
                for (auto& n : efEd.graph.nodes) if (n.op == "function") fn = n.id;
                const EGNode* f = efEd.graph.Find(fn);
                Log("[fxfunc] collapsed: %zu nodes, function %u with %zu inputs / %zu outputs", efEd.graph.nodes.size(), fn, f ? f->pins.size() : 0,
                    f ? f->outs.size() : 0);
            }
            if (frameCounter == 90) after = grab();
            if (frameCounter == 96 && before && after && before->ready && after->ready) CompareReadbacks("graph vs graph with a function", *before, *after, 1);
            if (frameCounter == 100) efEd.enterFunc = fn;
            if (frameCounter == 106) Log("[fxfunc] inside: depth %zu, %zu nodes", efEd.funcPath.size(), efEd.graph.nodes.size());
            if (frameCounter == 130 && std::string(getenv("SX_FXFUNC")) != "in") efEd.exitFunc = true;
            if (frameCounter == 136) Log("[fxfunc] back: depth %zu, %zu nodes", efEd.funcPath.size(), efEd.graph.nodes.size());
        }
        if (demoGraphOpen && getenv("SX_FXKEYS")) {
            // Synthetic keys in the effect editor: select 3 nodes, Ctrl+D, Delete, Ctrl+A, F.
            ImGuiIO& kio = ImGui::GetIO();
            auto key = [&](ImGuiKey k, bool down) { kio.AddKeyEvent(k, down); };
            auto count = [&] { return (int)efEd.graph.nodes.size(); };
            if (frameCounter == 70) {
                efEd.multi.clear();
                for (auto& n : efEd.graph.nodes)
                    if (efEd.multi.size() < 3 && n.op != "output") efEd.multi.insert(n.id);
                efEd.selected = *efEd.multi.begin();
                Log("[fxkeys] nodes %d, selected %zu", count(), efEd.multi.size());
            }
            if (frameCounter == 72) { key(ImGuiMod_Ctrl, true); key(ImGuiKey_LeftCtrl, true); key(ImGuiKey_D, true); }
            if (frameCounter == 73) { key(ImGuiKey_D, false); key(ImGuiKey_LeftCtrl, false); key(ImGuiMod_Ctrl, false); }
            if (frameCounter == 76) Log("[fxkeys] after Ctrl+D: nodes %d, selected %zu", count(), efEd.multi.size());
            if (frameCounter == 78) key(ImGuiKey_Delete, true);
            if (frameCounter == 79) key(ImGuiKey_Delete, false);
            if (frameCounter == 82) Log("[fxkeys] after Delete: nodes %d, selected %zu", count(), efEd.multi.size());
            if (frameCounter == 84) { key(ImGuiMod_Ctrl, true); key(ImGuiKey_LeftCtrl, true); key(ImGuiKey_A, true); }
            if (frameCounter == 85) { key(ImGuiKey_A, false); key(ImGuiKey_LeftCtrl, false); key(ImGuiMod_Ctrl, false); }
            if (frameCounter == 88) Log("[fxkeys] after Ctrl+A: selected %zu of %d", efEd.multi.size(), count());
        }
        if (frameCounter == 50 && demoGraphOpen && getenv("SX_FXBREAK")) {
            // A typing error in the HLSL node of the first filter: the editor marks the line.
            if (Layer* l = d.ActiveLayer(); l && !l->filters.empty()) {
                EffectInstance& f = l->filters[0];
                if (std::string(getenv("SX_FXBREAK")) == "code") {
                    ConvertToCustomCode(*v, f);
                    size_t at = f.customCode.find("sin(");
                    if (at != std::string::npos) f.customCode.replace(at, 4, "sinn(");
                    d.SyncGraph();
                    Log("[fxbreak] code effect broken");
                }
                EffectGraph tg;
                auto fdef = f.Def();
                if (std::string(getenv("SX_FXBREAK")) == "organize" && EffectGraph::Deserialize(f.graph.empty() && fdef ? fdef->graph : f.graph, tg)) {
                    // A reroute on the link into the output, a comment around the first nodes, a value on an input.
                    if (EGNode* o = tg.Find(tg.OutputNode()); o && !o->inputs.empty() && o->inputs[0].node) {
                        EGNode r;
                        r.id = tg.nextId++;
                        r.op = "reroute";
                        r.pos = o->pos - float2{80, -40};
                        r.inputs = {o->inputs[0]};
                        o->inputs[0] = {r.id, 0};
                        tg.nodes.push_back(r);
                    }
                    EGComment c;
                    c.id = tg.nextId++;
                    c.text = "Vague";
                    float2 mn{1e9f, 1e9f}, mx{-1e9f, -1e9f};
                    int k = 0;
                    for (auto& n : tg.nodes)
                        if ((n.op == "mul" || n.op == "sin" || n.op == "cos") && k++ < 3) {
                            mn = {std::min(mn.x, n.pos.x), std::min(mn.y, n.pos.y)};
                            mx = {std::max(mx.x, n.pos.x + 168), std::max(mx.y, n.pos.y + 230)};
                        }
                    if (mn.x < mx.x) {
                        c.pos = mn - float2{30, 54};
                        c.size = (mx - mn) + float2{60, 84};
                        tg.comments.push_back(c);
                    }
                    for (auto& n : tg.nodes)
                        if (n.op == "max") {
                            n.inputs.resize(2);
                            n.inputs[1] = {};
                            n.pinValues = {std::numeric_limits<float>::quiet_NaN(), 2.5f};
                            break;
                        }
                    EGCompileResult cr2 = CompileEffectGraph(tg);
                    Log("[fxorg] compile: %s, comments %zu", cr2.error.empty() ? "ok" : cr2.error.c_str(), tg.comments.size());
                    f.graph = tg.Serialize();
                    f.effectId.clear();
                    f.SyncParams(fdef);
                    d.SyncGraph();
                }
                else if (f.customCode.empty() && EffectGraph::Deserialize(f.graph.empty() && fdef ? fdef->graph : f.graph, tg)) {
                    for (auto& n : tg.nodes)
                        if (n.op == "custom") {
                            size_t at = n.text.find("sin(");
                            if (at != std::string::npos) n.text.replace(at, 4, "sinn(");
                        }
                    f.graph = tg.Serialize();
                    f.effectId.clear();
                    f.SyncParams(fdef);
                    d.SyncGraph();
                    Log("[fxbreak] code broken");
                }
            }
        }
        if (frameCounter == 150 && demoGraphOpen && getenv("SX_FXBREAK")) {
            for (auto& n : d.S().graph.nodes)
                if (n.kind == NodeKind::Effect) {
                    std::string e;
                    bool f = v->renderer.NodeFailed(n.id, &e);
                    Log("[fxbreak] node %u code %zu graph %zu failed %d", n.id, n.fx.customCode.size(), n.fx.graph.size(), (int)f);
                }
        }
        if (frameCounter == 60 && demoGraphOpen) {
            if (const char* z = getenv("SX_FXZOOM")) {
                // Zoomed view on the first computing nodes.
                efEd.zoom = (float)atof(z);
                float2 mn{1e9f, 1e9f};
                for (auto& n : efEd.graph.nodes)
                    if (n.op != "param" && n.op != "paramcolor" && n.op != "param2" && n.op != "const" && n.op != "color") {
                        mn.x = std::min(mn.x, n.pos.x);
                        mn.y = std::min(mn.y, n.pos.y);
                    }
                efEd.pan = {40 - mn.x, 60 - mn.y};
            }
            if (const char* lib = getenv("SX_FXLIB")) {
                efEd.browse = true;
                efEd.browseKind = std::string(lib).rfind("cy_blend_", 0) == 0 ? 1 : 0;
                efEd.browseSel = lib;
            }
        }
        if (frameCounter == 30 && demoGraphOpen && getenv("SX_FXRAMP")) {
            // A graph filter: luminance -> color ramp (SX_FXRAMP=normal: normal from height), the ramp selected.
            EffectGraph g = EffectGraph::Default();
            Id in = 0, out = g.OutputNode();
            for (auto& n : g.nodes) if (n.op == "input") in = n.id;
            EGNode a;
            a.id = g.nextId++;
            const bool normal = std::string(getenv("SX_FXRAMP")) == "normal";
            if (normal) {
                a.op = "normalheight";
                a.inputs.resize(2);
                a.pos = {200, 0};
            } else {
                EGNode l;
                l.id = g.nextId++;
                l.op = "luma";
                l.inputs = {EGLink{in, 0}};
                l.pos = {150, 0};
                g.nodes.push_back(l);
                a.op = "colorramp";
                a.inputs = {EGLink{l.id, 0}};
                a.text = "0 0.02 0.02 0.12 1;0.35 0.55 0.1 0.35 1;0.7 0.95 0.45 0.1 1;1 1 0.97 0.75 1";
                a.pos = {400, 0};
            }
            g.nodes.push_back(a);
            if (EGNode* o = g.Find(out)) {
                o->inputs.resize(3);
                o->inputs[0] = {a.id, 0};
                o->pos = {700, 0};
            }
            if (Layer* l = d.ActiveLayer()) {
                EffectInstance fx;
                fx.graph = g.Serialize();
                if (auto def = fx.Def()) fx.params = def->DefaultParams();
                l->filters.push_back(fx);
                d.SyncGraph();
                efEd.target = 0;
                efEd.filterIndex = (int)l->filters.size() - 1;
                efEd.browse = false;
                efEd.cachedJson.clear();
                FocusPanel("Éditeur d'effets");
            }
            demoRampNode = a.id;
        }
        if (frameCounter == 36 && demoRampNode) {
            efEd.selected = demoRampNode;
            efEd.multi = {demoRampNode};
            efEd.frameAll = true;
        }
        if (frameCounter == 30 && demoGraphOpen && !getenv("SX_FXRAMP")) {
            const char* want = getenv("SX_FXID");
            for (auto& def : Effects().All())
                if (!def->blend && !def->graph.empty() && (!want || def->id == want)) {
                    Log("[fxbrowser] open graph of %s", def->id.c_str());
                    ApplyLibraryEffect(*v, def->id, FxApply::Filter);
                    efEd.target = 0;
                    efEd.browse = false;
                    FocusPanel("Éditeur d'effets");
                    break;
                }
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
        // SX_PSDFILE: another PSD (Photoshop's 16-bit layout: layers in a Lr16 block), opened and compared too.
        static gpu::ReadbackPtr extra;
        char pf[512] = "";
        if (stage == 4 && GetEnvironmentVariableA("SX_PSDFILE", pf, sizeof(pf)) > 0) {
            OpenImage(pf, false);
            stage = 5;
            at = frameCounter;
        }
        if (stage == 5 && frameCounter == at + 10) {
            DocView& dv = *docs.back();
            size_t pixel = 0;
            for (auto& l : dv.doc->S().layers) pixel += l.kind == LayerKind::Pixel;
            Log("[psd16] %s: %zu layers (%zu pixel layers), precision %d", pf, dv.doc->S().layers.size(), pixel, (int)dv.doc->GetPrecision());
            extra = dv.renderer.ExportRGBA8(*ctx, dv.renderer.EvaluateComplete(*dv.doc, *ctx));
            stage = 6;
        }
        if (stage == 6 && extra && extra->ready && orig && orig->ready) {
            CompareReadbacks("original vs PSD (Lr16 layout)", *orig, *extra, 3);
            stage = 7;
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
    if (demoMode == 10 && v && frameCounter == 5) FocusPanel("Propriétés");
    if (demoMode == 10 && v && frameCounter == 6) FocusPanel("Histogramme");
    if (demoMode == 11 && v && frameCounter == 5) FocusPanel("Propriétés");
    if (demoMode == 12 && v && frameCounter == 5) FocusPanel("Propriétés");
    if (demoMode == 18 && v && frameCounter == 5) FocusPanel("Propriétés");
    if (demoMode == 11 && v && frameCounter == 7) FocusPanel("Éditeur d'effets");
    if (demoMode == 27 && v && frameCounter == 5) FocusPanel("Vue 3D");
    if (demoMode == 30 && v) {
        // Generative fill with the local stand-in (SX_AI_MOCK=1): ellipse selection -> masked AI layer.
        Document& d = *v->doc;
        if (frameCounter == 6) {
            v->renderer.SelectShape(d, *ctx, v->history, SelShape::Ellipse, {550, 300, 1050, 700}, SelMode::Replace, true, 0, "Ellipse");
            FocusPanel("Génération IA");
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
        if (frameCounter == 5) FocusPanel("Vue 3D");
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
        if (frameCounter == 5) FocusPanel("Vue 3D");
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
        if (frameCounter == 5) FocusPanel("Vue 3D");
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
        if (frameCounter == 16 && meshDebugCanvas) FocusPanel("Canevas");
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
