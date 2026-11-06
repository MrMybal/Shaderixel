---
id: delivery
title: Deliver UI, PBR and layered assets
description: Keep an editable native master and export a separate delivery.
section: Handbook
order: 41
parent: export
revision: 840eb5f6e6ce6c7a
---

Keep an editable native master and export a separate delivery. Confirm what the receiving application needs: a displayed color, a numeric map, packed channels, a layered document or an engine material.

```image
{"id": "delivery", "src": "assets/manual/delivery_en.png", "alt": "From editable source to verified delivered textures", "controls": "minimal"}
```

## Transparent UI images {#transparent-ui-images}

1. Finish the panel/text/icon composition at the desired pixel dimensions. Canvas zoom does not change export size.
2. Leave canvas space for shadows/glow, or crop intentionally. An opaque background generator removes transparency.
3. Use Export As with PNG and the intended alpha settings. JPEG needs a matte because it has no alpha.
4. Reopen the exported image and check dimensions and edges on both light and dark backgrounds. A nine-slice atlas also needs source margins and regions matching its exported layout.
5. Supply slice margins and coordinate origin to the consuming UI widget. A PNG does not contain an editable Shaderixel UV graph.

## Material export presets {#material-export-presets}

Export Project Textures assembles map channels and can bake height into the exported normal. Choose material set, folder, base name, resolution, format, normal convention and island padding. Data maps are written as data; set their import flags correctly in the receiving engine.

- Unreal Engine ORM: BaseColor, DirectX Normal, ORM with R=AO/G=roughness/B=metalness, Emissive.
- Unity HDRP: BaseMap, OpenGL Normal, MaskMap with R=metalness/G=AO/B=0/A=smoothness, Emissive.
- Unity URP/Built-in metallic: BaseMap, OpenGL Normal, MetallicSmoothness with RGB=metalness/A=smoothness, separate Occlusion and Emission.
- glTF/Blender: BaseColor, OpenGL Normal, OcclusionRoughnessMetallic with R=AO/G=roughness/B=metalness, Emissive.
- Godot: Albedo, OpenGL Normal, ORM with the same R/G/B packing, Emission.
- Separate textures: base color, roughness, metalness, DirectX normal, emissive, AO and height.

Smoothness is 1-roughness. DirectX/OpenGL normal conventions differ in the green/Y direction; convert once. These presets describe file contents, not the receiving application's automatic importer behavior. Check each material slot after import, especially packed maps and normal flags.

## Padding, size and precision {#padding-size-and-precision}

UV-island padding extends valid pixels into the surrounding gutter to resist filtering bleed. It does not repair a bad unwrap or create missing surface detail. Inspect distant/mipmapped views, not only a full-resolution flat texture.

Choose PNG 16-bit or supported higher-precision output when banding or map precision matters. A 32-bit float document exported through graph PNG tools still produces an 8-bit image. Neither a larger image nor an AI upscale reconstructs measured roughness or depth.

## Layered exchange and print {#layered-exchange-and-print}

PSD/PSB export supports 8/16-bit layered delivery, but not every procedural effect has an exact counterpart in another editor. Open the delivered file there and compare appearance and editability. Keep native .sxl/.sx3d as the working master.

For print, choose an appropriate destination ICC profile and inspect soft-proof/gamut warnings before CMYK TIFF export. Display proofing alone does not convert a PNG into a CMYK document. Numeric material maps should not undergo print color conversions.

## Unreal paths {#unreal-paths}

Model/material export and the ShaderixelLink connector deliver a project to Unreal. Effect Editor's Unreal export instead produces Custom-node HLSL and a material-building script; it has separate support constraints. An Unreal .uasset texture import differs from engine-assisted mesh conversion and may require an installed Unreal Editor for the latter.

## Handoff checklist {#handoff-checklist}

- Reopen native and exported files from their intended delivery location.
- Verify size, alpha, normal Y, color-space flags and packed-channel order.
- Include external linked images, fonts, meshes/HDRI and required workflows where applicable.
- Give each variant a distinct name. Graph export steps may overwrite an existing target; batch collision policies are configured separately.
- Inspect logs for errors and exclude credentials or private paths from shared reports.

[Export operations](doc:export)
[Color and data maps](doc:color)
[File formats](doc:formats)
