// Native regression fixture: --demo rasterize, never uses OS input devices.
#include "ui/App.h"
#include "doc/DocFile.h"
#include <imgui.h>
#include <cstdlib>
#include <cstring>
#include <climits>

namespace sx {
namespace {
struct RasterizeTest {
    std::string name, originalJson;
    Id layer = 0, surface = 0, smart = 0, sibling = 0;
    Layer original;
    size_t history = 0;
    int phase = 0, failures = 0;
    gpu::ReadbackPtr before, baked, painted, undone, restored, redone;
    void Check(bool ok, const char* what) {
        Log("[rasterize] %s %s", ok ? "PASS" : "FAIL", what);
        if (!ok) ++failures;
    }
    bool Same(const gpu::ReadbackPtr& a, const gpu::ReadbackPtr& b) {
        if (!a || !b || !a->ready || !b->ready || a->data.size() != b->data.size()) return false;
        for (size_t i = 0; i < a->data.size(); ++i)
            if (std::abs((int)a->data[i] - (int)b->data[i]) > 1) return false;
        return true;
    }
} test;
}

void App::RunRasterizeLayerDemo() {
    test = {};
    test.name = getenv("SX_RASTERIZE_CASE") ? getenv("SX_RASTERIZE_CASE") : "fill";
    for (const char* kernel : {"clear.slang", "clear_r.slang", "composite.slang", "display.slang",
                               "shape.slang", "smart_place.slang", "brush.slang", "gradient.slang"})
        gpu::Shaders().CompileNow(kernel, gpu::Shaders().File(kernel));
    auto precision = getenv("SX_RASTERIZE_FLOAT32") ? Precision::Float32 : Precision::Half16;
    NewDocument(256, 192, precision, DocMode::Layers, 0, "Layer bake test");
    auto& v = *Active(); auto& d = *v.doc;
    auto& back = d.AddPixelLayer("Backdrop");
    v.renderer.Prepare(d, *ctx);
    v.renderer.FillTexture(*ctx, d.GetSurface(back.surface)->tex.get(), {.1f, .2f, .3f, 1});
    d.TouchSurface(back.surface);
    Layer* l = nullptr;
    if (test.name == "shape") {
        ShapeStyle shape; shape.rect = {43.25f, 35.5f, 190.75f, 150.25f};
        shape.fill = {.9f, .35f, .1f, .75f}; shape.radius = 13.5f;
        l = &d.AddShapeLayer(shape);
    } else if (test.name == "text") {
        TextStyle text; text.content = "Bake me"; text.pos = {28, 72}; text.size = 38;
        text.color = {.9f, .5f, .1f, .8f}; l = &d.AddTextLayer(text);
    } else if (test.name == "smart") {
        auto content = gpu::Dev().CreateTexture(96, 64, SurfaceFormat(precision), "SharedContent");
        v.renderer.FillTexture(*ctx, content.get(), {.1f, .4f, .2f, .7f});
        SmartObject* so = NewSmartObject(d, "Shared original", content); test.smart = so->id;
        const float2 sibling[4] = {{4,4},{64,4},{64,34},{4,34}};
        test.sibling = AddSmartLayer(v, *so, sibling)->id;
        const float2 target[4] = {{52,48},{208,40},{218,155},{48,166}};
        l = AddSmartLayer(v, *so, target);
    } else if (test.name == "pixel" || test.name == "plain" || test.name == "material") {
        l = &d.AddPixelLayer("Pixels");
        v.renderer.Prepare(d, *ctx);
        v.renderer.FillTexture(*ctx, d.GetSurface(l->surface)->tex.get(), {.12f, .35f, .15f, .7f});
        d.TouchSurface(l->surface);
    } else if (test.name == "group") {
        l = &d.GroupLayer(d.S().activeLayer);
    } else {
        l = &d.AddEffectLayer(test.name == "adjustment" ? LayerKind::Adjustment : LayerKind::Fill,
                              test.name == "adjustment" ? "invert" : "gradient_fill");
        if (l->kind == LayerKind::Fill) {
            l->effect.params[0] = {.04f,.3f,.6f,.45f};
            l->effect.params[1] = {.8f,.2f,.03f,.75f};
        }
    }
    test.layer = l->id;
    if (test.name != "plain" && test.name != "group" && test.name != "adjustment") {
        EffectInstance fx; fx.effectId = "gaussian_blur"; fx.params = fx.Def()->DefaultParams(); fx.params[0].x = 2.5f;
        l->filters.push_back(fx);
        fx = {}; fx.effectId = "transform_2d"; fx.params = fx.Def()->DefaultParams();
        fx.params[0] = {8.5f,-3.25f,0,0}; fx.params[1].x = .78f;
        l->filters.push_back(fx);
        fx = {}; fx.effectId = "invert"; fx.params = fx.Def()->DefaultParams(); fx.enabled = false;
        l->filters.push_back(fx);
    }
    if (test.name == "many_filters") {
        auto tail = l->filters[1]; l->filters.clear();
        for (int i = 0; i < kMaxLayerFilters - 1; ++i) {
            EffectInstance fx; fx.effectId = "invert"; fx.params = fx.Def()->DefaultParams(); l->filters.push_back(fx);
        }
        l->filters.push_back(tail);
    }
    if (test.name == "invalid") {
        l->filters.back().enabled = true;
        l->filters.back().customCode = "float4 Effect(EffectContext ctx) { return definitely_invalid_shader; }";
    }
    l->opacity = .63f; l->blend = BlendMode::Screen; l->channelOff = 2;
    l->blendIf.self[0][1] = 12;
    if (test.name == "custom_blend") { l->blendFx.customCode = kCustomBlendTemplate; l->blendFx.params = l->blendFx.Def()->DefaultParams(); }
    if (test.name == "locked") l->locked = true;
    if (test.name == "hidden") l->visible = false;
    if (test.name == "tiled") d.forceTiled = true;
    if (test.name == "clipped") l->clipped = true;
    d.AddMask(test.layer); v.renderer.Prepare(d, *ctx);
    v.renderer.FillTexture(*ctx, d.GetSurface(l->mask)->tex.get(), {.55f,.55f,.55f,1});
    d.TouchSurface(l->mask);
    if (test.name == "disabled_mask") l->maskEnabled = false;
    if (test.name == "material") {
        l->material = d.NewId();
        auto& peer = d.AddPixelLayer("Shared material mask");
        peer.material = d.FindLayer(test.layer)->material;
        peer.mask = d.FindLayer(test.layer)->mask;
        peer.visible = false;
        test.sibling = peer.id;
    }
    if (test.name == "clipped_base") {
        auto& clip = d.AddEffectLayer(LayerKind::Fill, "solid_color");
        clip.clipped = true; clip.opacity = .4f;
    }
    d.S().activeLayer = test.layer; d.S().editMask = true;
    d.SyncGraph(); v.fitPending = true;
    panelOpen["Propriétés"].open = true; ShowSideTab(0); FocusPanel("Propriétés");
}

void App::RasterizeLayerDemoTick() {
    if (frameCounter < 8 || test.phase == 9 || !Active()) return;
    auto& v = *Active(); auto& d = *v.doc;
    auto grab = [&]() { return v.renderer.ExportRGBA8(*ctx, v.renderer.EvaluateComplete(d, *ctx)); };
    if (test.phase == 0) {
        auto* image = v.renderer.EvaluateComplete(d, *ctx);
        if (!image || v.renderer.AnyPending()) return;
        test.before = v.renderer.ExportRGBA8(*ctx, image);
        test.original = *d.FindLayer(test.layer); test.surface = test.original.surface;
        test.history = v.history.Count(); test.originalJson = SerializeDocument(d).Dump();
        if (test.name == "plain" || test.name == "locked" || test.name == "group" || test.name == "adjustment") {
            std::string why;
            test.Check(!CanRasterizeLayer(d, d.FindLayer(test.layer), &why) && !why.empty(), "unsupported layer explains why");
            test.Check(RasterizeLayerNow(v, test.layer) == LayerBakeResult::Failed &&
                       SerializeDocument(d).Dump() == test.originalJson && v.history.Count() == test.history, "rejected bake keeps document and history");
            Log("[rasterize] COMPLETE %d failures", test.failures); test.phase = 9; return;
        }
        if (test.name == "macro") {
            ActionStep step; step.cmd = "rasterize_layer";
            test.Check(StartActionSteps(v, "Bake layer regression", {step}, 0, INT_MAX, ""), "recorded bake action starts");
        } else {
            RequestRasterizeLayer(v, test.layer);
            RequestRasterizeLayer(v, test.layer); // repeated click must not create another undo step
        }
        test.phase = 1; return;
    }
    if (test.phase == 1) {
        if (!layerBakes.empty() || actionRun.active) return;
        auto* l = d.FindLayer(test.layer);
        if (test.name == "invalid") {
            test.Check(l->kind == test.original.kind && SerializeDocument(d).Dump() == test.originalJson &&
                       v.history.Count() == test.history, "invalid shader cannot bake a fallback image");
            Log("[rasterize] COMPLETE %d failures", test.failures); test.phase = 9; return;
        }
        test.Check(l->kind == LayerKind::Pixel && l->surface && l->surface != test.surface && l->filters.empty() && !l->smart,
                   "new independent pixel surface with effects baked");
        test.Check(l->maskEnabled == test.original.maskEnabled && l->opacity == test.original.opacity && l->blend == test.original.blend &&
                   l->visible == test.original.visible && l->parent == test.original.parent && l->clipped == test.original.clipped &&
                   l->channelOff == test.original.channelOff && std::memcmp(&l->blendIf, &test.original.blendIf, sizeof(BlendIf)) == 0 &&
                   l->blendFx.customCode == test.original.blendFx.customCode, "mask and compositing properties preserved");
        test.Check(!d.S().editMask && d.PaintTarget() == l->surface && v.history.Count() == test.history + 1,
                   "brush targets pixels and conversion is one undo step");
        if (test.smart) test.Check(d.GetSmart(test.smart) && d.FindLayer(test.sibling)->smart == test.smart, "shared smart original remains editable");
        if (test.name == "material") test.Check(!l->material && l->mask != test.original.mask &&
            d.FindLayer(test.sibling)->mask == test.original.mask, "material mask snapshot is independent");
        else test.Check(l->mask == test.original.mask, "original mask kept separate");
        test.baked = grab(); test.phase = 2; return;
    }
    if (test.phase == 2) {
        if (!test.before->ready || !test.baked->ready) return;
        test.Check(test.Same(test.before, test.baked), "bake preserves rendered appearance");
        bool paint = v.renderer.BeginStroke(d, *ctx, d.PaintTarget());
        test.Check(paint, "brush can begin on baked layer");
        if (paint) {
            v.renderer.StrokeDabs(d, *ctx, {{128,96,18,1}}, brush, {1,.1f,.8f,1}, false);
            v.renderer.EndStroke(d, *ctx, v.history, "Paint baked layer");
        }
        test.painted = grab(); test.phase = 3; return;
    }
    if (test.phase == 3) {
        if (!test.painted->ready) return;
        if (test.name != "hidden") test.Check(!test.Same(test.baked, test.painted), "paint changes rendered pixels");
        Undo(); test.undone = grab(); test.phase = 4; return;
    }
    if (test.phase == 4) {
        if (!test.undone->ready) return;
        test.Check(test.Same(test.before, test.undone), "undo paint restores baked appearance");
        Undo();
        test.Check(SerializeDocument(d).Dump() == test.originalJson, "undo bake restores original editable layer and controls");
        test.restored = grab(); test.phase = 5; return;
    }
    if (test.phase == 5) {
        if (!test.restored->ready) return;
        test.Check(test.Same(test.before, test.restored), "undo bake preserves appearance");
        Redo(); Redo(); test.redone = grab(); test.phase = 6; return;
    }
    if (test.phase == 6 && test.redone->ready) {
        test.Check(test.Same(test.painted, test.redone), "redo bake and paint restores result");
        Log("[rasterize] COMPLETE %d failures", test.failures); test.phase = 9;
    }
}
} // namespace sx
