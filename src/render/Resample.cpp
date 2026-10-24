// Canvas-level operations: image size, canvas size, crop, rotate, flip.
#include "doc/ShapePaths.h"
#include "render/Renderer.h"
#include "render/Dispatch.h"
#include "render/SeamCarve.h"

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

bool Renderer::CarveCanvas(Document& doc, gpu::CommandContext& ctx, History& hist, u32 newW, u32 newH, const SeamCarveResult& r,
                           const std::string& label) {
    if (newW == 0 || newH == 0 || newW > 16384 || newH > 16384 || r.newW == 0 || r.newH == 0) return false;
    if (r.mapX.size() < (size_t)r.newW * r.h || r.mapY.size() < (size_t)r.newW * r.newH) return false;
    auto* k = Kernel("carve.slang");
    if (!k) return false;
    auto mapX = gpu::Dev().CreateTexture(r.newW, r.h, gpu::Format::R32F, "CarveMapX");
    auto mapY = gpu::Dev().CreateTexture(r.newW, r.newH, gpu::Format::R32F, "CarveMapY");
    if (!mapX || !mapY) return false;
    ctx.Upload(mapX.get(), r.mapX.data(), r.newW * sizeof(float));
    ctx.Upload(mapY.get(), r.mapY.data(), r.newW * sizeof(float));
    struct CarveRoot {
        u32 dw, dh, sw, sh;
        u32 mxw, mxh, myw, myh;
        float2 red;
        u32 pad[2];
    };
    DocState before = doc.S();
    DocState& s = doc.S();
    for (auto& [id, surf] : s.surfaces) {
        if (!surf.tex) continue;
        auto tex = gpu::Dev().CreateTexture(newW, newH, surf.tex->GetFormat(), "Surface");
        if (!tex) return false;
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = surf.tex.get();
        d.srv[1] = mapX.get();
        d.srv[2] = mapY.get();
        d.uav[0] = tex.get();
        SetConstants(d, CarveRoot{newW, newH, surf.tex->Width(), surf.tex->Height(), r.newW, r.h, r.newW, r.newH, {(float)r.w, (float)r.h}, {0, 0}});
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
    float4 sdfBox;  // custom shape: distance field over [x, y] .. [x + z, y + w] (frame coordinates around its center)
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
    if (s.type == 5 && !s.custom.empty()) {
        // Custom outline: signed distance field computed on the CPU (cached per surface).
        const float bw = std::max(1.0f, std::abs(s.rect.z - s.rect.x)), bh = std::max(1.0f, std::abs(s.rect.w - s.rect.y));
        const float margin = s.strokeWidth + 4;
        u64 key = s.CustomHash();
        for (u64 part : {(u64)(bw * 16), (u64)(bh * 16), (u64)(margin * 64)}) key = (key ^ part) * 1099511628211ull;
        for (auto it = shapeSdf.begin(); it != shapeSdf.end();) it = doc.GetSurface(it->first) ? std::next(it) : shapeSdf.erase(it);
        auto& cache = shapeSdf[surface];
        if (cache.key != key || !cache.tex) {
            std::vector<float> field;
            int tw = 0, th = 0;
            float scale = 1;
            ShapeDistanceField(s.custom, bw, bh, margin, 1024, field, tw, th, scale);
            cache.tex = gpu::Dev().CreateTexture((u32)tw, (u32)th, gpu::Format::R32F, "ShapeSDF");
            ctx.Upload(cache.tex.get(), field.data(), (u32)tw * sizeof(float));
            cache.key = key;
            cache.box = {-bw * 0.5f - margin, -bh * 0.5f - margin, tw / scale, th / scale};
        }
        d.srv[0] = cache.tex.get();
        cb.sdfBox = cache.box;
    }
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
