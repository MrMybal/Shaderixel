---
name: shaderixel-presets
description: Create Shaderixel presets - effects / filters, Draw layers (generators), blend modes and brushes as annotated .slang shaders, smart materials for Paint 3D (.json), and sub-graphs / custom tools as .sxfn function graphs (typed pins, variables, execution flow with bake, upscale, ComfyUI, export, loops). Use when the user asks for new Shaderixel effects, layers, blend modes, sub-graphs, tools, functions or a base library of them.
---

# Shaderixel presets

The full reference is **PRESETS.md** (next to this file in the package, `docs/PRESETS.md` in the repository).
Read it before writing anything: it gives the file formats, every metadata line, parameter types, node kinds,
pin types, operations (`math` ops), execution actions and the pitfalls.

## Quick map

| Preset | File | Folder |
|---|---|---|
| Effect (filter), Draw layer (`//@generator`), blend mode (`//@blend`) | `<id>.slang` | `shaders/effects/` |
| Sub-graph (`"kind": "subgraph"`), tool (`"kind": "tool"`) | `<Name>.sxfn` (JSON) | `library/functions/` (repo), `functions/` (package), `%LOCALAPPDATA%\Shaderixel\Functions\` (user) |
| Brush (2D and Paint 3D tips) | `<id>.slang` (`float4 Tip(Brush b)`) | `shaders/brushes/` |
| Smart material (Paint 3D, layers with smart masks) | `<Name>.json` | `library/smartmaterials/` (repo), `smartmaterials/` (package), `%LOCALAPPDATA%\Shaderixel\SmartMaterials\` (user) |

Both are hot-reloaded by a running Shaderixel.

## Workflow

1. Effects: copy the closest existing `shaders/effects/*.slang`, keep the `//@` metadata header, write
   `float4 Effect(EffectContext ctx)`; declare `//@halo` when reading neighbour pixels. Colors are linear,
   alpha not premultiplied unless `//@premultiplied`.
2. Functions: start from the closest file in `library/functions/` (six examples: parameter pins, several
   inputs, bake + export, upscale, a loop with a variable). Node ids are small unique integers; `inputs` follow
   the pin order; a link from output k of a multi-output node uses `inputChannels` = k * 8.
3. Check: the effect appears in Effets (a compile error shows in the node), the function in the node menu >
   Bibliothèque with the expected pins; double-click it to see its graph.
4. Effects referenced by functions must exist (`effect.effect` = the `.slang` file name).
