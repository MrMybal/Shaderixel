---
id: dimensions
title: Numeric values and precise dimensions
description: Double-click a slider or drag field, or Ctrl+click it, to type.
section: Handbook
order: 6
parent: layers
revision: 52fcb05f9b5152a4
---

Double-click a slider or drag field, or Ctrl+click it, to type. Floating-point fields accept dot or comma. Enter confirms, Escape cancels and Tab advances. The text editor preserves precision even when the normal display rounds values.

## Layer transforms {#layer-transforms}

Layer transformation properties and Free Transform support Percentage (%) and Pixels (px). Switching units changes the display, not the object. Percentages refer to source dimensions.

Disable Constrain proportions for independent dimensions. For example, choose Pixels, width 375.5 and height 75.25. With proportions enabled, changing one dimension updates the other.

Pixel dimensions preserve existing flips. Negative percentages can mirror a transform. Rotation can make the visible bounding box larger than the entered dimensions.

## Shape frames {#shape-frames}

Shape Properties exposes frame position, width and height, including Boolean components. Shape-frame percentages refer to document width and height.

The frame is measured before rotation, stroke and filters. A 375 px frame with an outside stroke or glow can occupy more than 375 visible pixels.

## Transform and deformation {#transform-and-deformation}

Ctrl+T begins Free Transform on a supported target. Use numeric options or handles. Enter commits; Escape cancels.

Layer Transform (A) has Move (W), Rotate (E), Scale (R) and Pivot (P) modes. Those keys select modes while the tool is active.

Warp, Puppet Warp and Perspective Warp in Edit change how the image is sampled. They are separate operations, not alternate unit systems.

[Guides, alignment and artboards](doc:layout)
[Keyboard shortcuts](doc:shortcuts)
