// Retouch brushes: clone, heal, blur/sharpen, dodge/burn, sponge, smudge.
#include "render/Renderer.h"
#include "render/Dispatch.h"

namespace sx {

namespace {
struct RetouchRoot {
    i32 rx, ry;
    u32 rw, rh, dw, dh, count;
    float hardness, opacity;
    u32 mode, flags;
    float strength;
    i32 sx, sy;
    u32 range;
    float pad;
};
struct LiquifyRoot {
    i32 rx, ry;
    u32 rw, rh, dw, dh;
    float2 center, delta;
    float radius, hardness, strength;
    u32 mode, flags;
};
struct ClearRoot { i32 x, y; u32 w, h; float4 value; };
struct SmudgeRoot {
    i32 rx, ry;
    u32 rw, rh, dw, dh;
    float2 center, delta;
    float radius, hardness, strength;
    u32 flags;
};
}  // namespace

void Renderer::RetouchDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, const RetouchParams& rp) {
    if (!strokeSurface || dabs.empty()) return;
    Surface* s = doc.GetSurface(strokeSurface);
    auto* k = Kernel("retouch.slang");
    if (!s || !k) return;
    const int W = (int)s->tex->Width(), H = (int)s->tex->Height();
    Rect touched;
    Surface* sel = doc.Selection();
    for (size_t start = 0; start < dabs.size(); start += 4096) {
        size_t count = std::min<size_t>(4096, dabs.size() - start);
        Rect r;
        for (size_t i = start; i < start + count; ++i) {
            const Dab& d = dabs[i];
            r.Merge({(int)std::floor(d.x - d.radius - 2), (int)std::floor(d.y - d.radius - 2), (int)std::ceil(d.x + d.radius + 2), (int)std::ceil(d.y + d.radius + 2)});
        }
        r = r.Clipped(W, H);
        if (r.Empty()) continue;
        strokeRect.Merge(r);
        touched.Merge(r);
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = strokePre.get();
        d.srv[1] = sel ? sel->tex.get() : nullptr;
        d.uav[0] = s->tex.get();
        d.uav[1] = strokeMask.get();
        MapDabs(d, 3, (const float4*)&dabs[start], count, sizeof(Dab), 2);
        SetConstants(d, RetouchRoot{r.x0, r.y0, (u32)r.Width(), (u32)r.Height(), (u32)W, (u32)H, (u32)count, b.hardness, b.opacity,
                                    (u32)rp.mode, sel ? 1u : 0u, rp.strength, rp.srcOffset.x, rp.srcOffset.y, (u32)rp.range, 0.0f});
        d.cbData = &dabs[start];
        d.cbSize = count * sizeof(Dab);
        Dispatch2D(ctx, d, (u32)r.Width(), (u32)r.Height());
    }
    doc.TouchSurface(strokeSurface, &touched);
}

void Renderer::SmudgeDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, float strength) {
    if (!strokeSurface || dabs.empty()) return;
    Surface* s = doc.GetSurface(strokeSurface);
    auto* k = Kernel("smudge.slang");
    if (!s || !k) return;
    const int W = (int)s->tex->Width(), H = (int)s->tex->Height();
    Rect touched;
    Ensure(smudgeSnap, (u32)W, (u32)H, s->tex->GetFormat(), "SmudgeSnap");
    Surface* sel = doc.Selection();
    for (const Dab& dab : dabs) {
        float2 c = {dab.x, dab.y};
        if (!haveSmudge) {
            lastSmudge = c;
            haveSmudge = true;
            continue;
        }
        float2 delta = c - lastSmudge;
        lastSmudge = c;
        float reach = dab.radius + Length(delta) + 2;
        Rect r = Rect{(int)std::floor(c.x - reach), (int)std::floor(c.y - reach), (int)std::ceil(c.x + reach), (int)std::ceil(c.y + reach)}.Clipped(W, H);
        if (r.Empty()) continue;
        strokeRect.Merge(r);
        touched.Merge(r);
        ctx.CopyRegion(smudgeSnap.get(), r.x0, r.y0, s->tex.get(), r);
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = smudgeSnap.get();
        d.srv[1] = sel ? sel->tex.get() : nullptr;
        d.uav[0] = s->tex.get();
        d.uavRect[0] = r;
        SetConstants(d, SmudgeRoot{r.x0, r.y0, (u32)r.Width(), (u32)r.Height(), (u32)W, (u32)H, c, delta, dab.radius, b.hardness,
                                   strength * dab.alpha, sel ? 1u : 0u});
        Dispatch2D(ctx, d, (u32)r.Width(), (u32)r.Height());
    }
    doc.TouchSurface(strokeSurface, &touched);
}

void Renderer::LiquifyDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, int mode, float strength) {
    if (!strokeSurface || dabs.empty()) return;
    Surface* s = doc.GetSurface(strokeSurface);
    auto* k = Kernel("liquify.slang");
    auto* clear = Kernel("clear.slang");
    if (!s || !k || !clear) return;
    const int W = (int)s->tex->Width(), H = (int)s->tex->Height();
    if (!liqActive) {
        // Fresh displacement field for this stroke (sparse on large documents).
        Ensure(liqDisp, (u32)W, (u32)H, gpu::Format::RGBA32F, "LiquifyField");
        gpu::DispatchDesc c;
        c.pipeline = clear;
        c.uav[0] = liqDisp.get();
        c.uavNoMap = 1;
        SetConstants(c, ClearRoot{0, 0, (u32)W, (u32)H, {0, 0, 0, 0}});
        Dispatch2D(ctx, c, (u32)W, (u32)H);
        liqActive = true;
        haveLiq = false;
    }
    Surface* sel = doc.Selection();
    Rect touched;
    for (const Dab& dab : dabs) {
        float2 c = {dab.x, dab.y};
        float2 delta = haveLiq ? c - lastLiq : float2{0, 0};
        lastLiq = c;
        haveLiq = true;
        if (mode == 0 && Length(delta) < 0.01f) continue;
        float reach = dab.radius * 1.1f + Length(delta) + 3;
        Rect r = Rect{(int)std::floor(c.x - reach), (int)std::floor(c.y - reach), (int)std::ceil(c.x + reach), (int)std::ceil(c.y + reach)}.Clipped(W, H);
        if (r.Empty()) continue;
        if (!liqSnap || liqSnap->Width() < (u32)r.Width() || liqSnap->Height() < (u32)r.Height()) {
            u32 sw = std::max<u32>(liqSnap ? liqSnap->Width() : 0, (u32)r.Width()), sh = std::max<u32>(liqSnap ? liqSnap->Height() : 0, (u32)r.Height());
            liqSnap = gpu::Dev().CreateTexture(sw, sh, gpu::Format::RGBA32F, "LiquifySnap");
        }
        ctx.CopyRegion(liqSnap.get(), 0, 0, liqDisp.get(), r);
        strokeRect.Merge(r);
        touched.Merge(r);
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = strokePre.get();
        d.srv[1] = liqSnap.get();
        d.srv[2] = sel ? sel->tex.get() : nullptr;
        d.uav[0] = liqDisp.get();
        d.uav[1] = s->tex.get();
        d.uavRect[0] = d.uavRect[1] = r;
        SetConstants(d, LiquifyRoot{r.x0, r.y0, (u32)r.Width(), (u32)r.Height(), (u32)W, (u32)H, c, delta, dab.radius, b.hardness,
                                    strength * dab.alpha, (u32)mode, sel ? 1u : 0u});
        Dispatch2D(ctx, d, (u32)r.Width(), (u32)r.Height());
    }
    doc.TouchSurface(strokeSurface, &touched);
}

}  // namespace sx
