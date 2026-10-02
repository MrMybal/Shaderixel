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

struct MeshScene;

struct ViewParams {
    float2 origin;          // document pixel at the top-left of the view
    float invZoom = 1;      // document pixels per view pixel
    u32 viewW = 1, viewH = 1;
    bool nearest = true;
    float exposure = 0;
    float4 outside = {0.11f, 0.11f, 0.12f, 1};
    float checker = 8;
    gpu::Texture* selection = nullptr;  // marching ants
    bool quickMask = false;
    float time = 0;
};

enum class SelMode : u8 { Replace, Add, Subtract, Intersect };
enum class SelShape : u8 { Rect, Ellipse, Coverage, All, None };
enum class SelSource : u8 { Alpha, Luminance, ColorRange };

// Projective transform state shared by free transform and the move tool.
struct TransformParams {
    float hinv[9];   // destination pixel -> source pixel
    Rect src;        // region that moves
    int interp = 2;  // 0 nearest, 1 bilinear, 2 bicubic
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

enum class RetouchMode : u8 { Clone, Heal, Blur, Sharpen, Dodge, Burn, Saturate, Desaturate };
struct RetouchParams {
    RetouchMode mode = RetouchMode::Clone;
    float strength = 0.5f;
    int2 srcOffset;
    int range = 1;  // dodge/burn: 0 shadows, 1 midtones, 2 highlights
};

class Renderer {
public:
    // Process queued surface clears/copies; call once per frame before evaluation.
    void Prepare(Document& doc, gpu::CommandContext& ctx);

    // Evaluate the graph; returns the final image (or nullptr if nothing renders yet).
    gpu::Texture* Evaluate(Document& doc, gpu::CommandContext& ctx);
    // Like Evaluate but never returns a partially rendered image (tiled mode
    // finishes all remaining tiles); use for export, sampling and selections.
    gpu::Texture* EvaluateComplete(Document& doc, gpu::CommandContext& ctx);
    gpu::Texture* NodeOutput(Id node) const;
    bool NodeFailed(Id node, std::string* errors = nullptr) const;
    bool AnyPending() const { return pending; }
    // Tiled mode (very large documents): remaining tiles to render, view rect for priority.
    int TilesPending() const { return tilesPending; }
    int TilesTotal() const { return tilesX * tilesY; }
    void SetVisibleRect(const Rect& r) { visibleRect = r; hasVisible = true; }

    void Display(gpu::CommandContext& ctx, gpu::Texture* image, u32 docW, u32 docH, const ViewParams& v, gpu::Texture* target);
    // Image to display for the evaluated document at this zoom: the image itself,
    // or a box-reduced copy (kept up to date incrementally) when strongly zoomed out.
    gpu::Texture* DisplaySource(gpu::CommandContext& ctx, gpu::Texture* image, float invZoom);
    // Small preview texture for a panel; regenerated when `hash` changes (throttled).
    gpu::Texture* Thumbnail(gpu::CommandContext& ctx, u64 key, gpu::Texture* src, u64 hash, u32 w, u32 h);
    u64 NodeHash(Id node) const;

    // Painting ---------------------------------------------------------
    bool BeginStroke(Document& doc, gpu::CommandContext& ctx, Id surface);
    void StrokeDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, float4 colorLinear, bool erase);
    void EndStroke(Document& doc, gpu::CommandContext& ctx, History& hist, const std::string& label);
    bool Stroking() const { return strokeSurface != 0; }
    // Retouch brushes (Retouch.cpp); call between BeginStroke/EndStroke.
    void RetouchDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, const RetouchParams& p);
    void SmudgeDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, float strength);

    void ApplyGradient(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, float2 a, float2 b,
                       int shape, float4 colorA, float4 colorB, float opacity, const std::string& label);

    void Import(Document& doc, gpu::CommandContext& ctx, Id surface, const void* pixels, u32 w, u32 h, bool isFloat, int2 offset);
    gpu::ReadbackPtr ExportRGBA8(gpu::CommandContext& ctx, gpu::Texture* image, bool srgb = true);
    // Full-resolution output of one node (through the node cache, also in tiled mode).
    gpu::Texture* EvaluateNode(Document& doc, gpu::CommandContext& ctx, Id node) { return Eval(doc, ctx, node, 0).tex; }
    // Release full-size node caches created by EvaluateNode in tiled mode.
    void DropNodeCaches(const Document& doc) { if (doc.tiled) cache.clear(); }
    // Async 4 x 256 histogram (luminance, R, G, B; u32, alpha-weighted x255).
    gpu::ReadbackPtr Histogram(gpu::CommandContext& ctx, gpu::Texture* image);
    u64 ImageVersion() const { return imageVersion; }
    gpu::ReadbackPtr ReadPixel(gpu::CommandContext& ctx, gpu::Texture* image, int x, int y);

    // Selections (Selection.cpp). Each operation creates a new selection surface
    // and records a history step, so selections undo like Photoshop's.
    bool SelectShape(Document& doc, gpu::CommandContext& ctx, History& hist, SelShape shape, float4 bounds, SelMode mode,
                     bool antialias, float feather, const std::string& label);
    bool SelectCoverage(Document& doc, gpu::CommandContext& ctx, History& hist, const std::vector<u8>& coverage, const Rect& r,
                        SelMode mode, float feather, const std::string& label);
    bool SelectFrom(Document& doc, gpu::CommandContext& ctx, History& hist, gpu::Texture* image, SelSource src, SelMode mode,
                    float4 colorSrgb, float tolerance, const std::string& label);
    void Deselect(Document& doc, History& hist);
    void Reselect(Document& doc, History& hist);
    bool SelectionFilter(Document& doc, gpu::CommandContext& ctx, History& hist, int op, float radius, const std::string& label);
    void SelectionToMask(Document& doc, gpu::CommandContext& ctx, Id maskSurface, bool invert);
    // Fill (mode 0), erase (1) or keep-only-selected (2) on a surface, honoring the selection.
    void FillSelection(Document& doc, gpu::CommandContext& ctx, History* hist, Id surface, int mode, float4 colorLinear,
                       float opacity, const std::string& label, gpu::Texture* source = nullptr, gpu::Texture* coverage = nullptr);
    // Async bounding box of visible pixels (optionally restricted to the selection).
    // Result: 4 x u32 {x0, y0, x1, y1}; x0 > x1 means empty.
    // dst = src * selection (or src when there is no selection).
    void CopySelected(Document& doc, gpu::CommandContext& ctx, gpu::Texture* src, gpu::Texture* dst);
    gpu::ReadbackPtr ComputeBounds(gpu::CommandContext& ctx, gpu::Texture* image, gpu::Texture* selection, bool imageIsMask, bool useRed = false);
    // Rebuild every surface at a new size: dst pixel -> src pixel = (m0 x + m1 y + m2, m3 x + m4 y + m5).
    bool ResampleCanvas(Document& doc, gpu::CommandContext& ctx, History& hist, u32 newW, u32 newH, const float m[6], int interp,
                        const std::string& label);
    // 3D painting (MeshPaint.cpp)
    bool MeshPrepare(Document& doc, gpu::CommandContext& ctx, MeshScene& s);
    gpu::Texture* MeshRender(gpu::CommandContext& ctx, MeshScene& s, gpu::Texture* albedo, u32 w, u32 h);
    void MeshStrokeDabs(Document& doc, gpu::CommandContext& ctx, MeshScene& s, const std::vector<Dab>& dabs, const BrushSettings& b,
                        float4 colorLinear, bool erase);
    void RenderShape(Document& doc, gpu::CommandContext& ctx, Id surface, const ShapeStyle& s);
    void TransformPreview(Document& doc, gpu::CommandContext& ctx, Id surface, gpu::Texture* original, gpu::Texture* selection,
                          const TransformParams& t);

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

    // Tiled evaluation (TiledEval.cpp)
    struct TileRes {
        gpu::Texture* tex = nullptr;
        int2 origin;  // document pixel of texel (0,0)
    };
    struct PoolEntry {
        gpu::TexturePtr tex;
        bool used = false;
    };
    static constexpr int kTileSize = 512;
    gpu::Texture* EvaluateTiled(Document& doc, gpu::CommandContext& ctx, const Rect* visible, int budget);
    TileRes EvalTile(Document& doc, gpu::CommandContext& ctx, Id id, const Rect& need, int depth);
    u64 StructHash(Document& doc, Id id, std::unordered_map<Id, u64>& memo, int& maxHalo, bool& compiling);
    gpu::Texture* PoolGet(u32 w, u32 h, gpu::Format f);
    void PoolReset(bool trim);
    gpu::TexturePtr tiledOut;
    std::vector<u8> tileDirty;
    int tilesX = 0, tilesY = 0, tilesPending = 0;
    u64 tiledStruct = 0;
    std::unordered_map<Id, u64> tiledVersions;
    std::unordered_map<u64, TileRes> tileMemo;
    std::vector<PoolEntry> pool;
    Rect visibleRect;
    bool hasVisible = false;
    bool wasTiled = false;
    // Change tracking of the evaluated image (display reduction).
    u64 imageVersion = 1, lastOutHash = 0;
    gpu::Texture* lastOut = nullptr;
    Rect imageDirty;
    gpu::TexturePtr reduced, histoTex;
    u32 reducedFactor = 0;
    u64 reducedVersion = 0;
    gpu::Texture* reducedFor = nullptr;
    void MarkImageChanged(const Rect& r) { imageDirty.Merge(r); ++imageVersion; }
    gpu::Texture* Ensure(gpu::TexturePtr& t, u32 w, u32 h, gpu::Format f, const char* name);
    gpu::ProgramPtr ProgramFor(const EffectDefPtr& def);

    std::unordered_map<Id, NodeCache> cache;
    std::unordered_map<Id, EvalResult> frameEval;
    std::unordered_map<Id, std::string> failed;
    std::unordered_map<u64, ThumbCache> thumbs;
    std::unordered_map<const EffectDef*, ProgramEntry> programs;
    u64 effectGeneration = ~0ull;
    bool pending = false;

    gpu::TexturePtr selTmp[2];
    gpu::TexturePtr boundsTex;
    Id strokeSurface = 0;
    gpu::TexturePtr strokePre, strokeMask;
    Rect strokeRect;
    gpu::TexturePtr smudgeSnap;
    float2 lastSmudge;
    bool haveSmudge = false;
};

float HalfToFloat(u16 h);

}  // namespace sx
