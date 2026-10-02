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

}  // namespace sx
