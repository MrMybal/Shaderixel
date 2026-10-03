// Selection masks on the GPU: color range, edge refinement (guided filter) and
// its canvas preview, selection / layer mask from a mask texture.
#include "render/Renderer.h"
#include "render/Dispatch.h"

#include <cmath>

namespace sx {

namespace {
struct RangeCB {
    float4 samples[8];
    u32 w, h, preset, invert;
    float fuzz;
    u32 count;
    float pad[2];
};
struct RefineCB {
    u32 w, h, mode;
    i32 radius;
    float eps, contrast, shift;
    u32 view;
    float band;
    float pad[3];
};

float4 LinearToLab(float4 c) {
    float x = 0.4124f * c.x + 0.3576f * c.y + 0.1805f * c.z, y = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z,
          z = 0.0193f * c.x + 0.1192f * c.y + 0.9505f * c.z;
    x /= 0.95047f;
    z /= 1.08883f;
    auto f = [](float t) { return t > 0.008856f ? std::cbrt(t) : 7.787f * t + 16.0f / 116.0f; };
    float fx = f(x), fy = f(y), fz = f(z);
    return {116.0f * fy - 16.0f, 500.0f * (fx - fy), 200.0f * (fy - fz), 1.0f};
}
}  // namespace

gpu::Texture* Renderer::ColorRangeMask(gpu::CommandContext& ctx, gpu::Texture* image, const std::vector<float4>& samplesLinear, int preset, float fuzz,
                                       bool invert) {
    auto* k = Kernel("color_range.slang");
    if (!k || !image) return nullptr;
    const u32 W = image->Width(), H = image->Height();
    Ensure(rangeMask, W, H, gpu::Format::RGBA16F, "ColorRange");
    RangeCB cb = {};
    cb.count = (u32)std::min<size_t>(samplesLinear.size(), 8);
    for (u32 i = 0; i < cb.count; ++i) {
        cb.samples[i] = LinearToLab(samplesLinear[i]);
    }
    cb.w = W;
    cb.h = H;
    cb.preset = (u32)preset;
    cb.invert = invert ? 1u : 0u;
    cb.fuzz = fuzz;
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = image;
    d.uav[0] = rangeMask.get();
    d.cbData = &cb;
    d.cbSize = sizeof(cb);
    Dispatch2D(ctx, d, W, H);
    return rangeMask.get();
}

gpu::Texture* Renderer::RefineMask(gpu::CommandContext& ctx, gpu::Texture* image, gpu::Texture* selection, const RefineParams& p) {
    auto* k = Kernel("select_refine.slang");
    if (!k || !image || !selection) return nullptr;
    const u32 W = image->Width(), H = image->Height();
    for (auto* t : {&refineT[0], &refineT[1], &refineT[2]}) Ensure(*t, W, H, gpu::Format::RGBA32F, "RefineTmp");
    Ensure(refineMask, W, H, gpu::Format::RGBA16F, "RefineMask");
    Ensure(refineMask2, W, H, gpu::Format::RGBA16F, "RefineMask2");
    auto run = [&](u32 mode, gpu::Texture* a, gpu::Texture* b, gpu::Texture* out, int radius) {
        RefineCB cb = {};
        cb.w = W;
        cb.h = H;
        cb.mode = mode;
        cb.radius = radius;
        cb.eps = p.eps;
        cb.contrast = p.contrast;
        cb.shift = p.shift;
        cb.view = 0;
        cb.band = p.smartRadius ? 1.0f : 0.0f;
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = a;
        d.srv[1] = b;
        d.uav[0] = out;
        d.cbData = &cb;
        d.cbSize = sizeof(cb);
        Dispatch2D(ctx, d, W, H);
    };
    auto box = [&](gpu::Texture* t, gpu::Texture* tmp, int r) {
        if (r <= 0) return;
        run(1, t, nullptr, tmp, r);
        run(2, tmp, nullptr, t, r);
    };
    gpu::Texture* T0 = refineT[0].get();
    gpu::Texture* T1 = refineT[1].get();
    gpu::Texture* T2 = refineT[2].get();
    gpu::Texture* M = refineMask.get();
    gpu::Texture* M2 = refineMask2.get();
    const int r = (int)std::lround(p.radius);
    if (r > 0) {
        run(0, image, selection, T0, 0);  // (I, p, II, Ip)
        box(T0, T1, r);                   // means
        run(3, T0, nullptr, T2, 0);       // (a, b, mean p)
        box(T2, T1, r);                   // mean a, mean b
        run(4, image, T2, M, 0);          // q = mean a * I + mean b (near the edge)
    } else {
        run(5, selection, nullptr, M, 0);  // copy (no contrast / shift yet)
    }
    // Smooth (box), feather (3 boxes ~ gaussian), then contrast and shift.
    int s = (int)std::lround(p.smooth);
    if (s > 0) {
        run(1, M, nullptr, M2, s);
        run(2, M2, nullptr, M, s);
    }
    int f = (int)std::lround(p.feather * 0.6f);
    for (int i = 0; i < 3 && f > 0; ++i) {
        run(1, M, nullptr, M2, f);
        run(2, M2, nullptr, M, f);
    }
    run(5, M, nullptr, M2, 0);
    return M2;
}

gpu::Texture* Renderer::RefinePreview(gpu::CommandContext& ctx, gpu::Texture* image, gpu::Texture* mask, int view) {
    auto* k = Kernel("select_refine.slang");
    if (!k || !image || !mask) return nullptr;
    const u32 W = image->Width(), H = image->Height();
    Ensure(refinePreview, W, H, gpu::Format::RGBA16F, "RefinePreview");
    RefineCB cb = {};
    cb.w = W;
    cb.h = H;
    cb.mode = 6;
    cb.view = (u32)view;
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = image;
    d.srv[1] = mask;
    d.uav[0] = refinePreview.get();
    d.cbData = &cb;
    d.cbSize = sizeof(cb);
    Dispatch2D(ctx, d, W, H);
    return refinePreview.get();
}

bool Renderer::WriteMaskSurface(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, gpu::Texture* mask, const std::string& label) {
    // Layer mask <- mask (gray), undoable.
    Surface* s = doc.GetSurface(surface);
    auto* k = Kernel("select_refine.slang");
    if (!s || !mask || !k) return false;
    const u32 W = s->tex->Width(), H = s->tex->Height();
    auto before = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoBefore");
    ctx.CopyTexture(before.get(), s->tex.get());
    RefineCB cb = {};
    cb.w = W;
    cb.h = H;
    cb.mode = 5;
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = mask;
    d.uav[0] = s->tex.get();
    d.cbData = &cb;
    d.cbSize = sizeof(cb);
    Dispatch2D(ctx, d, W, H);
    auto after = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoAfter");
    ctx.CopyTexture(after.get(), s->tex.get());
    hist.PushPaint(label, surface, {0, 0, (int)W, (int)H}, before, after);
    doc.TouchSurface(surface);
    return true;
}

}  // namespace sx
