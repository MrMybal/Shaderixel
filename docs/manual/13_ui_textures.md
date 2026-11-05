# UI textures and slicing

Use 32-bit float for UV and signed-distance fields. A UV image is data; connect it through a sampler to see a skin instead of coordinate colors.

![Nine regions preserve corners while the middle repeats.](media/nine_slice_en.png)

![Map coordinates first, then sample the source image.](media/uv_chain_en.png)

## Nine-slice and eight-slice

1. Insert UI UV Nine Slice Panel from Library and connect the source skin.
2. Set source region and logical source size for an atlas.
3. Set margin units and left/top/right/bottom margins.
4. Set widget size in document pixels, position and anchor.
5. Choose Center mode and Edge mode independently.
6. Connect Image to Output. UV Map and Coverage are intermediate outputs.

Fractional margins refer to the source region. Pixel margins use logical source dimensions. Tile repeats the center; Mirror tile alternates direction. Transparent center creates an eight-slice border. Small destinations compress borders proportionally.

## Coordinate chains

Fields store R=U, G=V, B=coverage, A=1. Build coordinates, apply mapping/addressing/masks, then sample. UI UV Sample expects source image first, field second and optional mask third.

Clamp, repeat, mirrored repeat and transparent addressing differ at boundaries. Atlas gutters/insets reduce reads from adjacent cells. The field viewer shows coverage or distortion.

## Toolkit and export

UI Toolkit adds SDF shapes/Boolean operations, borders, flow/parallax, transitions, patterns and skin states. Signed-distance data needs float storage and should not pass through ordinary color adjustments.

Time, progress, hover, pressed and disabled are explicit graph values. These tools generate texture states; they do not register game-widget events.

UI UV Bake Skin PNG exports the whole canvas on Export PNG. For a tight sprite, match the document to the widget size. Package examples are examples/ui_uv_library_demo.sxl and examples/ui_toolkit_demo.sxl; their technical guides are in docs/.

[Main image graph](#image_graph)
[Export and delivery](#export)
