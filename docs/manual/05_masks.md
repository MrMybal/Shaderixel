# Selections, masks and paths

A selection limits operations to a region. A layer mask changes visible coverage. A clipping mask uses another layer's coverage. These mechanisms can be active together.

## Selections

Use Rectangle/Ellipse (M / Shift+M), Lasso (L), Polygonal Lasso (Shift+L), Quick Selection (W) or Magic Wand (Shift+W). Tool options control addition, subtraction and edge behavior.

Ctrl+A selects all; Ctrl+D deselects; Ctrl+Shift+D restores the previous selection; Ctrl+Shift+I inverts. Shift+F6 opens feathering for a selection. Quick Mask (Q) provides a mask-oriented view.

If painting affects only a small region, check for an active selection before changing brush settings.

## Layer masks and clipping

Add a mask from Layer or the layer controls. Select its thumbnail to paint coverage: white reveals, black hides and gray partially reveals. Select the image thumbnail to paint image color again.

Ctrl+Alt+G toggles clipping on a non-group layer. Clipping uses the base layer's coverage; it does not create independent painted mask pixels.

## Graph masks

Graph masks are images or channels connected to mask inputs. Some expect linear coverage; numeric grayscale PNGs imported as color images have transfer encoding.

Masks Imported Matte to Coverage converts imported numeric channels into linear coverage. Alpha Extraction and Cleanup refines source alpha. Avoid multiplying an already applied alpha twice.

## Build and refine a mask

1. Select the foreground with a suitable tool or Color Range. Add/subtract regions before feathering.
2. Inspect the matte in a contrasting view; refine edge softness and remove isolated unwanted regions. Select and Mask and Focus Area provide different starting/refinement paths, not identical selections.
3. Add a layer mask and select its thumbnail. Paint black to hide, white to reveal; gray is partial coverage. An active selection still restricts these strokes.
4. Return to the image thumbnail for color retouching. Check clipping and Blend If if the result remains partly hidden despite a white mask.

Apply Image combines a chosen source/channel into a target; Calculations combines channels for a selection/mask workflow. Check source document/layer/channel, destination, blend and inversion before confirming. These commands do not automatically identify foreground objects. Preserve original masks when experimenting.

## Paths

Pen (P) creates paths. The Paths panel fills, strokes and converts them into selections. Selection > Create Path from Selection traces a selection; tolerance controls boundary fidelity.

Paths, painted masks and selections have different editing controls. Keep the original path for later geometry edits.

[Painting and retouching](#painting)
[Main image graph](#image_graph)
