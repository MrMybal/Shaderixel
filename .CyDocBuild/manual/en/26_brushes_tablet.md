---
id: brushes-tablet
title: Brush libraries and tablets
description: Open Help > Brushes for this index.
section: Handbook
order: 25
parent: painting
revision: 5ffd39fa291ebe8f
---

## Choose and configure a brush {#choose-and-configure-a-brush}

Open Help > Brushes for this index. In the Brushes panel, select a preset and use Brush help or F1 to open its own reference. The eraser has an independent tip selection. Standard Round opens this guide; custom code and graph copies open the authoring guide.

The Brushes panel groups presets by category and search. A programmable brush defines its tip and dab placement; available parameters depend on that brush. Spacing, scattering, size/angle variation and airbrush behavior can change the stroke substantially.

Test a short stroke at the intended document scale. Very large tips, dense spacing and expensive tip shaders can increase GPU work. A mask target records coverage, while an image target records color.

## Import and create {#import-and-create}

File > Import Resources supports Photoshop .abr brushes. Imported resources and procedural brush shaders are different formats; inspect the resulting tip and dynamics.

The brush editor can create a brush from a template, edit its code or graph, and save it in the library. Ctrl+Enter compiles in the applicable code editor. Brush textures provide image-based tips; changing a texture can change the stroke independently of size/spacing.

## Tablet input {#tablet-input}

Tablet options offer Windows Ink and Wintab. Pressure, tilt, pen rotation and speed can feed brush dynamics where the brush supports them. Use the pressure curve and test area to check response before painting important work.

If pressure remains constant, check the selected input backend, driver and brush's pressure mapping. Wintab needs a compatible installed driver; a mouse cannot provide all pen channels. A brush must actually use a channel for that input to affect the result.

## Understand a stroke {#understand-a-stroke}

```image
{"id": "brush-pipeline", "src": "assets/manual/brush_pipeline_en.png", "alt": "From stylus samples to painted coverage", "controls": "minimal"}
```

1. Choose an unlocked pixel layer and its image or mask thumbnail. For procedural content, convert to a pixel layer before painting. In Paint 3D, check the active material channel instead.
2. Pick a tip. Selection applies its declared spacing, measured as a fraction of brush diameter. A value of 0.1 is a tenth of a diameter; 1.2 separates centers by more than a diameter.
3. Set size, opacity, flow and supported pressure controls. Double-click or Ctrl+click a numeric field to type a decimal value; Enter confirms, Escape cancels.
4. Test a curved stroke and a stationary press. Only presets with airbrush timing accumulate extra dabs while held still. A spray-like shape alone does not imply timed painting.
5. Compare overlap at the final document scale. High opacity does not replace missing coverage inside grainy tips. Dense spacing can fill small gaps and makes more GPU work.

Tip returns color and coverage. Dynamics can alter the center, angle, radius or color of a dab. Extent reserves a wider calculation area for such marks; increasing extent alone does not scatter a brush. Self-flow presets apply flow and pressure inside the tip; ordinary presets use the renderer's dab-opacity multiplication. Both retain global opacity and selection limits.

Length, width and noise settings generally use tip coordinates rather than document pixels. A color parameter has RGBA components; angle parameters such as Rotation, NibAngle and Slant use radians (about 1.5708 is a quarter turn). Stable parameter identifiers in square brackets let you identify a control in either language.

## Troubleshooting and safe editing {#troubleshooting-and-safe-editing}

- No paint: check layer lock, active mask/image target, selection, opacity, flow and foreground color. Compare with Standard Round to separate target problems from a procedural tip's sparse coverage.
- Missing texture: choose a readable image in the preset's texture slot. Images without alpha receive luminance-derived coverage; a white background can therefore paint as part of the stamp.
- Hardness seems inactive: not every shader reads the shared hardness value. Some tips define their own softness or fixed edge thresholds; consult that preset's page.
- A changed brush does not match the reference: the reference describes shipped defaults. Edited code/graph copies and replaced textures can change the result. Selecting a shipped preset again resets its shader parameters and texture overrides.
- Rust, rivets and cracks are coverage/color motifs. They do not infer geometry, detect model edges or generate a full PBR material by themselves. Paint the intended material channel explicitly.

## Rendered examples {#rendered-examples}

```image
{"id": "brush-atlas", "src": "assets/manual/brush_atlas_en.png", "alt": "Twelve shipped tips rendered by the brush engine", "controls": "minimal"}
```

These are actual GPU previews with simulated pressure, not tablet measurements. All use the same blue painting color, 110 px requested size, 0.7 hardness, 0.8 flow and full opacity; the preview limits the effective diameter to fit its height. Spacing and shader parameters retain the preset defaults. The numbered examples identify selected tips in the catalog below. A watercolor appearance does not run a fluid simulation.

[Painting and retouching](doc:painting)
[Effect and brush authoring](doc:authoring)

## Shipped brush catalog {#shipped-brush-catalog}

### Examples {#examples}

[Grainy airbrush](doc:brush-airbrush)

[Calligraphy nib](doc:brush-calligraphy)

[Paper-grain charcoal](doc:brush-charcoal)

[Star scatter](doc:brush-star-scatter)

### Materials {#materials}

[Bristle](doc:brush-cy-bristle)

[Chalk](doc:brush-cy-chalk)

[Charcoal](doc:brush-cy-charcoal)

[Dry media](doc:brush-cy-dry-media)

[Ink bleed](doc:brush-cy-ink-bleed)

[Rake](doc:brush-cy-rake)

[Sponge](doc:brush-cy-sponge)

[Watercolor](doc:brush-cy-watercolor)

### Shapes {#shapes}

[Calligraphy](doc:brush-cy-calligraphy)

[Cross](doc:brush-cy-cross)

[Diamond](doc:brush-cy-diamond)

[Ellipse](doc:brush-cy-ellipse)

[Halftone](doc:brush-cy-halftone)

[Hexagon](doc:brush-cy-hexagon)

[Leaf](doc:brush-cy-leaf)

[Noise](doc:brush-cy-noise)

[Ring](doc:brush-cy-ring)

[Round](doc:brush-cy-round)

[Soft square](doc:brush-cy-soft-square)

[Square](doc:brush-cy-square)

[Star](doc:brush-cy-star)

[Texture stamp](doc:brush-cy-texture)

[Triangle](doc:brush-cy-triangle)

### Details {#details}

[Chain](doc:brush-cy-chain)

[Crack](doc:brush-cy-crack)

[Fingerprint](doc:brush-cy-fingerprint)

[Grass](doc:brush-cy-grass)

[Scratch](doc:brush-cy-scratch)

[Stitch](doc:brush-cy-stitch)

### Particles {#particles}

[Cloud](doc:brush-cy-cloud)

[Fur](doc:brush-cy-fur)

[Smoke](doc:brush-cy-smoke)

[Sparkle](doc:brush-cy-sparkle)

[Spray](doc:brush-cy-spray)

### 3D texture {#3d-texture}

[Drips](doc:brush-sx3d-coulures)

[Thread stitches](doc:brush-sx3d-couture)

[Scales and disks](doc:brush-sx3d-ecailles)

[Splashes](doc:brush-sx3d-eclaboussures)

[Cellular cracks](doc:brush-sx3d-fissures)

[Fine grain](doc:brush-sx3d-grain-bruit)

[Soft cloud](doc:brush-sx3d-nuage-doux)

[Dry bristles](doc:brush-sx3d-pinceau-sec)

[Regular dots](doc:brush-sx3d-points)

[Fine scratches](doc:brush-sx3d-rayures-fines)

[Rivets](doc:brush-sx3d-rivets)

[Grunge dirt](doc:brush-sx3d-salete)

[Rust patches](doc:brush-sx3d-taches-rouille)

[Clean line](doc:brush-sx3d-trait-net)

[Chipped wear](doc:brush-sx3d-usure-ecaille)
