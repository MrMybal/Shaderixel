#pragma once
#include <map>
#include <set>
// Document model.
//
// The node GRAPH is what gets rendered, always. A document is created either in
// LAYER mode (Photoshop-like stack, the graph is generated from it and shown
// read-only) or in NODE mode (the graph is edited directly). A layer document
// can always be converted to nodes; a node document converts back to layers only
// while its graph is still a linear stack.
//
// Pixel data lives in Surfaces (GPU textures) referenced by id from layers and
// nodes, so structural edits never copy pixels and undo snapshots stay cheap.

#include "core/Common.h"
#include "doc/VectorPath.h"
#include "doc/Effects.h"
#include "io/TextRaster.h"
#include "gpu/Device.h"

#include <unordered_map>

namespace sx {

enum class BlendMode : u8 {
    Normal, Multiply, Screen, Overlay, LinearDodge, Subtract, Darken, Lighten, Difference,
    ColorDodge, ColorBurn, HardLight, SoftLight, Dissolve, LinearBurn, VividLight, LinearLight,
    PinLight, HardMix, Exclusion, Divide, Hue, Saturation, Color, Luminosity, DarkerColor, LighterColor,
    Count
};
const char* BlendModeName(BlendMode m);
// Photoshop menu order with separators (-1).
extern const int kBlendMenuOrder[];
extern const int kBlendMenuCount;

enum class DocMode : u8 { Layers, Nodes };
enum class Precision : u8 { Half16, Float32 };
gpu::Format SurfaceFormat(Precision p);

struct Surface {
    gpu::TexturePtr tex;
    u64 version = 1;  // bumped on every pixel change (render cache key)
};

// An effect instance: library effect (effectId) or inline custom code.
struct EffectInstance {
    std::string effectId;
    std::string customCode;  // non-empty => custom HLSL effect
    std::string graph;       // non-empty => visual effect graph (JSON), compiled to HLSL
    std::vector<float4> params;
    std::vector<CurvePoints> curves;  // per param index (curve params only)
    std::vector<Lut3DPtr> luts;       // per param index (lut params only)
    bool enabled = true;
    EffectDefPtr Def() const;  // resolves library / parses custom code (cached)
    // Keep parameter values aligned with a (new) definition, matching by id.
    void SyncParams(const EffectDefPtr& oldDef);
private:
    mutable EffectDefPtr cachedDef;
    mutable std::string cachedCode;
};

enum class LayerKind : u8 { Pixel, Fill, Adjustment, Group, Text, Shape, Smart };

class Document;

// Smart object (objet dynamique): contents kept at their own resolution, placed
// on the layer by a projective transform recomputed from the source (lossless
// transforms). Embedded (an inner layered document, edited in its own tab) or
// linked to a file (reloaded when the file changes). Shared by duplicated layers.
struct SmartObject {
    Id id = 0;
    std::string name;
    std::string linkedPath;            // linked file; empty = embedded
    long long linkedTime = 0;          // file time of the linked contents
    u32 width = 0, height = 0;
    gpu::TexturePtr content;           // contents, linear premultiplied
    u64 version = 1;                   // bumped when the contents change
    std::unique_ptr<Document> child;   // embedded layered document (null while it is open in a tab)
    bool childOpen = false;
    SmartObject();
    ~SmartObject();  // defined where Document is complete
};
using SmartObjectPtr = std::shared_ptr<SmartObject>;

// Vector shape layer (rendered by shaders/shape.slang).
// Extra component of a shape layer, combined with the shape by a boolean operation.
struct ShapePart {
    int op = 0;                      // 0 unite, 1 subtract, 2 intersect, 3 exclude
    int type = 1;
    float4 rect = {0, 0, 100, 100};
    float radius = 0;
    int sides = 5;
    float rotation = 0;
};

struct ShapeStyle {
    int type = 0;                    // 0 rectangle, 1 ellipse, 2 line, 3 polygon, 4 star
    float4 rect = {0, 0, 100, 100};  // x0 y0 x1 y1 (line: endpoints)
    float radius = 0;                // corner radius (rect) / inner ratio (star)
    int sides = 5;
    float rotation = 0;              // degrees
    bool fillOn = true, strokeOn = false;
    float4 fill = {0.2f, 0.45f, 0.9f, 1};  // sRGB straight
    float4 stroke = {0, 0, 0, 1};
    float strokeWidth = 4;
    std::vector<ShapePart> parts;    // boolean components (up to 8)
    u64 Hash() const {
        Hasher h;
        h.Pod(type); h.Pod(rect); h.Pod(radius); h.Pod(sides); h.Pod(rotation); h.Pod(fillOn); h.Pod(strokeOn);
        h.Pod(fill); h.Pod(stroke); h.Pod(strokeWidth);
        for (auto& p : parts) { h.Pod(p.op); h.Pod(p.type); h.Pod(p.rect); h.Pod(p.radius); h.Pod(p.sides); h.Pod(p.rotation); }
        return h.h;
    }
};

// Photoshop "Blend If": the layer shows where its own values and the underlying
// values fall inside [lo0..lo1 ramp up, hi0..hi1 ramp down] (0-255, sRGB), for
// the gray channel and each of red, green, blue (all conditions combine).
struct BlendIf {
    static constexpr int kGray = 0, kChannels = 4;  // gray, red, green, blue
    u8 self[kChannels][4] = {{0, 0, 255, 255}, {0, 0, 255, 255}, {0, 0, 255, 255}, {0, 0, 255, 255}};
    u8 under[kChannels][4] = {{0, 0, 255, 255}, {0, 0, 255, 255}, {0, 0, 255, 255}, {0, 0, 255, 255}};
    static bool Full(const u8 r[4]) { return !r[0] && !r[1] && r[2] == 255 && r[3] == 255; }
    bool Active(int c) const { return !Full(self[c]) || !Full(under[c]); }
    bool Active() const {
        for (int c = 0; c < kChannels; ++c) if (Active(c)) return true;
        return false;
    }
    // Shader constants: self ranges of the 4 channels, then under ranges (0-1).
    void Ranges(float4 out[8]) const {
        for (int c = 0; c < kChannels; ++c) {
            out[c] = {self[c][0] / 255.0f, self[c][1] / 255.0f, self[c][2] / 255.0f, self[c][3] / 255.0f};
            out[4 + c] = {under[c][0] / 255.0f, under[c][1] / 255.0f, under[c][2] / 255.0f, under[c][3] / 255.0f};
        }
    }
};

struct Layer {
    Id id = 0;
    std::string name;
    LayerKind kind = LayerKind::Pixel;
    Id surface = 0;           // Pixel layers
    EffectInstance effect;    // Fill (generator) / Adjustment
    std::vector<EffectInstance> filters;  // non-destructive filters on the layer content
    BlendMode blend = BlendMode::Normal;
    float opacity = 1.0f;
    BlendIf blendIf;
    u8 channelOff = 0;        // channels the layer leaves untouched (bit 0 R, 1 G, 2 B, 3 A)
    EffectInstance blendFx;   // custom blend (code or library effect: A below, B the layer, mask), see HasCustomBlend
    bool HasCustomBlend() const { return !blendFx.customCode.empty() || !blendFx.effectId.empty() || !blendFx.graph.empty(); }
    bool visible = true;
    bool locked = false;
    Id mask = 0;              // mask surface (0 = none)
    bool maskEnabled = true;
    Id parent = 0;            // enclosing group (0 = root)
    Id material = 0;          // material layer (one fill per PBR channel group, shared mask), 0 = none
    Id smart = 0;             // Smart layers: smart object (Document::smartObjects)
    float2 smartCorners[4];   // Smart layers: where the contents go (TL TR BR BL, document pixels)
    float2 smartPivot = {0.5f, 0.5f};  // Smart layers: transform pivot, in the contents (0..1)
    u8 smartInterp = 2;       // Smart layers: sampling (0 nearest, 1 bilinear, 2 bicubic)
    bool clipped = false;     // clipping mask: clipped to the layer below
    bool passThrough = true;  // groups: blend children straight into the parent stack
    bool collapsed = false;   // groups: UI state
    TextStyle text;           // Text layers (rasterized into `surface`)
    ShapeStyle shape;         // Shape layers (rendered into `surface`)
    bool artboard = false;    // groups: artboard (plan de travail), clipped to `board` by its mask
    Rect board;
};

// Layer comp: visibility / appearance / smart object placement of every layer.
struct LayerComp {
    Id id = 0;
    std::string name, comment;
    bool useVisibility = true, useAppearance = true, usePosition = true;
    struct Entry {
        Id layer = 0;
        bool visible = true;
        float opacity = 1;
        BlendMode blend = BlendMode::Normal;
        std::vector<u8> filterOn;
        float2 corners[4];
    };
    std::vector<Entry> entries;
};

// Split: passes its RGBA input through; its outputs R, G, B, A are channel links (1-4).
// Tool: graph tools working on their input as pixels (bake, upscale, ComfyUI...). Once baked the
// node outputs its stored pixels and the nodes before it are no longer evaluated ("PassLive":
// the input passes through live, to edit it before baking again).
// Function: a graph inside (sub-graph or custom tool). Its SubInput / SubOutput nodes are its pins
// (any number, in `port` order). Live: evaluated in place (the graph is flattened for rendering);
// Apply mode: its outputs are stored as pixels when applied, the nodes before it are frozen.
// Links from output k of a Function carry k in bits 3-7 of the input channel byte.
enum class NodeKind : u8 { Output, Surface, Effect, Blend, Split, Tool, Function, SubInput, SubOutput };

struct Graph;

struct Node {
    Id id = 0;
    NodeKind kind = NodeKind::Effect;
    std::string name;
    float2 pos;
    Id surface = 0;           // Surface nodes
    EffectInstance fx;        // Effect nodes
    BlendMode blend = BlendMode::Normal;  // Blend nodes
    float opacity = 1.0f;
    bool bypass = false;
    bool clip = false;        // Blend: keep the base alpha (clipping mask)
    BlendIf blendIf;          // Blend: conditional blending
    u8 channelOff = 0;        // Blend: channels kept from the base (bit 0 R, 1 G, 2 B, 3 A)
    std::vector<Id> inputs;   // per input pin, 0 = unconnected
    std::vector<u8> inputChannel;  // per input pin: output used on the source, 0 RGBA, 1 R, 2 G, 3 B, 4 A
    Id layer = 0;             // generated from this layer (layer mode)
    // Tool nodes
    std::string tool;         // "bake", "upscale", "comfyui"
    bool passLive = false;    // input passed through live instead of the baked pixels
    Id smart = 0;             // result at its own resolution (smart object; upscale / ComfyUI), 0 = none
    u64 bakedHash = 0;        // input hash when baked (shows when the input changed since)
    std::map<std::string, std::string> props;  // tool settings
    // Function nodes
    std::shared_ptr<const Graph> sub;  // inner graph (never modified in place: replaced whole)
    std::string funcKind;              // "subgraph" or "tool" (library category)
    bool applyMode = false;            // outputs used from outSurf once applied
    std::vector<Id> outSurf;           // applied outputs (pixels), one per output
    // SubInput / SubOutput: pin order
    int port = 0;
    std::string Prop(const std::string& k, const std::string& def = "") const {
        auto it = props.find(k);
        return it == props.end() ? def : it->second;
    }
    float PropF(const std::string& k, float def) const {
        auto it = props.find(k);
        return it == props.end() || it->second.empty() ? def : (float)atof(it->second.c_str());
    }
};
// A tool node that outputs its baked pixels (its inputs are not evaluated).
inline bool ToolBaked(const Node& n) { return n.kind == NodeKind::Tool && !n.passLive && n.surface; }
inline bool FunctionApplied(const Node& n) {
    if (n.kind != NodeKind::Function || !n.applyMode || n.outSurf.empty()) return false;
    for (Id s : n.outSurf) if (!s) return false;
    return true;
}
// Outputs stored as pixels: the nodes before are not evaluated.
inline bool NodeBaked(const Node& n) { return ToolBaked(n) || FunctionApplied(n); }
inline u8 LinkChannel(u8 c) { return c & 7; }
inline int LinkPort(u8 c) { return c >> 3; }

int NodeInputCount(const Node& n);
inline u8 InputChannel(const Node& n, size_t i) { return i < n.inputChannel.size() ? n.inputChannel[i] : 0; }
// Value type of pins: an RGBA image or a single channel (gray image).
enum class PinType : u8 { RGBA, Channel };
PinType NodeInputType(const Node& n, int i);
int NodeOutputCount(const Node& n);
PinType NodeOutputType(const Node& n, int k);
u8 NodeOutputChannel(const Node& n, int k);  // channel a link from output k carries
std::string NodeOutputName(const Node& n, int k);
std::string NodeInputName(const Node& n, int i);

struct Graph {
    std::vector<Node> nodes;
    Id output = 0;
    std::string description;  // functions: shown in the library
    Node* Find(Id id);
    const Node* Find(Id id) const;
    bool WouldCycle(Id from, Id to) const;  // linking from -> to's input
};
// Pins of a function graph: its SubInput (or SubOutput) nodes in port order.
std::vector<const Node*> FunctionPorts(const Graph& g, NodeKind k);
bool GraphHasFunctions(const Graph& g);
// Graph with every Function replaced by its contents (or its applied pixels), for rendering.
// `liveOverride`: that function is expanded even when applied (to apply it again).
void FlattenGraph(const Graph& in, Graph& out, Id liveOverride = 0);
// Id of the flattened node that carries output `port` of function `f`.
Id FunctionOutputFlatId(const Node& f, int port);
// Surfaces used by a graph (pixel nodes, baked tools, applied functions, inner graphs).
void CollectGraphSurfaces(const Graph& g, std::set<Id>& out);

// Everything except pixels; copied whole for structural undo.
struct DocState {
    u32 width = 1, height = 1;  // in the state so canvas size/crop can be undone
    DocMode mode = DocMode::Layers;
    std::vector<Layer> layers;  // [0] = bottom
    Graph graph;
    Id activeLayer = 0;
    Id activeNode = 0;
    bool editMask = false;     // painting targets the active layer's mask
    Id selection = 0;          // R8 selection surface (0 = no selection: everything)
    Id lastSelection = 0;      // for Reselect
    std::vector<VectorPath> paths;  // Pen tool paths
    std::vector<float> guidesX, guidesY;  // vertical guides (x) and horizontal guides (y), document pixels
    Id activePath = 0;
    // Layer mode: positions the user gave to generated graph nodes (by node id).
    std::map<Id, float2> layerNodePos;
    std::vector<LayerComp> comps;
    Id activeComp = 0;
    std::vector<CharStyle> charStyles;  // text styles shared by the text layers
    std::vector<ParaStyle> paraStyles;
    std::unordered_map<Id, Surface> surfaces;
    Id nextId = 100;
};

class Document {
public:
    Document(u32 w, u32 h, Precision p, DocMode mode, const std::string& name);

    u32 Width() const { return state.width; }
    u32 Height() const { return state.height; }
    Precision GetPrecision() const { return precision; }
    const std::string& Name() const { return name; }
    std::string path;
    bool blendInGamma = true;  // Photoshop-compatible blending (sRGB space)
    // Color management: working RGB profile ("" = sRGB), CMYK profile for proofing / CMYK mode,
    // document mode (0 RGB, 1 CMYK: colors kept in the CMYK gamut and shown through it, 2 Lab).
    std::string iccProfile, cmykProfile;
    std::string functionKind;  // document holding a function graph (library): "subgraph" or "tool"
    int colorMode = 0;
    // Document kind: 0 image (Photoshop-like), 1 Paint 3D project (texture set of a mesh, .sx3d).
    int kind = 0;
    u32 pbrChannels = 0x7F;  // Paint 3D: channels of the texture set (bit per PbrChannel)
    int exportTemplate = 0;  // Paint 3D: 0 separate maps, 1 Unreal (ORM), 2 Unity (mask map)
    bool dirty = false;

    DocState& S() { return state; }
    const DocState& S() const { return state; }
    DocMode Mode() const { return state.mode; }

    Id NewId() { return state.nextId++; }
    Id CreateSurface(const float4& clear);  // premultiplied linear clear value
    Id CreateSurfaceRaw(gpu::Format f, const char* name);  // contents written by the caller
    void CreateSurfaceWithId(Id id, gpu::Format f);       // loading
    void SetName(const std::string& n) { name = n; }
    Surface* Selection() { return state.selection ? GetSurface(state.selection) : nullptr; }
    Surface* GetSurface(Id id);
    // Mark a surface changed (whole surface, or only `rect`, used by tiled rendering).
    void TouchSurface(Id id, const Rect* rect = nullptr);
    std::unordered_map<Id, Rect> dirtyRects;  // accumulated since the renderer last consumed them
    bool tiled = false;                       // tiled rendering (very large documents)
    bool forceTiled = false;                  // tiled rendering at any size (tests)
    // Give every surface a fresh version (after undo/redo swapped the state).
    void InvalidateSurfaces();
    std::vector<std::pair<Id, float4>> pendingClears;  // consumed by renderer
    void CancelPendingClear(Id id) {
        std::erase_if(pendingClears, [&](auto& c) { return c.first == id; });
    }

    // Layer mode -------------------------------------------------------
    Layer* FindLayer(Id id);
    int LayerIndex(Id id) const;
    Layer* ActiveLayer() { return FindLayer(state.activeLayer); }
    Layer& AddPixelLayer(const std::string& name, int index = -1);
    Layer& AddEffectLayer(LayerKind kind, const std::string& effectId, int index = -1);
    Layer& AddTextLayer(const TextStyle& style);
    Layer& AddShapeLayer(const ShapeStyle& style);
    void RemoveLayer(Id id);
    void MoveLayer(Id id, int newIndex);
    void MoveLayerSibling(Id id, int dir);           // +1 up, -1 down among siblings
    void MoveLayerTo(Id id, Id target, bool into);   // above target, or into a group
    Layer& GroupLayer(Id id);                         // wrap a layer in a new group
    void Ungroup(Id groupId);
    bool IsDescendant(Id layer, Id group) const;
    int LayerDepth(Id id) const;
    int BlockStart(Id id) const;                      // first index of a layer's block
    Layer& DuplicateLayer(Id id);
    void AddMask(Id layerId);
    std::vector<std::pair<Id, Id>> pendingCopies;  // (dst surface, src surface)
    // Smart objects referenced by layers (kept when layers are deleted, for undo).
    std::map<Id, SmartObjectPtr> smartObjects;
    SmartObject* GetSmart(Id id) {
        auto it = smartObjects.find(id);
        return it == smartObjects.end() ? nullptr : it->second.get();
    }

    // Rebuild the generated graph (call after any layer change in layer mode).
    void SyncGraph();
    // View "up to the layer": graph without the layers above `layer` (0 = full graph). Not a document edit.
    void BuildViewGraph(Id layer) {
        if (state.mode != DocMode::Layers) return;
        Id keep = state.activeNode;
        viewUpTo = layer;
        RebuildFromLayers();
        viewUpTo = 0;
        state.activeNode = keep;
    }
    Id viewUpTo = 0;

    // Mode conversion.
    void ConvertToNodes();
    bool CanConvertToLayers(std::string* why = nullptr) const;
    bool ConvertToLayers();

    // Node mode ---------------------------------------------------------
    Node& AddNode(NodeKind kind, float2 pos);
    Node& AddEffectNode(const std::string& effectId, const std::string& customCode, float2 pos);
    Node& AddToolNode(const std::string& tool, float2 pos);
    // Function with one input passed to one output, to fill in its editor ("subgraph" / "tool").
    Node& AddFunctionNode(const std::string& funcKind, float2 pos);
    void RemoveNode(Id id);
    bool Link(Id from, Id to, int input, u8 channel = 0);
    void Unlink(Id to, int input);

    // Surface that brush tools paint into (active layer / mask / surface node).
    Id PaintTarget() const;

private:
    void RebuildFromLayers();
    u64 versionCounter = 1;
    Precision precision;
    std::string name;
    DocState state;
};

}  // namespace sx
