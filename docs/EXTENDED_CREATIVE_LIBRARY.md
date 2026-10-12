# Extended Creative Library

16 new shaders: the 15 creative effects plus a companion filter for height-map contours. 20 editable subgraphs: one wrapper per shader plus four compositions. All names and controls use English.

## Install and use

Repository files are mirrored as additions into `dist/Shaderixel-20261005/shaders/effects/` and `dist/Shaderixel-20261005/functions/`. Effects appear by category; functions appear in the node library under Creative Draw, Creative Filter and Creative Blend. Double-click a function to edit its composition. Shader internals are HLSL; these additions do not claim a native primitive-only effect-editor graph.

## Input conventions

- Colors and color controls use linear RGB. Coverage masks use linear red multiplied by alpha; use Channel to Coverage Mask to extract alpha or decode numeric PNG masks first.
- Depth Transition and Topographic Lines from Height decode numeric grayscale PNG values by default. Turn decoding off for live linear coverage or raw linear scalar inputs. Unconnected depth uses 0.5.
- Ink Bleed expands existing alpha. Convert opaque scans to transparent colored ink first. Screen Print Wear removes alpha; composite over paper to see missing ink.
- Selective Detail and Detail Transfer use an alpha-weighted sparse nine-tap reference. Their radius defines sample spacing, not a Gaussian blur sigma. Detail Transfer retains background alpha.
- Edge Reveal uses an eight-ray bounded distance approximation, not an exact distance transform. Search Reach limits the animated front; progress 1 always reveals the entire supplied mask. Without a mask, foreground alpha supplies its contour.
- Alpha Edge Padding searches the nearest qualifying source within a circular radius. A tiny carrier alpha (default 0.0001) preserves RGB through premultiplied storage. It rounds to zero on native 8-bit PNG export; float and 16-bit outputs retain tiny nonzero alpha. Apply padding last, use 16/32-bit document precision, and avoid another operation that thresholds alpha. Padding is local rather than a global flood fill.
- All procedural seeds are deterministic. Pixel settings are document-space pixels; UV centers and paper half sizes are fractions of document dimensions.

## Added files

- `shaders/effects/art_ink_bleed.slang` â€” Spread colored alpha ink into irregular absorbent edges; opaque scans should first become ink on transparency.
- `shaders/effects/art_screen_print_wear.slang` â€” Remove ink alpha with deterministic holes and elongated scratches, optionally restricted by a linear coverage mask.
- `shaders/effects/art_crosshatch_shading.slang` â€” Convert image shadows into up to four directional hatch layers while preserving source alpha.
- `shaders/effects/texture_selective_detail.slang` â€” Sparse alpha-weighted high-pass detail with luminance gating and a flat-region threshold.
- `shaders/effects/texture_alpha_edge_padding.slang` â€” Extend nearest nontransparent RGB into empty alpha; tiny carrier alpha quantizes to zero in 8-bit PNG.
- `shaders/effects/draw_terrazzo.slang` â€” Deterministic convex polygon chips with three palette colors, density and size variation.
- `shaders/effects/draw_topographic_lines.slang` â€” Procedural terrain contours with pixel-sized lines and emphasized major contours.
- `shaders/effects/art_topographic_from_height.slang` â€” Extract styled contours from an existing grayscale height map; PNG numeric decoding is on by default.
- `shaders/effects/draw_flow_lines.slang` â€” Curved contours of a smooth directional field with gradient-corrected stroke width.
- `shaders/effects/draw_isometric_tiles.slang` â€” Isometric diamond or three-face cube tiling with independent palette, border and pixel offset.
- `shaders/effects/draw_torn_paper.slang` â€” Rotatable paper cutout with multiscale torn edges, fine grain and transparent surroundings.
- `shaders/effects/blend_depth_transition.slang` â€” Reveal the foreground from low to high depth, with explicit numeric PNG decoding and endpoint control.
- `shaders/effects/blend_noise_dissolve.slang` â€” Progressive procedural noise reveal with optional linear coverage mask and exact transition endpoints.
- `shaders/effects/blend_edge_reveal.slang` â€” Reveal inward from a mask contour using bounded eight-direction distance approximation; progress one reveals the full mask.
- `shaders/effects/blend_detail_transfer.slang` â€” Transfer source high-frequency luminance or color detail while retaining background alpha and broad tone.
- `shaders/effects/blend_protected_highlights.slang` â€” Alpha-correct foreground compositing with independent soft highlight and shadow protection.
- `library/functions/Creative Filter Ink Bleed.sxfn` â€” Spread colored alpha ink into irregular absorbent edges; opaque scans should first become ink on transparency.
- `library/functions/Creative Filter Screen Print Wear.sxfn` â€” Remove ink alpha with deterministic holes and elongated scratches, optionally restricted by a linear coverage mask.
- `library/functions/Creative Filter Crosshatch Shading.sxfn` â€” Convert image shadows into up to four directional hatch layers while preserving source alpha.
- `library/functions/Creative Filter Selective Detail.sxfn` â€” Sparse alpha-weighted high-pass detail with luminance gating and a flat-region threshold.
- `library/functions/Creative Filter Alpha Edge Padding.sxfn` â€” Extend nearest nontransparent RGB into empty alpha; tiny carrier alpha quantizes to zero in 8-bit PNG.
- `library/functions/Creative Draw Terrazzo.sxfn` â€” Deterministic convex polygon chips with three palette colors, density and size variation.
- `library/functions/Creative Draw Topographic Lines.sxfn` â€” Procedural terrain contours with pixel-sized lines and emphasized major contours.
- `library/functions/Creative Filter Topographic Lines from Height.sxfn` â€” Extract styled contours from an existing grayscale height map; PNG numeric decoding is on by default.
- `library/functions/Creative Draw Flow Lines.sxfn` â€” Curved contours of a smooth directional field with gradient-corrected stroke width.
- `library/functions/Creative Draw Isometric Tiles.sxfn` â€” Isometric diamond or three-face cube tiling with independent palette, border and pixel offset.
- `library/functions/Creative Draw Torn Paper.sxfn` â€” Rotatable paper cutout with multiscale torn edges, fine grain and transparent surroundings.
- `library/functions/Creative Blend Depth Transition.sxfn` â€” Reveal the foreground from low to high depth, with explicit numeric PNG decoding and endpoint control.
- `library/functions/Creative Blend Noise Dissolve.sxfn` â€” Progressive procedural noise reveal with optional linear coverage mask and exact transition endpoints.
- `library/functions/Creative Blend Edge Reveal.sxfn` â€” Reveal inward from a mask contour using bounded eight-direction distance approximation; progress one reveals the full mask.
- `library/functions/Creative Blend Detail Transfer.sxfn` â€” Transfer source high-frequency luminance or color detail while retaining background alpha and broad tone.
- `library/functions/Creative Blend Protected Highlights.sxfn` â€” Alpha-correct foreground compositing with independent soft highlight and shadow protection.
- `library/functions/Creative Engraved Illustration.sxfn` â€” Crosshatching and colored source contours printed on textured paper. Hatching and transparent edge ink are separate editable outputs.
- `library/functions/Creative Worn Screen Print.sxfn` â€” Halftone colored ink, irregular spread and mask-controlled missing ink over procedural paper. Outputs include transparent ink for reuse.
- `library/functions/Creative Paper Collage.sxfn` â€” Two torn paper pieces over a paper background. An optional image is clipped onto the front piece; the back piece carries terrazzo chips. Outputs retain reusable piece transparency.
- `library/functions/Creative Procedural Map Background.sxfn` â€” Terrain contour generator with a secondary flow-line layer and a soft noise reveal. All intermediate layers remain separate outputs.

## Portable example

Drag `examples/creative_library_demo.sxl` into Shaderixel to open a self-contained 1024 x 768 document. The four quadrants show Engraved Illustration, Worn Screen Print, Paper Collage and Procedural Map Background, in reading order. No external images or effect IDs are required; all compositions and shader definitions are embedded. The companion PNG is the native document export, not a UI screenshot.

- `examples/creative_library_demo.sxl` — Editable portable demonstration of the four compositions.
- `examples/creative_library_demo_preview.png` — Native PNG render of the portable demonstration.
- `docs/EXTENDED_CREATIVE_LIBRARY.md` — Installation, input conventions, known limitations and complete created-file inventory.

## Distribution copies

The following are byte-identical installed copies of the source presets above; their controls and purpose match the corresponding source file.

- `dist/Shaderixel-20261005/shaders/effects/art_ink_bleed.slang` — Spread colored alpha ink into irregular absorbent edges; opaque scans should first become ink on transparency.
- `dist/Shaderixel-20261005/shaders/effects/art_screen_print_wear.slang` — Remove ink alpha with deterministic holes and elongated scratches, optionally restricted by a linear coverage mask.
- `dist/Shaderixel-20261005/shaders/effects/art_crosshatch_shading.slang` — Convert image shadows into up to four directional hatch layers while preserving source alpha.
- `dist/Shaderixel-20261005/shaders/effects/texture_selective_detail.slang` — Sparse alpha-weighted high-pass detail with luminance gating and a flat-region threshold.
- `dist/Shaderixel-20261005/shaders/effects/texture_alpha_edge_padding.slang` — Extend nearest nontransparent RGB into empty alpha; tiny carrier alpha quantizes to zero in 8-bit PNG.
- `dist/Shaderixel-20261005/shaders/effects/draw_terrazzo.slang` — Deterministic convex polygon chips with three palette colors, density and size variation.
- `dist/Shaderixel-20261005/shaders/effects/draw_topographic_lines.slang` — Procedural terrain contours with pixel-sized lines and emphasized major contours.
- `dist/Shaderixel-20261005/shaders/effects/art_topographic_from_height.slang` — Extract styled contours from an existing grayscale height map; PNG numeric decoding is on by default.
- `dist/Shaderixel-20261005/shaders/effects/draw_flow_lines.slang` — Curved contours of a smooth directional field with gradient-corrected stroke width.
- `dist/Shaderixel-20261005/shaders/effects/draw_isometric_tiles.slang` — Isometric diamond or three-face cube tiling with independent palette, border and pixel offset.
- `dist/Shaderixel-20261005/shaders/effects/draw_torn_paper.slang` — Rotatable paper cutout with multiscale torn edges, fine grain and transparent surroundings.
- `dist/Shaderixel-20261005/shaders/effects/blend_depth_transition.slang` — Reveal the foreground from low to high depth, with explicit numeric PNG decoding and endpoint control.
- `dist/Shaderixel-20261005/shaders/effects/blend_noise_dissolve.slang` — Progressive procedural noise reveal with optional linear coverage mask and exact transition endpoints.
- `dist/Shaderixel-20261005/shaders/effects/blend_edge_reveal.slang` — Reveal inward from a mask contour using bounded eight-direction distance approximation; progress one reveals the full mask.
- `dist/Shaderixel-20261005/shaders/effects/blend_detail_transfer.slang` — Transfer source high-frequency luminance or color detail while retaining background alpha and broad tone.
- `dist/Shaderixel-20261005/shaders/effects/blend_protected_highlights.slang` — Alpha-correct foreground compositing with independent soft highlight and shadow protection.
- `dist/Shaderixel-20261005/functions/Creative Filter Ink Bleed.sxfn` — Spread colored alpha ink into irregular absorbent edges; opaque scans should first become ink on transparency.
- `dist/Shaderixel-20261005/functions/Creative Filter Screen Print Wear.sxfn` — Remove ink alpha with deterministic holes and elongated scratches, optionally restricted by a linear coverage mask.
- `dist/Shaderixel-20261005/functions/Creative Filter Crosshatch Shading.sxfn` — Convert image shadows into up to four directional hatch layers while preserving source alpha.
- `dist/Shaderixel-20261005/functions/Creative Filter Selective Detail.sxfn` — Sparse alpha-weighted high-pass detail with luminance gating and a flat-region threshold.
- `dist/Shaderixel-20261005/functions/Creative Filter Alpha Edge Padding.sxfn` — Extend nearest nontransparent RGB into empty alpha; tiny carrier alpha quantizes to zero in 8-bit PNG.
- `dist/Shaderixel-20261005/functions/Creative Draw Terrazzo.sxfn` — Deterministic convex polygon chips with three palette colors, density and size variation.
- `dist/Shaderixel-20261005/functions/Creative Draw Topographic Lines.sxfn` — Procedural terrain contours with pixel-sized lines and emphasized major contours.
- `dist/Shaderixel-20261005/functions/Creative Filter Topographic Lines from Height.sxfn` — Extract styled contours from an existing grayscale height map; PNG numeric decoding is on by default.
- `dist/Shaderixel-20261005/functions/Creative Draw Flow Lines.sxfn` — Curved contours of a smooth directional field with gradient-corrected stroke width.
- `dist/Shaderixel-20261005/functions/Creative Draw Isometric Tiles.sxfn` — Isometric diamond or three-face cube tiling with independent palette, border and pixel offset.
- `dist/Shaderixel-20261005/functions/Creative Draw Torn Paper.sxfn` — Rotatable paper cutout with multiscale torn edges, fine grain and transparent surroundings.
- `dist/Shaderixel-20261005/functions/Creative Blend Depth Transition.sxfn` — Reveal the foreground from low to high depth, with explicit numeric PNG decoding and endpoint control.
- `dist/Shaderixel-20261005/functions/Creative Blend Noise Dissolve.sxfn` — Progressive procedural noise reveal with optional linear coverage mask and exact transition endpoints.
- `dist/Shaderixel-20261005/functions/Creative Blend Edge Reveal.sxfn` — Reveal inward from a mask contour using bounded eight-direction distance approximation; progress one reveals the full mask.
- `dist/Shaderixel-20261005/functions/Creative Blend Detail Transfer.sxfn` — Transfer source high-frequency luminance or color detail while retaining background alpha and broad tone.
- `dist/Shaderixel-20261005/functions/Creative Blend Protected Highlights.sxfn` — Alpha-correct foreground compositing with independent soft highlight and shadow protection.
- `dist/Shaderixel-20261005/functions/Creative Engraved Illustration.sxfn` — Crosshatching and colored source contours printed on textured paper. Hatching and transparent edge ink are separate editable outputs.
- `dist/Shaderixel-20261005/functions/Creative Worn Screen Print.sxfn` — Halftone colored ink, irregular spread and mask-controlled missing ink over procedural paper. Outputs include transparent ink for reuse.
- `dist/Shaderixel-20261005/functions/Creative Paper Collage.sxfn` — Two torn paper pieces over a paper background. An optional image is clipped onto the front piece; the back piece carries terrazzo chips. Outputs retain reusable piece transparency.
- `dist/Shaderixel-20261005/functions/Creative Procedural Map Background.sxfn` — Terrain contour generator with a secondary flow-line layer and a soft noise reveal. All intermediate layers remain separate outputs.

## Validation

- Both `build/Release/Shaderixel.exe --test-effects` and the installed `dist/Shaderixel-20261005/Shaderixel.exe --test-effects` completed normally: 403 / 403 effects compiled. Both runs were launched minimized and allowed to exit themselves.
- The installed distribution recognized all 20 new functions with the expected image, numeric, color and vector ports. All shader references resolve.
- 30 native GPU fixture documents rendered successfully, including all Draw layers, all function wrappers and all four compositions. Fifteen byte-exact comparisons confirmed neutral filters, transition endpoints, constant-detail identity and padding wrapper equivalence.
- Numeric depth ordering, highlight protection, transparent RGB padding after native PNG export, constant height-map handling and semi-transparent source-over compositing were checked separately.
- The portable example was saved by the installed distribution, reopened there, and exported by the current build. It contains no external effect IDs or image files.
- Existing preset files were preserved; all 36 distribution additions match their source preset bytes.
