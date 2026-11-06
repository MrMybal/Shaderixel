---
id: layers
title: Layers, filters and pixel baking
description: Layers compose from the bottom of the stack upward.
section: Create / 2D editing
order: 3
revision: bd823e511906b3db
---

Layers compose from the bottom of the stack upward. Select a layer to edit it in Properties. Visibility hides its contribution; locking prevents editing. Opacity and blending control how it combines with layers below.

```image
{"id": "layers", "src": "assets/manual/layers_en.png", "alt": "Baking changes the content while keeping mask, opacity and blend editable.", "controls": "minimal"}
```

## Layer types {#layer-types}

- Pixel: paintable image content.
- Fill / Draw: procedural content without an input image.
- Adjustment: an effect applied to the composite below.
- Shape: editable geometry, fill and stroke.
- Text: editable text and typography.
- Smart object: embedded or linked reusable content.
- Group: a container for child layers.

Ctrl+Shift+N adds a pixel layer. Ctrl+J duplicates, or creates a layer via copy if a selection exists. Ctrl+G groups; Ctrl+Shift+G ungroups. Reordering changes the composite.

## Non-destructive filters {#non-destructive-filters}

A layer filter keeps its parameters editable. Disable or remove it to compare with the original. A destructive filter changes the pixel target directly. An adjustment layer processes the result below rather than becoming paintable content.

Styles such as shadows, glows, borders and bevels depend on coverage. Leave canvas space for effects extending beyond the shape.

## Bake to Pixel Layer {#bake-to-pixel-layer}

Use Properties > Bake to Pixel Layer, Layer > Bake to Pixel Layer or the layer context menu. This converts rendered content, its transform and its filters into document-sized editable pixels. It supports generators, text, shapes, smart objects and pixel layers with filters.

The layer keeps its stack position, name, visibility, mask, opacity, clipping and blending settings. These composition controls remain editable. Source text, shape and filter controls are replaced by pixels. Ctrl+Z restores the original editable layer.

Unlock the target first. Groups and adjustment layers use merge commands instead. Baking one smart-object instance does not edit its source or other instances.

## History and snapshots {#history-and-snapshots}

Layer compositions record variants of layer state, including visibility, opacity, blending, filter enablement and smart-object placement. They reuse the same layer pixels rather than making independent document copies. Name a composition, adjust the stack and record another, then recall/export the intended state. Painting shared source pixels can affect several variants.

Undo/Redo steps through edits in the current session. Create Snapshot in History captures a complete document state, including layers, parameters, pixels and color profile. Selecting a snapshot restores that state, so preserve the current work before switching to an older snapshot. A snapshot is not an exported image or a substitute for saving the native project.

## Merge choices {#merge-choices}

Ctrl+E merges down. Ctrl+Shift+E merges visible layers. Flatten combines the visible result. Save an editable native copy before discarding the original stack.

[Masks and selections](doc:masks)
[Blending and Blend If](doc:blending)
