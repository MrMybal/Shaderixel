#pragma once
// Post-process chain (Marmoset-like camera effects), shared by the 3D view and the
// path-traced render: exposure, sharpen, chromatic aberration, white balance, bloom,
// tone curve, contrast, saturation, vignette, film grain.

#include "gpu/Device.h"
#include "core/Json.h"

namespace sx {

struct PostSettings {
    bool enabled = true;
    float exposure = 0;        // EV
    int tonemap = 1;           // 0 linear, 1 ACES, 2 Reinhard, 3 PBR Neutral, 4 AgX, 5 soft
    float bloom = 0.0f, bloomThreshold = 1.0f, bloomRadius = 0.65f;
    float sharpen = 0, chroma = 0;
    float temperature = 0, tint = 0;
    float contrast = 0, saturation = 1;
    float vignette = 0, vignetteSoft = 0.6f;
    float grain = 0;
};
Json PostToJson(const PostSettings& p);
PostSettings PostFromJson(const Json& j, PostSettings def = {});

class PostChain {
public:
    // src: linear HDR (accumulated: rgb sum, a = sample count); raw: display values copied as they are.
    // Returns the sRGB display image (RGBA8, owned by the chain).
    gpu::Texture* Run(gpu::CommandContext& ctx, gpu::Texture* src, bool accumulated, bool raw, const PostSettings& s, float time);

private:
    gpu::TexturePtr prep, out;
    std::vector<gpu::TexturePtr> mips, upCache;
};

}  // namespace sx
