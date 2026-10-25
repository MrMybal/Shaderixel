# Shaderixel Link (Unreal Engine 4.25–5.8)

Editor plugin for painting a static mesh or editing a texture in Shaderixel,
then automatically reimporting the result into Unreal Engine.

## Installation

Copy the `ShaderixelLink` folder built for your engine version from the Shaderixel
package (`Unreal/<version>/ShaderixelLink`) into your project's `Plugins` folder,
or into the engine's `Engine/Plugins/Marketplace` folder. Enable **Shaderixel Link**
under **Edit > Plugins**.

If your package contains source only, copy `Unreal/Sources/ShaderixelLink` into
the `Plugins` folder of a C++ Unreal project, then build the project's Editor
target for your engine version.

The plugin automatically locates the most recently launched Shaderixel executable
on the computer. To select it manually, open **Project Settings > Plugins >
Shaderixel Link > Executable Path**.

## Usage

The current French plugin menu labels are included in parentheses where needed.

- **Content Browser > right-click a Static Mesh > Paint in Shaderixel**
  (`Peindre dans Shaderixel`) **> material slot** opens a Paint 3D project with
  the mesh, the UV channel selected in the settings, and that slot's material
  textures. BaseColor, Normal, ORM, Roughness, Metallic, AO, Emissive and Height
  are recognized from their parameter or texture names. The original textures
  are placed in an Unreal source-texture layer. Paint on top, then press
  **Ctrl+S** or choose **Send to Unreal** to reimport the textures into the same
  assets with their settings preserved.
  Enable **Live** (`Direct`) in Shaderixel's status bar to send changes
  automatically after each edit and update the material in the Unreal Editor.
- For a material without textures, the plugin creates `T_<mesh>_BC`,
  `T_<mesh>_N`, `T_<mesh>_ORM` and an instance named `MI_<mesh>`. Its parent,
  `M_ShaderixelPBR2`, is created once under `/Game/Shaderixel`. The new assets are
  placed in a `Shaderixel` folder beside the mesh and assigned to its material slot.
- **Right-click a Texture 2D > Edit in Shaderixel** (`Éditer dans Shaderixel`)
  opens the texture as an image. Press **Ctrl+S** to reimport it.
- **Tools > Shaderixel > Import a Shaderixel Model...**
  (`Importer un modèle Shaderixel...`) accepts either:
  - a Paint 3D project (`*.sx3d`), exported in the background by Shaderixel using
    `--export-unreal`; or
  - a description written by Shaderixel's **Export for Unreal** command
    (`*.sxmat.json`, alongside the OBJ model and textures).

  The import creates the following assets in the current Content Browser folder:
  a static mesh named `SM_<name>` with its original units and position,
  adjustable scale and generated lightmap UVs; `T_<name>_BaseColor`, `_Normal`,
  `_ORM` and `_Emissive` textures with the appropriate sRGB and compression
  settings; and an `MI_<name>` material instance already assigned to the mesh.
  The parent is `M_ShaderixelPBR2` (BaseColor, Normal, ORM and Emissive multiplied
  by EmissiveStrength), or the parent selected in Shaderixel. The name, output
  folder, scale, parent, two-sided setting and emissive strength are saved in
  the `.sx3d` project. The same menu also imports texture-only descriptions
  (`*.sxue.json`).
- A `.sxmat.json` file placed beside an FBX or OBJ file **with the same base
  name** is applied automatically when the model is imported normally through
  drag-and-drop or the Import command, including textures and the material instance.
- Console commands: `Shaderixel.Import <file> [folder]`, `Shaderixel.Paint`,
  `Shaderixel.EditTexture` and `Shaderixel.Poll`. **Tools > Shaderixel** also
  provides access to the sessions folder.

Shaderixel projects remain in their session folder
(`%LOCALAPPDATA%\Shaderixel\Link\sessions\...`). Reopening the same mesh restores
its layers as long as its geometry has not changed.

## Exchange format

The plugin writes `session.json` with:

- `kind`: `mesh` or `texture`;
- `mesh`: OBJ file, Y-up, in meters;
- `textures`: input roles, files and assets;
- `outputs`: output roles, destination files and target assets;
- `project`: the Shaderixel project to reopen.

After writing the output files, Shaderixel writes `result.json` with the revision
and outputs. The plugin watches it and reimports the results. An already running
Shaderixel instance receives new sessions through `Link\inbox`.

## Build and test

From the Shaderixel repository:

```bat
python tools/build_unreal_plugin.py [versions]
python tools/test_unreal_link.py [versions]
```

The first command builds the plugin for the selected installed engines. The
second runs an end-to-end test without showing application windows.

Additional console command forms:

```text
Shaderixel.Paint <mesh asset path> [material slot]
Shaderixel.EditTexture <texture asset path>
Shaderixel.Poll
```

## License

The Shaderixel Link connector in this folder is licensed under the
[MIT License](LICENSE). Unreal Engine is not covered by this license and remains
subject to Epic's terms. The separate Shaderixel application is licensed under
GPL-3.0-or-later; see the [repository README](../../README.md#license).
