---
id: troubleshooting
title: Troubleshooting, updates and support
description: Preserve an editable copy.
section: Help
order: 23
revision: 8e50c9e964ebb507
---

Preserve an editable copy. Check the view and selected target before changing source content.

## Blank image or missing controls {#blank-image-or-missing-controls}

- Check layer visibility, opacity, mask, clipping and blending.
- Switch View Mode to Final Image.
- Connect nonempty content to the graph Output.
- Paint on an empty pixel layer or check generator alpha.
- A .sxfn is a node, not a complete example. Open .sxl for the full composition.
- Clear panel filters if controls/resources are hidden.

## Painting and shader problems {#painting-and-shader-problems}

Check the target is unlocked and paintable. Select the image thumbnail for color rather than its mask. Deselect unintended selections. Bake procedural/text/shape/smart content to pixels when needed.

Inspect Edit > Shader Errors and shaderixel.log beside the executable. Removed/renamed effects can break presets. Neighbor-sampling shaders need halo metadata. Float data fields need the correct precision and interpretation.

## AI and updates {#ai-and-updates}

For AI check server, endpoint, model and API workflow. Input preview is not a completed result.

Help > Check for updates checks GitHub releases. Startup checks and inclusion of prereleases are preferences there. Downloads are verified before Install and restart. Unsaved documents offer save/discard/cancel; cancellation keeps the editor open. Offline checks can be retried.

The first 2026-11-03 installer lacks the updater; install a later alpha once. The public channel needs no GitHub account.

## Support and offline help {#support-and-offline-help}

Report version, D3D12/Vulkan backend, GPU/driver, steps and a minimal project. Inspect shared logs/screenshots for private content. The repository is github.com/MrMybal/Shaderixel.

The CyDoc handbook, reader scripts, media and references are distributed together in Documentation/. Keep this complete directory beside the executable. If help files are missing, repair or reinstall Shaderixel. Reading requires no Internet connection.

[Getting started](doc:getting-started)
[AI, ComfyUI and upscaling](doc:ai)

[Detailed diagnosis, recovery and performance](doc:recovery)
[Settings and verified updates](doc:settings)
