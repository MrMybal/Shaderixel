---
id: libraries
title: Choosing a library
description: Search the node Library for a family.
section: Handbook
order: 10
parent: image-graph
revision: ac541bb957af6384
---

Search the node Library for a family. This manual's Effects reference describes each shipped shader's metadata and defaults. Functions describes each .sxfn, its ports, exposed variables and execution buttons. These references are generated from the distributed files.

## Photo, compositing and graphic work {#photo-compositing-and-graphic-work}

Photo Essentials provides development, local contrast, split tone, grain and vignette. Masks provides keying, luminance selection, matte cleanup and alpha extraction. Strength and optional masks limit the correction.

Graph Layers creates procedural backgrounds and panels; Graph Effects combines adjustments. Their .sxfn assemblies belong to the main graph; underlying shaders can carry separate editable effect graphs.

Game Art provides pixel grids, palettes, outlines, sprite states and transparency inspection. Leave transparent padding for outlines/shadows. Other creative families provide materials, stylized looks, print/scan and logo treatments.

## Texture and UI work {#texture-and-ui-work}

PBR and Textures cover cleanup, estimated maps, normal conversion, packing, seams and export. ComfyUI tools require a server and appropriate models/workflows.

UI UV covers coordinates, slicing, atlases, repeat and sampling. UI Toolkit adds SDF shapes, border styling, flow/parallax, transitions, patterns and skin states.

## Live versus executed {#live-versus-executed}

Live subgraphs update with input changes. Export and AI tools run only on their execution event. Retained results remain until rerun or cleared. User-installed presets are not automatically added to the embedded reference.

[Photo to PBR maps](doc:pbr)
[UI textures and slicing](doc:ui-textures)
[AI, ComfyUI and upscaling](doc:ai)
