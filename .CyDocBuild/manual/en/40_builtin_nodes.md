---
id: builtin-nodes
title: Complete built-in node reference
description: This index covers every registered node operation in the shipped build.
section: Handbook
order: 39
parent: image-graph
revision: cbdf44d1624f2587
---

This index covers every registered node operation in the shipped build. Use the node branches below to choose the calculation system. F1 on a selected main-graph node or selected shader operation opens its matching reference; custom library effects and functions retain their own references.

## Choose the correct graph {#choose-the-correct-graph}

```image
{"id": "node-types", "src": "assets/manual/node_types_en.png", "alt": "Single values, image fields and per-pixel shader calculations", "controls": "minimal"}
```

- Main image graph: full images/channels, typed values, variables, arrays and explicit execution. Math Sin/Cos use degrees.
- Effect Editor: per-pixel scalar/vector math, image sampling and effect/blend/brush outputs. Shader trigonometry and UV rotation use radians.
- Recorded actions: a sequence of editor commands managed by Action Editor, separate from graph Exec links.

A green scalar image channel contains one number per pixel. A Float pin contains one typed value for the whole calculation. Converting the two graph systems into each other is not implicit. UV coordinates are data until an image is sampled at them.

## Read a reference {#read-a-reference}

Each operation has a stable identifier, purpose, ordered inputs, defaults, outputs and limits. The defaults describe a newly added operation. Contextual shader defaults such as current UV or brush hardness are labeled as such rather than displayed as negative sentinel values.

Bare structural nodes have dynamic interfaces: an effect gets its pins from shader metadata, a function from its inner ports, and a variable from its declared type. The instantiated Properties panel is authoritative for that node's current configuration.

## Troubleshoot connections {#troubleshoot-connections}

1. Confirm the editor: an image effect belongs in the image graph, arithmetic inside a shader belongs in Effect Editor.
2. Inspect the pin type and array state. A Texture handle only connects to a Texture pin in shader HLSL; it is not an RGBA value.
3. Choose the intended output: RGBA/RGB and individual channels differ, and execution actions often put Result after Exec.
4. Check disconnected defaults and expose important controls explicitly.
5. Connect image output for a visible result and run an event only when retained work or file output is intended.

[Main image graph](doc:image-graph)
[Shader authoring](doc:shader-authoring)
[Recorded actions](doc:actions)

## Image graph structure {#image-graph-structure}

[Document output](doc:node-output)

[Pixel surface](doc:node-surface)

[Image effect / generator](doc:node-effect)

[Image blend](doc:node-blend)

[Image channel split](doc:node-split)

[Legacy image tool](doc:node-tool)

[Image function instance](doc:node-function)

[Image function input](doc:node-subinput)

[Image function output](doc:node-suboutput)

[Typed constant / array](doc:node-value)

[Typed value operation](doc:node-math)

[Get variable](doc:node-varget)

[Set variable](doc:node-varset)

[Execution event](doc:node-event)

[Graph execution action](doc:node-action)

[Graph reroute](doc:node-reroute)

## Image graph values {#image-graph-values}

[Add](doc:math-add)

[Subtract](doc:math-sub)

[Multiply](doc:math-mul)

[Divide](doc:math-div)

[Minimum](doc:math-min)

[Maximum](doc:math-max)

[Power](doc:math-pow)

[Modulo](doc:math-mod)

[Absolute value](doc:math-abs)

[Opposite](doc:math-neg)

[Bottom rounding](doc:math-floor)

[Top rounding](doc:math-ceil)

[Rounding](doc:math-round)

[Square root](doc:math-sqrt)

[Sine (degrees)](doc:math-sin)

[Cosine (degrees)](doc:math-cos)

[Clamp](doc:math-clamp)

[Interpolate](doc:math-lerp)

[Remap](doc:math-remap)

[Seeded random](doc:math-random)

[Greater (A > B)](doc:math-greater)

[Less (A < B)](doc:math-less)

[Equal (A = B)](doc:math-equal)

[And](doc:math-and)

[Or](doc:math-or)

[No](doc:math-not)

[Select (if)](doc:math-select)

[Vec2 (X, Y)](doc:math-vec2)

[Split Vec2](doc:math-break-vec2)

[Color (R, G, B, A)](doc:math-color)

[Break color](doc:math-break-color)

[HSV color (hue 0–1)](doc:math-hsv)

[Mix colors](doc:math-mix-color)

[Append / concatenate](doc:math-concat)

[Number to text](doc:math-to-string)

[Text to name](doc:math-to-name)

[Text byte length](doc:math-text-length)

[To integer](doc:math-to-int)

[To float](doc:math-to-float)

[Array length](doc:math-array-length)

[Array element](doc:math-array-get)

[Array color element](doc:math-array-get-color)

[Array text element](doc:math-array-get-text)

[Numeric range](doc:math-range)

[Array sum](doc:math-array-sum)

## Image graph execution {#image-graph-execution}

[Sequence](doc:exec-sequence)

[Branch (if)](doc:exec-branch)

[For range](doc:exec-for-range)

[For each number](doc:exec-foreach)

[For each text](doc:exec-foreach-text)

[Bake](doc:exec-bake)

[Upscale](doc:exec-upscale)

[ComfyUI](doc:exec-comfyui)

[Export as PNG](doc:exec-export-png)

[Call function](doc:exec-call)

[Message](doc:exec-print)

## Per-pixel shader operations {#per-pixel-shader-operations}

[Input image](doc:shader-input)

[Sample input (UV)](doc:shader-sample)

[UV coordinates](doc:shader-uv)

[Input texture](doc:shader-texture)

[Canvas size (float4)](doc:shader-canvassize)

[Pixel position](doc:shader-pixel)

[Image size](doc:shader-size)

[Sample offset (pixels)](doc:shader-sampleoffset)

[Input blur](doc:shader-blur)

[Input edges (Sobel)](doc:shader-edges)

[Normal from input height](doc:shader-normalheight)

[Constant](doc:shader-const)

[Constant 2D](doc:shader-const2)

[Color](doc:shader-color)

[Pi](doc:shader-pi)

[Parameter (float)](doc:shader-param)

[Parameter switch](doc:shader-paramswitch)

[Parameter (color)](doc:shader-paramcolor)

[Parameter (Vec2)](doc:shader-param2)

[Add](doc:shader-add)

[Subtract](doc:shader-sub)

[Multiply](doc:shader-mul)

[Divide](doc:shader-div)

[Minimum](doc:shader-min)

[Maximum](doc:shader-max)

[Power](doc:shader-pow)

[Interpolation (lerp)](doc:shader-lerp)

[Clamp](doc:shader-clamp)

[Smoothstep](doc:shader-smoothstep)

[Threshold (step)](doc:shader-step)

[1 - x](doc:shader-oneminus)

[Absolute value](doc:shader-abs)

[Opposite](doc:shader-negate)

[Saturate (0-1)](doc:shader-saturate)

[Fractional part](doc:shader-frac)

[Bottom rounding](doc:shader-floor)

[Top rounding](doc:shader-ceil)

[Square root](doc:shader-sqrt)

[Sine](doc:shader-sin)

[Cosine](doc:shader-cos)

[Exp2](doc:shader-exp2)

[Log2](doc:shader-log2)

[If (A > B)](doc:shader-if)

[Tangent](doc:shader-tan)

[Arctangent (Y, X)](doc:shader-atan2)

[Modulo](doc:shader-fmod)

[Rounding](doc:shader-round)

[Sign](doc:shader-sign)

[Remap](doc:shader-remap)

[Posterize](doc:shader-posterize)

[Dot product](doc:shader-dot)

[Length](doc:shader-length)

[Distance](doc:shader-distance)

[Normalize](doc:shader-normalize)

[Cross product](doc:shader-cross)

[Split (R G B A)](doc:shader-split)

[Append / concatenate](doc:shader-append)

[Compose RGBA](doc:shader-make4)

[Compose RGB](doc:shader-make3)

[Make vector2](doc:shader-make2)

[Component mask](doc:shader-mask)

[Luminance](doc:shader-luma)

[Desaturate](doc:shader-desaturate)

[RVB vers TSV](doc:shader-rgb2hsv)

[TSV vers RVB](doc:shader-hsv2rgb)

[Linear to sRGB](doc:shader-tosrgb)

[sRGB to linear](doc:shader-tolinear)

[Contrast](doc:shader-contrast)

[Color ramp](doc:shader-colorramp)

[Hue shift](doc:shader-hueshift)

[Invert colors](doc:shader-invert)

[Noise](doc:shader-noise)

[Checker](doc:shader-checker)

[UV rotation](doc:shader-rotateuv)

[Offset UVs](doc:shader-offsetuv)

[Twirl (UV)](doc:shader-twirl)

[Voronoi (cells)](doc:shader-voronoi)

[Fractal noise](doc:shader-fbm)

[Radial gradient](doc:shader-radial)

[Linear gradient](doc:shader-lineargrad)

[Polar coordinates](doc:shader-polar)

[Tile UV](doc:shader-tiling)

[Circle](doc:shader-circle)

[Rectangle](doc:shader-rect)

[Layer opacity](doc:shader-opacity)

[Compose blend](doc:shader-blendover)

[Brush dab](doc:shader-brush)

[Brush texture](doc:shader-brushtex)

[Round tip](doc:shader-roundtip)

[Per-dab random](doc:shader-random)

[Custom HLSL](doc:shader-custom)

[Reroute](doc:shader-reroute)

[Function input](doc:shader-funcin)

[Function output](doc:shader-funcout)

[Function](doc:shader-function)

[Output](doc:shader-output)
