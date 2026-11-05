# Visual effect graph and shader editor

Effect Editor builds one effect using per-pixel math, sampling, color and procedural nodes. The result compiles to shader code. The main graph assembles complete effects and images.

![Visual effect editor with connected calculations and live preview.](media/effect_graph_en.png)

## Modify an effect

Select an effect and choose Modify a copy (graph) when an editable graph is available. Supplied effects are protected; a copy preserves the original.

Connect the calculation to the effect output. Expose parameters with units and useful defaults. Inspect the preview and compiler messages. Missing inputs or compiler errors can prevent a result.

HLSL/Slang source editing is also available. Ctrl+Enter compiles in the applicable code editor. File > Import HLSL Effect imports annotated source.

## Effect kinds

- Filter: reads an image and processes it.
- Draw / generator: creates content without an image input.
- Blend: combines background, foreground and mask.

The .slang filename is the stable identifier. Renaming it can break saved references even if its displayed label is unchanged.

## Sampling and export

Shaders reading neighboring pixels must declare a halo: fixed radius, parameter-dependent radius or full-image support. Otherwise tiled rendering can differ from the preview.

Use appropriate alpha conventions when resampling edges. Inspect transparency and full-scale output. Exporting an Unreal material is separate from saving an effect or a document; verify supported behavior in the target engine.

[Authoring effects and reusable presets](#authoring)
[Color, precision and data maps](#color)

[Step-by-step shader, blend and brush authoring](#shader_authoring)
[All 96 registered shader operations](#builtin_nodes)
