// Scripted demo scene used for automated visual checks (--demo [nodes]).
#include "ui/App.h"
#include "gpu/ShaderSystem.h"
#include "doc/EffectGraph.h"

#include <fstream>

#include <cmath>

namespace sx {

void App::DemoTick() {
    DocView* v = Active();
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
    if (demoMode == 11 && v && frameCounter == 7) ImGui::SetWindowFocus("Éditeur d'effets");
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
    if (demoMode == 7 || demoMode == 8 || demoMode == 9) {
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

}  // namespace sx
