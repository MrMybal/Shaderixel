# Print, scan and logo libraries

These libraries add 15 shader effects and 18 graph functions. All names, descriptions, controls and execution buttons use English as the base language. They contain no embedded images, external models or network actions.

## Quick start

- **Print:** connect an image to **Print Halftone Studio** or **Print Duotone Look**. Use **Print Two Ink Risograph** for two separately accessible pigment layers and misregistration.
- **Scans:** connect a paper scan to **Scan Extract Transparent Ink**. Its outputs are transparent colored ink, linear coverage and a paper preview. For uneven lighting, try **Scan Adaptive Ink Extraction**.
- **Logos:** connect rasterized text or a logo with a transparent background to any **Logo** function. Alpha supplies the silhouette; an opaque rectangular image produces a rectangular style.

Find functions in the node library. Input pins expose the principal image, colors and settings. Other controls are exposed under the function's variables. Double-click a function to inspect its graph. New `.slang` files also appear as individual effects; **Print Paper** is a Draw generator and **Print Ink Overprint** is a blend.

## Print and Halftone

Halftone spacing is measured in document pixels, independent of aspect ratio. `Pattern` selects 0 = dots, 1 = lines or 2 = crosshatch. `Transparent Paper` removes the unprinted background. Ink and paper alpha control opacity, then source alpha limits the result. Pure white has no ink and pure black has full ink with the default tone direction.

The screen modulates tone at each pixel. It does not average the source across each cell, so very fine source detail can modulate individual dots. Reduce detail upstream if you want a more uniform screened appearance. Small spacing can alias at reduced preview zoom; inspect at 100% or in an export.

The risograph function derives red- and blue-absorption plates from display RGB, then multiplies their pigments over generated paper. `Registration Offset` moves the second plate in pixels. Regions outside the document are transparent. This is an artistic print simulation, without printer profiles, spot-color separations or CMYK proofing.

`Print Artwork on Aged Paper` treats white artwork as unprinted paper. `Print Ink Overprint` accepts an optional mask: no connection means full coverage. Its alpha-correct blend also supports a transparent background. `Display-Space Mix` selects display-space pigment multiplication or linear-light multiplication.

`Print Duotone Look` keeps source alpha; its two fill colors use RGB. The tone curve operates on perceptual luminance. Paper generation is deterministic for a fixed seed, document size and settings. Grain, mottling and fibers are separate controls.

## Scan and Line Art

Paper normalization divides display RGB by a broad local brightness reference, then applies black/white points. Keep `White Point` above `Black Point`. A useful paper radius is larger than the scanned strokes but smaller than broad lighting variation. Large filled dark regions may be mistaken for paper shading: lower `Paper Correction` or use direct adaptive extraction.

Global extraction selects pixels darker than `Threshold`; increasing the threshold retains more pale strokes. Adaptive extraction compares each pixel with its blurred local paper reference minus `Local Bias`. A larger bias rejects more weak marks. Keep `Adaptive Radius` larger than stroke width. `Threshold Softness` controls the coverage transition; zero approaches a hard threshold.

`Expand Radius` uses square dilation to thicken strokes; zero preserves width. `Feather Radius` softens their coverage; zero preserves the threshold result. Both are limited to 16 pixels. Source alpha is included once during extraction. `Scan Recolor Transparent Line Art` instead uses existing alpha and expects paper already removed.

Coverage outputs are opaque grayscale **linear mask values**. They are ready for mask processing and recoloring in the graph. They are not numeric data-map encodings: an ordinary sRGB PNG export applies its transfer curve to coverage RGB. Transparent ink PNGs carry coverage in alpha and are the convenient output for compositing elsewhere.

`Scan Photo to Edge Line Art` uses Sobel luminance gradients. It extracts tonal boundaries, including texture and noise where present. These tools perform neither OCR nor semantic segmentation nor vector tracing.

## Text and Logo Styles

The gradient and reflection patterns use normalized coordinates across the document. Move `Gradient Center`, change `Gradient Span`, or shift `Reflection Phase` to fit the logo. Fill colors use RGB and preserve the face alpha. `Bevel Width` samples the silhouette's alpha gradient; this is a raster bevel, not a distance-field or 3D geometry operation.

`Extrusion Length` is 0 to 128 pixels. Angle 0 points right and angle 90 points down. Zero length is an identity operation. `Extrusion Fade` attenuates distant copies. The extrusion stays behind the original face.

Logo outlines use square dilation of soft alpha, with widths 0 to 32 pixels. They preserve fractional coverage rather than thresholding it into a binary outline. `Logo Sticker Border` stacks a white inner border, a dark outer border and an offset shadow; every border's width and color is exposed.

The neon function combines a bright face, colored tube outline and Gaussian alpha glow. `Glow Radius` is Gaussian sigma, 0 to 64 pixels; zero disables glow. Add document padding of approximately three radii around the silhouette. Borders, shadows and extrusions also need padding: these effects do not enlarge the document or recover pixels clipped at its boundary.

## Export tools

Export buttons explicitly bake and export at the current document resolution. Outputs retain baked images until function results are cleared. Relative paths resolve according to Shaderixel's document/export folder rules; absolute paths choose a specific destination. Parent folders must exist. Reusing a filename replaces that output.

| Tool | Button and files |
|---|---|
| Print Halftone Variants Export | **Export Halftone Variants** processes the input spacing array, default `[4, 8, 12]`. It exports `prefix_spacing_0.png`, `prefix_spacing_1.png`, etc. Index suffixes identify array positions, not spacing values. The final spacing and image remain selected. An empty array exports nothing. |
| Scan Line Art PNG Export | **Export Ink and Paper Preview** exports `prefix_ink.png` with transparency, then `prefix_paper.png` with the paper preview. Both baked outputs remain accessible. |
| Logo Chrome PNG Export | **Bake and Export Chrome Logo** exports one transparent styled image, default `logo_chrome.png`. |

## File inventory

### Shader effects

| File in `shaders/effects/` | Purpose |
|---|---|
| `print_duotone.slang` | Two-color tone mapping with source alpha and strength control. |
| `print_halftone_screen.slang` | Rotatable pixel-scale dots, lines and crosshatch with optional transparent paper. |
| `print_ink_plate.slang` | RGB absorption or darkness plate with pigment color, density and registration offset. |
| `print_draw_paper.slang` | Draw paper generator with fine grain, broad mottling and horizontal fibers. |
| `print_blend_ink.slang` | Alpha-correct masked pigment multiply overprint. |
| `scan_paper_normalize.slang` | Local paper correction and scan tone normalization. |
| `scan_ink_threshold.slang` | Global or adaptive dark-stroke extraction into linear coverage. |
| `scan_edge_coverage.slang` | Sobel luminance edges converted into linear coverage. |
| `scan_colorize_ink.slang` | Linear coverage converted into transparent colored ink. |
| `logo_gradient_fill.slang` | Adjustable document-space gradient inside source alpha. |
| `logo_chrome_fill.slang` | Procedural metallic reflection bands inside source alpha. |
| `logo_long_extrusion.slang` | Directional raster extrusion behind the source. |
| `logo_alpha_bevel.slang` | Directional highlight and shadow from the alpha gradient. |
| `logo_soft_outline.slang` | Two-pass square outline preserving soft alpha. |
| `logo_soft_glow.slang` | Two-pass Gaussian alpha glow with transparent outside boundaries. |

### Functions

| File in `library/functions/` | Purpose |
|---|---|
| `Print Duotone Look.sxfn` | Duotone colors, strength and perceptual tone curve. |
| `Print Halftone Studio.sxfn` | Dot, line or crosshatch screening with exposed ink and spacing. |
| `Print Two Ink Risograph.sxfn` | Two pigment plates, misregistration, paper and overprint. |
| `Print Artwork on Aged Paper.sxfn` | Artwork printed onto procedural paper; also outputs paper alone. |
| `Print Ink Overprint.sxfn` | Custom ink layer compositing with optional coverage mask. |
| `Print Halftone Variants Export.sxfn` | Tool baking and exporting multiple screen spacings. |
| `Scan Paper Cleanup.sxfn` | Denoise, local paper correction and tone normalization. |
| `Scan Extract Transparent Ink.sxfn` | Clean scan to transparent ink, coverage and paper preview. |
| `Scan Adaptive Ink Extraction.sxfn` | Local threshold extraction with exposed radius and bias. |
| `Scan Photo to Edge Line Art.sxfn` | Image to transparent colored edge strokes and paper preview. |
| `Scan Recolor Transparent Line Art.sxfn` | Recolor existing transparent strokes with optional width cleanup. |
| `Scan Line Art PNG Export.sxfn` | Tool exporting transparent ink and a paper preview. |
| `Logo Gradient Extrusion.sxfn` | Gradient face, raster bevel and long extrusion. |
| `Logo Neon Glow.sxfn` | Bright core, colored tube outline and Gaussian glow. |
| `Logo Chrome and Bevel.sxfn` | Metallic reflection bands, raster bevel and dark outline. |
| `Logo Sticker Border.sxfn` | Two adjustable borders and offset shadow. |
| `Logo Embossed Badge.sxfn` | Gradient face, directional bevel and soft shadow. |
| `Logo Chrome PNG Export.sxfn` | Tool baking and exporting a transparent chrome logo. |

## Verification

The native `--test-effects` run was launched minimized and allowed to exit on its own: **371 / 371 effects compiled**. All 18 functions were inserted through Shaderixel's native library demo. Static validation checked effect IDs, parameter IDs, variable references, contiguous interface ports, graph cycles and 286 compatible typed links across 65 effect references. The 400 files present in the effect, function and documentation directories before this addition retain their original hashes.

Twenty-three temporary native GPU render fixtures were captured after shader compilation. Ten complete canvas comparisons matched reference renders exactly, including zero-strength identities, flat paper, alpha-preserving recoloring, linear pigment overprint and pure-black/pure-white halftone endpoints. Nine targeted pixel assertions checked extrusion length and source preservation, fractional outline coverage and adaptive scan foreground/background extraction. Complete neon, chrome, gradient/extrusion, risograph and cleaned-scan graphs also rendered successfully.

The export tools' execution chains, channels, array inputs and filename construction were checked structurally against the delivered examples. These checks did not execute their export buttons. GUI captures are preview validation, not full-precision exported RGBA comparisons. All native launches used minimized windows and no real mouse or keyboard input.
