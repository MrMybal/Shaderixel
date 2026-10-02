// Scripted demo scene used for automated visual checks (--demo [nodes]).
#include "ui/App.h"
#include "gpu/ShaderSystem.h"

#include <cmath>

namespace sx {

void App::DemoTick() {
    DocView* v = Active();
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
