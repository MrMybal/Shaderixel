# Build and share effects, blends and brushes

Effect Editor builds the shader inside one effect. A reusable image .sxfn assembles whole effects; a shader function lives inside the effect graph. These formats have different ports and execution rules.

## A first generator

1. Create a custom visual-effect fill layer or edit a copy in Effect Editor.
2. Choose an Effect graph with zero image inputs for a generator.
3. Add a Color parameter and a Circle mask. Connect the color to Output RGB and the mask to Output Alpha. Connected RGB/Alpha override their respective components of a connected RGBA input.
4. Add a Vec2 parameter for the circle center and a Float parameter for radius. Set useful defaults and ranges.
5. Compile, inspect transparent edges and change the exposed values in the parent document. Save a user copy with a stable name/category.

## A first image distortion

Connect UV to Rotate UV, then to Sample input UV, and Sample RGBA to Output RGBA. Select the correct image index. Angle is in radians; pi/2 is a quarter turn. Set the center to (0.5,0.5). Changing only UV data without sampling it will show coordinates rather than the rotated source.

Full-image sampling support is generated for arbitrary UV sampling, custom HLSL and neighbor-sampling nodes. In handwritten source, declare //@halo for every neighbor read: a fixed radius, a parameter-based radius or full. Tiled large-image output must match normal rendering.

## A custom blend

Choose Blend kind. It supplies background A, layer B and mask. Build the desired RGB blend from those inputs, then feed Compose Blend together with A/B, mask and layer opacity. Return its RGBA. A color-only formula is insufficient to compose transparent coverage correctly.

## A brush graph

Choose New (graph) in Brushes. Brush Dab supplies local coordinates, color, pressure and pen channels. Use Round Tip as coverage and the brush color as RGB. Brush graphs have no ordinary image input; Brush Texture supplies up to four image slots instead. Configure spacing, timed airbrush and optional scatter/size/angle/opacity variations in brush graph settings.

Follow, tilt angle, tilt size, pen twist and speed-to-size mappings only act when configured and supplied by the input. Preview strokes use simulated channels; validate hardware response separately. Extent must cover scattered or enlarged dabs.

## HLSL nodes and exposed pins

Legacy Custom HLSL has A/B/C/D float4 inputs and returns float4. Declared pins support float, float2/3/4, texture, int, bool and color. One output uses a return value; several outputs use named out parameters. Declare helpers in the node declaration field rather than placing unrelated code into another node.

An unconnected non-texture declared input becomes an effect parameter using its default, range, label and group. An unconnected texture reads white. Connect Texture input handles only to Texture pins; use Texture2DSample inside the custom node. A texture is not a sampled color until read at UV.

## Handwritten source contract

```hlsl
//@name Adjustable tint
//@category User/Color
//@halo 0
//@param tint color 1 0.8 0.6 1 "Tint"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    return float4(c.rgb * tint.rgb, c.a);
}
```

The .slang file stem is the stable ID; use lowercase letters, numbers and underscores. Colors are linear and normally straight alpha. A resampling effect that declares //@premultiplied must read and return the corresponding convention. Coordinates refer to the whole document even in tiled rendering. Do not clamp HDR, signed distances or UV fields unless intended.

Parameters support float, int, bool, color, float2/3/4, enum, curve and LUT controls. A shader has up to four image inputs and 32 parameters. //@passes requests 1–16 passes; ctx.pass identifies the pass and Prev/PrevUV read the previous result. A generator has //@generator; a blend has //@blend and background/layer/mask inputs.

## Reusable functions and testing

In a shader graph, collapse a selection into a function to create named function ports; nested calls are expanded during compilation. In an image graph, create a subgraph/tool, expose important variables and save it in Library. Keep .sxfn functions pixel-free, refer only to installed effects, and preserve stable parameter IDs when exposing them as pins.

Compile in the editor (Ctrl+Enter in the applicable code view), inspect Shader Errors and the log, then test several parameter extremes, alpha edges and final-size output. For a development checkout, Shaderixel.exe --test-effects compiles shipped effects and writes [test-effects] results to shaderixel.log. Unreal export has its own supported-node constraints; a successful local shader is not proof of an equivalent engine material.

[Complete built-in nodes](#builtin_nodes)
[Effect and preset authoring](#authoring)
[Brushes](#brushes_tablet)
