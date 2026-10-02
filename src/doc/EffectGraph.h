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
#include <memory>

namespace sx {

struct EGLink {
    Id node = 0;
    int output = 0;
};

struct EGNode {
    Id id = 0;
    std::string op;
    float2 pos;
    std::vector<EGLink> inputs;  // per input pin (node 0 = unconnected, uses default)
    float4 value;                // constants / defaults
    float min = 0, max = 1;      // parameters
    std::string name;            // parameter id / label
    std::string text;            // custom code
    int index = 0;               // input index, mask bits...
};

struct EffectGraph {
    std::string name = "Effet visuel";
    std::string category = "Graphes";
    int inputs = 1;              // image inputs (0 = generator)
    bool premultiplied = false;
    std::vector<EGNode> nodes;
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
const EGOpDef* FindOp(const std::string& op);

// Code generation -----------------------------------------------------------
struct EGCompileResult {
    std::string code;            // Shaderixel effect text (metadata + body)
    std::string error;
    std::vector<Id> errorNodes;
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
