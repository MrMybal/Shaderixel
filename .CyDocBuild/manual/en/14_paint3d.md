---
id: paint3d
title: Paint 3D, materials and baking
description: Create a Paint 3D project from File.
section: Create / 3D painting
order: 13
revision: 4a383d092b35cc7d
---

Create a Paint 3D project from File. Import a supported mesh or choose a primitive. Inspect UVs and texture resolution before detailed painting.

```image
{"id": "paint3d-view", "src": "assets/manual/paint3d_view_en.png", "alt": "Paint 3D workspace with material layers and the model view", "controls": "minimal"}
```

## First material from start to delivery {#first-material-from-start-to-delivery}

1. Create the project with a suitable texture resolution and a primitive or imported mesh. Inspect the checker and material assignments before painting.
2. Choose a material/texture set. Add a fill layer with base color and roughness; disable channels it should not change.
3. Add a paint layer or mask, select its target and paint a test mark. Check the active 3D material channels.
4. Rotate the model and inspect seams, back faces and small UV islands. A good flat preview can hide poor UV coverage.
5. Add a smart material or geometry-based mask after its required maps are available. Inspect mask coverage before increasing contrast.
6. Save .sx3d, export the channel set and validate it on the mesh in the receiving engine.

Start with a full base layer, then localize paint/wear, cavity dirt and directional dust. Roughness and metalness are numbers rather than cosmetic RGB tints. Follow-layer masks can anchor on, at the edge of, around or away from another layer; preserve that relationship when reordering.

For image fills, inspect UV/triplanar mapping, scale, rotation and offsets. Baked photo lighting may look plausible in base color while giving incorrect roughness/height estimates. Inspect individual channels under neutral lighting.

## Channels and materials {#channels-and-materials}

The brush supports twelve channels. Base color, roughness, metallic, normal, emissive, occlusion and height are joined by opacity and four user channels. Enable the channels to change in the brush material. The same targets apply on the model and the UV canvas. The material eyedropper also samples emissive color.

Select the clone tool in the 3D view and Ctrl-click the model to place the source. Choose the active layer or visible result, then paint the destination. Smudge drags material in the enabled channels. Paint a quick mask to protect an area and Ctrl-paint to release it. Invert or clear the protection in the tool options.

Use a surface path for a seam or a row of rivets. Click the model to place points and drag a point to adjust the path. Choose the stroke, patterned ribbon, eraser or fill mode. The points stay attached as you orbit and remain editable after reopening the project.

In Brush Material, model-space size keeps the 3D brush width consistent when the camera zooms. In Simulation, try Rust under a rivet on a layer with base color, roughness and height. Enable Particles and click below the rivet. Adjust lifetime, gravity or per-channel strength, then replay before freezing the paint.

Mud splatter leaves thick impacts and earthy trails. Surface rain lowers roughness and adds a little height while preserving the underlying color. Settled dust adds a light matte deposit to upward-facing surfaces. Open Impact and trail material to customize and save these settings.

Choose a UV selection or an existing path as the emitter source to distribute particles over an area or along a line. Wet impact deposits transfer to Fluid Paint so they can flow before you freeze them.

### Code a particle brush {#coding-particles}

In Simulation, choose Wandering drops (script), then Edit script. Apply script validates the code for the next simulation. Use Replay gesture to try it on an open preview. Save brush stores the code with the preset, and .sxstroke files also store the code.

Under **Script settings**, **Impact** starts open. Click its title to collapse it. Impact radius scales the droplet radius and Impact strength controls the amount deposited on contact. Clear Deposit on impact to skip contact deposits while leaving trails enabled. Choose Bounce in Impact response to show the **Bounce** group, which starts collapsed. Expand it to set restitution between 0 and 1 with Bounce strength.

```text
group impact "Impact" open {
    slider impact_radius_scale "Impact radius" = 2.1 [0, 4];
    slider impact_force "Impact strength" = 1 [0, 2];
    checkbox paint_impacts "Deposit on impact" = true;
    choice impact_mode "Impact response" = SLIDE [
        SLIDE "Slide", BOUNCE "Bounce", STOP "Stop"
    ];
}

group bounce "Bounce" collapsed when impact_mode == BOUNCE {
    slider bounce_strength "Bounce strength" = 0.35 [0, 1];
}
```

Declare controls before the blocks with a unique identifier, a label and a default. The script reads a checkbox as `true` or `false`. Give each list label a distinct numeric value or a constant such as `BOUNCE`. The script reads the selected option’s value, for example with `response = impact_mode`.

Declare top-level `group` blocks with an identifier and a label. After the label, use `open` to start expanded or `collapsed` to start collapsed. Groups without an option start open. Then add `when` to control the group's visibility. Individual controls also accept `when`. A condition can combine earlier controls with constants, comparisons and operators such as `&&`, `||` and `!`. The script can read values from collapsed or hidden groups, and the brush and gesture store them.

The Reset button next to a group title restores that group's values and its open or collapsed state from the script. In Wandering drops, **Impact** opens and returns to Slide, while **Bounce** collapses.

Reset all, at the top of **Script settings**, restores every script value and each group's open or collapsed state, including hidden groups.

Save brush stores the current settings and whether each group is open or collapsed, including hidden groups. Loading the brush restores these states. A group keeps its chosen state when its identifier carries over to an edited script. A .sxstroke file stores the values used to paint the gesture. After an adjustment, select Replay gesture to apply it to the preview before saving. A control keeps its setting when its identifier and type carry over. Slider values are kept within the updated bounds. Removing a selected list value restores the default.

- `emit` runs once for each new particle. It can change position `x/y/z`, velocity `vx/vy/vz`, `radius`, `life`, `alpha`, `alive`, and `on_surface`.
- `update` runs once per 1/120-second step. It can change velocity, acceleration `ax/ay/az`, gravity multiplier `gravity`, `radius`, `life`, `alpha`, and `alive`.
- `impact` sets `impact_radius`, deposit `strength`, and `response`. `NATIVE` uses the selected brush behavior, `SLIDE` follows the surface, `STOP` ends the particle, and `BOUNCE` uses `restitution`.

The language supports numeric expressions, block-local variables declared with `let`, and `if/else` conditions. `age`, `life`, `time`, and `dt` are in seconds. `nx/ny/nz` gives the normal and `speed` gives velocity magnitude. `random()` draws a value between 0 and 1 using the gesture seed. Blocks can call `min`, `max`, `clamp`, `mix`, `abs`, `floor`, `sqrt`, `sin`, and `cos`. Each scripted particle has a maximum lifetime of 15 seconds.

The 3D view evaluates the material on the mesh. Lighting/HDRI changes the preview, not the exported base-color pixels. Use channel views to diagnose unexpected shading.

Automatic unwrapping supplies UVs for a mesh without them. Review seams, packing and texel density before relying on the layout.

## Smart materials {#smart-materials}

Smart materials assemble fills and masks such as edge wear, cavity dirt, dust, gradients and noise. Some use baked maps: occlusion, convexity, concavity or thickness.

Choose a smart material in the 3D layer controls and inspect each mask. User materials are saved in %LOCALAPPDATA%\Shaderixel\SmartMaterials. A smart material is different from a 2D smart object.

## Baking and delivery {#baking-and-delivery}

Open the Paint 3D map baker from the File menu. For assemblies, match parts by name to prevent adjacent geometry from affecting the projection. Door_low and Door_high form a pair. A custom cage must share the low-resolution mesh topology.

Mesh-map baking derives information from geometry. Bake to Pixel Layer rasterizes one 2D layer. A graph Bake action retains an image result. These are separate operations.

Set the resolution per tile when creating a project to define the native density of its UDIMs. In 3D Layers, add a masked stack to confine its child layers to a material, tile or selection. In Texture set channels, choose channels for a material or an individual tile. For a projected material, open its per-channel images and assign individual files or import a set such as Wood_BaseColor.png and Wood_Roughness.png.

In Export Project Textures, choose native size to keep the dimensions of each tile. Choose Project atlas for one image per map when the model uses atlas UVs. For a custom packed map, duplicate a preset, set each RGBA component and save the preset. It becomes available in other projects.

[Photo to PBR maps](doc:pbr)
[Export and delivery](doc:export)
