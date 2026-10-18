#pragma once
// Visual effect graphs (Unreal-material-editor style). A graph is compiled to the
// regular effect text format (//@param metadata + `float4 Effect(EffectContext)`),
// so it runs through exactly the same pipeline as library and custom HLSL effects.
//
// Values are typed by component count (1..4). Binary/ternary ops broadcast
// scalars and use the widest operand, like Unreal.
//
// A second backend produces self-contained HLSL for an Unreal Custom node and a
// Python script that builds the material in the Unreal editor.

#include "core/Common.h"
#include "core/Json.h"

#include <functional>
#include <map>
#include <memory>

namespace sx {

struct EGLink {
    Id node = 0;
    int output = 0;
};

// Declared pin of a custom HLSL node. Types: 1..4 float..float4, 5 texture (an
// image read at any UV with Texture2DSample(T, S, uv)), 6 int, 7 bool, 8 color.
// An unconnected input (not a texture) is a parameter of the effect: default,
// range, label and category (group in Propriétés).
struct EGPinDecl {
    std::string name;
    int type = 4;
    float4 def;
    float min = 0, max = 1;
    std::string category, label;
};
enum EGPinType : int { kPinFloat = 1, kPinFloat2, kPinFloat3, kPinFloat4, kPinTexture, kPinInt, kPinBool, kPinColor, kPinTypeCount = kPinColor };
int PinValueSize(int pinType);            // components carried by a link of that type
const char* PinHlslType(int pinType);     // type of the Run() parameter
const char* PinTypeName(int pinType);

struct EGNode {
    Id id = 0;
    std::string op;
    float2 pos;
    std::vector<EGLink> inputs;  // per input pin (node 0 = unconnected, uses default)
    float4 value;                // constants / defaults
    float min = 0, max = 1;      // parameters
    std::string name;            // parameter id / label
    std::string text;            // custom code (body of the function)
    int index = 0;               // input index, mask bits; parameters: 1 = id is the name as written (imported effects)
    // Custom HLSL with declared pins (empty = legacy A, B, C, D float4 -> float4).
    std::vector<EGPinDecl> pins, outs;
    std::string decl;            // declarations placed before the node (helper functions, constants)
    // Values of the unconnected inputs set on this node (NaN: the default of the operation).
    std::vector<float> pinValues;
    bool HasPins() const { return !pins.empty() || !outs.empty(); }
    float PinValue(size_t i, float opDefault) const { return i < pinValues.size() && pinValues[i] == pinValues[i] ? pinValues[i] : opDefault; }
    bool PinValueSet(size_t i) const { return i < pinValues.size() && pinValues[i] == pinValues[i]; }
};

enum class EGKind : int { Effect = 0, Blend = 1, Brush = 2 };

// Comment frame drawn behind the nodes (title, color); moving it moves the nodes inside.
struct EGComment {
    Id id = 0;
    float2 pos, size = {320, 200};
    std::string text = "Commentaire";
    float4 color = {0.30f, 0.42f, 0.70f, 1};
};

struct EffectGraph {
    std::string name = "Effet visuel";
    std::string category = "Graphes";
    EGKind kind = EGKind::Effect;
    int inputs = 1;              // image inputs (0 = generator); blends: A below, B layer, mask
    bool premultiplied = false;
    // Brushes: placement of the dabs (generated Dynamics()).
    float spacing = 0.1f, airbrush = 0, follow = 0, scatter = 0, sizeJitter = 0, angleJitter = 0, opacityJitter = 0;
    // Brushes: reaction to the pen (tablet): tilt direction -> angle, tilt -> size, barrel rotation -> angle, speed -> size.
    float tiltAngle = 0, tiltSize = 0, twistAngle = 0, speedSize = 0;
    std::vector<EGNode> nodes;
    std::vector<EGComment> comments;
    Id nextId = 1;

    EGNode* Find(Id id);
    const EGNode* Find(Id id) const;
    Id OutputNode() const;
    bool WouldCycle(Id from, Id to) const;

    Json ToJson() const;
    static bool FromJson(const Json& j, EffectGraph& out);
    std::string Serialize() const { return ToJson().Dump(0); }
    static bool Deserialize(const std::string& s, EffectGraph& out);
    static EffectGraph Default();
    static EffectGraph DefaultBlend();
    static EffectGraph DefaultBrush();
};

// Operation registry --------------------------------------------------------
struct EGPinDef {
    const char* name;
    float defValue;
};

struct EGOpDef {
    const char* op;
    const char* label;
    const char* category;
    std::vector<EGPinDef> inputs;
    std::vector<const char*> outputs;  // names; most ops have one output ""
    int fixedOutType = 0;              // 0 = widest input, else component count
    bool hasValue = false;             // shows value editor (constants, params)
};

const std::vector<EGOpDef>& EffectGraphOps();
// Operations offered for a kind of graph (no image input in brushes, blend / brush nodes).
bool OpAllowed(const EGOpDef& op, EGKind kind);
const EGOpDef* FindOp(const std::string& op);
// Op definition seen by a node: a custom node with declared pins gets its own inputs / outputs
// (names point into the node: use the result while the node is alive).
EGOpDef EffectiveOp(const EGNode& n);
inline constexpr int kEGTexture = 5;  // value type of texture links

// Code generation -----------------------------------------------------------
struct EGCompileResult {
    std::string code;            // Shaderixel effect text (metadata + body)
    std::string error;
    std::vector<Id> errorNodes;
    // Component count (1 scalar, 2 vector2, 3 RGB, 4 RGBA) of every node output,
    // key = node id * 16 + output index; for typed pins and links in the editor.
    std::map<u64, int> outTypes;
    int OutType(Id node, int output) const {
        auto it = outTypes.find((u64)node * 16 + output);
        return it == outTypes.end() ? 0 : it->second;
    }
};
EGCompileResult CompileEffectGraph(const EffectGraph& g);

// Unreal: HLSL for a Custom node + Python script that builds the material.
struct EGUnrealExport {
    std::string customCode;
    std::string python;
    std::string error;
};
EGUnrealExport ExportEffectGraphToUnreal(const EffectGraph& g, const std::string& materialName, const std::string& packagePath);

}  // namespace sx
