#pragma once
// GPU evaluation of the document graph + viewport/thumbnail presentation + painting.
//
// Every node output is cached in a GPU texture together with a content hash
// (node settings + input hashes + surface versions). Each frame the graph is
// walked; only nodes whose hash changed are re-dispatched, so idle frames cost
// nothing and painting only recomputes what sits downstream of the brush.

#include "doc/Document.h"
#include "doc/History.h"
#include "gpu/ShaderSystem.h"

#include <unordered_map>

namespace sx {

struct ViewParams {
    float2 origin;          // document pixel at the top-left of the view
    float invZoom = 1;      // document pixels per view pixel
    u32 viewW = 1, viewH = 1;
    bool nearest = true;
    float exposure = 0;
    float4 outside = {0.11f, 0.11f, 0.12f, 1};
    float checker = 8;
};

struct BrushSettings {
    float size = 40;        // diameter in pixels
    float hardness = 0.8f;
    float opacity = 1.0f;
    float flow = 1.0f;
    float spacing = 0.12f;  // fraction of diameter
    float smoothing = 0.25f;
    bool pressureSize = true;
    bool pressureOpacity = false;
};

struct Dab { float x, y, radius, alpha; };

class Renderer {
public:
    // Process queued surface clears/copies; call once per frame before evaluation.
    void Prepare(Document& doc, gpu::CommandContext& ctx);

    // Evaluate the graph; returns the final image (or nullptr if nothing renders yet).
    gpu::Texture* Evaluate(Document& doc, gpu::CommandContext& ctx);
    gpu::Texture* NodeOutput(Id node) const;
    bool NodeFailed(Id node, std::string* errors = nullptr) const;
    bool AnyPending() const { return pending; }

    void Display(gpu::CommandContext& ctx, gpu::Texture* image, u32 docW, u32 docH, const ViewParams& v, gpu::Texture* target);
    // Small preview texture for a panel; regenerated when `hash` changes (throttled).
    gpu::Texture* Thumbnail(gpu::CommandContext& ctx, u64 key, gpu::Texture* src, u64 hash, u32 w, u32 h);
    u64 NodeHash(Id node) const;

    // Painting ---------------------------------------------------------
    bool BeginStroke(Document& doc, gpu::CommandContext& ctx, Id surface);
    void StrokeDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, float4 colorLinear, bool erase);
    void EndStroke(Document& doc, gpu::CommandContext& ctx, History& hist, const std::string& label);
    bool Stroking() const { return strokeSurface != 0; }

    void ApplyGradient(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, float2 a, float2 b,
                       int shape, float4 colorA, float4 colorB, float opacity, const std::string& label);

    void Import(Document& doc, gpu::CommandContext& ctx, Id surface, const void* pixels, u32 w, u32 h, bool isFloat, int2 offset);
    gpu::ReadbackPtr ExportRGBA8(gpu::CommandContext& ctx, gpu::Texture* image);
    gpu::ReadbackPtr ReadPixel(gpu::CommandContext& ctx, gpu::Texture* image, int x, int y);

    void Reset();  // drop all caches (document switch)

private:
    struct NodeCache {
        gpu::TexturePtr tex, tmp;
        u64 hash = 0;
    };
    struct EvalResult {
        gpu::Texture* tex = nullptr;
        u64 hash = 0;
    };
    struct ThumbCache {
        gpu::TexturePtr tex;
        u64 hash = 0;
        u64 frame = 0;
    };
    struct ProgramEntry {
        EffectDefPtr def;
        gpu::ProgramPtr program;
    };

    EvalResult Eval(Document& doc, gpu::CommandContext& ctx, Id id, int depth);
    gpu::Texture* Ensure(gpu::TexturePtr& t, u32 w, u32 h, gpu::Format f, const char* name);
    gpu::ProgramPtr ProgramFor(const EffectDefPtr& def);

    std::unordered_map<Id, NodeCache> cache;
    std::unordered_map<Id, EvalResult> frameEval;
    std::unordered_map<Id, std::string> failed;
    std::unordered_map<u64, ThumbCache> thumbs;
    std::unordered_map<const EffectDef*, ProgramEntry> programs;
    u64 effectGeneration = ~0ull;
    bool pending = false;

    Id strokeSurface = 0;
    gpu::TexturePtr strokePre, strokeMask;
    Rect strokeRect;
};

float HalfToFloat(u16 h);

}  // namespace sx
