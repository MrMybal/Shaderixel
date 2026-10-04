// GPU side of the path tracer: scene packing into float textures, the progressive
// kernel (shaders/pathtrace.slang), channel packing and tone mapping.
#include "render/PathTracer.h"
#include "render/Dispatch.h"

#include <algorithm>
#include <cmath>

namespace sx {

namespace {

constexpr u32 kGeoWidth = 4096;
constexpr int kMaxMaterials = 32, kMaxLights = 16;

struct SceneCB {
    float4 camPos, camRight, camUp, camFwd;
    u32 maxBounces, numLights, numMaterials, flags;
    u32 nodeBase, envW, envH, pad0;
    float4 background, envInfo;
    float4 mat[kMaxMaterials * 4];
    float4 light[kMaxLights * 4];
};

float AsFloat(u32 v) {
    float f;
    memcpy(&f, &v, 4);
    return f;
}

}  // namespace

PtCamera PtRenderState::Camera() const {
    PtCamera c;
    float p = std::clamp(pitch, -1.5f, 1.5f);
    c.target = target;
    c.position = target + float3{std::cos(p) * std::sin(yaw), std::sin(p), -std::cos(p) * std::cos(yaw)} * distance;
    return c;
}

void PtGpuRenderer::SetScene(std::shared_ptr<const PtScene> s) {
    scene = std::move(s);
    geoDirty = true;
    Reset();
}

void PtGpuRenderer::EnsureAccum(u32 w, u32 h) {
    if (accum && accum->Width() == w && accum->Height() == h) return;
    accum = gpu::Dev().CreateTexture(w, h, gpu::Format::RGBA32F, "PtAccum");
    view = gpu::Dev().CreateTexture(w, h, gpu::Format::RGBA8, "PtView");
    Reset();
}

void PtGpuRenderer::BeginAtlas(u32 size) {
    for (auto& t : packed)
        if (!t || t->Width() != size || t->Height() != size) t = gpu::Dev().CreateTexture(size, size, gpu::Format::RGBA16F, "PtAtlas");
    Reset();
}

void PtGpuRenderer::PackTile(gpu::CommandContext& ctx, gpu::Texture* const* channels, u32 x, u32 y, u32 tile, float defRough, float defMetal) {
    auto* k = Kernel("pt_pack.slang");
    if (!k || tile == 0 || !packed[0]) return;
    gpu::DispatchDesc d;
    d.pipeline = k;
    u32 flags = 0;
    for (int c = 0; c < 7; ++c) {
        d.srv[c] = channels[c];
        if (channels[c]) flags |= 1u << c;
    }
    for (int i = 0; i < 4; ++i) d.uav[i] = packed[i].get();
    struct { u32 x, y, tile, flags; float strength, defRough, defMetal, pad; } root = {x, y, tile, flags, 32.0f, defRough, defMetal, 0};
    SetConstants(d, root);
    Dispatch2D(ctx, d, tile, tile);
    Reset();
}

bool PtGpuRenderer::Render(gpu::CommandContext& ctx, const std::vector<PtMaterial>& materials, const std::vector<PtLight>& lights, const PtSettings& s,
                           const PtCamera& cam, u32 w, u32 h, u32 rows) {
    auto* k = Kernel("pathtrace.slang");
    if (!k || !scene || w == 0 || h == 0) return false;
    EnsureAccum(w, h);
    if (geoDirty) {
        // Triangles (8 texels each), then the BVH nodes (2 texels each).
        const size_t triTexels = scene->triangles.size() * 8, total = triTexels + scene->nodes.size() * 2;
        const u32 geoH = (u32)std::max<size_t>(1, (total + kGeoWidth - 1) / kGeoWidth);
        std::vector<float4> px((size_t)kGeoWidth * geoH, float4{0, 0, 0, 0});
        for (size_t t = 0; t < scene->triangles.size(); ++t) {
            const PtTriangle& tr = scene->triangles[t];
            float4* o = &px[t * 8];
            o[0] = {tr.p[0].x, tr.p[0].y, tr.p[0].z, (float)tr.material};
            o[1] = {tr.p[1].x, tr.p[1].y, tr.p[1].z, tr.uv[0].x};
            o[2] = {tr.p[2].x, tr.p[2].y, tr.p[2].z, tr.uv[0].y};
            o[3] = {tr.n[0].x, tr.n[0].y, tr.n[0].z, tr.uv[1].x};
            o[4] = {tr.n[1].x, tr.n[1].y, tr.n[1].z, tr.uv[1].y};
            o[5] = {tr.n[2].x, tr.n[2].y, tr.n[2].z, tr.uv[2].x};
            o[6] = {tr.uv[2].y, 0, 0, 0};
        }
        nodeBase = (u32)triTexels;
        for (size_t n = 0; n < scene->nodes.size(); ++n) {
            const PtNode& nd = scene->nodes[n];
            px[triTexels + n * 2] = {nd.bmin.x, nd.bmin.y, nd.bmin.z, AsFloat(nd.leftFirst)};
            px[triTexels + n * 2 + 1] = {nd.bmax.x, nd.bmax.y, nd.bmax.z, AsFloat(nd.count)};
        }
        geo = gpu::Dev().CreateTexture(kGeoWidth, geoH, gpu::Format::RGBA32F, "PtGeometry");
        if (geo) ctx.Upload(geo.get(), px.data(), kGeoWidth * sizeof(float4));
        envTex.reset();
        envCdf.reset();
        if (scene->env && scene->env->width > 0) {
            const PtEnvironment& e = *scene->env;
            envTex = gpu::Dev().CreateTexture(e.width, e.height, gpu::Format::RGBA32F, "PtEnv");
            if (envTex) ctx.Upload(envTex.get(), e.pixels.data(), e.width * sizeof(float4));
            std::vector<float> cdf((size_t)e.width * (e.height + 1), 1.0f);
            memcpy(cdf.data(), e.conditional.data(), e.conditional.size() * sizeof(float));
            for (int y = 0; y < e.height; ++y) cdf[(size_t)e.height * e.width + y] = e.marginal[y];
            envCdf = gpu::Dev().CreateTexture(e.width, e.height + 1, gpu::Format::R32F, "PtEnvCdf");
            if (envCdf) ctx.Upload(envCdf.get(), cdf.data(), e.width * sizeof(float));
        }
        geoDirty = false;
    }
    if (row >= h) row = 0;
    rows = std::clamp(rows, 1u, h - row);

    static SceneCB cb;
    cb = {};
    float3 fwd = Normalize(cam.target - cam.position);
    float3 right = Normalize(Cross(cam.up, fwd)), up = Cross(fwd, right);
    float ty = std::tan(s.fov * 0.5f), tx = ty * (float)w / h;
    cb.camPos = {cam.position.x, cam.position.y, cam.position.z, s.aperture};
    cb.camRight = {right.x * tx, right.y * tx, right.z * tx, s.focusDistance};
    cb.camUp = {up.x * ty, up.y * ty, up.z * ty, 0};
    cb.camFwd = {fwd.x, fwd.y, fwd.z, s.envRotation / 360.0f};
    cb.maxBounces = (u32)std::max(s.maxBounces, 0);
    cb.numMaterials = (u32)std::min<size_t>(materials.size(), kMaxMaterials);
    for (u32 i = 0; i < cb.numMaterials; ++i) {
        const PtMaterial& m = materials[i];
        float3 e = m.emission * m.emissionStrength;
        cb.mat[i * 4] = {m.baseColor.x, m.baseColor.y, m.baseColor.z, m.roughness};
        cb.mat[i * 4 + 1] = {e.x, e.y, e.z, m.metallic};
        cb.mat[i * 4 + 2] = {m.transmission, m.ior, m.useProject ? 1.0f : 0.0f, 0};
        cb.mat[i * 4 + 3] = {m.coat, m.coatRoughness, 0, 0};
    }
    u32 nl = 0;
    for (const PtLight& l : lights) {
        if (!l.visible || nl >= (u32)kMaxLights) continue;
        float3 dir = l.Direction(), rad = l.color * l.intensity;
        float outer = std::cos(l.spotAngle * 0.5f * 3.14159265f / 180), inner = std::cos(l.spotAngle * 0.4f * 3.14159265f / 180);
        cb.light[nl * 4] = {l.position.x, l.position.y, l.position.z, (float)(int)l.type};
        cb.light[nl * 4 + 1] = {dir.x, dir.y, dir.z, l.size};
        cb.light[nl * 4 + 2] = {rad.x, rad.y, rad.z, outer};
        cb.light[nl * 4 + 3] = {inner, 0, 0, 0};
        ++nl;
    }
    cb.numLights = nl;
    const bool hasEnv = envTex && envCdf;
    cb.flags = (hasEnv ? 1u : 0u) | (s.envVisible ? 2u : 0u) | (packed[0] ? 4u : 0u) | (!scene->nodes.empty() ? 8u : 0u);
    cb.nodeBase = nodeBase;
    cb.envW = hasEnv ? scene->env->width : 0;
    cb.envH = hasEnv ? scene->env->height : 0;
    cb.background = {s.background.x, s.background.y, s.background.z, s.envIntensity};
    cb.envInfo = {hasEnv ? scene->env->total : 0.0f, s.clampIndirect, 0, 0};

    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = geo.get();
    d.srv[1] = envTex.get();
    d.srv[2] = envCdf.get();
    for (int i = 0; i < 4; ++i) d.srv[3 + i] = packed[i].get();
    d.uav[0] = accum.get();
    d.cbData = &cb;
    d.cbSize = sizeof(cb);
    struct { u32 w, h, rowStart, pass, seed; } root = {w, h, row, (u32)samples, (u32)samples * 7919u + 17u};
    SetConstants(d, root);
    d.threadsX = w;
    d.threadsY = rows;
    ctx.Dispatch(d);
    row += rows;
    if (row >= h) {
        row = 0;
        ++samples;
    }
    return true;
}

void PtGpuRenderer::UploadAverage(gpu::CommandContext& ctx, const std::vector<float4>& pixels, u32 w, u32 h) {
    if (pixels.size() != (size_t)w * h) return;
    EnsureAccum(w, h);
    if (accum) ctx.Upload(accum.get(), pixels.data(), w * sizeof(float4));
}

gpu::Texture* PtGpuRenderer::Tonemap(gpu::CommandContext& ctx, const PtSettings& s) {
    auto* k = Kernel("pt_tonemap.slang");
    if (!k || !accum || !view) return nullptr;
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = accum.get();
    d.uav[0] = view.get();
    struct { u32 w, h; float exposure; u32 tonemap; } root = {accum->Width(), accum->Height(), std::exp2(s.exposure), (u32)s.tonemap};
    SetConstants(d, root);
    Dispatch2D(ctx, d, accum->Width(), accum->Height());
    return view.get();
}

}  // namespace sx
