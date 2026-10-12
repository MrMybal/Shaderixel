# Photo, PBR and ComfyUI Texture Library

This library adds 17 shaders and 15 functions. English is the base language for
preset names, categories, controls, descriptions and messages. Shader filenames
and parameter identifiers remain stable; translating display text does not change
effect references. Function filenames now match their English display names.

Functions appear in the node library under **PBR**, **Textures** and **ComfyUI**.
Main controls are function inputs; additional controls are exposed variables.

## Start from a photo

1. Load a front-facing material photo with lighting as uniform as possible.
2. If needed, use **Textures Seam Blending and Inspection** before map extraction.
3. Connect the photo to **PBR Photo to Maps**.
4. Adjust detail height, normal strength and base roughness.
5. Adjust lighting radius, height inversion and occlusion in the exposed variables.
6. Choose metalness manually: `0` for a dielectric, `1` for a metal.
7. To bake and save all seven maps, use **PBR Photo to Maps and PNG Export**,
   set the file prefix and run **Extract and Export**.

With the prefix `material`, the files are `material_albedo.png`, `material_height.png`,
`material_normal.png`, `material_occlusion.png`, `material_roughness.png`,
`material_metal.png` and `material_orm.png`. Relative paths resolve beside the saved
document, or under `exports/` beside the executable for an unsaved document.
Exporting with the same prefix replaces those files; change the prefix to keep variants.

Albedo correction, height, occlusion and roughness are **estimates**. Photo luminance
mixes material color, surface relief and lighting; dark areas are not necessarily cavities.
Metalness is selected manually. AI scene depth does not measure surface microrelief.

## Data values and 3D conventions

Shaderixel stores linear colors and encodes PNG exports as sRGB. These new map
shaders compensate by storing `SrgbToLinear(data)` and reading
`LinearToSrgb(color)`. Exported PNGs therefore contain the expected numeric values:
a flat normal is approximately `(128, 128, 255)` and roughness `0.5` is approximately `128`.

- Import **albedo with sRGB enabled** in the 3D engine.
- Import **height, normals, roughness, occlusion, metalness and packed maps with sRGB disabled**.
- In Shaderixel, import these PNGs through the normal color-image path; these shaders
  recover their numeric data with the inverse conversion.
- Older `cy_effect_*` height and normal effects may use different storage conventions;
  use the `texture_*` effects for this processing chain.
- Normals default to **OpenGL**. Enable **DirectX Normals** to flip green.
  **PBR Normal Blend and Conversion** also converts existing maps.
- ORM means `R = occlusion`, `G = roughness`, `B = metalness`.
  RMA and MRA are available through **PBR Channel Packing**.
- Packed alpha defaults to opaque. Enable height in alpha only when the target engine
  expects it; the preview will then appear transparent.
- Generated scalar maps and normals are opaque. Use a separate material opacity mask.

The export tool produces **8-bit PNG**. For more precise displacement height, use a
16-bit document export with sRGB encoding preserved to recover the data values.
A raw linear floating-point export instead contains the compensated storage values.

## ComfyUI and upscaling

**Textures Upscale and Export** defaults to local bicubic upscaling. Enable
**Use ComfyUI** to use an AI model installed in `models/upscale_models`; enter its
exact filename in **Upscale Model**. Scale factor is 1 to 8. The PNG export keeps
the full enlarged resolution, while the graph output is fitted to document size.
AI upscaling is primarily intended for color textures: it can invent detail or
change the numeric values of normals, height and packed maps.

**ComfyUI Texture Image to Image** uses the server and checkpoint configured in AI
settings. An empty workflow path uses the built-in image-to-image pipeline.
Prompt and AI denoise are inputs; final cleanup is an exposed variable.
The tiled preview helps inspect seams but does not guarantee tileability.

**ComfyUI Depth to Surface Maps** and **ComfyUI PBR Map Workflow** require a workflow
exported from ComfyUI in **API format**. The workflow, its nodes and models must
already be available in your installation. These presets do not install specialized
models or workflows. An empty path shows a message without running AI.

Use `LoadImage` for the input photo and a single `SaveImage` for the expected map.
Shaderixel retrieves the first generated image: use one tool instance and one
workflow per map. Custom workflow denoise remains the value saved in that workflow.
Supported substitutions include LoadImage, text encoder prompts, seeds and certain
latent dimensions.

Actions preview their input until executed. Only a computed result is an AI map;
clearing results restores the live preview. No ComfyUI requests were run during validation.

## Seam blending and texture processing

At strength `1`, seam blending matches opposite edge pixels by blending reflected
samples. This can change patterns and does not guarantee derivative continuity.
Wrapped offset moves seams to the center for inspection. Procedural microrelief
produces periodic height; normals, occlusion and curvature use clamped document
edges, so perfectly continuous boundary derivatives are not guaranteed.
Normal blending expects both maps to use the same OpenGL or DirectX convention.
Use color processing and ordinary sharpening for albedo, and dedicated normal
adjustment effects for normal maps.

## Added shaders

| File | Description |
|---|---|
| `texture_albedo_photo.slang` | Albedo from Photo. |
| `texture_ao_hauteur.slang` | Estimated Occlusion from Height. |
| `texture_apercu_repetition.slang` | Tiled Texture Preview. |
| `texture_carte_reglage.slang` | Data Map Adjustment. |
| `texture_courbure_hauteur.slang` | Curvature from Height. |
| `texture_debruitage.slang` | Edge-Preserving Texture Denoise. |
| `texture_decalage_boucle.slang` | Wrapped Texture Offset. |
| `texture_details.slang` | Texture Detail and High Pass. |
| `texture_draw_carte_constante.slang` | Constant Data Map. |
| `texture_draw_microrelief.slang` | Tileable Procedural Microrelief. |
| `texture_empaquetage_pbr.slang` | PBR Map Packing. |
| `texture_fusion_normales.slang` | Reoriented Normal Blend. |
| `texture_hauteur_photo.slang` | Estimated Height from Photo. |
| `texture_normales_hauteur.slang` | Normals from Height. |
| `texture_normales_reglage.slang` | Normal Adjustment and Conversion. |
| `texture_raccord_bords.slang` | Texture Edge Seam Blending. |
| `texture_rugosite_photo.slang` | Estimated Roughness from Photo. |

## Added functions

| File | Description |
|---|---|
| `ComfyUI Depth to Surface Maps.sxfn` | Requires a depth API workflow installed in ComfyUI with its models and nodes, LoadImage and a single depth SaveImage. Empty path: show a message without running AI. Adapt scene depth into height, normals, occlusion and curvature; depth does not measure surface microrelief. Outputs preview the input before execution. |
| `ComfyUI PBR Map Workflow.sxfn` | Run a supplied API workflow for one PBR map: normal, roughness, albedo, etc. Requires LoadImage and a single SaveImage for the required map. Shaderixel retrieves only the first output image; use one instance per map. Empty path: show a message without running image-to-image. Output previews the photo before execution. |
| `ComfyUI Texture Image to Image.sxfn` | Process Texture runs ComfyUI image-to-image, then light denoise and tiled inspection. Empty workflow: built-in pipeline using the AI settings checkpoint. Optional API workflow: only documented Shaderixel substitutions apply; denoise remains the value saved in that workflow. |
| `PBR Channel Packing.sxfn` | Pack occlusion, roughness and metalness into ORM, RMA or MRA. Optional height in alpha makes the map appear transparent. Missing inputs use occlusion 1, adjustable roughness and adjustable metalness. |
| `PBR Data Map Adjustment.sxfn` | Adjust levels and curve for a channel: height, depth, roughness, occlusion or metalness mask. Also extract channels from packed maps. Output is opaque grayscale. |
| `PBR Height to Surface Maps.sxfn` | Convert a grayscale height or depth map into adjusted height, OpenGL or DirectX normals, estimated occlusion and signed curvature. Map values correspond to the imported/exported PNG values. |
| `PBR Normal Blend and Conversion.sxfn` | Blend two normal maps using the same convention with reorientation, optional mask and detail strength. Flip output green to convert OpenGL / DirectX. PNG data maps; avoid color adjustments on normals. |
| `PBR Photo to Maps and PNG Export.sxfn` | Extract and Export estimates, bakes and exports albedo, height, normals, occlusion, roughness, manual metalness and ORM. File prefix without extension. PNG is 8-bit; disable sRGB for data maps in the 3D engine. For precise height, use a 16-bit document export that keeps sRGB encoding. |
| `PBR Photo to Maps.sxfn` | Estimate seven maps from a photo: albedo, height, normals, occlusion, roughness, manually chosen metalness and ORM. Shading and color can be confused with surface relief. Data maps preserve numeric values through Shaderixel sRGB PNG export; disable sRGB in the 3D engine for all maps except albedo. |
| `PBR Procedural Microrelief.sxfn` | Generate periodic multi-octave height, normals and occlusion. Seed and density are exposed. No source image required. Height is tileable; normal derivatives use clamped document edges. |
| `PBR Roughness and Glossiness.sxfn` | Estimate roughness from photo detail and luminance, then output complementary glossiness. Base roughness remains an artistic and physical choice. |
| `Textures Cleanup and Albedo.sxfn` | Edge-preserving denoise, approximate removal of large-scale lighting variations, then adjustable sharpening. Outputs cleaned photo and corrected albedo. |
| `Textures Detail and High Pass.sxfn` | Separate high frequencies using Gaussian blur. Outputs adjustable sharpening and a high-pass map centered on gray 0.5. For color textures, not normal renormalization. |
| `Textures Seam Blending and Inspection.sxfn` | Blend opposite texture edges, output a wrapped offset for seam inspection and a tiled preview. At strength 1, opposite edge pixels match; patterns may still need retouching. Apply before generating normals. |
| `Textures Upscale and Export.sxfn` | Upscale and Export uses local bicubic or a ComfyUI AI model, then exports PNG at full enlarged resolution. Graph output is fitted to document size. Scale factor 1 to 8; install the selected model in ComfyUI when AI is enabled. |

## Validation

- `build\Release\Shaderixel.exe --test-effects`: minimized launch, autonomous exit, exit code `0`.
- `shaderixel.log`: **335 / 335 effects compiled**, including all 17 new `texture_*` shaders.
- Native `--demo library` insertion verifies the 15 function signatures.
- Structural validation checks effect and parameter references, variable names, typed
  connections, output ports and acyclic graphs.
- Numeric checks cover all 256 PNG values, a neutral normal, neutral normal blending
  and complementary roughness/glossiness.
- No real mouse or keyboard input, and no ComfyUI requests.
