// Scripted demo scene used for automated visual checks (--demo [nodes]).
#include "ui/App.h"
#include "gpu/ShaderSystem.h"

#include <cmath>

namespace sx {

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
