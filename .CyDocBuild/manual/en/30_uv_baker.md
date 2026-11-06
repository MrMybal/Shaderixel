---
id: uv-baker
title: UV layout, texture sets and mesh maps
description: UV coordinates map mesh triangles into texture space.
section: Handbook
order: 29
parent: paint3d
revision: e495861f42c89acb
---

UV coordinates map mesh triangles into texture space. Before painting, inspect UV coverage and seams with a checker. Texture resolution and island scale determine how much detail a face can hold.

## Automatic unwrap {#automatic-unwrap}

Open the UV tools for a mesh, choose an unwrap/packing preset and compare the proposed UVs with the current layout. Inspect island separation, padding and orientation, then apply. Existing paint is transferred where supported, but inspect seams and small features afterward.

Changing UVs changes the relationship between geometry and texture pixels. Save a native copy before a major repack. A flat texture preview alone does not reveal whether the model's UVs overlap undesirably.

## Several material sets {#several-material-sets}

Multi-material projects can allocate texture tiles/sets by material. Select the intended material when painting or baking. Export produces a corresponding set of textures per material; verify filenames and assignments in the receiving engine.

## Bake geometry maps {#bake-geometry-maps}

### Full baking workflow {#full-baking-workflow}

1. Save the project and verify destination UVs, overlaps, normals and material sets.
2. Choose tangent normal, AO, curvature, thickness, height, position, world normal or color ID as needed. ID can distinguish parts, materials or UV islands.
3. For a high-definition source, choose its mesh and front/back cage distances relative to model size. Too small misses detail; too large can capture neighboring geometry.
4. Set resolution (512–4096 in this dialog), AO rays/range, edge width and texel padding. More rays reduce sampling noise at a cost.
5. Choose project channels and/or PNG files. Height has a separate channel option. Start and inspect every map under neutral lighting.

Cancel requests job cancellation; a partial preview is not a completed map set. Keep the source to adjust and rebake. Correct tangent/normal interpretation is still required in the receiving engine.

Use the map baker for occlusion and other geometry-derived maps. Check mesh selection, UVs, resolution and bake settings, then inspect each output. Smart masks such as edge wear or cavity dirt can depend on these maps.

A geometry bake differs from extracting approximate maps from a photograph. It also differs from rasterizing a 2D layer. Use the operation matching the data you actually have.

[Paint 3D](doc:paint3d)
[Photo-derived PBR](doc:pbr)
