---
id: resources
title: Resource browser and reusable assets
description: Open Resources from Window.
section: Handbook
order: 26
parent: libraries
revision: 7c2e9ef6d08a11f6
---

## Browse and place {#browse-and-place}

Open Resources from Window. Navigate folders, pin useful locations and inspect thumbnails. The context menu offers actions suited to the resource and current document: open, place as a smart object, place as a linked file, or add an image node.

The drop destination matters. Dropping a document opens its project; placing an image into an existing composition can create a layer/node. When a drop does not give the intended result, use the explicit context-menu command.

## Resource families {#resource-families}

- Brushes: .abr imports and programmable tips.
- Patterns: imported .pat patterns and reusable image patterns.
- Swatches: .aco, .ase and .gpl color collections.
- Functions: image-graph subgraphs/tools.
- Templates: complete starting documents.
- Smart materials: Paint 3D layer-and-mask stacks.
- Scenes: reusable rendering setups.

These resources are not interchangeable. An .sxfn inserts a function node and does not contain a complete painted scene. A template creates the initial document rather than a parameterized live effect.

## Import and verify a resource pack {#import-and-verify-a-resource-pack}

Use File > Import Resources for ABR brushes, PAT patterns and ACO/ASE/GPL swatches. Inspect one imported item before importing a large collection: an image stamp, procedural brush shader and dynamic brush preset are different resources. Define Pattern and Define Brush derive reusable resources from current image content; check selection/transparency before defining them.

Insert a function through the graph library, open a complete document through File, and create a document from a template. An imported resource does not install a required AI model or a native plugin. Keep dependencies and licenses with shared packs, and use stable effect IDs in reusable functions.

## Portability {#portability}

Linked images, brush textures, meshes and environment files can depend on external paths. Embed supported content or collect the dependencies when moving a project. Shipped resources live beside the application; user libraries use their documented Shaderixel folders.

[Documents and templates](doc:documents)
[Smart objects](doc:smart-objects)
