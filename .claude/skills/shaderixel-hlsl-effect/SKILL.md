---
name: shaderixel-hlsl-effect
description: Write an image effect, a generator or a blend mode for Shaderixel in HLSL, with its inputs, outputs and parameters declared in comments, then import it into Shaderixel (MCP tool shaderixel.effect_from_hlsl, a .hlsl file dropped on the window, or File > Importer un effet HLSL). Use when the user asks for a new Shaderixel effect, filter, generator, blend mode or shader node.
---

# Shaderixel HLSL effects

Shaderixel turns an annotated HLSL snippet into a node of its visual effect editor:
the node's inputs and outputs come from the code, unconnected inputs become
parameters (sliders, checkboxes, color pickers) grouped by category in the
Propriétés panel. Textures are wired to the effect's image inputs, `UV` to the
image coordinates, the first output to the effect's result.

## Two ways to write it

### 1. A function (preferred)

The last function of the code (or the one named by `//@main`) is the node:

- parameters = inputs; `out` / `inout` parameters = extra outputs
- return value = first output (name it with `//@out Name type`, default `Resultat`)
- `Texture2D X` = an image input (max 4), `SamplerState s` is accepted and ignored
- read a texture with `X.Sample(s, uv)`, `X.SampleLevel(s, uv, 0)` or `Texture2DSample(X, XSampler, uv)`
- helper functions and constants can be written above it

```hlsl
//@name Vignette douce
//@category Mes effets
//@in Amount float 0.6 0 1 "Intensité" "Réglages"
//@in Radius float 0.75 0.1 1.5 "Rayon" "Réglages"
//@in Tint color 0 0 0 1 "Couleur du bord" "Couleur"
//@out Result float4

float4 Vignette(Texture2D Image, SamplerState S, float2 UV, float Amount, float Radius, float4 Tint)
{
    float4 c = Image.Sample(S, UV);
    float d = length(UV - 0.5) * 1.41421;
    float k = smoothstep(Radius, Radius - 0.5, d);
    return float4(lerp(Tint.rgb, c.rgb, lerp(1.0, k, Amount)), c.a);
}
```

### 2. A body only

Without any function, the code is the body of the node: declare every input with
`//@in` and the output(s) with `//@out`, and `return` the single output.

```hlsl
//@name Gain
//@in Image texture
//@in UV float2
//@in Gain float 2 0 4 "Gain"
//@out Result float3
return Texture2DSample(Image, ImageSampler, UV).rgb * Gain;
```

## Annotations

| Line | Meaning |
|---|---|
| `//@name Name` | name of the effect (and of the node) |
| `//@category Category` | category in the effects gallery |
| `//@kind blend` | a blend mode: inputs are A (below), B (the layer), mask; the layer opacity drives a parameter named `Opacity` |
| `//@main Function` | function used as the node (default: the last one) |
| `//@in name type [default...] [min max] ["Label"] ["Category"]` | input: type, default, range, label, category |
| `//@out name type ["Category"]` | output name / type |

Types: `float float2 float3 float4 int bool color texture`.
Defaults: 1 number for float/int/bool (`true`/`false` accepted), 2/3/4 for vectors, 4 for color (r g b a, 0-1).
The range follows the default (`min max`), not for bool and color.
In a function, `//@in` lines only add details to parameters of the same name (and `color` turns a `float4` into a color picker).

## Rules

- Inputs named `UV` / `uv` (float2) receive the image coordinates: 0-1 over the whole document, pixel centers.
- A `float4` input whose name contains `CanvasSize` receives `(width, height, 1/width, 1/height)`.
- Every other unconnected input is a parameter of the effect.
- Colors are linear and straight (not premultiplied); return alpha in `.a`.
- A `float3` / `float` result is the RGB of the output; alpha comes from the first image (or 1 for generators).
- No texture input = a generator (fill layer).
- Do not name a function `main`; at most 4 textures, 32 parameters.

## Importing

1. **MCP (Shaderixel running, bridge active)**: call `shaderixel.effect_from_hlsl` with
   `{"code": "<the HLSL>", "target": "document"}` to add it to the open document
   (adjustment layer, fill layer for generators, or the blend of the active layer for `//@kind blend`),
   or `"target": "library"` to save it in the library (`"lock": true` protects it,
   `"replace": true` replaces your own item of the same name; built-in items are never overwritten).
   The shader is compiled first: a `CompileError` returns the compiler message; fix the code and call again.
   Writing to the document requires "Autoriser l'IA à modifier le document" in the Assistant IA panel.
2. **File**: save the code as `name.hlsl` and drop it on the Shaderixel window, or use
   Fichier > Importer un effet HLSL..., or paste it in Éditeur d'effets > « Importer du HLSL... ».

After importing, the effect is a graph in the Éditeur d'effets: the user can add nodes
before the inputs or after the output, and tune the parameters in Propriétés.
