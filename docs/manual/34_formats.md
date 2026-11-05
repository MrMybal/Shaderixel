# Import formats and interoperability

## File families

- Editable projects: .sxl and .sx3d. Open the native file to recover the stack/graph rather than importing its flattened export.
- Common image choices: PNG, JPEG, BMP, TGA, HDR, GIF and layered PSD/PSB. Image opening and animation-frame editing are separate paths; do not assume a GIF import recreates an editable frame timeline.
- Extended decoder paths: TIFF, WebP, HEIC/HEIF/JXR and camera RAW families such as DNG, CR2/CR3, NEF, ARW, ORF, RW2 and RAF. File-picker recognition does not guarantee every camera variant/codec is available in every installation.
- Image delivery: PNG, JPEG, TIFF, TGA and BMP through the applicable export dialog; layered PSD/PSB and animated GIF/MP4 have their own commands.
- Reusable resources: .slang effects/brushes, .sxfn image functions, native templates plus metadata/thumbnail, smart-material JSON, .cube LUTs and .sxactions recorded actions. Import each through the matching editor/panel rather than treating it as an ordinary bitmap.

An alpha-capable format does not guarantee the source is transparent. A 16-bit file is not necessarily a floating-point data file. Check the actual decoding/output path and reopen a sample before converting a large collection.

## Images and layered documents

Open supported raster formats through File or the resource browser. PSD/PSB import/export preserves supported layer information, but another application's complete effect/adjustment model is not guaranteed to round-trip exactly. Keep a native Shaderixel project as the working master.

Camera RAW decoding depends on the installed/distributed decoder and file. LUTs use .cube files. HDR environments, normal maps and scalar maps need their respective interpretation rather than arbitrary color grading.

## Meshes and Unreal assets

Mesh import supports the formats available in the current build, including OBJ, glTF/GLB and FBX paths. Check UVs, material assignments, orientation and texture links after import.

Unreal texture .uasset support differs from mesh .uasset conversion. Mesh conversion can require an installed Unreal Editor and its commandlet/glTF path. Copying a .uasset is not a general replacement for engine-assisted export.

The separate ShaderixelLink connector exchanges project/material data with Unreal. Check its version/setup and the generated normal, color-space and packed-channel settings in Unreal before relying on the result.

## Portable handoff

Collect external images, linked objects, fonts, meshes, HDRIs and workflows as needed. API workflows still require their ComfyUI models/custom nodes. Exporting a flattened image does not transfer its editable node graph.

[Export](#export)
[Resources](#resources)
