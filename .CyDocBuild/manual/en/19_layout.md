---
id: layout
title: Guides, alignment and artboards
description: Rulers, guides, grids and snapping help placement.
section: Handbook
order: 18
parent: layers
revision: 1ca80db41dc23dba
---

Rulers, guides, grids and snapping help placement. They are editing aids rather than image pixels.

## Placement {#placement}

Ctrl+R toggles rulers; Ctrl+; guides; Ctrl+Shift+; snapping; Ctrl+' the grid. View contains visibility and grid settings.

Smart guides compare layer geometry. If dragging jumps unexpectedly, check snapping. Enter numeric dimensions for exact sizes; fractional pixel boundaries may be antialiased.

Select layers and use Layer > Align. Distribution requires enough selected layers. Visual bounds can differ from the unrotated shape frame.

Linked layers move together where supported by the transform tool. Linking differs from grouping, which creates a stack container.

## Artboards and compositions {#artboards-and-compositions}

For a UI layout, set canvas size first, create shapes/text on separate layers, enter geometry in pixels, then align selected layers. Distribution needs at least three selected layers; alignment and snapping do not change the font or shape source dimensions. Disable snapping temporarily if it fights an intentional fractional placement.

Create/name an artboard region, put the intended content in its area and use the dedicated artboard export. Check each exported file rather than assuming a visual guide is an export boundary. Layer compositions store variant states; they are a separate mechanism from artboard rectangles.

Artboards define layout regions and have a dedicated export operation. Layer compositions store named stack states for recall/export.

Check export area and variant names. Hiding guides does not crop the document. Glows/outlines extending past export bounds can be clipped.

For repeatable UI work, set document dimensions, enter widget sizes in pixels, and inspect at 100%. Nine-slice source margins describe the skin; target size describes the destination.

[Numeric values and precise dimensions](doc:dimensions)
[Export and delivery](doc:export)
