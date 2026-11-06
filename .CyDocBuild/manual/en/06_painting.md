---
id: painting
title: Painting and retouching
description: Select an unlocked pixel target, choose Brush (B), and adjust size, opacity and brush behavior in Tools or Painting.
section: Handbook
order: 5
parent: layers
revision: 95ef2bfcca26c7b8
---

Select an unlocked pixel target, choose Brush (B), and adjust size, opacity and brush behavior in Tools or Painting. A fill, text or smart-object layer is not automatically paintable; bake it or add a pixel layer above it.

## Brush controls {#brush-controls}

[ and ] change brush size. X swaps foreground/background colors; D restores black and white. Alt+Enter or Alt+Backspace fills with foreground color, respecting the selection.

Shift+B cycles Brush, Mixer Brush and Color Replacement. E selects Eraser; Shift+E cycles related erasers. Painting a selected mask edits coverage rather than RGB.

## Retouch tools {#retouch-tools}

- Clone (S) copies from a chosen source; configure the source first.
- J selects a healing-family tool; Shift+J cycles Spot Healing, Healing, Patch, Content-Aware Move and Red Eye.
- Shift+F5 runs Content-Aware Fill for a selection on a pixel target.
- R selects Blur; Shift+R cycles Blur, Sharpen and Smudge.
- O selects Dodge; Shift+O cycles Dodge, Burn and Sponge.
- Y selects History Brush.
- G selects Gradient; Shift+G switches Gradient/Bucket; K also selects Bucket.

Retouching needs surrounding information or an appropriate source. It cannot recover detail absent from the image. Keep a duplicate or compare with undo.

## Troubleshoot a stroke {#troubleshoot-a-stroke}

Check the active document and layer, visibility, lock, opacity, selection and mask. If the entire canvas looks different, check View Mode and the active PBR channel.

Bake to Pixel Layer makes procedural content paintable while preserving composition settings. It differs from baking mesh maps or freezing graph output.

[Layers, filters and pixel baking](doc:layers)
[Photo development and analysis](doc:photo)
