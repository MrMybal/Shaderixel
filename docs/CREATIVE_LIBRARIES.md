# Photo Essentials, Masks and Compositing, and 2D Game Art

This library adds **20 shaders and 18 functions**. English is the base language for
names, categories, controls, descriptions and execution buttons. It extends the
existing library without replacing any existing files. All processing is local;
no AI server, extra model or external workflow is required.

## Photo Essentials

Start with **Photo Essentials Develop** for white balance, exposure, shadow/highlight
balance, vibrance, local contrast and light sharpening. Use **Strength** to crossfade
with the source. Its optional **Mask** input accepts linear coverage: white applies
the complete grade and black keeps the source unchanged. With no mask, the effect
applies everywhere. Every photo preset preserves source alpha.

Use **Photo Essentials Shadows and Highlights** to adjust tones and expose reusable
shadow/highlight masks. **Photo Essentials Local Contrast and Sharpen** provides
independent structural and fine-detail radii. **Photo Essentials Split Tone Look**
and **Photo Essentials Grain and Vignette** create adjustable looks. Grain uses a
fixed seed, so it remains stable while editing. Tonal adjustments do not reconstruct
detail that was clipped in the original photo.

**Photo Essentials Exposure Bracket Export** is an explicit execution tool. Set the
exposure array and file prefix, then run **Export Exposure Bracket**. The default
array `[-1, 0, 1]` produces `photo_exposure_0.png`, `photo_exposure_1.png` and
`photo_exposure_2.png`. The suffix is the array index, not the exposure value.
The output retains the last baked image, and **Exposure Offset** ends at the last
array value. An empty array writes no files.

## Masks and Compositing

**Masks Color Key and Despill** removes a selected background color using color
distance, with optional matte contraction, feathering and spill suppression. This
is color keying, not semantic background removal. RGB distance includes brightness;
chroma distance compares normalized color. Outputs include the cutout, its coverage
mask and a checker preview.

**Masks Luminance Selection** selects a perceptual luminance band. **Masks Matte
Cleanup** expands or contracts a matte with a square neighborhood, feathers it,
remaps coverage and outputs the inverse as well. Coverage outside the document is
zero, so expansion and feathering remain clipped to the document bounds.

**Masks Alpha Extraction and Cleanup** reads source alpha directly and uses the
refined coverage to replace alpha, avoiding accidental alpha multiplication twice.
Defaults preserve source alpha exactly. Expanding alpha cannot reconstruct color
already lost in fully transparent source pixels.

**Masks Composite with Matte** expects background, foreground and an explicit matte.
Use a white matte to keep the whole foreground. The matte multiplies foreground
alpha before normal source-over compositing. Output includes composite, isolated
foreground and refined matte.

### Linear coverage versus imported grayscale data

The new mask effects store **linear coverage** in RGB with opaque alpha. Downstream
mask controls read the red channel directly. A coverage of `0.5` means half opacity,
even though its opaque grayscale preview may appear brighter because display uses sRGB.
Use these masks directly with the new mask controls and selective photo controls.

For a numeric grayscale PNG imported through Shaderixel's normal color-image path,
use **Masks Imported Matte to Coverage** first. With **Decode PNG Numeric Values**
enabled, an imported PNG byte value `128` becomes approximately `0.5` coverage.
Disable that option for an already linear coverage mask. Alpha is always read directly.
Channel selection supports R, G, B, alpha and luminance.

## 2D Game Art

Start with **Game Art Pixel Sprite Pipeline** to sample an integer pixel grid,
apply a four-color palette and add a pixel outline. Set block size to `1` for art
that is already at its intended pixel scale. Pixel-grid processing changes the
appearance without resizing the document. Ordered dither uses a deterministic 4x4 pattern.

**Game Art Outline and Shadow** adds square or round outlines and integer-aligned
shadows. Leave transparent padding around the sprite: the effects cannot draw
outside the document. Shadow softness is a small box-filter radius.

**Game Art Four Color Recolor** maps each pixel to its nearest palette color in
perceptual RGB space. Four colors are inputs; dither and palette strength are exposed
variables. Palette alpha can deliberately fade a mapped color.

**Game Art Sprite States** provides separate original, hit-flash, silhouette and
outline image outputs. These are live alternatives, not an animation timeline.
**Game Art Transparency Inspection** composites the sprite over checker, light and
dark backgrounds and outputs its alpha as coverage. Preview backgrounds are not
baked into the original sprite output.

**Game Art Hit Flash Export** exports flash variants. The default strength array
`[0, 0.35, 0.65, 1]` produces `sprite_flash_0.png` through `sprite_flash_3.png`.
It preserves alpha and retains the last variant in the output and **Flash Strength**
variable. Change the array to choose the number and strengths of the exported states.

## Export behavior

Exports run only when their explicit execution button is pressed. PNG exports use
8-bit sRGB color and straight alpha. Relative paths resolve beside a saved document,
or under `exports/` beside the executable for an unsaved document. Use a prefix
without an extension. Reusing the same prefix replaces matching files; change the
prefix to keep earlier variants. Clear stored results to return to live previews.

## Added shaders in `shaders/effects/`

| File | Description |
|---|---|
| `mask_apply_coverage.slang` | Apply Coverage Mask. |
| `mask_channel_selection.slang` | Channel to Coverage Mask. |
| `mask_color_selection.slang` | Color Range Mask. |
| `mask_edge_despill.slang` | Edge Color Despill. |
| `mask_feather_levels.slang` | Mask Feather and Levels. |
| `mask_luminance_selection.slang` | Luminance Range Mask. |
| `mask_morphology.slang` | Mask Expand and Contract. |
| `photo_film_finish.slang` | Photo Grain and Vignette. |
| `photo_local_contrast.slang` | Photo Local Contrast. |
| `photo_masked_mix.slang` | Masked Grade Mix. |
| `photo_split_tone.slang` | Photo Split Tone. |
| `photo_tone_balance.slang` | Photo Tone Balance. |
| `photo_white_balance.slang` | Photo White Balance. |
| `sprite_draw_checker.slang` | Sprite Transparency Checker. |
| `sprite_drop_shadow.slang` | Sprite Drop Shadow. |
| `sprite_hit_flash.slang` | Sprite Hit Flash. |
| `sprite_outline.slang` | Sprite Outline. |
| `sprite_palette_four.slang` | Sprite Four-Color Palette. |
| `sprite_pixel_grid.slang` | Sprite Pixel Grid. |
| `sprite_silhouette.slang` | Sprite Silhouette. |

## Added functions in `library/functions/`

| File | Description |
|---|---|
| `Game Art Four Color Recolor.sxfn` | Nearest-color palette remapping with optional ordered dither. |
| `Game Art Hit Flash Export.sxfn` | Bake and export hit-flash strength variants from a numeric array. |
| `Game Art Outline and Shadow.sxfn` | Integer-width outline and pixel-aligned shadow. |
| `Game Art Pixel Sprite Pipeline.sxfn` | Pixel grid, four-color palette, outline and checker preview. |
| `Game Art Sprite States.sxfn` | Separate original, flash, silhouette and outline outputs. |
| `Game Art Transparency Inspection.sxfn` | Checker, light/dark previews and exact alpha inspection. |
| `Masks Alpha Extraction and Cleanup.sxfn` | Exact alpha extraction and optional cleanup without applying alpha twice. |
| `Masks Color Key and Despill.sxfn` | Background color key, matte cleanup, edge despill and checker preview. |
| `Masks Composite with Matte.sxfn` | Foreground isolation and normal source-over compositing through a required matte. |
| `Masks Imported Matte to Coverage.sxfn` | Convert numeric grayscale PNG values into linear coverage. |
| `Masks Luminance Selection.sxfn` | Luminance-band selection and masked image. |
| `Masks Matte Cleanup.sxfn` | Matte expansion/contraction, feathering, levels and inverse mask. |
| `Photo Essentials Develop.sxfn` | White balance, tones, vibrance, cleanup, local contrast and sharpening with a selective mask. |
| `Photo Essentials Exposure Bracket Export.sxfn` | Bake and export exposure variants from a numeric array. |
| `Photo Essentials Grain and Vignette.sxfn` | Deterministic grain and aspect-aware edge darkening. |
| `Photo Essentials Local Contrast and Sharpen.sxfn` | Independent structure and fine-detail enhancement. |
| `Photo Essentials Shadows and Highlights.sxfn` | Shadow/highlight adjustment plus separate selection masks. |
| `Photo Essentials Split Tone Look.sxfn` | Shadow/highlight tinting with optional grain and vignette. |

## Validation

- `Shaderixel.exe --test-effects` launched minimized and closed autonomously with exit code `0`.
- `shaderixel.log`: **356 / 356 effects compiled**, including all 20 new shaders.
- Native `--demo library` insertion recognizes all 18 new function signatures, including arrays and multiple outputs.
- Structural checks validate 61 effect references and 260 typed links, parameter names,
  variable references, output ports and acyclic graphs.
- Eight native GPU render scenarios validate neutral white balance, zero-strength
  grade mixing, alpha cleanup, numeric PNG matte conversion, exact palette matching,
  outline extent, shadow offset and color keying. Five complete rendered-canvas
  comparisons match independent reference documents exactly; eight targeted pixel
  assertions verify foreground, outline and shadow behavior.
- SHA-256 checks confirm all 361 pre-existing shaders, presets and documentation files are unchanged.
- Validation uses CLI launches and temporary documents; no real mouse or keyboard input.
