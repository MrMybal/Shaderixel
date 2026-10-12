# Graph layers and graph effects

This addition provides eight Draw generators, eight image effects and twenty main-graph functions. Names and controls use English as the base language. The sixteen `.slang` files preserve their editable visual graphs in `//@graph` metadata. Their graphs use native mathematical, color, sampling and procedural nodes; none contains a custom HLSL node.

## Two graph editors

**Main graph:** insert a `Graph Layers ...` or `Graph Effects ...` function from the node library. Its input pins expose principal controls; its variables expose the remaining settings. Double-click it to inspect the assembled image graph. Generator functions require no source image: connect their `Layer` output to a blend or to the document output.

**Visual effect graph:** select one of the new underlying effects, open the effect editor, and choose **Modify a copy (graph)**. The button is localized by Shaderixel; the French interface calls it **Modifier une copie (graphe)**. This creates an editable copy in the document and leaves the delivered library file intact. The same operation works on a new Draw fill layer, an image-effect node or an effect inside a function.

The `.sxfn` functions and the `//@graph` definitions are different graph formats. Functions connect image effects, values and layers. A visual effect graph defines the internal per-pixel calculation that compiles into an effect. Both are supplied here.

## Draw layers

| Layer | Controls and behavior |
|---|---|
| Gradient | Start/end RGBA colors, center in UV, angle in degrees and span relative to the shorter image side. Geometry preserves aspect ratio. |
| Rings | Center in UV, spacing and width in pixels, softness and ring/background RGBA. Zero width leaves only the background. |
| Dot Grid | Pixel spacing, radius, softness and offset, with dot/background RGBA. Radius is limited to half the spacing. Zero radius leaves only the background. |
| Stripes | Pixel period, angle, coverage, phase and two RGBA colors. Coverage zero gives the background; coverage one gives the stripe color. |
| Rounded Panel | Center in UV, half size in pixels, radius and edge softness. Corner radius cannot exceed the smaller half size. Outside the panel is transparent. |
| Soft Noise | Deterministic value noise with pixel scale, offset, contrast and two RGBA colors. Offset changes the sampled region. |
| Woven Pattern | Alternating warp/weft colors, pixel thread period and cosine shading. A graphic weave pattern, without physical material calibration. |
| Glow Disc | Soft radial disc on transparency, with UV center, pixel radius, falloff and opacity. This draws a disc; it does not blur a source image. |

Positive image Y points down. Consequently, angle 90 runs vertically down for gradients and displacement. Pattern coordinates use the whole document and remain stable during tiled rendering. Small pattern spacing can alias in reduced previews; inspect at full scale when needed. Generated color alpha controls the layer's coverage.

## Image effects

Most effect functions accept **Image**, **Optional Mask** and **Strength**. An unconnected mask gives full coverage. The wrapper mixes the processed image against the source in premultiplied linear light. Strength zero preserves the source; strength one uses the complete effect. The luminance-mask function instead produces a coverage output directly.

- **Perceptual Levels:** black point, white point and tone curve act in display RGB. Keep white above black. The remapped branch is limited to the display range; strength zero preserves the original floating-point values.
- **Dithered Posterize:** quantizes each display RGB channel to an integer number of levels, 2 to 64. Fractional level inputs are rounded down. Dither uses deterministic pixel noise; this is not a fixed indexed palette or error diffusion.
- **Gradient Map:** maps perceptual luminance between two linear RGB colors, with a tone curve. The original alpha is preserved; map-color alpha is ignored.
- **Soft Luminance Mask:** selects bright pixels using threshold and softness. Invert 0 selects highlights, 1 selects dark pixels; intermediate values interpolate between them. Source alpha limits coverage once. Output RGB is linear coverage with opaque alpha, suitable for mask processing inside the graph.
- **Color Wash:** tints in linear RGB. Luminance Preservation 1 rescales the tint to the source luminance; 0 uses a flat color. Tint alpha is ignored. Bright source pixels can create RGB values above one.
- **RGB Channel Shift:** samples red and blue at opposing pixel offsets while retaining central green and alpha. It creates chromatic color fringes without expanding source alpha.
- **Wave Warp:** displaces the image sinusoidally, with amplitude and period in pixels, phase and displacement angle. It resamples both color and alpha. Zero amplitude preserves the source.
- **Directional Relief:** subtracts two offset luminance samples and adds the difference to source RGB. Sampling distance, light angle, strength and mix are adjustable. It preserves alpha and can produce RGB outside the display range.

Channel shift, wave warp and relief read neighboring pixels and declare `//@halo full`. Their document boundaries clamp to the edge; they do not wrap a texture. All other new effects declare `//@halo 0`. The mask is linear coverage rather than a numeric data-map encoding; ordinary sRGB PNG export applies its transfer curve to mask RGB.

## Layer assemblies

**Patterned Background** blends gradient and noise, then composites transparent dots over them. It outputs the combined background and each of the three source layers.

**Glowing Panel** combines a rounded-panel Draw graph, a soft square outline and Gaussian glow. It outputs the finished panel, the face and the face with border. Leave room around the panel for the border and roughly three glow radii. This assembly reuses the existing outline/glow effects; their internals are shader code, while the panel's internals are a visual graph.

**Textured Stripes** blends stripe and noise layers and adds directional relief. Stripe and noise outputs remain separate. This creates a graphic surface pattern, without calibrated PBR maps.

**Graphic Color Stack** chains levels, dithered posterization and gradient mapping, then applies the overall strength and optional mask. Intermediate leveled and posterized images remain available as separate outputs.

## Created files

### Native visual-graph shaders in `shaders/effects/`

| File | Purpose |
|---|---|
| `graph_draw_gradient.slang` | Aspect-aware directional RGBA gradient. |
| `graph_draw_rings.slang` | Concentric pixel-scale rings with soft edges. |
| `graph_draw_dot_grid.slang` | Repeating antialiased dot layer. |
| `graph_draw_stripes.slang` | Rotatable pixel-scale stripe layer. |
| `graph_draw_rounded_panel.slang` | Rounded rectangle with transparent antialiased edges. |
| `graph_draw_soft_noise.slang` | Smooth deterministic value-noise color layer. |
| `graph_draw_woven_pattern.slang` | Alternating woven pattern with thread shading. |
| `graph_draw_glow_disc.slang` | Radial colored glow disc on transparency. |
| `graph_effect_levels.slang` | Perceptual RGB levels and tone curve. |
| `graph_effect_posterize.slang` | RGB quantization with deterministic dithering. |
| `graph_effect_gradient_map.slang` | Two-color perceptual luminance mapping. |
| `graph_effect_luminance_mask.slang` | Soft bright/dark selection into linear coverage. |
| `graph_effect_color_wash.slang` | Color wash with optional luminance preservation. |
| `graph_effect_channel_shift.slang` | Opposing red/blue offsets with central alpha. |
| `graph_effect_wave_warp.slang` | Directional sinusoidal image displacement. |
| `graph_effect_relief.slang` | Directional relief from neighboring luminance samples. |

### Main-graph functions in `library/functions/`

| File | Purpose |
|---|---|
| `Graph Layers Gradient.sxfn` | Gradient layer with color inputs and exposed geometry. |
| `Graph Layers Rings.sxfn` | Ring layer with color/spacing inputs and exposed geometry. |
| `Graph Layers Dot Grid.sxfn` | Dot layer with color/spacing inputs and exposed shape. |
| `Graph Layers Stripes.sxfn` | Stripe layer with color/period inputs and exposed angle/coverage. |
| `Graph Layers Rounded Panel.sxfn` | Panel layer with color/size inputs and exposed rounding. |
| `Graph Layers Soft Noise.sxfn` | Noise layer with ramp/scale inputs and exposed contrast/offset. |
| `Graph Layers Woven Pattern.sxfn` | Weave layer with color/period inputs and exposed shading. |
| `Graph Layers Glow Disc.sxfn` | Glow disc with color/radius inputs and exposed center/falloff. |
| `Graph Effects Perceptual Levels.sxfn` | Levels with optional mask and overall strength. |
| `Graph Effects Dithered Posterize.sxfn` | Adjustable quantization with optional mask and strength. |
| `Graph Effects Gradient Map.sxfn` | Two-color mapping with optional mask and strength. |
| `Graph Effects Soft Luminance Mask.sxfn` | Threshold selection with exposed softness and inversion. |
| `Graph Effects Color Wash.sxfn` | Tint input, optional mask and luminance preservation. |
| `Graph Effects RGB Channel Shift.sxfn` | Channel offset input with optional mask and strength. |
| `Graph Effects Wave Warp.sxfn` | Wave amplitude input with optional mask and exposed geometry. |
| `Graph Effects Directional Relief.sxfn` | Relief controls with optional mask and overall strength. |
| `Graph Layers Patterned Background.sxfn` | Gradient/noise/dot layer assembly with separate layer outputs. |
| `Graph Layers Glowing Panel.sxfn` | Rounded face, border and glow with intermediate outputs. |
| `Graph Layers Textured Stripes.sxfn` | Stripe/noise/relief assembly with source-layer outputs. |
| `Graph Effects Graphic Color Stack.sxfn` | Levels/posterize/map stack with intermediate image outputs. |

## Validation

The native minimized `--test-effects` run exited on its own with **387 / 387 effects compiled** and **198 / 198 effect graphs compiled**. The new sixteen visual graphs contain 475 primitive nodes and no custom HLSL nodes. Static checks validated effect IDs, parameter IDs, variable bindings, interface ports, cycles and all 195 typed main-graph connections across 38 effect references. All twenty functions were inserted successfully through the native library demo.

Forty-one temporary native GPU fixtures rendered successfully. Sixteen complete canvas comparisons between delivered shaders and native document copies compiled from their visual graphs matched exactly. Four additional comparisons verified neutral levels, zero channel offset, zero wave amplitude and zero relief depth against the original image, also with zero byte difference. The four composite functions rendered successfully; targeted checks confirmed the panel face and transparent exterior. These are rendered-preview comparisons, not full-precision RGBA export comparisons.

The 434 files present in the effects, function and documentation directories before this addition retain their original hashes. All native launches used minimized windows without real mouse or keyboard input.
