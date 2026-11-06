---
id: pbr
title: Photo to PBR maps
description: A photo mixes material color and lighting.
section: Create / Materials
order: 11
revision: f3a2b1d625fa36cc
---

A photo mixes material color and lighting. PBR Photo to Maps estimates albedo, height, normal, occlusion and roughness; it does not measure them. Metalness is an explicit choice.

```image
{"id": "pbr", "src": "assets/manual/pbr_en.png", "alt": "Photo processing and the color-space choice for exported maps.", "controls": "minimal"}
```

```image
{"id": "pbr-view", "src": "assets/manual/pbr_view_en.png", "alt": "Inspecting the roughness channel on a 3D sphere.", "controls": "minimal"}
```

## Extraction workflow {#extraction-workflow}

1. Choose a front-facing photo with even lighting and little perspective distortion.
2. Clean it with Textures Cleanup and Albedo; inspect seams before generating normals.
3. Connect it to PBR Photo to Maps.
4. Adjust height detail, normal strength and base roughness. Variables expose lighting radius, inversion and occlusion.
5. Choose metalness: 0 for a dielectric, 1 for a metal.
6. Inspect each channel and the material on a model.
7. In PBR Photo to Maps and PNG Export, set a prefix and run Extract and Export.

Prefix material creates material_albedo.png, material_height.png, material_normal.png, material_occlusion.png, material_roughness.png, material_metal.png and material_orm.png. Reusing it replaces those files.

## Conventions {#conventions}

The texture_* chain compensates for Shaderixel's sRGB PNG export so bytes preserve numeric data. In the engine, enable sRGB for albedo and disable it for normals, height, roughness, occlusion, metalness and packed maps.

Normals default to OpenGL. DirectX Normals flips green. A flat normal exports near 128,128,255. Normal blending expects matching input conventions.

ORM is R=occlusion, G=roughness, B=metalness. PBR Channel Packing also supports RMA/MRA. Height in alpha is optional and can make previews transparent.

## Quality {#quality}

The tool writes 8-bit PNG. Precise displacement needs a suitable high-precision export and transfer encoding. Scene depth is not surface microrelief.

Full-strength seam blending matches opposite edge values, but patterns and normal derivatives can still need retouching. A tiled preview is an inspection aid.

[Color, precision and data maps](doc:color)
[Paint 3D, materials and baking](doc:paint3d)
