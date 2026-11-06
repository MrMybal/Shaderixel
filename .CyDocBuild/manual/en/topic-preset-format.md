---
id: preset-format
title: Preset file formats
description: Choose shader metadata, typed ports and explicit execution for reusable presets.
section: Develop / Effects
order: 100
parent: authoring
revision: 0d7117ea885e9734
---

## Pick the format {#formats}

An image effect, Draw generator or blend is a `.slang` shader. A main-graph function is `.sxfn` JSON with `format: shaderixel-function`. Use `kind: subgraph` for live evaluation or `kind: tool` for explicit execution events. `applyMode: true` retains outputs after Apply; false uses Live.

A function containing retained pixels is saved as a native `.sxl` with its metadata rather than a pixel-free `.sxfn`. Start from a file saved by the editor to retain its current schema.

## Shader metadata {#metadata}

- `//@name` sets the display label; `//@category` sets a family/subcategory path.
- `//@param id type ... "Label"` declares an exposed value. `//@group` groups the following controls.
- `//@input` names image inputs, up to four. Without an explicit image input, a regular effect has Image. Named scalar-mask inputs have channel pins.
- `//@generator` removes image inputs; `//@blend` supplies background, layer and mask.
- `//@premultiplied` keeps sampled and returned color premultiplied.
- `//@passes N` requests one to sixteen passes; `ctx.pass` selects the pass and `Prev` reads its previous result.
- `//@halo 8`, `//@halo radius 3 2` and `//@halo full` declare a fixed neighborhood, a parameter-scaled neighborhood, or the full image. Declare support for every neighboring read.
- `//@user` and `//@locked` are editor-managed protection markers.

The filename stem is the stable shader ID. Keep it lowercase with digits and underscores. Colors use linear values; ordinary effects use straight alpha unless marked premultiplied. Context coordinates belong to the whole document, including tiled rendering.

## Controls and shader functions {#parameters}

Scalar types are `float`, `int` and `bool`. Vectors use `float2`, `float3`, `float4`; `color` is linear RGBA. `enum` stores a zero-based index with choice labels. `curve` and `lut` use their dedicated controls. There are at most 32 exposed parameters.

```hlsl
//@param radius float 8 0 64 "Radius"
//@param tint color 1 0.8 0.6 1 "Tint"
//@param mode enum 0 "Soft|Hard" "Mode"
```

The shader entry signature is `float4 Effect(EffectContext ctx)`. `ctx.px`, `ctx.uv`, `ctx.size`, `ctx.texel` and `ctx.pass` supply coordinates, dimensions and pass state. `Input(i, px)` reads a pixel; `InputUV(i, uv)` samples normalized UVs; `HasInput(i)` checks a connection; `Prev(px)` and `PrevUV(uv)` read the previous pass. Keep data values outside zero to one when the operation needs them.

## Function ports and connections {#function-ports}

`graph.nodes` stores unique integer node IDs, kinds, names, positions and ordered `inputs`. Zero means unconnected. `subinput` and `suboutput` define the public interface with `port` indices and typed `value` defaults. `graph.output` identifies the output previewed during function editing.

For an effect node, `effect.effect` is its stable shader ID and `effect.set` overrides parameter defaults. `paramPinNames` adds parameter pins after image inputs. All referenced effects must exist.

`inputChannels[i]` selects the source output for input i. Output k uses `k * 8`; split R/G/B/A use 1/2/3/4. A data link supplies a value; an Exec link requests execution. `event` nodes expose buttons, while connected actions can bake, upscale, call ComfyUI, export or update variables.

`graph.variables` contains typed values. `exposed: true` makes a variable editable on each function instance; its `desc` explains the control. Keep exposed names stable when other nodes address them.

[Worked shader examples](doc:shader-authoring) · [Graph execution](doc:automation)
