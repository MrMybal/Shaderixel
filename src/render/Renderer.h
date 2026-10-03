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

#include <array>
#include <unordered_map>

namespace sx {

struct MeshScene;
struct MeshMaterial;

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
    gpu::Texture* lut = nullptr;  // display / proof 3D LUT (n*n x n, alpha = in gamut)
    u32 lutSize = 0;
    bool gamutWarning = false;
};

enum class SelMode : u8 { Replace, Add, Subtract, Intersect };
enum class SelShape : u8 { Rect, Ellipse, Coverage, All, None };
enum class SelSource : u8 { Alpha, Luminance, ColorRange, Mask };  // Mask: red channel of a mask texture

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
    // Programmable tip (shaders/brushes): library id, or edited code (overrides the id).
    std::string tipId, tipCode;
    std::string tipGraph;  // visual brush graph (JSON), compiled to tip code
    std::vector<float4> tipParams;
    std::vector<std::string> tipTextures;  // images chosen for the tip's //@texture slots ("" = default)
    std::array<gpu::Texture*, 4> tipTexBound{};  // bound for the next stroke / preview (runtime)
};

struct Dab { float x, y, radius, alpha; };

enum class RetouchMode : u8 { Clone, Heal, Blur, Sharpen, Dodge, Burn, Saturate, Desaturate, History, BackgroundErase, RedEye };
struct RetouchParams {
    RetouchMode mode = RetouchMode::Clone;
    float strength = 0.5f;
    int2 srcOffset;
    int range = 1;  // dodge/burn: 0 shadows, 1 midtones, 2 highlights
    gpu::Texture* source = nullptr;  // history brush: snapshot of the layer
    float tolerance = 0.25f;         // background eraser
};

// Point of a bicubic Bezier patch (4x4 control points, row-major) at (u, v).
float2 BezierPatch(const float2 grid[16], float u, float v);
// Deformation mesh: (nx + 1) x (ny + 1) vertices (row-major), each with its destination
// and the source pixel it comes from (document pixels).
struct WarpMesh {
    int nx = 0, ny = 0;
    std::vector<float2> dst, src;
};
WarpMesh BezierWarpMesh(const float2 grid[16], const Rect& src, int n = 48);

struct BrushDef;

class Renderer {
public:
    // Programmable brushes (doc/Brushes.h): dabs + per-dab extras: one float4 per dab (pressure, direction angle,
    // time, distance), or two interleaved per dab, the second being the pen (tilt x, tilt y, rotation, speed).
    struct BrushTargets {
        gpu::Texture* pre;
        gpu::Texture* sel;
        gpu::Texture* layer;
        gpu::Texture* mask;
        gpu::Texture* color;
        u32 w, h;
        bool preview;
    };
    const gpu::ComputePipeline* BrushPipeline(const BrushDef& b, std::string* errors = nullptr);
    bool StrokeProgram(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const std::vector<float4>& extra, const BrushDef& b,
                       const EffectInstance& tip, const BrushSettings& s, float4 color, bool erase, u32 dabBase);
    // Sample stroke of a brush (S curve with a pressure ramp) for the brush panel.
    gpu::Texture* BrushPreview(gpu::CommandContext& ctx, const BrushDef& b, const EffectInstance& tip, const BrushSettings& s, float4 color,
                               u32 w, u32 h, std::string* errors = nullptr);
    // Test pad of the brush editor: a small canvas painted with the brush (strokes composited like on a layer).
    gpu::Texture* PadTexture() const { return padLayer.get(); }
    void PadClear(gpu::CommandContext& ctx, u32 w, u32 h);
    void PadBeginStroke(gpu::CommandContext& ctx);
    bool PadStroke(gpu::CommandContext& ctx, const BrushDef& b, const EffectInstance& tip, const BrushSettings& s, float4 color,
                   const std::vector<Dab>& dabs, const std::vector<float4>& extra, bool erase, u32 dabBase);
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

    // Returns false when nothing was drawn (display kernel still compiling).
    bool Display(gpu::CommandContext& ctx, gpu::Texture* image, u32 docW, u32 docH, const ViewParams& v, gpu::Texture* target);
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
    // Liquify brush (mode: 0 push, 1 twirl cw, 2 twirl ccw, 3 pucker, 4 bloat, 5 reconstruct).
    void LiquifyDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, int mode, float strength);

    void ApplyGradient(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, float2 a, float2 b,
                       int shape, float4 colorA, float4 colorB, float opacity, const std::string& label);

    // Off-graph helpers (layer merging): composite `top` over `base` into `dst`
    // like a Blend node; apply an effect to `in` (nullptr while compiling); fill.
    void Composite(Document& doc, gpu::CommandContext& ctx, gpu::Texture* dst, gpu::Texture* base, gpu::Texture* top, gpu::Texture* mask,
                   BlendMode mode, float opacity, bool clip, const BlendIf* blendIf);
    // `refW/refH`: reference size the effect sees as the document (0 = the document's).
    gpu::TexturePtr ApplyEffect(Document& doc, gpu::CommandContext& ctx, const EffectInstance& fx, gpu::Texture* in, u32 refW = 0, u32 refH = 0);
    // Several inputs (blends: A, B, mask); output sized like the first input.
    gpu::TexturePtr ApplyEffectInputs(Document& doc, gpu::CommandContext& ctx, const EffectInstance& fx, std::array<gpu::Texture*, 4> in, u32 refW = 0, u32 refH = 0);
    // Small copy of `src` fitted into w x h (aspect kept, centered), for previews.
    gpu::TexturePtr PreviewCopy(gpu::CommandContext& ctx, gpu::Texture* src, u32 w, u32 h);
    // Destructive filter on a pixel surface (selection-limited), recorded in the history.
    bool ApplyEffectToSurface(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, const EffectInstance& fx, const std::string& label);
    void FillTexture(gpu::CommandContext& ctx, gpu::Texture* dst, float4 value);

    void Import(Document& doc, gpu::CommandContext& ctx, Id surface, const void* pixels, u32 w, u32 h, bool isFloat, int2 offset);
    // Selection masks (SelectRefine.cpp).
    struct RefineParams {
        float radius = 8, smooth = 0, feather = 0, contrast = 0, shift = 0, eps = 0.003f;
        bool smartRadius = true;
    };
    gpu::Texture* ColorRangeMask(gpu::CommandContext& ctx, gpu::Texture* image, const std::vector<float4>& samplesLinear, int preset, float fuzz, bool invert);
    gpu::Texture* RefineMask(gpu::CommandContext& ctx, gpu::Texture* image, gpu::Texture* selection, const RefineParams& p);
    gpu::Texture* RefinePreview(gpu::CommandContext& ctx, gpu::Texture* image, gpu::Texture* mask, int view);
    bool WriteMaskSurface(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, gpu::Texture* mask, const std::string& label);
    // Region healing / fill paste (HealFill.cpp): membrane blend of `source` (null: the surface) shifted by
    // `offset` into the region `mask`; paste of a computed crop through the mask. Undoable.
    bool HealRegion(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, gpu::Texture* source, gpu::Texture* mask, int2 offset,
                    int radius, const std::string& label);
    bool PasteMasked(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, gpu::Texture* crop, int2 origin, gpu::Texture* mask,
                     const std::string& label);
    void BoxBlurInPlace(gpu::CommandContext& ctx, gpu::Texture* t, gpu::Texture* tmp, int radius, int passes);
    // Smart objects: an image as a new texture (linear premultiplied), and its placement on a layer surface.
    gpu::TexturePtr ImportTexture(gpu::CommandContext& ctx, const void* pixels, u32 w, u32 h, bool isFloat, gpu::Format format);
    void PlaceSmart(Document& doc, gpu::CommandContext& ctx, Id surface, gpu::Texture* content, const float2 corners[4]);
    gpu::ReadbackPtr ExportRGBA8(gpu::CommandContext& ctx, gpu::Texture* image, bool srgb = true);
    // Same conversion (straight, optional sRGB) at 16 bits per channel (little-endian u16).
    gpu::ReadbackPtr ExportRGBA16(gpu::CommandContext& ctx, gpu::Texture* image, bool srgb = true);
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
    // Move the selection outline by an integer offset (new selection + history step).
    bool OffsetSelection(Document& doc, gpu::CommandContext& ctx, History& hist, int2 offset, const std::string& label);
    // Translated copy of the selection for live preview while dragging the outline.
    gpu::Texture* OffsetSelectionPreview(Document& doc, gpu::CommandContext& ctx, int2 offset);
    void Reselect(Document& doc, History& hist);
    bool SelectionFilter(Document& doc, gpu::CommandContext& ctx, History& hist, int op, float radius, const std::string& label);
    void SelectionToMask(Document& doc, gpu::CommandContext& ctx, Id maskSurface, bool invert);
    // Fill (mode 0), erase (1) or keep-only-selected (2) on a surface, honoring the selection.
    void FillSelection(Document& doc, gpu::CommandContext& ctx, History* hist, Id surface, int mode, float4 colorLinear,
                       float opacity, const std::string& label, gpu::Texture* source = nullptr, gpu::Texture* coverage = nullptr,
                       bool useSelection = true);
    // Edit > Stroke: paint a band along the selection edge (position 0 inside, 1 center, 2 outside).
    bool StrokeSelection(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, float width, int position, float4 colorLinear,
                         float opacity);
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
    // Normal channel with the Height channel baked in (either may be null), for export.
    gpu::TexturePtr BakeNormal(gpu::CommandContext& ctx, gpu::Texture* normal, gpu::Texture* height, u32 w, u32 h);
    gpu::Texture* MeshRender(gpu::CommandContext& ctx, MeshScene& s, const MeshMaterial& material, u32 w, u32 h);
    void MeshStrokeDabs(Document& doc, gpu::CommandContext& ctx, MeshScene& s, const std::vector<Dab>& dabs, const BrushSettings& b,
                        float4 colorLinear, bool erase, u32 extraFlags = 0, gpu::Texture* decal = nullptr);
    // Multi-channel strokes (3D projects): one coverage, several layers each with its own value.
    bool BeginStrokeMulti(Document& doc, gpu::CommandContext& ctx, const std::vector<Id>& surfaces);
    void MeshStrokeDabsMulti(Document& doc, gpu::CommandContext& ctx, MeshScene& s, const std::vector<Dab>& dabs, const BrushSettings& b,
                             const std::vector<float4>& colorsLinear, bool erase);
    bool meshSymmetryX = false;  // 3D strokes mirrored across the model's X axis
    // 2D multi-channel strokes: the other layers rebuilt from the shared coverage (after StrokeDabs / StrokeProgram).
    void StrokeApplySlots(Document& doc, gpu::CommandContext& ctx, const BrushSettings& b, const std::vector<float4>& colorsLinear, bool erase);
    // Decal projected from the view: center / half size in view pixels, rotation (radians); the first layer can take the image colors.
    void MeshDecalMulti(Document& doc, gpu::CommandContext& ctx, MeshScene& s, gpu::Texture* image, float2 center, float2 halfSize, float rotation,
                        float opacity, const std::vector<float4>& colorsLinear, bool firstUsesImageColor);
    void EndStrokeMulti(Document& doc, gpu::CommandContext& ctx, History& hist, const std::string& label);
    void RenderShape(Document& doc, gpu::CommandContext& ctx, Id surface, const ShapeStyle& s);
    // Warp: `src` drawn as the bicubic Bezier patch `grid` (4x4 control points,
    // row-major, document pixels) over what stays (unselected pixels). `mask`:
    // the original is a selection, warped as a whole.
    void WarpPreview(Document& doc, gpu::CommandContext& ctx, Id surface, gpu::Texture* original, gpu::Texture* selection, const Rect& src,
                     const float2 grid[16], bool mask) {
        WarpPreview(doc, ctx, surface, original, selection, src, BezierWarpMesh(grid, src), mask);
    }
    // Triangles: dst / src document pixels per vertex (UV remapping of 3D textures).
    void WarpTriangles(Document& doc, gpu::CommandContext& ctx, Id surface, gpu::Texture* original, gpu::Texture* selection, const Rect& src,
                       const std::vector<float2>& dst, const std::vector<float2>& srcPts, const std::vector<u32>& indices, bool mask);
    // Any deformation mesh (puppet warp, perspective warp).
    void WarpPreview(Document& doc, gpu::CommandContext& ctx, Id surface, gpu::Texture* original, gpu::Texture* selection, const Rect& src,
                     const WarpMesh& mesh, bool mask);
    void TransformPreview(Document& doc, gpu::CommandContext& ctx, Id surface, gpu::Texture* original, gpu::Texture* selection,
                          const TransformParams& t);

    void Reset();  // drop all caches (document switch)

private:
    gpu::TexturePtr warpTarget;  // WarpPreview render target (bounding box of the warped patch)
    struct StrokeSlot {
        Id surface = 0;
        gpu::TexturePtr pre;
    };
    std::vector<StrokeSlot> strokeSlots;  // multi-channel strokes: layers after the first
    struct NodeCache {
        gpu::TexturePtr tex, tmp;
        u64 hash = 0;
    };
    struct EvalResult {
        gpu::Texture* tex = nullptr;
        u64 hash = 0;
    };
    // Input i of node n, through a single-channel extraction when linked from R/G/B/A.
    EvalResult EvalInput(Document& doc, gpu::CommandContext& ctx, const Node& n, int i, int depth);
    std::unordered_map<u64, struct NodeCache> channelCache;  // (source node, channel)

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
    TileRes EvalTileInput(Document& doc, gpu::CommandContext& ctx, const Node& n, int i, const Rect& need, int depth);
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
    // Bind the LUT textures of lut params at t5.. (uploaded once per LUT content).
    void BindLuts(gpu::CommandContext& ctx, gpu::DispatchDesc& d, const EffectDef& def, const EffectInstance& fx);
    std::unordered_map<u64, gpu::TexturePtr> lutTextures;

    std::unordered_map<Id, NodeCache> cache;
    std::unordered_map<Id, EvalResult> frameEval;
    std::unordered_map<Id, std::string> failed;
    std::unordered_map<u64, ThumbCache> thumbs;
    std::unordered_map<const EffectDef*, ProgramEntry> programs;
    u64 effectGeneration = ~0ull;
    bool pending = false;

    gpu::TexturePtr selTmp[2], selMove;
    void DispatchSelOffset(gpu::CommandContext& ctx, gpu::Texture* src, gpu::Texture* dst, int2 offset);
    gpu::TexturePtr boundsTex;
    Id strokeSurface = 0;
    gpu::TexturePtr strokePre, strokeMask;
    gpu::TexturePtr strokeColor, previewLayer, previewMask, previewColor;  // programmable brushes
    gpu::TexturePtr padLayer, padPre, padMask, padColor;                   // brush test pad
    gpu::TexturePtr rangeMask, refineT[3], refineMask, refineMask2, refinePreview;  // selection masks
    gpu::TexturePtr healD, healW, healTmp;  // region healing
    bool strokeColorReady = false;
    u64 previewHash = 0;
    std::unordered_map<u64, gpu::ProgramPtr> brushPrograms;
    void RunBrush(gpu::CommandContext& ctx, const gpu::ComputePipeline* pipe, const BrushDef& b, const EffectInstance& tip, const std::vector<Dab>& dabs,
                  const std::vector<float4>& extra, u32 base, const BrushSettings& s, float4 color, bool erase, const BrushTargets& t, Rect* touched);
    Rect strokeRect;
    gpu::TexturePtr smudgeSnap;
    gpu::TexturePtr liqDisp, liqSnap;  // stroke displacement field, per-dab snapshot
    bool liqActive = false, haveLiq = false;
    float2 lastLiq;
    float2 lastSmudge;
    bool haveSmudge = false;
};

float HalfToFloat(u16 h);

}  // namespace sx
