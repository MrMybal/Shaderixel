---
id: text-shapes
title: Text, shapes and Boolean geometry
description: Choose Text (T), click the canvas to create a text layer, and enter content in Properties.
section: Handbook
order: 24
parent: layers
revision: 7b75f4ecbc5653d9
---

## Editable text {#editable-text}

Choose Text (T), click the canvas to create a text layer, and enter content in Properties. Clicking an existing text layer with the text tool edits it. Set font, size, color, alignment and line spacing before refining placement with Move or numeric transforms.

Keep the text layer editable while checking spelling and layout. A gradient clipped above the text colors the glyphs through their coverage. Shadows, borders and other layer filters affect the rendered result without changing the text string.

Fonts must be available on the receiving machine for a reproducible editable document. For an image delivery, render/export the final composition. Bake to Pixel Layer makes the rendered glyphs paintable; undo restores editable text.

## Text styles {#text-styles}

Use Text Styles to store reusable typography settings separately from the text string. Check font availability, alignment, line spacing and the resulting bounds when applying a style to different content. A text layer can remain editable while its rendered pixels pass through layer filters.

## Shapes {#shapes}

For exact geometry, set the shape's own width/height in pixels with the aspect link disabled when appropriate. A rotated frame, stroke, shadow and layer transform can make visible bounds differ from that geometry. Enter fractional values when needed and inspect antialiasing at final size.

Choose Shape (U), draw a frame, then use Properties for type, fill, stroke and geometry. Rectangle, ellipse, polygon, star and line-like shapes have different controls. Width and height describe the shape frame before rotation, stroke and effects.

Use Boolean components to combine shapes by union, subtraction, intersection or exclusion where offered. Inspect the component list and its order: subtracting a component is not the same as simply hiding it. Move/resize the intended component rather than the whole layer when refining a compound shape.

## Practical UI badge {#practical-ui-badge}

1. Create a rounded rectangle on a transparent canvas.
2. Enter its exact pixel dimensions with proportions disabled.
3. Add text above it and align the layers.
4. Add a subtle shadow or border, leaving room inside the document bounds.
5. Save the native document and export PNG with transparency.

[Precise dimensions](doc:dimensions)
[Layers and baking](doc:layers)
