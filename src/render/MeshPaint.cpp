// Mesh viewport rendering, UV bake and projection painting.
#include "render/Renderer.h"
#include "render/MeshPaint.h"

#include <chrono>

#include <stb_image.h>
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

float3 MeshScene::KeyLightDir() const {
    return lightFollow ? Normalize(camPos - target + float3{0.6f, 1.2f, 0.0f})
                       : Normalize(float3{std::cos(lightPitch) * std::sin(lightYaw), std::sin(lightPitch), -std::cos(lightPitch) * std::cos(lightYaw)});
}

float3 MeshScene::LightDir(const MeshLight& l) const {
    // Same angles as the camera orbit: follow = offset from the camera's yaw / pitch.
    const float y = l.yaw + (l.follow ? yaw : 0.0f), p = std::clamp(l.pitch, -1.55f, 1.55f);
    return Normalize(float3{std::cos(p) * std::sin(y), std::sin(p), -std::cos(p) * std::cos(y)});
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
    float envIntensity, envRotation, viewMode;
    float exposure;
    u32 tonemap;
    float pad[2];
    float lightVP[16];
    float4 lightCol, shadow;
    u32 opts[4];
    float4 xDir[kMaxMeshLights], xCol[kMaxMeshLights];  // extra lights
};
struct BrushRootM {
    u32 dw, dh, vw, vh, count;
    float hardness, opacity;
    u32 mode, flags;
    float facingMin;
    float pad[2];
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

gpu::TexturePtr Renderer::BakeNormal(gpu::CommandContext& ctx, gpu::Texture* normal, gpu::Texture* height, u32 w, u32 h) {
    auto* k = Kernel("bake_normal.slang");
    if (!k) return nullptr;
    auto out = gpu::Dev().CreateTexture(w, h, gpu::Format::RGBA16F, "BakedNormal");
    struct { u32 w, h, flags; float strength; } root = {w, h, (normal ? 1u : 0u) | (height ? 2u : 0u), 32.0f};
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = normal;
    d.srv[1] = height;
    d.uav[0] = out.get();
    SetConstants(d, root);
    Dispatch2D(ctx, d, w, h);
    return out;
}

gpu::Texture* Renderer::MeshRender(gpu::CommandContext& ctx, MeshScene& s, const MeshMaterial& material, u32 w, u32 h) {
    if (!s.vb || w == 0 || h == 0) return nullptr;
    // Linear HDR target; the post-process (exposure, bloom, tone curve...) makes the display image.
    if (!s.color || s.color->Width() != w || s.color->Height() != h) {
        s.color = gpu::Dev().CreateTexture(w, h, gpu::Format::RGBA16F, "MeshView", gpu::UsageRenderTarget);
        s.depth = gpu::Dev().CreateTexture(w, h, gpu::Format::D32F, "MeshDepth");
    }
    gpu::GraphicsDesc gd;
    gd.colorFormats = {gpu::Format::RGBA16F};
    auto* p = MeshPipeline("mesh_view.slang", gd);
    if (!p) return nullptr;
    s.UpdateCamera((float)w / h);
    ViewCB cb = {};
    memcpy(cb.vp, s.viewProj.m, sizeof(cb.vp));
    cb.camPos = {s.camPos.x, s.camPos.y, s.camPos.z, 1};
    // Key light: attached to the camera, or fixed in the scene (yaw / pitch).
    float3 l = s.KeyLightDir();
    cb.lightDir = {l.x, l.y, l.z, 0};
    u32 nx = 0;
    for (const MeshLight& x : s.lights) {
        if (!x.on || x.intensity <= 0 || nx >= (u32)kMaxMeshLights) continue;
        const float3 d = s.LightDir(x), c = x.color * x.intensity;
        cb.xDir[nx] = {d.x, d.y, d.z, 0};
        cb.xCol[nx] = {c.x, c.y, c.z, 0};
        ++nx;
    }
    cb.opts[2] = nx;
    cb.flags = 0;
    if (!s.envPixels.empty()) {
        s.envAtlas = gpu::Dev().CreateTexture(s.envW, s.envH, gpu::Format::RGBA32F, "Environment");
        if (s.envAtlas) ctx.Upload(s.envAtlas.get(), s.envPixels.data(), s.envW * sizeof(float4));
        s.envPixels.clear();
    }
    if (s.envAtlas) cb.flags |= 128;
    cb.envIntensity = s.envIntensity;
    cb.envRotation = s.envRotation;
    cb.viewMode = (float)s.viewChannel;
    cb.exposure = 0;
    cb.tonemap = 0;
    const float li = s.envAtlas ? s.sunIntensity : s.lightIntensity;
    cb.lightCol = {1.0f * li, 0.97f * li, 0.92f * li, 0};
    cb.opts[0] = 4;  // HDR output
    const bool quality = s.quality >= 1 && s.viewChannel == 0;
    // Shadow map of the key light (orthographic, around the model).
    if (quality && li > 0 && s.shadowStrength > 0) {
        gpu::GraphicsDesc sd;
        sd.colorFormats = {};
        auto* sp = MeshPipeline("mesh_shadow.slang", sd);
        const u32 S = 2048;
        if (!s.shadowMap) s.shadowMap = gpu::Dev().CreateTexture(S, S, gpu::Format::D32F, "MeshShadow");
        if (sp && s.shadowMap) {
            const float3 c = (s.data.boundsMin + s.data.boundsMax) * 0.5f;
            const float r = std::max(Length(s.data.boundsMax - s.data.boundsMin) * 0.5f, 0.01f) * 1.05f;
            Mat4 view = Mat4::LookAtLH(c + l * (r * 3.0f), c, std::fabs(l.y) > 0.98f ? float3{0, 0, 1} : float3{0, 1, 0});
            Mat4 ortho = Mat4::Identity();
            const float zn = r * 1.0f, zf = r * 5.0f;
            ortho.m[0][0] = 1.0f / r;
            ortho.m[1][1] = 1.0f / r;
            ortho.m[2][2] = 1.0f / (zf - zn);
            ortho.m[2][3] = -zn / (zf - zn);
            Mat4 lvp = ortho * view;
            memcpy(cb.lightVP, lvp.m, sizeof(cb.lightVP));
            gpu::DrawDesc dd;
            dd.pipeline = sp;
            dd.depth = s.shadowMap.get();
            dd.clearDepth = true;
            dd.cbData = cb.lightVP;
            dd.cbSize = sizeof(cb.lightVP);
            dd.vertices = s.vb.get();
            dd.indices = s.ib.get();
            dd.count = (u32)s.data.indices.size();
            ctx.Draw(dd);
            cb.opts[0] |= 1;
            cb.shadow = {s.shadowStrength, 0.0015f, 1.0f / S, 0};
        }
    }
    if (quality && s.maps && s.aoStrength > 0) {
        cb.opts[0] |= 2;
        cb.opts[1] = (u32)std::lround(std::clamp(s.aoStrength, 0.0f, 1.0f) * 1000);
    }
    bool sky = false;
    if (s.envAtlas && s.envBackground && s.viewChannel == 0) {
        if (auto* k = Kernel("mesh_sky.slang")) {
            // Camera basis (left-handed look-at) scaled by the frustum half extents.
            float3 fwd = Normalize(s.target - s.camPos);
            float3 right = Normalize(Cross(float3{0, 1, 0}, fwd));
            float3 up = Cross(fwd, right);
            float ty = std::tan(s.fov * 0.5f), tx = ty * (float)w / h;
            struct { float4 right, up, fwd; float rotation; float pad[3]; } scb = {
                {right.x * tx, right.y * tx, right.z * tx, 0}, {up.x * ty, up.y * ty, up.z * ty, 0}, {fwd.x, fwd.y, fwd.z, 0}, s.envRotation, {}};
            struct { u32 w, h; float blur, intensity; u32 hdr; } root = {w, h, s.envBlur, s.envIntensity, 1};
            gpu::DispatchDesc sd;
            sd.pipeline = k;
            sd.srv[0] = s.envAtlas.get();
            sd.uav[0] = s.color.get();
            sd.cbData = &scb;
            sd.cbSize = sizeof(scb);
            SetConstants(sd, root);
            Dispatch2D(ctx, sd, w, h);
            sky = true;
        }
    }
    gpu::DrawDesc d;
    d.pipeline = p;
    for (int c = 0; c < (int)PbrChannel::Count; ++c) {
        d.srv[c] = material.tex[c];
        if (material.tex[c]) cb.flags |= 1u << c;
    }
    d.srv[7] = s.envAtlas.get();
    d.srv[8] = s.maps.get();
    d.srv[9] = (cb.opts[0] & 1) ? s.shadowMap.get() : nullptr;
    if (s.viewChannel >= 8) d.srv[5] = s.maps.get();  // baked maps shown in place of the occlusion slot
    d.rt[0] = s.color.get();
    d.depth = s.depth.get();
    d.clearColor = !sky;
    d.clearDepth = true;
    // Background: linear value of the former display gray.
    d.clearValue = {0, 0, 0, 0};  // alpha 0: background (post_final shows the usual gray)
    if (s.viewChannel != 0) d.clearValue = {0.09f, 0.095f, 0.11f, 1};
    d.cbData = &cb;
    d.cbSize = sizeof(cb);
    d.vertices = s.vb.get();
    d.indices = s.ib.get();
    d.count = (u32)s.data.indices.size();
    ctx.Draw(d);
    // Channel views are display values: copied as they are.
    static const auto t0 = std::chrono::steady_clock::now();
    const float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
    gpu::Texture* out = s.postChain.Run(ctx, s.color.get(), false, s.viewChannel != 0, s.post, time);
    return out ? out : s.color.get();
}

void Renderer::MeshStrokeDabs(Document& doc, gpu::CommandContext& ctx, MeshScene& s, const std::vector<Dab>& dabs, const BrushSettings& b,
                              float4 color, bool erase, u32 extraFlags, gpu::Texture* decal, const std::vector<float4>* dabEx) {
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
        float4 camPos, color, tip, stencil, stencil2, stencil3;
        float4 dabs[1024];
        float4 dabsEx[1024];
    };
    static PaintCB cb;
    memcpy(cb.vp, s.viewProj.m, sizeof(cb.vp));
    cb.camPos = {s.camPos.x, s.camPos.y, s.camPos.z, 1};
    cb.color = color;
    // Programmable tip: shapes from the atlas (not for decals).
    const bool useTip = meshTipOn && meshTipAtlas && !(extraFlags & (4u | 256u));
    if (useTip) extraFlags |= 32u;
    cb.tip = {meshTipExtent, (float)meshTipVariants, 0, 0};
    const bool useStencil = meshStencilOn && meshStencilTex && !(extraFlags & (4u | 256u));
    if (useStencil) {
        extraFlags |= 512u;
        cb.stencil = meshStencil;
        cb.stencil2 = meshStencil2;
        cb.stencil3 = meshStencil3;
        decal = meshStencilTex;
    }
    const u32 sym = meshSymmetry == 1 ? 16u : meshSymmetry == 2 ? 64u : meshSymmetry == 3 ? 128u : 0u;
    if (!(extraFlags & (4u | 256u))) extraFlags |= sym;
    for (size_t start = 0; start < dabs.size(); start += 1024) {
        size_t count = std::min<size_t>(1024, dabs.size() - start);
        for (size_t i = 0; i < count; ++i) {
            cb.dabs[i] = {dabs[start + i].x, dabs[start + i].y, dabs[start + i].radius, dabs[start + i].alpha};
            cb.dabsEx[i] = dabEx && start + i < dabEx->size() ? (*dabEx)[start + i] : float4{1, 0, 0, 0};
        }
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = strokePre.get();
        d.srv[1] = s.posMap.get();
        d.srv[2] = s.nrmMap.get();
        d.srv[3] = s.depth.get();
        d.srv[4] = sel ? sel->tex.get() : nullptr;
        d.srv[5] = decal;
        d.srv[6] = useTip ? meshTipAtlas.get() : nullptr;
        d.uav[0] = sf->tex.get();
        d.uav[1] = strokeMask.get();
        BrushRootM root = {W, H, s.depth->Width(), s.depth->Height(), (u32)count, b.hardness, b.opacity, erase ? 1u : 0u, (sel ? 1u : 0u) | extraFlags,
                           std::clamp(meshFacingMin, -0.5f, 0.95f), {0, 0}};
        memcpy(d.constants, &root, sizeof(root));
        d.numConstants = sizeof(root) / 4;
        d.cbData = &cb;
        d.cbSize = (useTip || (extraFlags & 256u)) ? offsetof(PaintCB, dabsEx) + count * sizeof(float4) : offsetof(PaintCB, dabs) + count * sizeof(float4);
        Dispatch2D(ctx, d, W, H);
    }
    strokeRect = {0, 0, (int)W, (int)H};
    doc.TouchSurface(strokeSurface);
}

bool Renderer::BeginStrokeMulti(Document& doc, gpu::CommandContext& ctx, const std::vector<Id>& surfaces) {
    strokeSlots.clear();
    if (surfaces.empty() || !BeginStroke(doc, ctx, surfaces[0])) return false;  // runs the pending clears of every layer
    for (size_t i = 1; i < surfaces.size(); ++i) {
        Surface* s = doc.GetSurface(surfaces[i]);
        if (!s) continue;
        StrokeSlot slot;
        slot.surface = surfaces[i];
        slot.pre = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "StrokePreSlot");
        ctx.CopyTexture(slot.pre.get(), s->tex.get());
        strokeSlots.push_back(std::move(slot));
    }
    return true;
}

void Renderer::MeshStrokeDabsMulti(Document& doc, gpu::CommandContext& ctx, MeshScene& s, const std::vector<Dab>& dabs, const BrushSettings& b,
                                   const std::vector<float4>& colors, bool erase, const std::vector<float4>* dabEx) {
    if (!strokeSurface) return;
    // First layer: accumulates the coverage; the others composite with it (symmetry is added by MeshStrokeDabs).
    const u32 sym = 0;
    MeshStrokeDabs(doc, ctx, s, dabs, b, colors.empty() ? float4{1, 1, 1, 1} : colors[0], erase, sym, nullptr, dabEx);
    for (size_t i = 0; i < strokeSlots.size(); ++i) {
        std::swap(strokeSurface, strokeSlots[i].surface);
        std::swap(strokePre, strokeSlots[i].pre);
        MeshStrokeDabs(doc, ctx, s, dabs, b, i + 1 < colors.size() ? colors[i + 1] : colors.back(), erase, 2u | sym, nullptr, dabEx);
        std::swap(strokeSurface, strokeSlots[i].surface);
        std::swap(strokePre, strokeSlots[i].pre);
    }
}

void Renderer::MeshWorldDabsMulti(Document& doc, gpu::CommandContext& ctx, MeshScene& s, const std::vector<float4>& dabs, const std::vector<float4>& ex,
                                  const BrushSettings& b, const std::vector<float4>& colors, bool erase) {
    if (!strokeSurface || dabs.empty()) return;
    // The dab array carries (x, y, z, radius); the extra one (alpha, normal).
    std::vector<Dab> d(dabs.size());
    for (size_t i = 0; i < dabs.size(); ++i) d[i] = {dabs[i].x, dabs[i].y, dabs[i].z, dabs[i].w};
    MeshStrokeDabs(doc, ctx, s, d, b, colors.empty() ? float4{1, 1, 1, 1} : colors[0], erase, 256u, nullptr, &ex);
    for (size_t i = 0; i < strokeSlots.size(); ++i) {
        std::swap(strokeSurface, strokeSlots[i].surface);
        std::swap(strokePre, strokeSlots[i].pre);
        MeshStrokeDabs(doc, ctx, s, d, b, i + 1 < colors.size() ? colors[i + 1] : colors.back(), erase, 256u | 2u, nullptr, &ex);
        std::swap(strokeSurface, strokeSlots[i].surface);
        std::swap(strokePre, strokeSlots[i].pre);
    }
}

void Renderer::MeshDecalMulti(Document& doc, gpu::CommandContext& ctx, MeshScene& s, gpu::Texture* image, float2 center, float2 halfSize,
                              float rotation, float opacity, const std::vector<float4>& colors, bool firstUsesImageColor) {
    if (!strokeSurface || !image) return;
    // The decal rides in the dab constants: [0] center + half size, [1] cos, sin, opacity.
    std::vector<Dab> d = {{center.x, center.y, halfSize.x, halfSize.y}, {std::cos(rotation), std::sin(rotation), opacity, 0}};
    BrushSettings b;
    b.hardness = 1;
    b.opacity = 1;
    MeshStrokeDabs(doc, ctx, s, d, b, colors.empty() ? float4{1, 1, 1, 1} : colors[0], false, 4u | (firstUsesImageColor ? 8u : 0u), image);
    for (size_t i = 0; i < strokeSlots.size(); ++i) {
        std::swap(strokeSurface, strokeSlots[i].surface);
        std::swap(strokePre, strokeSlots[i].pre);
        MeshStrokeDabs(doc, ctx, s, d, b, i + 1 < colors.size() ? colors[i + 1] : colors.back(), false, 4u | 2u, image);
        std::swap(strokeSurface, strokeSlots[i].surface);
        std::swap(strokePre, strokeSlots[i].pre);
    }
}

void Renderer::EndStrokeMulti(Document& doc, gpu::CommandContext& ctx, History& hist, const std::string& label) {
    hist.BeginGroup();
    for (auto& slot : strokeSlots) {
        std::swap(strokeSurface, slot.surface);
        std::swap(strokePre, slot.pre);
        Surface* s = doc.GetSurface(strokeSurface);
        if (s && !strokeRect.Empty()) {
            auto fmt = s->tex->GetFormat();
            auto before = gpu::Dev().CreateTexture(strokeRect.Width(), strokeRect.Height(), fmt, "UndoBefore");
            auto after = gpu::Dev().CreateTexture(strokeRect.Width(), strokeRect.Height(), fmt, "UndoAfter");
            if (before && after) {
                ctx.CopyRegion(before.get(), 0, 0, strokePre.get(), strokeRect);
                ctx.CopyRegion(after.get(), 0, 0, s->tex.get(), strokeRect);
                hist.PushPaint(label, strokeSurface, strokeRect, before, after);
            }
        }
        std::swap(strokeSurface, slot.surface);
        std::swap(strokePre, slot.pre);
    }
    EndStroke(doc, ctx, hist, label);
    hist.EndGroup();
    strokeSlots.clear();
}

bool Renderer::MeshGenerate(Document& doc, gpu::CommandContext& ctx, MeshScene& s, Id surface, const MeshGenParams& p) {
    Surface* sf = doc.GetSurface(surface);
    auto* k = Kernel("mesh_generator.slang");
    if (!sf || !k || !s.posMap || !s.nrmMap) return false;
    const u32 W = sf->tex->Width(), H = sf->tex->Height();
    if (s.posMap->Width() != W || s.posMap->Height() != H) return false;
    struct Root {
        u32 w, h, type, flags;
        float amount, contrast, noise, noiseScale, seed, blur, pad[2];
    } root = {W, H, (u32)p.type, (p.invert ? 1u : 0u) | (s.maps ? 2u : 0u), p.amount, p.contrast, p.noise, p.noiseScale, p.seed, 0, {0, 0}};
    struct Gen {
        float4 dir, bmin, bmax;
    } gen = {{p.dir.x, p.dir.y, p.dir.z, 0}, {s.data.boundsMin.x, s.data.boundsMin.y, s.data.boundsMin.z, 0}, {s.data.boundsMax.x, s.data.boundsMax.y, s.data.boundsMax.z, 0}};
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = s.posMap.get();
    d.srv[1] = s.nrmMap.get();
    d.srv[2] = s.maps.get();
    d.uav[0] = sf->tex.get();
    memcpy(d.constants, &root, sizeof(root));
    d.numConstants = sizeof(root) / 4;
    d.cbData = &gen;
    d.cbSize = sizeof(gen);
    Dispatch2D(ctx, d, W, H);
    doc.TouchSurface(surface);
    return true;
}

bool Renderer::MeshProject(Document& doc, gpu::CommandContext& ctx, MeshScene& s, Id surface, gpu::Texture* image, const MeshProjectParams& p) {
    Surface* sf = doc.GetSurface(surface);
    auto* k = Kernel("mesh_triplanar.slang");
    if (!sf || !k || !image || !s.posMap || !s.nrmMap) return false;
    const u32 W = sf->tex->Width(), H = sf->tex->Height();
    if (s.posMap->Width() != W || s.posMap->Height() != H) return false;
    struct Root {
        u32 w, h, mode;
        float scale, rotation, sharpness, lo, hi, ox, oy, contrast;
        u32 invert;
    } root = {W, H, (u32)p.mode, p.scale, p.rotation, p.sharpness, p.lo, p.hi, p.offsetX, p.offsetY, p.contrast, p.invert ? 1u : 0u};
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = s.posMap.get();
    d.srv[1] = s.nrmMap.get();
    d.srv[2] = image;
    d.uav[0] = sf->tex.get();
    SetConstants(d, root);
    Dispatch2D(ctx, d, W, H);
    doc.CancelPendingClear(surface);
    doc.TouchSurface(surface);
    return true;
}

bool LoadEnvironment(MeshScene& s, const std::string& path, std::string* error) {
    int w = 0, h = 0, comp = 0;
    FILE* f = nullptr;
    if (_wfopen_s(&f, Widen(path).c_str(), L"rb") != 0 || !f) { if (error) *error = "fichier introuvable"; return false; }
    float* src = stbi_loadf_from_file(f, &w, &h, &comp, 3);
    fclose(f);
    if (!src) { if (error) *error = "image illisible"; return false; }
    // Resample to 1024 x 512 (bilinear, wrapping horizontally), then box-downsample 5 times.
    const int W = 1024, H = 512, levels = 6;
    auto at = [&](int x, int y) {
        x = ((x % w) + w) % w;
        y = std::clamp(y, 0, h - 1);
        const float* p = &src[((size_t)y * w + x) * 3];
        return float4{p[0], p[1], p[2], 1};
    };
    std::vector<std::vector<float4>> lv(levels);
    lv[0].resize((size_t)W * H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float fx = (x + 0.5f) * w / W - 0.5f, fy = (y + 0.5f) * h / H - 0.5f;
            int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
            float tx = fx - x0, ty = fy - y0;
            float4 a = at(x0, y0), b = at(x0 + 1, y0), c = at(x0, y0 + 1), d = at(x0 + 1, y0 + 1);
            auto mix = [](float4 p, float4 q, float t) { return float4{p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t, p.z + (q.z - p.z) * t, 1}; };
            lv[0][(size_t)y * W + x] = mix(mix(a, b, tx), mix(c, d, tx), ty);
        }
    stbi_image_free(src);
    for (int k = 1; k < levels; ++k) {
        int pw = W >> (k - 1), ph = H >> (k - 1), cw = W >> k, ch = H >> k;
        lv[k].resize((size_t)cw * ch);
        // 2x2 box + a light 3-tap blur so rough levels stay smooth.
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x) {
                float4 sum = {};
                for (int dy = -1; dy <= 2; ++dy)
                    for (int dx = -1; dx <= 2; ++dx) {
                        int sx = ((2 * x + dx) % pw + pw) % pw, sy = std::clamp(2 * y + dy, 0, ph - 1);
                        float wgt = (dx == -1 || dx == 2 ? 0.5f : 1.0f) * (dy == -1 || dy == 2 ? 0.5f : 1.0f);
                        const float4& p = lv[k - 1][(size_t)sy * pw + sx];
                        sum.x += p.x * wgt; sum.y += p.y * wgt; sum.z += p.z * wgt;
                    }
                lv[k][(size_t)y * cw + x] = {sum.x / 9.0f, sum.y / 9.0f, sum.z / 9.0f, 1};
            }
    }
    s.envW = W;
    s.envH = 0;
    for (int k = 0; k < levels; ++k) s.envH += H >> k;
    s.envPixels.assign((size_t)s.envW * s.envH, float4{0, 0, 0, 1});
    u32 y0 = 0;
    for (int k = 0; k < levels; ++k) {
        int cw = W >> k, ch = H >> k;
        for (int y = 0; y < ch; ++y) memcpy(&s.envPixels[(size_t)(y0 + y) * W], &lv[k][(size_t)y * cw], cw * sizeof(float4));
        y0 += ch;
    }
    s.envName = Narrow(Utf8Path(path).filename().wstring());
    s.envPath = path;
    return true;
}

}  // namespace sx
