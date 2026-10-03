// Region healing (membrane blend of a source area into a region) and pasting
// a computed fill, both undoable (shaders/heal_region.slang).
#include "render/Renderer.h"
#include "render/Dispatch.h"

namespace sx {

namespace {
struct HealCB {
    u32 w, h, mode, pad0;
    i32 ox, oy, gx, gy;
};
struct BoxCB {
    u32 w, h, mode;
    i32 radius;
    float eps, contrast, shift;
    u32 view;
    float band;
    float pad[3];
};
}  // namespace

void Renderer::BoxBlurInPlace(gpu::CommandContext& ctx, gpu::Texture* t, gpu::Texture* tmp, int radius, int passes) {
    auto* k = Kernel("select_refine.slang");
    if (!k || radius <= 0) return;
    for (int p = 0; p < passes; ++p)
        for (int dir = 0; dir < 2; ++dir) {
            BoxCB cb = {};
            cb.w = t->Width();
            cb.h = t->Height();
            cb.mode = dir == 0 ? 1u : 2u;
            cb.radius = radius;
            gpu::DispatchDesc d;
            d.pipeline = k;
            d.srv[0] = dir == 0 ? t : tmp;
            d.uav[0] = dir == 0 ? tmp : t;
            d.cbData = &cb;
            d.cbSize = sizeof(cb);
            Dispatch2D(ctx, d, cb.w, cb.h);
        }
}

bool Renderer::HealRegion(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, gpu::Texture* source, gpu::Texture* mask, int2 offset,
                          int radius, const std::string& label) {
    Surface* s = doc.GetSurface(surface);
    auto* k = Kernel("heal_region.slang");
    if (!s || !mask || !k) return false;
    const u32 W = s->tex->Width(), H = s->tex->Height();
    auto before = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoBefore");
    ctx.CopyTexture(before.get(), s->tex.get());
    Ensure(healD, W, H, gpu::Format::RGBA32F, "HealDiff");
    Ensure(healW, W, H, gpu::Format::RGBA32F, "HealWeight");
    Ensure(healTmp, W, H, gpu::Format::RGBA32F, "HealTmp");
    gpu::Texture* src = source ? source : before.get();
    auto run = [&](u32 mode, gpu::Texture* out0, gpu::Texture* out1) {
        HealCB cb = {W, H, mode, 0, offset.x, offset.y, 0, 0};
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = before.get();
        d.srv[1] = src;
        d.srv[2] = mask;
        d.srv[3] = healD.get();
        d.srv[4] = healW.get();
        d.uav[0] = out0;
        d.uav[1] = out1;
        d.cbData = &cb;
        d.cbSize = sizeof(cb);
        Dispatch2D(ctx, d, W, H);
    };
    run(0, healD.get(), healW.get());
    // Membrane: the border differences spread inside the region (normalized blur).
    BoxBlurInPlace(ctx, healD.get(), healTmp.get(), radius, 3);
    BoxBlurInPlace(ctx, healW.get(), healTmp.get(), radius, 3);
    run(3, s->tex.get(), nullptr);
    auto after = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoAfter");
    ctx.CopyTexture(after.get(), s->tex.get());
    hist.PushPaint(label, surface, {0, 0, (int)W, (int)H}, before, after);
    doc.TouchSurface(surface);
    return true;
}

bool Renderer::PasteMasked(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, gpu::Texture* crop, int2 origin, gpu::Texture* mask,
                           const std::string& label) {
    Surface* s = doc.GetSurface(surface);
    auto* k = Kernel("heal_region.slang");
    if (!s || !crop || !mask || !k) return false;
    const u32 W = s->tex->Width(), H = s->tex->Height();
    auto before = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoBefore");
    ctx.CopyTexture(before.get(), s->tex.get());
    HealCB cb = {W, H, 4, 0, 0, 0, origin.x, origin.y};
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = before.get();
    d.srv[1] = crop;
    d.srv[2] = mask;
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
