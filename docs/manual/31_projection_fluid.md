# Projection and fluid painting

## Project an image

Projection painting places an image/decal from the view onto a model. Set the source, framing and affected material channels, then inspect side faces and occluded regions. A camera projection is not the same as a globally seamless UV unwrap.

Triplanar material projection blends image sampling along three spatial directions. Its controls include scale, rotation, offsets and seam sharpness. Roughness/height can be derived from the source image or set independently, depending on the material configuration.

## Fluid paint

Fluid painting simulates a film of paint on the GPU, including material channels in Paint 3D. Choose a paint preset, amount, viscosity, retained film, drying and absorption before starting. A 2D canvas uses plane tilt/direction; a model can use world gravity or the view direction.

Pause to inspect. Freeze (Enter) commits the simulated paint as one history operation; Cancel (Escape) discards the session. Run Active Layer makes existing content flow. Check that the intended layer/material is selected before starting that operation.

## Tune deliberately

High viscosity flows slowly. Retained thickness limits how far paint drains. Drying stops motion; absorption can leave a trail. Wet paint can appear shinier, while height interaction deflects flow around relief.

Simulation resolution affects cost and detail and applies according to the tool's next-stroke settings. UV edge behavior and dripping options affect 3D results. The simulation is an artistic tool, not a calibrated physical measurement.

[Painting](#painting)
[Paint 3D](#paint3d)
