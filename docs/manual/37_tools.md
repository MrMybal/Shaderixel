# Tools and editing commands

Each of the 46 toolbar tools has a reference page in the Tools category. Search by its English/French name, stable tool ID, operation or prerequisite. With the canvas, toolbar or Options focused, F1 opens the active tool's page. Options also offers Tool help. Help > Tool reference opens this index.

![The toolbar chooses an interaction; Properties and Options edit its current target and settings.](media/layers_view_en.png)

## Choose the target before the tool

An image thumbnail targets image pixels; a mask thumbnail targets coverage. A selected graph node can be a generated effect, a live function or a retained result rather than paintable pixels. The current selection restricts pixel operations independently of layer visibility and masking.

Canvas and 3D view keep context-specific tool choices. Some tools appear only in the 3D workflow. Right-click a toolbar group to choose its exact member. Keys printed in a tool hint can be shared by a group or depend on the active view; the shortcut chapter describes cycling and exceptions.

If a stroke seems ineffective, check the active document, selected layer, image/mask thumbnail, lock, opacity, selection and visible PBR channel. F1 is available while typing; inside the reader it retains the current article.

## Move, frame and inspect

[Move](#tool:move)
[Layer Transform](#tool:layer_transform)
[Crop](#tool:crop)
[Ruler and straightening](#tool:ruler)
[Hand](#tool:hand)
[Zoom](#tool:zoom)
[Eyedropper and persistent samplers](#tool:picker)

## Select regions and build geometry

[Rectangular Selection](#tool:rect_select)
[Elliptical Selection](#tool:ellipse_select)
[Lasso](#tool:lasso)
[Polygonal Lasso](#tool:poly_lasso)
[Magnetic Lasso](#tool:magnetic_lasso)
[Magic Wand](#tool:wand)
[Quick Selection](#tool:quick_select)
[Pen and paths](#tool:pen)
[Text](#tool:text)
[Shape](#tool:shape)
[Trim Sheet](#tool:trim)

## Paint, fill and erase

[Brush](#tool:brush)
[Mixer Brush](#tool:mixer)
[Color Replacement](#tool:color_replace)
[Gradient](#tool:gradient)
[Paint Bucket](#tool:bucket)
[Pattern Stamp](#tool:pattern_stamp)
[Eraser](#tool:eraser)
[Magic Eraser](#tool:magic_eraser)
[Background Eraser](#tool:background_eraser)

## Repair and adjust local detail

[Clone Stamp](#tool:clone)
[Healing Brush](#tool:heal)
[Spot Healing](#tool:spot_heal)
[Patch](#tool:patch)
[Content-Aware Move](#tool:content_move)
[Red Eye](#tool:red_eye)
[History Brush](#tool:history_brush)
[Blur Brush](#tool:blur)
[Sharpen Brush](#tool:sharpen)
[Smudge](#tool:smudge)
[Dodge](#tool:dodge)
[Burn](#tool:burn)
[Sponge](#tool:sponge)
[Liquify](#tool:liquify)

## Paint 3D, simulations and extensions

[Projection](#tool:projection)
[Polygon Fill](#tool:poly_fill)
[Paint Particles](#tool:particles)
[Fluid Paint on canvas or model](#tool:fluid)
[Plugin Tool](#tool:plugin)

## Choose the command that produces the intended result

- Bake to Pixel Layer: replace one rendered layer's content with independent paintable pixels. Its transform/filters become content; mask, opacity and blend remain editable. Use this when a generated/text/shape/smart layer needs direct painting. Undo restores the original while that history remains available.
- Merge Down: combine the active layer with the eligible layer below in the same group. Groups are not supported, and the lower layer cannot be an adjustment. Use this for a joint pixel result, rather than converting just one layer.
- Merge Visible: replace the visible composition with a merged pixel layer while keeping hidden layers. Hidden children of a group that merged can move to the root; groups left empty disappear. Transparency is retained.
- Flatten Image: replace the layer stack with one pixel layer, remove hidden layers and composite transparent areas over white. Keep an editable native copy when you need layers or transparent delivery later.
- Graph Bake or Apply: retain a computed graph result. Upstream edits do not automatically regenerate that result. Rerun the event, reapply or clear results as appropriate. This does not promise a paintable layer in the layer stack.
- Mesh bake: derive maps such as occlusion or curvature from geometry and UVs. It needs a suitable model/UV setup and differs from both graph freezing and 2D layer conversion.
- Free Transform: start a transform session on a supported target; Enter commits and Escape cancels. Layer Transform is a separate tool with mode keys and numeric properties.
- Image Size: change document pixel dimensions and resample as configured. Zoom changes the view; Canvas Size changes the framing/extent. They are different operations.
- Clear/Deselect: removing a selection with Ctrl+D removes the editing restriction, not the selected image pixels. A layer mask can still hide content afterward.
- Save/Save As: preserve an editable native document. Export produces a delivery image and does not replace the editable graph/layer structure in the project.
- Run tool event: deliberately execute an export, upscale or AI action. Connecting an image or opening a practice document does not execute the event.

Use the command's enabled state and status feedback when a target is unsupported. Keep a native working copy before operations that collapse editable structure.

[Layer operations and snapshots](#layers)
[Dimensions and transformation sessions](#dimensions)
[Live graphs, results and execution](#automation)
[UVs and mesh baking](#uv_baker)
[Saving and delivery formats](#formats)
[Guided practice projects](#tutorials)
