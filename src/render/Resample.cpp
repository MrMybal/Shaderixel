// Canvas-level operations: image size, canvas size, crop, rotate, flip.
#include "render/Renderer.h"
#include "render/Dispatch.h"

#include <set>

namespace sx {

namespace {
struct ResampleRoot {
    u32 dw, dh, sw, sh;
    float4 m;
    float2 t;
    u32 interp, flags;
    float4 outside;
};
}  // namespace

bool Renderer::ResampleCanvas(Document& doc, gpu::CommandContext& ctx, History& hist, u32 newW, u32 newH, const float m[6], int interp,
                              const std::string& label) {
    if (newW == 0 || newH == 0 || newW > 16384 || newH > 16384) return false;
    auto* k = Kernel("resample.slang");
    if (!k) return false;
    DocState before = doc.S();
    DocState& s = doc.S();
    std::set<Id> masks;
    for (auto& l : s.layers) if (l.mask) masks.insert(l.mask);
    for (auto& [id, surf] : s.surfaces) {
        if (!surf.tex) continue;
        auto fmt = surf.tex->GetFormat();
        auto tex = gpu::Dev().CreateTexture(newW, newH, fmt, "Surface");
        if (!tex) return false;
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = surf.tex.get();
        d.uav[0] = tex.get();
        float4 outside = masks.count(id) ? float4{1, 1, 1, 1} : float4{0, 0, 0, 0};
        SetConstants(d, ResampleRoot{newW, newH, surf.tex->Width(), surf.tex->Height(), {m[0], m[1], m[3], m[4]}, {m[2], m[5]}, (u32)interp, 0u, outside});
        Dispatch2D(ctx, d, newW, newH);
        surf.tex = tex;
    }
    s.width = newW;
    s.height = newH;
    doc.InvalidateSurfaces();
    doc.SyncGraph();
    hist.PushState(label, before, doc.S());
    return true;
}

namespace {
struct ShapeCB {
    float4 rect, fill, stroke;
    u32 w, h, type, flags;
    float radius, strokeWidth, rotation;
    i32 sides;
    u32 partCount, pad[3];
    float4 partRect[8], partParam[8], partOp[8];
};
}  // namespace

void Renderer::RenderShape(Document& doc, gpu::CommandContext& ctx, Id surface, const ShapeStyle& s) {
    Surface* sf = doc.GetSurface(surface);
    auto* k = Kernel("shape.slang");
    if (!sf || !k) return;
    auto lin = [](float4 c) { return float4{SrgbToLinear(c.x), SrgbToLinear(c.y), SrgbToLinear(c.z), c.w}; };
    ShapeCB cb;
    cb.rect = s.rect;
    cb.fill = lin(s.fill);
    cb.stroke = lin(s.stroke);
    cb.w = sf->tex->Width();
    cb.h = sf->tex->Height();
    cb.type = (u32)s.type;
    cb.flags = (s.fillOn ? 1u : 0u) | (s.strokeOn ? 2u : 0u);
    cb.radius = s.type == 4 ? s.radius : s.radius;
    cb.strokeWidth = s.strokeWidth;
    cb.rotation = s.rotation * 3.14159265f / 180.0f;
    cb.sides = s.sides;
    cb.partCount = (u32)std::min<size_t>(s.parts.size(), 8);
    for (u32 i = 0; i < cb.partCount; ++i) {
        const ShapePart& p = s.parts[i];
        cb.partRect[i] = p.rect;
        cb.partParam[i] = {(float)p.type, p.radius, (float)p.sides, p.rotation * 3.14159265f / 180.0f};
        cb.partOp[i] = {(float)p.op, 0, 0, 0};
    }
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.uav[0] = sf->tex.get();
    {
        // Only the shape's (rotated) extent receives non-zero pixels.
        Rect all;
        auto add = [&](const float4& r) {
            float cx = (r.x + r.z) * 0.5f, cy = (r.y + r.w) * 0.5f;
            float rad = 0.5f * std::sqrt((r.z - r.x) * (r.z - r.x) + (r.w - r.y) * (r.w - r.y)) + s.strokeWidth + 4;
            all.Merge({(int)std::floor(cx - rad), (int)std::floor(cy - rad), (int)std::ceil(cx + rad), (int)std::ceil(cy + rad)});
        };
        add(s.rect);
        for (u32 i = 0; i < cb.partCount; ++i) if (s.parts[i].op == 0 || s.parts[i].op == 3) add(s.parts[i].rect);
        d.uavRect[0] = all;
    }
    d.cbData = &cb;
    d.cbSize = sizeof(cb);
    Dispatch2D(ctx, d, cb.w, cb.h);
    doc.TouchSurface(surface);
}

}  // namespace sx
