#pragma once
// Effect definitions. An effect is a Slang/HLSL function `float4 Effect(EffectContext)`
// plus metadata comments describing its inputs and parameters:
//
//   //@name Gaussian Blur          display name
//   //@category Blur               menu category
//   //@generator                   no input (creates an image)
//   //@input Name                  declare an input (default: one input "Image")
//   //@passes 2                    multi-pass; Prev() reads the previous pass
//   //@premultiplied               work on premultiplied colors (blurs, distortions)
//   //@param <id> <type> <default...> [min max] ["Label"]
//        types: float, int (min max), bool, color (r g b a), float2 (x y),
//               enum <default> "A|B|C", curve (tone curve, shader gets <id>_eval(x)),
//               lut (3D color lookup loaded from a .cube file, shader gets
//               float3 <id>_sample(float3 rgb01) and <id>.x = size, 0 if none)
//
// The same format is used by library files (shaders/effects/*.slang), by custom
// HLSL nodes stored inside documents, and (later) by the visual effect editor,
// which will generate this code.

#include "core/Common.h"

#include <map>
#include <memory>

namespace sx {

enum class ParamType : u8 { Float, Int, Bool, Color, Float2, Enum, Curve, Lut };

// 3D color lookup table (.cube), embedded in documents.
struct Lut3D {
    std::string name;
    int size = 0;
    std::vector<float> rgb;  // size^3 * 3, red fastest
    u64 hash = 0;
};
using Lut3DPtr = std::shared_ptr<const Lut3D>;
Lut3DPtr LoadCubeLut(const std::string& path, std::string* error);
Lut3DPtr MakeLut(std::string name, int size, std::vector<float> rgb);
constexpr int kMaxLutParams = 3;  // bound at t5..t7

// Tone curve control points (x, y in 0..1, sorted by x).
using CurvePoints = std::vector<float2>;
constexpr int kCurveSlots = 64;  // 256 baked samples packed in float4s

// Monotone cubic interpolation (Fritsch-Carlson) sampled at 256 points.
void BakeCurve(const CurvePoints& pts, float out[256]);
float EvalCurve(const CurvePoints& pts, float x);

struct ParamDef {
    std::string id;
    std::string label;
    ParamType type = ParamType::Float;
    float4 def;
    float min = 0, max = 1;
    std::vector<std::string> enumItems;
};

struct EffectDef {
    std::string id;        // file stem for library effects, "" for inline
    std::string name;
    std::string category;
    std::vector<std::string> inputs;
    std::vector<ParamDef> params;
    int passes = 1;
    bool premultiplied = false;
    bool blend = false;
    std::string graph;     // //@graph <json>: source visual graph (library files saved from the graph editor)    // //@blend: a blend mode (inputs A below, B layer, mask), listed in Fusions, not in Effets
    std::string body;      // code after metadata (the Effect function)
    // Neighborhood read by the effect (tiled rendering): offset + sum(param * scale), or full image.
    bool haloDeclared = false, haloFull = false;
    float haloOffset = 0;
    std::vector<std::pair<std::string, float>> haloTerms;
    std::string error;     // metadata parse error
    std::vector<float4> DefaultParams() const;
};
using EffectDefPtr = std::shared_ptr<const EffectDef>;

// Parse metadata + body. `id` is stored as-is.
EffectDefPtr ParseEffect(const std::string& id, const std::string& text);

// Pixels of input needed around each output pixel; -1 = the whole image.
int EffectHalo(const EffectDef& def, const std::vector<float4>& values);

// Complete Slang source for an effect: prelude + generated params cbuffer + body + entry.
std::string BuildEffectSource(const EffectDef& def);
// cbuffer fields of the parameters (one float4 slot each, curves 64), as packed by PackParams.
std::string EffectParamFields(const EffectDef& def);

// Pack parameter values into the cbuffer layout used by BuildEffectSource
// (one float4 slot per param; int/bool/enum written as int bits).
std::vector<float4> PackParams(const EffectDef& def, const std::vector<float4>& values, const std::vector<CurvePoints>* curves = nullptr,
                               const std::vector<Lut3DPtr>* luts = nullptr);

class EffectLibrary {
public:
    void Load(const std::string& dir);
    void ReloadIfChanged();  // re-scan when shader files changed (hot reload)
    void Reload() { Load(dir); }
    EffectDefPtr Find(const std::string& id) const;
    const std::vector<EffectDefPtr>& All() const { return effects; }
    std::vector<std::string> Categories() const;
    u64 Generation() const { return generation; }

private:
    std::string dir;
    std::vector<EffectDefPtr> effects;
    std::map<std::string, EffectDefPtr> byId;
    u64 shaderGeneration = ~0ull;
    u64 generation = 0;
};

EffectLibrary& Effects();

// Template for a new custom HLSL node.
extern const char* kCustomEffectTemplate;
// Template for a custom blend: inputs A (below), B (layer), mask.
extern const char* kCustomBlendTemplate;

}  // namespace sx
