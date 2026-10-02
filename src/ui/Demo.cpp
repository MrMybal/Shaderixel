// Scripted demo scene used for automated visual checks (--demo [nodes]).
#include "ui/App.h"
#include "gpu/ShaderSystem.h"
#include "doc/EffectGraph.h"

#include <fstream>

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
    if (demoMode == 11 && v && frameCounter == 5) ImGui::SetWindowFocus("Propriétés");
    if (demoMode == 12 && v && frameCounter == 5) ImGui::SetWindowFocus("Propriétés");
    if (demoMode == 11 && v && frameCounter == 7) ImGui::SetWindowFocus("Éditeur d'effets");
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
    if (demoMode == 7 || demoMode == 8 || demoMode == 9 || demoMode == 17) {
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
        Id nz = add("noise", {40, 220});
        Id scale = add("const", {-160, 260});
        g.Find(scale)->value = {6, 0, 0, 0};
        Id tint = add("paramcolor", {40, 360});
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
    if (demoMode == 14) {
        AttachMesh(v, MakeSphere());
        tool = Tool::Brush;
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
