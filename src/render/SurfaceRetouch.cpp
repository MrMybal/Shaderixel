#include "render/Renderer.h"
#include "render/Dispatch.h"
#include "render/MeshPaint.h"

namespace sx {

gpu::Texture* Renderer::SurfaceUvView(gpu::CommandContext& ctx, MeshScene& s, u32 w, u32 h) {
    if (!s.vb || !s.ib || !w || !h) return nullptr;
    const u64 gen = gpu::Shaders().FileGeneration();
    if (!surfacePipeline || gen != surfacePipelineVersion) {
        const char* file = "mesh_surface_uv.slang";
        auto prog = gpu::Shaders().CompileGraphicsNow(file, gpu::Shaders().File(file));
        if (!prog || prog->state.load() != gpu::Program::State::Ready) return nullptr;
        gpu::GraphicsDesc desc;
        desc.colorFormats = {gpu::Format::RGBA32F};
        surfacePipeline = gpu::Dev().CreateGraphicsPipeline(prog->vs.data(), prog->vs.size(), prog->ps.data(), prog->ps.size(), desc);
        surfacePipelineVersion = gen;
        surfaceVertices = nullptr;
        if (!surfacePipeline) return nullptr;
    }
    s.UpdateCamera((float)w / h);
    if (surfaceUv && surfaceDepth && surfaceUv->Width() == w && surfaceUv->Height() == h && surfaceVertices == s.vb.get() &&
        surfaceRevision == s.revision && !memcmp(surfaceMatrix.m, s.viewProj.m, sizeof(surfaceMatrix.m)))
        return surfaceUv.get();
    if (!surfaceUv || surfaceUv->Width() != w || surfaceUv->Height() != h)
        surfaceUv = gpu::Dev().CreateTexture(w, h, gpu::Format::RGBA32F, "SurfaceUV", gpu::UsageRenderTarget);
    if (!surfaceDepth || surfaceDepth->Width() != w || surfaceDepth->Height() != h)
        surfaceDepth = gpu::Dev().CreateTexture(w, h, gpu::Format::D32F, "SurfaceUVDepth");
    if (!surfaceUv || !surfaceDepth) return nullptr;
    gpu::DrawDesc draw;
    draw.pipeline = surfacePipeline.get();
    draw.rt[0] = surfaceUv.get();
    draw.depth = surfaceDepth.get();
    draw.clearDepth = draw.clearColor = true;
    draw.clearValue = {0, 0, 0, 0};
    draw.vertices = s.vb.get();
    draw.indices = s.ib.get();
    draw.count = (u32)s.data.indices.size();
    draw.cbData = &s.viewProj;
    draw.cbSize = sizeof(s.viewProj.m);
    ctx.Draw(draw);
    surfaceMatrix = s.viewProj;
    surfaceVertices = s.vb.get();
    surfaceRevision = s.revision;
    return surfaceUv.get();
}

namespace {
struct Retouch3DRoot {
    u32 w, h, vw, vh;
    float2 from, to, offset;
    float radius, hardness, strength, zNear, zFar;
    u32 flags;
};

struct RetouchView {
    Mat4 vp;
    Rect extent;
};

Rect RetouchRegion(const MeshScene& s, gpu::Texture* view, float2 from, float2 to, float radius) {
    const float4 dabs[] = {{from.x, from.y, radius, 1}, {to.x, to.y, radius, 1}};
    return MeshBrushRegion(s, dabs, 2, view->Width(), view->Height());
}

gpu::DispatchDesc MakeSurfaceDispatch(Document& doc, MeshScene& s, gpu::Texture* view, float2 from, float2 to, float2 offset,
                                      const BrushSettings& b, float strength, u32 mode, const RetouchView& projection) {
    gpu::DispatchDesc d;
    d.pipeline = Kernel("mesh_surface_retouch.slang");
    d.srv[0] = s.posMap.get();
    d.srv[1] = view;
    d.srv[3] = mode == 0 && doc.Selection() ? doc.Selection()->tex.get() : nullptr;
    SetConstants(d, Retouch3DRoot{doc.Width(), doc.Height(), view->Width(), view->Height(), from, to, offset, std::max(0.5f, b.size * 0.5f),
                                  b.hardness, std::clamp(strength * b.flow * b.opacity, 0.0f, 1.0f),
                                  s.ortho ? MeshScene::kOrthoNear : MeshScene::kNear, s.ortho ? MeshScene::kOrthoFar : MeshScene::kFar,
                                  mode | (d.srv[3] ? 4u : 0u)});
    d.cbData = &projection;
    d.cbSize = sizeof(projection);
    return d;
}
} // namespace

bool Renderer::RetouchSurface(Document& doc, gpu::CommandContext& ctx, MeshScene& s, gpu::Texture* view, float2 from, float2 to,
                              float2 offset, const BrushSettings& b, float strength, bool smudge,
                              const std::vector<gpu::TexturePtr>& merged) {
    if (!view || !s.posMap || !strokeSurface || !strokeMask) return false;
    const Rect extent = RetouchRegion(s, view, from, to, b.size * 0.5f);
    if (extent.Empty()) return false;
    const RetouchView projection{s.viewProj, extent};
    auto d = MakeSurfaceDispatch(doc, s, view, from, to, offset, b, strength, 0, projection);
    if (!d.pipeline) return false;
    Rect source;
    if (smudge) {
        const Rect r = RetouchRegion(s, view, from + offset, to + offset, b.size * 0.5f + 1);
        if (r.Empty()) return false;
        source = Rect{r.x0 - 1, r.y0 - 1, r.x1 + 1, r.y1 + 1}.Clipped(doc.Width(), doc.Height());
    }
    d.uavRect[0] = d.uavRect[1] = extent;
    bool wrote = false;
    d.uav[1] = strokeMask.get();
    for (size_t i = 0; i <= strokeSlots.size(); ++i) {
        const Id id = i ? strokeSlots[i - 1].surface : strokeSurface;
        auto* sf = doc.GetSurface(id);
        if (!sf || !sf->tex) continue;
        if (smudge) {
            Ensure(surfaceSmudge, doc.Width(), doc.Height(), sf->tex->GetFormat(), "SurfaceSmudgeSource");
            if (!surfaceSmudge) continue;
            ctx.CopyRegion(surfaceSmudge.get(), source.x0, source.y0, sf->tex.get(), source);
            d.srv[2] = surfaceSmudge.get();
        } else {
            d.srv[2] = i < merged.size() && merged[i] ? merged[i].get() : i ? strokeSlots[i - 1].pre.get() : strokePre.get();
        }
        if (!d.srv[2]) continue;
        d.uav[0] = sf->tex.get();
        Dispatch2D(ctx, d, extent.Width(), extent.Height());
        PreserveDisabledPaint(ctx,id,sf->tex.get(),i ? strokeSlots[i-1].pre.get() : strokePre.get(),extent);
        doc.TouchSurface(id, &extent);
        wrote = true;
    }
    if (wrote) {
        strokeRect.Merge(extent);
        TrackMeshStrokeCoverage();
    }
    return wrote;
}

void Renderer::BrushSurfaceMask(Document& doc, gpu::CommandContext& ctx, MeshScene& s, gpu::Texture* view, gpu::Texture* coverage,
                                float2 from, float2 to, const BrushSettings& b, bool uncover) {
    if (!view || !coverage || !s.posMap) return;
    const auto extent = RetouchRegion(s, view, from, to, b.size * 0.5f);
    if (extent.Empty()) return;
    const RetouchView projection{s.viewProj, extent};
    auto d = MakeSurfaceDispatch(doc, s, view, from, to, {}, b, 1, uncover ? 2u : 1u, projection);
    if (!d.pipeline) return;
    d.uav[1] = coverage;
    d.uavRect[1] = extent;
    Dispatch2D(ctx, d, extent.Width(), extent.Height());
}

gpu::Texture* Renderer::SurfaceMaskOverlay(gpu::CommandContext& ctx, gpu::Texture* image, gpu::Texture* view, gpu::Texture* mask) {
    if (!image || !view || !mask || view->Width() != image->Width() || view->Height() != image->Height()) return image;
    gpu::DispatchDesc d;
    d.pipeline = Kernel("mesh_mask_overlay.slang");
    if (!d.pipeline) return image;
    Ensure(surfaceOverlay, image->Width(), image->Height(), image->GetFormat(), "SurfaceMaskOverlay");
    if (!surfaceOverlay) return image;
    d.srv[0] = image;
    d.srv[1] = view;
    d.srv[2] = mask;
    d.uav[0] = surfaceOverlay.get();
    const u32 size[2] = {image->Width(), image->Height()};
    SetConstants(d, size);
    Dispatch2D(ctx, d, size[0], size[1]);
    return surfaceOverlay.get();
}

} // namespace sx
