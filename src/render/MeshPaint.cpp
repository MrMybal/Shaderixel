// Mesh viewport rendering, UV bake and projection painting.
#include "render/Renderer.h"
#include "render/MeshPaint.h"

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
    if (!s.envPixels.empty()) {
        s.envAtlas = gpu::Dev().CreateTexture(s.envW, s.envH, gpu::Format::RGBA32F, "Environment");
        if (s.envAtlas) ctx.Upload(s.envAtlas.get(), s.envPixels.data(), s.envW * sizeof(float4));
        s.envPixels.clear();
    }
    if (s.envAtlas) cb.flags |= 128;
    cb.envIntensity = s.envIntensity;
    cb.envRotation = s.envRotation;
    cb.viewMode = (float)s.viewChannel;
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
            struct { u32 w, h; float blur, intensity; } root = {w, h, s.envBlur, s.envIntensity};
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
        d.srv[7] = s.envAtlas.get();
        if (material.tex[c]) cb.flags |= 1u << c;
    }
    d.rt[0] = s.color.get();
    d.depth = s.depth.get();
    d.clearColor = !sky;
    d.clearDepth = true;
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
                              float4 color, bool erase, u32 extraFlags, gpu::Texture* decal) {
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
        d.srv[5] = decal;
        d.uav[0] = sf->tex.get();
        d.uav[1] = strokeMask.get();
        BrushRootM root = {W, H, s.depth->Width(), s.depth->Height(), (u32)count, b.hardness, b.opacity, erase ? 1u : 0u, (sel ? 1u : 0u) | extraFlags, {0, 0, 0}};
        memcpy(d.constants, &root, sizeof(root));
        d.numConstants = sizeof(root) / 4;
        d.cbData = &cb;
        d.cbSize = offsetof(PaintCB, dabs) + count * sizeof(float4);
        Dispatch2D(ctx, d, W, H);
    }
    strokeRect = {0, 0, (int)W, (int)H};
    doc.TouchSurface(strokeSurface);
}

bool Renderer::BeginStrokeMulti(Document& doc, gpu::CommandContext& ctx, const std::vector<Id>& surfaces) {
    strokeSlots.clear();
    if (surfaces.empty() || !BeginStroke(doc, ctx, surfaces[0])) return false;
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
                                   const std::vector<float4>& colors, bool erase) {
    if (!strokeSurface) return;
    // First layer: accumulates the coverage; the others composite with it.
    const u32 sym = meshSymmetryX ? 16u : 0u;
    MeshStrokeDabs(doc, ctx, s, dabs, b, colors.empty() ? float4{1, 1, 1, 1} : colors[0], erase, sym);
    for (size_t i = 0; i < strokeSlots.size(); ++i) {
        std::swap(strokeSurface, strokeSlots[i].surface);
        std::swap(strokePre, strokeSlots[i].pre);
        MeshStrokeDabs(doc, ctx, s, dabs, b, i + 1 < colors.size() ? colors[i + 1] : colors.back(), erase, 2u | sym);
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
    return true;
}

}  // namespace sx
