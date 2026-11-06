---
id: photo
title: Photo development and analysis
description: Keep the original and use filters/adjustment layers for reversible editing.
section: Handbook
order: 20
parent: color
revision: 79e7ce2cf3f63229
---

Keep the original and use filters/adjustment layers for reversible editing. Photo Essentials Develop combines common controls; individual effects remain available.

```image
{"id": "photo-view", "src": "assets/manual/photo_view_en.png", "alt": "Photo RAW development and adjustment controls", "controls": "minimal"}
```

## A repeatable editing order {#a-repeatable-editing-order}

1. Confirm the source profile and preserve the original. Set exposure/white balance without clipping useful highlights.
2. Balance shadows/highlights, then curves and local contrast. Judge skin, neutrals and high-contrast edges at a meaningful zoom.
3. Apply local corrections with masks, then retouch a pixel copy. Reserve grain and output sharpening for the delivery size.
4. Compare the final composite and overlays, disable the adjustment for before/after, save natively and export separately.

Photo RAW includes local/lens controls where supported. Lens correction depends on matching available camera/lens data; a missing decoder/profile is not fixed by a preset. Inspect metadata and the correction preview before applying.

## Panorama, alignment, HDR and contact sheets {#panorama-alignment-hdr-and-contact-sheets}

Photomerge takes at least two source files and offers automatic, perspective, cylindrical or reposition layouts, plus optional blending masks. Use suitable overlap and inspect moving subjects/seams. It cannot supply measured detail in uncovered canvas corners.

Auto-Align works on visible pixel layers against the bottom layer, using perspective or translation. Auto-Blend creates masks for panoramas or focus stacks; it does not invent sharp detail absent from every input. Keep the source layers to adjust their masks.

HDR Merge aligns different exposures of one scene and creates a 32-bit linear document. Movement can leave artifacts. Display exposure/tonemapping is separate from retained HDR range. Contact sheets arrange source images; verify page dimensions, labels and destination before processing a folder.

## Development {#development}

Adjust white balance/exposure, then shadows/highlights, local contrast, vibrance and sharpening. Strength mixes with the original; masks limit the correction.

Curves provides channel curves. Color Lookup loads .cube LUTs. Grain, split tone and vignette are finishing choices; they cannot restore clipped detail.

Photo RAW has a dedicated development interface. Camera RAW support depends on the distributed decoder and file format; inspect the decoded result.

## Analysis and compositing {#analysis-and-compositing}

Histogram and Image Analysis show brightness, channels and clipping. Canvas overlays are display modes; return to the normal final view to assess the look.

Before/After and layer visibility provide comparisons. Confirm the inspected result when a histogram seems empty or unexpected.

Masks Color Key and Despill uses color distance, not semantic recognition. Matte Cleanup refines coverage. Composite with Matte combines foreground/background with an explicit mask.

Retouch on a pixel target or baked copy; inspect high-contrast edges at full scale. Panorama/Photomerge, alignment, blending, HDR, contact sheets and batch actions are available through Automation and depend on suitable source images.

[Painting and retouching](doc:painting)
[Blending and Blend If](doc:blending)
