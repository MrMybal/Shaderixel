#pragma once
// HLSL import: annotated HLSL (comments //@in, //@out, ...) or a plain HLSL
// function becomes a custom node of the visual effect editor, with its inputs
// and outputs declared, and a complete graph around it (textures, UV, output).
//
// Annotations (all optional):
//   //@name Effect name             //@category Category        //@kind effect|blend
//   //@in  <name> <type> [default...] [min max] ["Label"] ["Category"]
//   //@out <name> <type> ["Category"]
//   //@main <function>   function to call (default: the last function of the code)
// Types: float float2 float3 float4 int bool color texture.
// With a function, its parameters give the inputs (Texture2D = texture, out = output,
// SamplerState ignored) and the return value the first output; //@in / //@out lines
// with the same names add defaults, ranges, labels and categories. Without a
// function, the code is the body of the node (it returns its single output).
// Textures are read with Texture2DSample(T, S, uv) or T.Sample(S, uv).

#include "doc/EffectGraph.h"

namespace sx {

struct HlslImport {
    EGNode node;           // custom node (pins, outs, decl, text)
    std::string name = "Effet HLSL", category = "Mes effets";
    EGKind kind = EGKind::Effect;
    std::string error;
};

HlslImport ParseHlslNode(const std::string& source);
// Complete graph: texture inputs, UV / canvas size sources, the node, the output.
bool GraphFromHlsl(const std::string& source, EffectGraph& graph, std::string* error);
// Adds the node (and its texture / UV sources) to an existing graph at `pos`; returns the node id.
Id AddHlslNode(EffectGraph& graph, const HlslImport& imp, float2 pos);

}  // namespace sx
