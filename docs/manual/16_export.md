# Export and delivery

Save the native project first. A delivery image generally stores rendered output, whereas native files preserve the editing structure.

## Formats

File > Export provides direct export. Export As offers format, transparency and delivery options, including PNG, JPEG, TIFF 16-bit, TGA and BMP.

PNG supports alpha; JPEG does not, so choose the intended matte/background. Graph PNG export tools use 8-bit PNG regardless of document precision.

Check dimensions, transparency, encoding and export area. Zoom does not affect output size. Export As can produce variants from layers or selected areas according to its settings.

## Editable files and variants

File provides layered PSD/PSB, artboard and layer-composition export. Keep the native project: another editor may not reproduce every Shaderixel effect.

Graph export tools run only on their event. Relative filenames resolve beside a saved document or under exports/ beside the executable for an unsaved one. Reusing a filename/prefix replaces output; change it to keep variants.

## Engines and print

Enable sRGB for albedo and disable it for numeric PBR maps in the target engine. Verify OpenGL/DirectX normals and packed-channel layouts. ORM means R=occlusion, G=roughness, B=metalness.

UI skin export uses the full canvas unless the chosen operation crops it. Leave space for shadows or intentionally create a tight canvas.

CMYK TIFF and soft-proof controls support print preparation. Preview proofing does not remove the need for correct destination profiles and export settings.

[Color, precision and data maps](#color)
[Guides, alignment and artboards](#layout)
