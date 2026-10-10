// Post-process chain (see PostProcess.h): post_prep, post_bloom (down / up), post_final.
#include "render/PostProcess.h"
#include "render/Dispatch.h"

#include <algorithm>

namespace sx {

Json PostToJson(const PostSettings& p) {
    Json j = Json::Object();
    j["enabled"] = p.enabled;
    j["exposure"] = p.exposure;
    j["tonemap"] = p.tonemap;
    j["bloom"] = p.bloom;
    j["bloomThreshold"] = p.bloomThreshold;
    j["bloomRadius"] = p.bloomRadius;
    j["sharpen"] = p.sharpen;
    j["chroma"] = p.chroma;
    j["temperature"] = p.temperature;
    j["tint"] = p.tint;
    j["contrast"] = p.contrast;
    j["saturation"] = p.saturation;
    j["vignette"] = p.vignette;
    j["vignetteSoft"] = p.vignetteSoft;
    j["grain"] = p.grain;
    return j;
}

PostSettings PostFromJson(const Json& j, PostSettings d) {
    if (!j.IsObject()) return d;
    d.enabled = j["enabled"].B(d.enabled);
    d.exposure = j["exposure"].F(d.exposure);
    d.tonemap = j["tonemap"].I(d.tonemap);
    d.bloom = j["bloom"].F(d.bloom);
    d.bloomThreshold = j["bloomThreshold"].F(d.bloomThreshold);
    d.bloomRadius = j["bloomRadius"].F(d.bloomRadius);
    d.sharpen = j["sharpen"].F(d.sharpen);
    d.chroma = j["chroma"].F(d.chroma);
    d.temperature = j["temperature"].F(d.temperature);
    d.tint = j["tint"].F(d.tint);
    d.contrast = j["contrast"].F(d.contrast);
    d.saturation = j["saturation"].F(d.saturation);
    d.vignette = j["vignette"].F(d.vignette);
    d.vignetteSoft = j["vignetteSoft"].F(d.vignetteSoft);
    d.grain = j["grain"].F(d.grain);
    return d;
}

gpu::Texture* PostChain::Denoise(gpu::CommandContext& ctx, gpu::Texture* accum, int samples, float strength, gpu::Texture* depth) {
    auto* k = Kernel("pt_denoise.slang");
    if (!k || !accum || strength <= 0) return nullptr;
    const u32 W = accum->Width(), H = accum->Height();
    for (auto& t : dn)
        if (!t || t->Width() != W || t->Height() != H) t = gpu::Dev().CreateTexture(W, H, gpu::Format::RGBA16F, "PtDenoise");
    // Expected relative noise of a path-traced pixel ~ 1 / sqrt(samples).
    const float sigma = std::max(strength * 1.6f / std::sqrt((float)std::max(samples, 1)), 1e-3f);
    if (depth && (depth->Width() != W || depth->Height() != H)) depth = nullptr;
    gpu::Texture* src = accum;
    const u32 steps[] = {1, 2, 4};
    for (int i = 0; i < 3; ++i) {
        gpu::Texture* dst = dn[i & 1].get();
        struct { u32 w, h, step, first; float sigma, mix; u32 last, hasDepth; } root = {W, H, steps[i], i == 0 ? 1u : 0u, sigma, 0.0f, i == 2 ? 1u : 0u, depth ? 1u : 0u};
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = src;
        d.srv[1] = accum;
        d.srv[2] = depth;
        d.uav[0] = dst;
        SetConstants(d, root);
        Dispatch2D(ctx, d, W, H);
        src = dst;
    }
    return src;
}

gpu::Texture* PostChain::Run(gpu::CommandContext& ctx, gpu::Texture* src, bool accumulated, bool raw, const PostSettings& sIn, float time) {
    auto* kPrep = Kernel("post_prep.slang");
    auto* kBloom = Kernel("post_bloom.slang");
    auto* kFinal = Kernel("post_final.slang");
    if (!src || !kPrep || !kBloom || !kFinal) return nullptr;
    const u32 W = src->Width(), H = src->Height();
    auto ensure = [](gpu::TexturePtr& t, u32 w, u32 h, gpu::Format f, const char* name) {
        if (!t || t->Width() != w || t->Height() != h) t = gpu::Dev().CreateTexture(w, h, f, name);
    };
    ensure(prep, W, H, gpu::Format::RGBA16F, "PostPrep");
    ensure(out, W, H, gpu::Format::RGBA8, "PostOut");
    PostSettings s = sIn;
    if (!s.enabled) {
        // Off: exposure and tone curve only.
        PostSettings off;
        off.exposure = s.exposure;
        off.tonemap = s.tonemap;
        s = off;
    }
    {
        struct { u32 w, h, flags; float exposure, sharpen, chroma, temperature, tint; } root = {
            W, H, (accumulated ? 1u : 0u) | (raw ? 4u : 0u), std::exp2(s.exposure), s.sharpen, s.chroma * std::min(W, H) * 0.01f, s.temperature, s.tint};
        gpu::DispatchDesc d;
        d.pipeline = kPrep;
        d.srv[0] = src;
        d.uav[0] = prep.get();
        SetConstants(d, root);
        Dispatch2D(ctx, d, W, H);
    }
    // Bloom: half size down to ~16 px, then back up, each level adding the smaller one.
    const bool bloom = !raw && s.bloom > 0.0f;
    if (bloom) {
        int levels = 0;
        for (u32 w = W / 2, h = H / 2; w >= 8 && h >= 8 && levels < 7; w /= 2, h /= 2) ++levels;
        if (mips.size() != (size_t)levels) mips.assign(levels, nullptr);
        if (upCache.size() < (size_t)levels) upCache.resize(levels);  // targets of the upsample pass
        u32 w = W, h = H;
        for (int i = 0; i < levels; ++i) {
            w = std::max(1u, w / 2), h = std::max(1u, h / 2);
            ensure(mips[i], w, h, gpu::Format::RGBA16F, "PostBloom");
            ensure(upCache[i], w, h, gpu::Format::RGBA16F, "PostBloomUp");
            struct { u32 w, h, mode, first; float threshold, knee, radius, pad; } root = {w, h, 0, i == 0 ? 1u : 0u, s.bloomThreshold, s.bloomThreshold * 0.5f + 1e-3f, 0, 0};
            gpu::DispatchDesc d;
            d.pipeline = kBloom;
            d.srv[0] = i == 0 ? prep.get() : mips[i - 1].get();
            d.uav[0] = mips[i].get();
            SetConstants(d, root);
            Dispatch2D(ctx, d, w, h);
        }
        // Up: level i receives level i + 1 (already accumulated), the result in upCache[i].
        gpu::Texture* smaller = mips[levels - 1].get();
        for (int i = levels - 2; i >= 0; --i) {
            const u32 lw = mips[i]->Width(), lh = mips[i]->Height();
            struct { u32 w, h, mode, first; float threshold, knee, radius, pad; } root = {lw, lh, 1, 0, 0, 0, s.bloomRadius, 0};
            gpu::DispatchDesc d;
            d.pipeline = kBloom;
            d.srv[0] = smaller;
            d.srv[1] = mips[i].get();
            d.uav[0] = upCache[i].get();
            SetConstants(d, root);
            Dispatch2D(ctx, d, lw, lh);
            smaller = upCache[i].get();
        }
        {
            struct { u32 w, h, flags, tonemap; float bloom, contrast, saturation, vignette, soft, grain, time, pad; } root = {
                W, H, 2u, (u32)s.tonemap, s.bloom * 0.25f, s.contrast, s.saturation, s.vignette, s.vignetteSoft, s.grain, time, 0};
            gpu::DispatchDesc d;
            d.pipeline = kFinal;
            d.srv[0] = prep.get();
            d.srv[1] = levels > 1 ? upCache[0].get() : mips[0].get();
            d.uav[0] = out.get();
            SetConstants(d, root);
            Dispatch2D(ctx, d, W, H);
        }
        return out.get();
    }
    struct { u32 w, h, flags, tonemap; float bloom, contrast, saturation, vignette, soft, grain, time, pad; } root = {
        W, H, raw ? 4u : 0u, (u32)s.tonemap, 0, s.contrast, s.saturation, s.vignette, s.vignetteSoft, s.grain, time, 0};
    gpu::DispatchDesc d;
    d.pipeline = kFinal;
    d.srv[0] = prep.get();
    d.uav[0] = out.get();
    SetConstants(d, root);
    Dispatch2D(ctx, d, W, H);
    return out.get();
}

}  // namespace sx
