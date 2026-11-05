# Troubleshooting, updates and support

Preserve an editable copy. Check the view and selected target before changing source content.

## Blank image or missing controls

- Check layer visibility, opacity, mask, clipping and blending.
- Switch View Mode to Final Image.
- Connect nonempty content to the graph Output.
- Paint on an empty pixel layer or check generator alpha.
- A .sxfn is a node, not a complete example. Open .sxl for the full composition.
- Clear panel filters if controls/resources are hidden.

## Painting and shader problems

Check the target is unlocked and paintable. Select the image thumbnail for color rather than its mask. Deselect unintended selections. Bake procedural/text/shape/smart content to pixels when needed.

Inspect Edit > Shader Errors and shaderixel.log beside the executable. Removed/renamed effects can break presets. Neighbor-sampling shaders need halo metadata. Float data fields need the correct precision and interpretation.

## AI and updates

For AI check server, endpoint, model and API workflow. Input preview is not a completed result.

Help > Check for updates checks GitHub releases. Startup checks and inclusion of prereleases are preferences there. Downloads are verified before Install and restart. Unsaved documents offer save/discard/cancel; cancellation keeps the editor open. Offline checks can be retried.

The first 2026-11-03 installer lacks the updater; install a later alpha once. The public channel needs no GitHub account.

## Support and offline help

Report version, D3D12/Vulkan backend, GPU/driver, steps and a minimal project. Inspect shared logs/screenshots for private content. The repository is github.com/MrMybal/Shaderixel.

The handbook and references are embedded and remain readable if help files move. The browser edition is docs/manual/index.html in a normal distribution.

[Getting started](#getting_started)
[AI, ComfyUI and upscaling](#ai)

[Detailed diagnosis, recovery and performance](#recovery)
[Settings and verified updates](#settings)
