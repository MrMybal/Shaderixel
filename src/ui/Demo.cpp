// Scripted demo scene used for automated visual checks (--demo [nodes]).
#include "ui/App.h"
#include "io/ColorMgmt.h"
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
        if (frameCounter == 20) {
            PtRenderState& r = RenderStateFor(*a);
            if (GetEnvironmentVariableA("SX_HDRI", b, sizeof(b)) > 0) r.settings.hdri = b;
            if (GetEnvironmentVariableA("SX_CPU", b, sizeof(b)) > 0) r.settings.device = 1;
            if (GetEnvironmentVariableA("SX_SAMPLES", b, sizeof(b)) > 0) r.settings.maxSamples = atoi(b);
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
        if (frameCounter == 22) ImGui::SetWindowFocus("Rendu");
        if (frameCounter % 30 == 0 && a->render) Log("[demo] render: %d samples (%s)", RenderSamples(*a->render), a->render->settings.device ? "CPU" : "GPU");
        // Export once every sample is done (SX_SIZE=WxH: fixed resolution instead of the view size).
        if (frameCounter == 20 && GetEnvironmentVariableA("SX_SIZE", b, sizeof(b)) > 0) {
            PtRenderState& r = RenderStateFor(*a);
            if (sscanf(b, "%dx%d", &r.settings.width, &r.settings.height) == 2) r.settings.viewSize = false;
        }
        static bool exported = false;
        if (!exported && frameCounter > 30 && a->render && RenderSamples(*a->render) >= a->render->settings.maxSamples &&
            GetEnvironmentVariableA("SX_EXPORT", b, sizeof(b)) > 0) {
            ExportRender(*a, b);
            exported = true;
            captureFrame = std::min(captureFrame, (int)frameCounter + 20);  // done: capture and quit soon after
            Log("[demo] render exported at frame %d", frameCounter);
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
        if (frameCounter == 8) ImGui::SetWindowFocus("Vue 3D");
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
        if (frameCounter == 8) ImGui::SetWindowFocus("Vue 3D");
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
        if (frameCounter == 8) ImGui::SetWindowFocus("Vue 3D");
        if (frameCounter == 14 && s.depth) {
            fg = {1.0f, 0.78f, 0.34f, 1};
            brush3D.use[(int)PbrChannel::Roughness] = brush3D.use[(int)PbrChannel::Metallic] = true;
            brush3D.rough = 0.25f;
            brush3D.metal = 1.0f;
            std::vector<float4> colors;
            std::vector<Id> targets = Paint3DTargets(*a, colors);
            Log("[demo] paint3d: layout %d, %zu targets, %zu layers, channels 0x%X", layoutKind, targets.size(), d.S().layers.size(), d.pbrChannels);
            a->renderer.meshSymmetryX = GetEnvironmentVariableA("SX_SYM", nullptr, 0) > 0;
            if (a->renderer.meshSymmetryX) s.yaw = 1.5707963f;  // facing the mirror plane: it is vertical on screen
            if (a->renderer.meshSymmetryX) a->renderer.MeshRender(*ctx, s, GatherMaterial(*a), s.depth->Width(), s.depth->Height());
            if (a->renderer.BeginStrokeMulti(d, *ctx, targets)) {
                std::vector<Dab> dabs;
                float cx = s.depth->Width() * 0.5f, cy = s.depth->Height() * 0.5f, r = std::min(cx, cy) * 0.45f;
                if (a->renderer.meshSymmetryX) r *= 0.4f, cx -= r * 1.4f;
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
            ImGui::SetWindowFocus("Éditeur d'effets");
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
        if (frameCounter == 9) ImGui::SetWindowFocus("Éditeur d'effets");
        if (frameCounter == 12 && GetEnvironmentVariableA("SX_PAL", nullptr, 0) > 0) {
            // Node palette: one node in the effect graph (center of the view) and one in the layer graph.
            ShowSideTab(2);
            ImGui::SetWindowFocus("Palette de nœuds");
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
            batch.action = actionSel;
            batch.source = src;
            batch.dest = dst;
            batch.format = 0;
            batch.suffix = "_lot";
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
            ImGui::SetWindowFocus("Propriétés");
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
        ImGui::SetWindowFocus("Propriétés");
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
