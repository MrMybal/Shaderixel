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

#include <functional>

#include <array>
#include <unordered_map>

namespace sx {

struct MeshScene;
struct SeamCarveResult;
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
    gpu::Texture* inpaint = nullptr;    // AI inpainting mask shown as a tint (r = 1: to regenerate)
    float time = 0;
    gpu::Texture* lut = nullptr;  // display / proof 3D LUT (n*n x n, alpha = in gamut)
    u32 lutSize = 0;
    bool gamutWarning = false;
    int scope = 0;           // analysis overlay: 1 zebras, 2 clipping, 3 false color, 4 focus peaking, 5 luminance
    gpu::Texture* snapshot = nullptr;  // before / after
    int compare = 0;         // 1 split, 2 snapshot only, 3 difference
    float split = 0;         // view pixels
    float scopeLevel = 0.95f;
    gpu::Texture* overflow = nullptr;  // layer contents shown outside the document (transform tool)
    float overflowInv[9] = {};         // document pixel -> contents UV (homography, row-major)
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

enum class RetouchMode : u8 { Clone, Heal, Blur, Sharpen, Dodge, Burn, Saturate, Desaturate, History, BackgroundErase, RedEye, ColorReplace };
struct RetouchParams {
    RetouchMode mode = RetouchMode::Clone;
    float strength = 0.5f;
    int2 srcOffset;
    int range = 1;  // dodge/burn: 0 shadows, 1 midtones, 2 highlights
    gpu::Texture* source = nullptr;  // history brush: snapshot of the layer
    float tolerance = 0.25f;         // background eraser, color replacement
    float4 color;                    // color replacement: the new color (linear); range: 0 hue, 1 saturation, 2 color, 3 luminosity
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
    // Tiled documents keep no per-node images: a node's result over `area` (a detail of the document), computed
    // with the tile evaluator, cached until the node, its inputs or the pixels change; one new preview per frame.
    gpu::Texture* TiledNodePreview(Document& doc, gpu::CommandContext& ctx, Id node, const Rect& area, u64* hash);
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
    // The preview last drawn under `key` (never refreshed here), nullptr if none.
    gpu::Texture* CachedThumbnail(u64 key) const {
        auto it = thumbs.find(key);
        return it != thumbs.end() ? it->second.tex.get() : nullptr;
    }
    u64 NodeHash(Id node) const;

    // Painting ---------------------------------------------------------
    bool BeginStroke(Document& doc, gpu::CommandContext& ctx, Id surface);
    void StrokeDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, float4 colorLinear, bool erase);
    void EndStroke(Document& doc, gpu::CommandContext& ctx, History& hist, const std::string& label);
    bool Stroking() const { return strokeSurface != 0; }
    // Retouch brushes (Retouch.cpp); call between BeginStroke/EndStroke.
    void RetouchDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, const RetouchParams& p);
    void SmudgeDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, float strength);
    // Mixer brush: a reservoir of paint carried by the brush, laid down and refilled from the canvas at every dab.
    struct MixerParams {
        float wet = 0.5f, load = 0.15f, flow = 0.8f;
        bool clean = true;  // the reservoir starts every stroke with the brush color only
        float4 color;       // linear, premultiplied
    };
    void MixerDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, const MixerParams& p);
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
    void PlaceSmart(Document& doc, gpu::CommandContext& ctx, Id surface, gpu::Texture* content, const float2 corners[4], int interp = 2);
    // Same projective placement into any texture (corners in `dst` pixels).
    void PlaceTexture(gpu::CommandContext& ctx, gpu::Texture* dst, gpu::Texture* content, const float2 corners[4], int interp = 2);
    gpu::ReadbackPtr ExportRGBA8(gpu::CommandContext& ctx, gpu::Texture* image, bool srgb = true);
    // Same conversion (straight, optional sRGB) at 16 bits per channel (little-endian u16).
    gpu::ReadbackPtr ExportRGBA16(gpu::CommandContext& ctx, gpu::Texture* image, bool srgb = true);
    // Full-resolution output of one node (through the node cache, also in tiled mode).
    gpu::Texture* EvaluateNode(Document& doc, gpu::CommandContext& ctx, Id node, Id liveOverride = 0) {
        BeginGraph(doc, liveOverride);
        return Eval(doc, ctx, node, 0).tex;
    }
    // Graph actually evaluated: the document's, or a flattened copy when it holds functions.
    void BeginGraph(const Document& doc, Id liveOverride = 0);
    // Values (ValueEval.cpp): output `port` of a node, an input of a node (linked or its own value),
    // effect parameters with their linked pins.
    Value EvalValue(Document& doc, Id id, int port, int depth = 0);
    Value InputValue(Document& doc, const Node& n, int i, int depth = 0);
    std::vector<float4> EffectParams(Document& doc, const Node& n);
    // Values produced while an execution flow runs (loop element, index...), by (node << 8 | port).
    std::unordered_map<u64, Value> runtimeValues;
    // Forget this frame's evaluated nodes (an execution flow changed values between two evaluations).
    void NewEvalPass() { frameEval.clear(); frameValues.clear(); }
    const Graph& EvalGraph(const Document& doc) const;
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
    // Content-aware resize: every surface read through the seam carving maps (SeamCarve.h), new canvas size.
    bool CarveCanvas(Document& doc, gpu::CommandContext& ctx, History& hist, u32 newW, u32 newH, const SeamCarveResult& r, const std::string& label);
    // Perspective crop: every surface resampled through a homography (h: 3x3 row-major, destination pixel -> source).
    bool ResampleCanvasPerspective(Document& doc, gpu::CommandContext& ctx, History& hist, u32 newW, u32 newH, const float h[9], int interp,
                                   const std::string& label);
    // Paste in perspective: `src` (its rectangle srcRect) laid on a plane of `dst` through h (3x3 row-major, document
    // pixel -> plane coordinates 0..1); tiles: repetitions across / down the plane (0: once).
    void WarpPerspective(gpu::CommandContext& ctx, gpu::Texture* src, gpu::Texture* dst, const float h[9], float4 srcRect, float2 tiles, float opacity);
    bool ResampleCanvas(Document& doc, gpu::CommandContext& ctx, History& hist, u32 newW, u32 newH, const float m[6], int interp,
                        const std::string& label);
    // 3D painting (MeshPaint.cpp)
    bool MeshPrepare(Document& doc, gpu::CommandContext& ctx, MeshScene& s);
    // Normal channel with the Height channel baked in (either may be null), for export.
    gpu::TexturePtr BakeNormal(gpu::CommandContext& ctx, gpu::Texture* normal, gpu::Texture* height, u32 w, u32 h);
    gpu::Texture* MeshRender(gpu::CommandContext& ctx, MeshScene& s, const MeshMaterial& material, u32 w, u32 h);
    void MeshStrokeDabs(Document& doc, gpu::CommandContext& ctx, MeshScene& s, const std::vector<Dab>& dabs, const BrushSettings& b,
                        float4 colorLinear, bool erase, u32 extraFlags = 0, gpu::Texture* decal = nullptr, const std::vector<float4>* dabEx = nullptr);
    // Multi-channel strokes (3D projects): one coverage, several layers each with its own value.
    bool BeginStrokeMulti(Document& doc, gpu::CommandContext& ctx, const std::vector<Id>& surfaces);
    void MeshStrokeDabsMulti(Document& doc, gpu::CommandContext& ctx, MeshScene& s, const std::vector<Dab>& dabs, const BrushSettings& b,
                             const std::vector<float4>& colorsLinear, bool erase, const std::vector<float4>* dabEx = nullptr);  // dabEx: cos, sin, tip variant
    int meshSymmetry = 0;        // 3D strokes mirrored across the model's axis: 0 none, 1 X, 2 Y, 3 Z
    float meshFacingMin = 0.05f; // texels seen more edge-on than this are not painted (dot(n, view))
    Rect meshClip;               // 3D strokes only paint inside this rectangle (empty: anywhere)
    // Brush tip in 3D: a few variants of the programmable tip rendered side by side (BuildMeshTip).
    gpu::TexturePtr meshTipAtlas, meshTipLayer, meshTipColor;
    u64 meshTipHash = 0;
    int meshTipVariants = 8;
    float meshTipExtent = 1.0f;
    bool meshTipOn = false;      // used by the next MeshStrokeDabs
    // Stencil: an image fixed on screen, strokes paint through it (center xy, half size zw; cos, sin, invert, luminance).
    bool meshStencilOn = false;
    gpu::Texture* meshStencilTex = nullptr;
    float4 meshStencil, meshStencil2, meshStencil3;  // 3: luminance threshold, contrast
    bool BuildMeshTip(gpu::CommandContext& ctx, const BrushDef& b, const EffectInstance& tip, const BrushSettings& s);
    // Smart mask: `surface` (document size) generated from the mesh maps (shaders/mesh_generator.slang).
    struct MeshGenParams {
        int type = 0;
        bool invert = false;
        float amount = 0.5f, contrast = 4, noise = 0.3f, noiseScale = 6, seed = 0;
        float4 dir = {0, 1, 0, 0};
        // Type 8 (follows a layer): its coverage (alpha of a paint layer, red of a mask), mode, blur radius (texels).
        gpu::Texture* source = nullptr;
        bool sourceAlpha = false;
        int anchorMode = 0;
        float anchorRadius = 8;
        gpu::Texture* edits = nullptr;  // painted corrections laid over the result
    };
    bool MeshGenerate(Document& doc, gpu::CommandContext& ctx, MeshScene& s, Id surface, const MeshGenParams& p);
    // Projected material: `image` projected triplanar on the model into `surface` (shaders/mesh_triplanar.slang).
    struct MeshProjectParams {
        int mode = 0;  // 0 color, 1 gray from the luminance in [lo, hi]
        float scale = 1.5f, rotation = 0, sharpness = 6, lo = 0, hi = 1, offsetX = 0, offsetY = 0, contrast = 1;
        bool invert = false;
    };
    bool MeshProject(Document& doc, gpu::CommandContext& ctx, MeshScene& s, Id surface, gpu::Texture* image, const MeshProjectParams& p);
    // Particle simulation: dabs in model space (center, radius) with alpha + surface normal, every stroke layer.
    void MeshWorldDabsMulti(Document& doc, gpu::CommandContext& ctx, MeshScene& s, const std::vector<float4>& dabs, const std::vector<float4>& ex,
                            const BrushSettings& b, const std::vector<float4>& colorsLinear, bool erase);
    // 2D multi-channel strokes: the other layers rebuilt from the shared coverage (after StrokeDabs / StrokeProgram).
    void StrokeApplySlots(Document& doc, gpu::CommandContext& ctx, const BrushSettings& b, const std::vector<float4>& colorsLinear, bool erase);
    // Decal projected from the view: center / half size in view pixels, rotation (radians); the first layer can take the image colors.
    void MeshDecalMulti(Document& doc, gpu::CommandContext& ctx, MeshScene& s, gpu::Texture* image, float2 center, float2 halfSize, float rotation,
                        float opacity, const std::vector<float4>& colorsLinear, bool firstUsesImageColor);
    void EndStrokeMulti(Document& doc, gpu::CommandContext& ctx, History& hist, const std::string& label);
    // Liquid paint (FluidPaint.cpp): a film of paint simulated in texture space that carries a whole material
    // (color, roughness, metal, emissive; its thickness is the relief) and runs with gravity, composited live
    // into the stroke layers (one per channel: kinds 0 base color, 1 roughness, 2 metal, 3 height, 4 emissive).
    struct FluidParams {
        float4 gravity = {0, -1, 0, 0};  // model: world direction; image: texel direction * slope (xy), facing down (z)
        float strength = 1;
        float speed = 300;            // texels / s on a vertical slope
        float retention = 0.35f;      // film that stays (does not run)
        float spread = 0.3f;          // the paint levels itself (sideways)
        float cohesion = 0.15f;       // the moving paint gathers into rivulets
        float pickup = 0;             // the liquid takes the color of the paint already in the layer
        float bristle = 0, bristleCount = 7, bristleNoise = 0.6f;  // grooves of the bristles along the stroke
        float bristleAngle = 0;  // image strokes: direction of the stroke so far (radians), for a batch of one dab
        float coat = 0.4f;            // thickness laid by the brush (the flow adds on top)
        float setTime = 6;            // s: the paint stops running
        float noise = 0.5f, noiseScale = 6;  // irregular runs
        float absorb = 0.05f;         // stain left in the support
        float damping = 6;            // inertia (1 / s)
        float density = 2.5f;         // coverage per thickness
        float relief = 1, heightBase = 0.5f, wetGloss = 0.7f;
        float bed = 0;                // the existing relief diverts the paint
        float dripOff = 0.5f;         // drops falling from overhangs
        float hardness = 0.4f;
        float seed = 0;
        float4 color = {1, 0, 0, 0};  // linear color, emissive part
        float rough = 0.4f, metal = 0;
        float4 emissive = {0, 0, 0, 1};
        bool offEdges = true;         // runs off the image / UV island borders
    };
    bool FluidBegin(Document& doc, gpu::CommandContext& ctx, const std::vector<Id>& surfaces, const std::vector<int>& kinds, MeshScene* mesh,
                    u32 simW, u32 simH, gpu::Texture* selection, gpu::Texture* bed);
    void FluidGravity(gpu::CommandContext& ctx, const FluidParams& p);
    // Dabs: image (x, y px, radius px, -) or world (x, y, z, radius); extras: amount, normal of the painted side.
    void FluidDeposit(gpu::CommandContext& ctx, const std::vector<float4>& dabs, const std::vector<float4>& ex, bool world, bool erase, const FluidParams& p);
    int FluidStep(gpu::CommandContext& ctx, float dt, const FluidParams& p);  // returns the substeps
    void FluidComposite(Document& doc, gpu::CommandContext& ctx, const FluidParams& p, bool final = false);  // final: over the whole painted area
    void FluidEnd(Document& doc, gpu::CommandContext& ctx, History& hist, const std::string& label, bool keep);
    bool FluidActive() const { return fluid.active; }
    std::vector<Id> FluidSurfaces() const;
    // Called when another stroke starts on this renderer while the paint is wet (the app sets the paint first).
    std::function<void()> fluidInterrupt;
    u32 FluidWidth() const { return fluid.w; }
    float FluidTime() const { return fluid.time; }
    int FluidPortalCount() const { return fluid.portalKey ? fluid.portals : -1; }  // -1: not built yet
    // Fallen drops: start collecting them (false while the last collection is not read yet), then read them:
    // per block (volume, volume * color linear) and (volume * world position, texels); the counts restart from 0.
    bool FluidCollectDrops(gpu::CommandContext& ctx);
    bool FluidDrops(std::vector<float4>& volColor, std::vector<float4>& posCount);
    // Make the layer run: the paint of the stroke layers (as they were) becomes the liquid, `thickness` where opaque.
    void FluidLoadLayers(gpu::CommandContext& ctx, const FluidParams& p, float thickness);
    float FluidTexelWorld() const { return fluid.texelWorld; }
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
    struct FluidState {
        bool active = false;
        u32 w = 0, h = 0, docW = 0, docH = 0;
        gpu::TexturePtr f0[2], f1[2], d0[2], d1[2], flux[2], grav, selection, bed;
        int cur = 0, fluxCur = 0;
        MeshScene* mesh = nullptr;
        std::vector<int> kinds;
        float texelWorld = 1, time = 0;
        bool beginning = false;
        // UV seams (kept between sessions while the model and the size stay): portal maps built on the CPU
        // from the position / normal maps read back once.
        gpu::TexturePtr portalOut, portalIn;
        u64 portalKey = 0, portalPending = 0;
        gpu::ReadbackPtr rbPos, rbNrm;
        int portals = 0;
        // Fallen drops: collected per 16 x 16 block and read back (the app makes them land lower).
        gpu::TexturePtr dropA, dropB;
        gpu::ReadbackPtr rbDropA, rbDropB;
        bool resetDrops = false;
        bool replace = false;  // the layers' paint became the liquid
        // Image documents: simulated and composited on the active area only (simulation texels).
        bool windowed = false, finalComposite = false;
        Rect simRect, paintRect, dabsSince;
        gpu::TexturePtr boundsTex;
        gpu::ReadbackPtr rbBounds;
        int boundsFrame = 0, boundsGrowth = 0;
    } fluid;
    void FluidPortals(gpu::CommandContext& ctx);
    void FluidDispatch(gpu::CommandContext& ctx, u32 pass, u32 kind, float dt, const FluidParams& p, const std::vector<float4>* dabs,
                       const std::vector<float4>* ex, u32 extraFlags);
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
    struct PreviewTile {
        gpu::TexturePtr tex;
        u64 hash = 0;
    };
    std::unordered_map<Id, PreviewTile> tiledPreviews;
    u64 tiledPreviewFrame = 0;
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
    Graph flatGraph;
    bool useFlat = false;
    std::unordered_map<u64, Value> frameValues;
    std::unordered_map<Id, std::string> failed;
    std::unordered_map<u64, ThumbCache> thumbs;
    struct ShapeSdfCache {
        u64 key = 0;
        gpu::TexturePtr tex;
        float4 box;
    };
    std::unordered_map<Id, ShapeSdfCache> shapeSdf;  // custom shape layers: distance field of their outline
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
    gpu::TexturePtr replaceColor;  // color replacement: 1 x 1 texel holding the new color
    gpu::TexturePtr mixRes[2];   // mixer brush reservoir (ping-pong, brush space)
    int mixCur = 0;
    bool mixFresh = true, mixInit = false;
    gpu::TexturePtr liqDisp, liqSnap;  // stroke displacement field, per-dab snapshot
    bool liqActive = false, haveLiq = false;
    float2 lastLiq;
    float2 lastSmudge;
    bool haveSmudge = false;
};

float HalfToFloat(u16 h);
u16 FloatToHalf(float f);

}  // namespace sx
