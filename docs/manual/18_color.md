# Color, precision and data maps

Shaderixel stores linear colors. Display and ordinary image import/export involve transfer encoding. Data maps need the convention of their own processing chain.

## Alpha and masks

Alpha describes coverage. Premultiplied sampling helps avoid dark fringes at filtered transparent edges.

New mask-library effects store linear coverage in RGB with opaque alpha. A value of 0.5 means half coverage even if its displayed gray looks brighter than byte 128. Use Masks Imported Matte to Coverage to decode numeric grayscale PNGs. Alpha is read directly.

## Numeric PBR maps

The texture_* library compensates for sRGB PNG export so exported bytes contain intended numeric data. In the engine disable sRGB for height, normals, roughness, metalness, occlusion and packed maps; keep it enabled for albedo.

A raw linear float export can instead contain compensated storage values. Check encoding before treating it as equivalent to numeric PNG data.

## UV and distance fields

UI fields use R=U, G=V, B=coverage, A=1. Signed distances can be negative. Use 32-bit float, avoid color adjustments/lossy intermediate exports, and inspect with a field viewer.

Connect a sampler or distance-to-coverage operation before expecting a normal visible image.

## Profiles, proofing and inspection

An image profile describes how color values should be interpreted; assigning an interpretation and converting pixel colors are different operations. Keep a known original when changing color-management settings. Soft proof and gamut warnings help inspect a destination profile but are display aids, distinct from CMYK TIFF delivery.

Choose histogram/analysis source deliberately: final image, selected layer, channel or selection can produce different statistics. False color, clipping and focus overlays do not paint those colors into the export. Before/after comparisons require the intended reference state; return to normal display for final assessment.

Use an 8-bit delivery for ordinary web/UI colors when adequate, 16-bit output for finer gradients where supported, and float storage for HDR/signed processing. Data maps, alpha and color RGB do not share one automatic transfer interpretation. Test a known middle value through the complete import/process/export chain when exact bytes matter.

## Preview

Inspect patterns at 100%; thumbnails can hide aliasing. Test exported bytes when exact values matter. Float precision preserves calculation range, but does not reconstruct clipped input or guarantee the export preserves that range.

[Photo to PBR maps](#pbr)
[UI textures and slicing](#ui_textures)
