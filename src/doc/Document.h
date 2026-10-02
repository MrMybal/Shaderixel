#pragma once
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

enum class LayerKind : u8 { Pixel, Fill, Adjustment, Group, Text, Shape };

// Vector shape layer (rendered by shaders/shape.slang).
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
    u64 Hash() const {
        Hasher h;
        h.Pod(type); h.Pod(rect); h.Pod(radius); h.Pod(sides); h.Pod(rotation); h.Pod(fillOn); h.Pod(strokeOn);
        h.Pod(fill); h.Pod(stroke); h.Pod(strokeWidth);
        return h.h;
    }
};

// Photoshop "Blend If" (gray): the layer shows where its own luminance and the
// underlying luminance fall inside [lo0..lo1 ramp up, hi0..hi1 ramp down] (0-255, sRGB).
struct BlendIf {
    u8 self[4] = {0, 0, 255, 255};
    u8 under[4] = {0, 0, 255, 255};
    bool Active() const {
        return self[0] || self[1] || self[2] != 255 || self[3] != 255 || under[0] || under[1] || under[2] != 255 || under[3] != 255;
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
    bool visible = true;
    bool locked = false;
    Id mask = 0;              // mask surface (0 = none)
    bool maskEnabled = true;
    Id parent = 0;            // enclosing group (0 = root)
    bool clipped = false;     // clipping mask: clipped to the layer below
    bool passThrough = true;  // groups: blend children straight into the parent stack
    bool collapsed = false;   // groups: UI state
    TextStyle text;           // Text layers (rasterized into `surface`)
    ShapeStyle shape;         // Shape layers (rendered into `surface`)
};

enum class NodeKind : u8 { Output, Surface, Effect, Blend };

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
    std::vector<Id> inputs;   // per input pin, 0 = unconnected
    Id layer = 0;             // generated from this layer (layer mode)
};

int NodeInputCount(const Node& n);
std::string NodeInputName(const Node& n, int i);

struct Graph {
    std::vector<Node> nodes;
    Id output = 0;
    Node* Find(Id id);
    const Node* Find(Id id) const;
    bool WouldCycle(Id from, Id to) const;  // linking from -> to's input
};

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

    // Rebuild the generated graph (call after any layer change in layer mode).
    void SyncGraph();

    // Mode conversion.
    void ConvertToNodes();
    bool CanConvertToLayers(std::string* why = nullptr) const;
    bool ConvertToLayers();

    // Node mode ---------------------------------------------------------
    Node& AddNode(NodeKind kind, float2 pos);
    Node& AddEffectNode(const std::string& effectId, const std::string& customCode, float2 pos);
    void RemoveNode(Id id);
    bool Link(Id from, Id to, int input);
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
