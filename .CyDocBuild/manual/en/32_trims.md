---
id: trims
title: Trim sheets and texture atlases
description: Trim sheets divide a texture into named strips or regions for reuse on models.
section: Handbook
order: 31
parent: pbr
revision: 61ff0f003c73eab1
---

Trim sheets divide a texture into named strips or regions for reuse on models. The Trim Sheet panel stores layout independently of the painted pixels.

## Build a layout {#build-a-layout}

Apply a supplied layout, enter custom strip heights or create bands from horizontal guides. Custom heights run top to bottom; a zero-height entry uses the remaining space according to the panel's layout rule. Applying a layout replaces existing trims.

Use the trim tool to select and resize edges. The default drag step is 8 px; Shift refines to 1 px. Double-clicking a trim selects its pixels. Properties exposes the selected rectangle and name.

## Texture operations {#texture-operations}

Split a trim into columns when several horizontal tiles are needed. Make Seamless blends the active pixel layer with shifted content within the trim. Inspect the result: blending can change motifs and cannot recover missing pattern structure.

Generate a trim-ID layer for color selection/masking, and use the 3D preview to inspect repeated strips. Padding helps avoid adjacent regions bleeding under filtering.

## Export {#export}

Export Layout writes JSON with names, pixel rectangles and UV coordinates in top-left and bottom-left conventions. Export the image separately and make sure the consuming tool uses the same coordinate convention.

UI sprite atlases and nine-slice use related coordinate ideas but are separate graph functions. Atlas gutters and source regions must agree with the actual image layout.

[UI textures](doc:ui-textures)
[Guides and layout](doc:layout)
