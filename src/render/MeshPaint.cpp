// Mesh viewport rendering, UV bake and projection painting.
#include "render/Renderer.h"
#include "render/MeshPaint.h"
#include "render/Dispatch.h"

#include <cstring>

namespace sx {

void MeshScene::UpdateCamera(float aspect) {
    pitch = std::clamp(pitch, -1.5f, 1.5f);
    distance = std::clamp(distance, 0.2f, 50.0f);
    camPos = target + float3{std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)} * distance;
    Mat4 view = Mat4::LookAtLH(camPos, target, {0, 1, 0});
    Mat4 proj = Mat4::PerspectiveLH(fov, std::max(aspect, 0.01f), 0.02f, 200.0f);
    viewProj = proj * view;
}

namespace {
const gpu::GraphicsPipeline* MeshPipeline(const char* file, const gpu::GraphicsDesc& desc) {
    static std::unordered_map<std::string, gpu::GraphicsPipelinePtr> cache;
    static std::unordered_map<std::string, u64> cacheGen;
    std::string key = file;
    u64 gen = gpu::Shaders().FileGeneration();
    auto it = cache.find(key);
    if (it != cache.end() && cacheGen[key] == gen) return it->second.get();
    auto prog = gpu::Shaders().CompileGraphicsNow(file, gpu::Shaders().File(file));
    if (prog->state.load() != gpu::Program::State::Ready) return nullptr;
    auto p = gpu::Dev().CreateGraphicsPipeline(prog->vs.data(), prog->vs.size(), prog->ps.data(), prog->ps.size(), desc);
    cache[key] = p;
    cacheGen[key] = gen;
    return p.get();
}

struct ViewCB {
    float vp[16];
    float4 camPos, lightDir;
    u32 flags;
    float pad[3];
};
struct BrushRootM {
    u32 dw, dh, vw, vh, count;
    float hardness, opacity;
    u32 mode, flags;
    float pad[3];
};
}  // namespace

bool Renderer::MeshPrepare(Document& doc, gpu::CommandContext& ctx, MeshScene& s) {
    if (!s.vb || s.gpuDirty) {
        s.vb = gpu::Dev().CreateBuffer(ctx, s.data.vertices.data(), s.data.vertices.size() * sizeof(MeshVertex), "MeshVB");
        s.ib = gpu::Dev().CreateBuffer(ctx, s.data.indices.data(), s.data.indices.size() * sizeof(u32), "MeshIB");
        s.gpuDirty = false;
        s.bakedW = 0;
    }
    const u32 W = doc.Width(), H = doc.Height();
    if (s.bakedW == W && s.bakedH == H && s.posMap) return true;
    gpu::GraphicsDesc gd;
    gd.colorFormats = {gpu::Format::RGBA32F, gpu::Format::RGBA32F};
    gd.useDepth = false;
    auto* bake = MeshPipeline("mesh_bake.slang", gd);
    auto* dilate = Kernel("mesh_dilate.slang");
    if (!bake || !dilate) return false;
    s.posMap = gpu::Dev().CreateTexture(W, H, gpu::Format::RGBA32F, "PosMap", gpu::UsageRenderTarget);
    s.nrmMap = gpu::Dev().CreateTexture(W, H, gpu::Format::RGBA32F, "NrmMap", gpu::UsageRenderTarget);
    s.tmpPos = gpu::Dev().CreateTexture(W, H, gpu::Format::RGBA32F, "PosTmp");
    s.tmpNrm = gpu::Dev().CreateTexture(W, H, gpu::Format::RGBA32F, "NrmTmp");
    gpu::DrawDesc d;
    d.pipeline = bake;
    d.rt[0] = s.posMap.get();
    d.rt[1] = s.nrmMap.get();
    d.clearColor = true;
    d.clearValue = {0, 0, 0, 0};
    d.vertices = s.vb.get();
    d.indices = s.ib.get();
    d.count = (u32)s.data.indices.size();
    ctx.Draw(d);
    // Dilate islands by 6 texels (ping-pong) to cover seams.
    for (int i = 0; i < 6; ++i) {
        gpu::Texture *srcP = (i & 1) ? s.tmpPos.get() : s.posMap.get(), *srcN = (i & 1) ? s.tmpNrm.get() : s.nrmMap.get();
        gpu::Texture *dstP = (i & 1) ? s.posMap.get() : s.tmpPos.get(), *dstN = (i & 1) ? s.nrmMap.get() : s.tmpNrm.get();
        gpu::DispatchDesc k;
        k.pipeline = dilate;
        k.srv[0] = srcP;
        k.srv[1] = srcN;
        k.uav[0] = dstP;
        k.uav[1] = dstN;
        u32 size[2] = {W, H};
        memcpy(k.constants, size, 8);
        k.numConstants = 2;
        Dispatch2D(ctx, k, W, H);
    }
    s.bakedW = W;
    s.bakedH = H;
    return true;
}

gpu::Texture* Renderer::MeshRender(gpu::CommandContext& ctx, MeshScene& s, const MeshMaterial& material, u32 w, u32 h) {
    if (!s.vb || w == 0 || h == 0) return nullptr;
    if (!s.color || s.color->Width() != w || s.color->Height() != h) {
        s.color = gpu::Dev().CreateTexture(w, h, gpu::Format::RGBA8, "MeshView", gpu::UsageRenderTarget);
        s.depth = gpu::Dev().CreateTexture(w, h, gpu::Format::D32F, "MeshDepth");
    }
    gpu::GraphicsDesc gd;
    gd.colorFormats = {gpu::Format::RGBA8};
    auto* p = MeshPipeline("mesh_view.slang", gd);
    if (!p) return nullptr;
    s.UpdateCamera((float)w / h);
    ViewCB cb = {};
    memcpy(cb.vp, s.viewProj.m, sizeof(cb.vp));
    cb.camPos = {s.camPos.x, s.camPos.y, s.camPos.z, 1};
    float3 l = Normalize(s.camPos - s.target + float3{0.6f, 1.2f, 0.0f});
    cb.lightDir = {l.x, l.y, l.z, 0};
    cb.flags = 0;
    gpu::DrawDesc d;
    d.pipeline = p;
    for (int c = 0; c < (int)PbrChannel::Count; ++c) {
        d.srv[c] = material.tex[c];
        if (material.tex[c]) cb.flags |= 1u << c;
    }
    d.rt[0] = s.color.get();
    d.depth = s.depth.get();
    d.clearColor = d.clearDepth = true;
    d.clearValue = {0.09f, 0.095f, 0.11f, 1};
    d.cbData = &cb;
    d.cbSize = sizeof(cb);
    d.vertices = s.vb.get();
    d.indices = s.ib.get();
    d.count = (u32)s.data.indices.size();
    ctx.Draw(d);
    return s.color.get();
}

void Renderer::MeshStrokeDabs(Document& doc, gpu::CommandContext& ctx, MeshScene& s, const std::vector<Dab>& dabs, const BrushSettings& b,
                              float4 color, bool erase) {
    if (!strokeSurface || dabs.empty() || !s.posMap || !s.depth) {
        Log("MeshStrokeDabs skipped: stroke=%u dabs=%zu pos=%d depth=%d", strokeSurface, dabs.size(), (int)(bool)s.posMap, (int)(bool)s.depth);
        return;
    }
    Surface* sf = doc.GetSurface(strokeSurface);
    auto* k = Kernel("mesh_brush.slang");
    if (!sf || !k) { Log("MeshStrokeDabs: kernel unavailable"); return; }
    const u32 W = sf->tex->Width(), H = sf->tex->Height();
    Surface* sel = doc.Selection();
    struct PaintCB {
        float vp[16];
        float4 camPos, color;
        float4 dabs[1024];
    };
    static PaintCB cb;
    memcpy(cb.vp, s.viewProj.m, sizeof(cb.vp));
    cb.camPos = {s.camPos.x, s.camPos.y, s.camPos.z, 1};
    cb.color = color;
    for (size_t start = 0; start < dabs.size(); start += 1024) {
        size_t count = std::min<size_t>(1024, dabs.size() - start);
        for (size_t i = 0; i < count; ++i) cb.dabs[i] = {dabs[start + i].x, dabs[start + i].y, dabs[start + i].radius, dabs[start + i].alpha};
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = strokePre.get();
        d.srv[1] = s.posMap.get();
        d.srv[2] = s.nrmMap.get();
        d.srv[3] = s.depth.get();
        d.srv[4] = sel ? sel->tex.get() : nullptr;
        d.uav[0] = sf->tex.get();
        d.uav[1] = strokeMask.get();
        BrushRootM root = {W, H, s.depth->Width(), s.depth->Height(), (u32)count, b.hardness, b.opacity, erase ? 1u : 0u, sel ? 1u : 0u, {0, 0, 0}};
        memcpy(d.constants, &root, sizeof(root));
        d.numConstants = sizeof(root) / 4;
        d.cbData = &cb;
        d.cbSize = offsetof(PaintCB, dabs) + count * sizeof(float4);
        Dispatch2D(ctx, d, W, H);
    }
    strokeRect = {0, 0, (int)W, (int)H};
    doc.TouchSurface(strokeSurface);
}

}  // namespace sx
