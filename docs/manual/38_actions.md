# Recorded actions and batch processing

Actions are ordered editor commands with JSON parameters. They differ from image-graph execution nodes: an action replays document edits, whereas a graph event runs the flow inside a function. The Commands reference contains all 20 supported action command types, including their new-step defaults and replay limitations.

Help > Action reference opens this guide. In Action Editor, select a step and press F1 or use Command help to open its individual reference. Commands is also a search category in the native and HTML readers.

![Explicit execution is separate from an image connection or live preview.](media/execution_en.png)

## Record and refine an action

1. Open Actions or Action Editor from Window. Create a new action and give it a descriptive name.
2. Start Record, perform supported menu/dialog commands, then stop recording. Recording appends steps; it is not a video of every gesture.
3. Inspect the actual step list. Image/canvas size, rotations, crop, filters, layer operations, selections, fill/erase and supported operations can be represented. Arbitrary brush strokes, pointer motion and external AI results are not automatically reproduced as recorded pixels.
4. Select a step to inspect its parameters and description. Numeric controls accept typed input. Advanced Parameters (JSON) applies valid object JSON immediately; incomplete text does not replace the previous valid parameters.
5. Reorder, duplicate or disable steps using the editor. Copy a read-only built-in action into My Actions before modifying it.
6. Test the entire action on a working copy before applying it to several files. Check dimensions, active layer, alpha and written output, not only the completion message.

For a recorded non-destructive filter, its tuned numeric parameters are finalized when the next step is recorded or recording stops. Curve/LUT resources are not arbitrary payloads in the numeric filter params array. Keep required user effects and resources available on the receiving installation.

## Execution order and partial runs

An entire action skips disabled steps. The context-menu command Run This Step intentionally runs the selected step even if its checkbox is off. Run Up To Here and Run From Here run their ranges with normal enabled-state filtering.

The runner waits for operations such as compilation, readback and export. A reported step error stops subsequent steps, but earlier edits/files remain. Several unsupported layer commands instead report status or do nothing without stopping the runner. A successful run therefore does not prove that every input had the expected layer arrangement.

Targets are stateful: New Layer and Duplicate Layer change the active layer; Select changes the restriction for later fill/filter commands. A command does not select a layer by a recorded display name unless that command explicitly implements such selection. No automatic rollback turns the whole action into one transaction.

## Import, export and JSON

Action Editor imports .sxactions or JSON and exports .sxactions. It accepts an object containing an actions array, or one action object containing steps. Imported names are numbered when they already exist. Importing adds actions; it does not run them.

This complete action can be saved as a JSON file and imported. It fits into 1024 × 1024 without enlarging smaller images and does not export until you add an Export step or use the batch final output:

```json
{
  "name": "Practice - Fit inside 1024",
  "steps": [
    {"cmd":"image_size","enabled":true,"params":{"mode":"fit","w":1024,"h":1024,"enlarge":false,"interp":2}}
  ]
}
```

Each step has cmd, params and enabled. Identifiers, parameter keys and enum strings stay in English in both language editions. New Step fills factory defaults; an imported empty params object uses that command's runtime fallbacks instead. Those are not always identical: for example, new filter steps choose destructive black_white, while omitted destructive is false at execution.

## Prepare a batch

1. Open File > Automation > Batch, or Batch from Action Editor.
2. Add actions in the required order. With no actions, a chosen output format performs a simple conversion. With neither actions nor final output, there is nothing to run.
3. Choose Folder or Open Documents. Folder can include subfolders and an extension filter such as png jpg. Open Documents uses the documents currently open when the batch starts.
4. Set final format, destination, naming pattern and existing-file behavior. A separate writable destination makes the result easier to inspect.
5. Test on a small input set. Start the batch and inspect the log, output files and counts, including skipped/error entries.

Folder inputs are opened, processed and closed; their in-memory edits are not otherwise saved unless an export/native output writes them. Open Documents is different: actions modify those documents directly, with their history, and the documents remain open.

## Names, folders and collisions

Naming tokens remain literal in every interface language:

- {nom}: original document/file stem.
- {n}: one-based input number; {n3}: the same padded to three digits.
- {l}, {h}: current document width and height when the name is evaluated.
- {dossier}: source folder name; {ext}: original extension without its dot.
- {date}: current date as YYYYMMDD.

For example, {nom}_{n3}_{l}x{h} can produce brick_001_512x256 after resizing. Name previews can show original sizes before processing; the final name uses the current dimensions. Invalid filename characters are replaced by underscores.

Recreate Subfolders applies to folder input when recursion is enabled and a destination is supplied. An empty destination uses source/document folders; an unsaved document falls back to the settings folder.

The batch final output offers Replace, Skip or Numbered Copy. If that final output resolves to the source file itself, the implementation numbers a copy instead of replacing the source. This protection and the selected collision policy do not cover internal Export steps: those steps can directly replace their own destination.

## Final output versus Export steps

Format None means only explicit Export steps write files. A processed item with no such step can correctly report processed (no file written).

Choosing a final format writes after the action chain. If the actions also contain Export steps, both kinds of output can be written. The batch destination/name can override those internal steps' location/stem; each internal suffix remains appended. Avoid giving internal and final output the same filename unless replacement is intentional.

Use lower-case format IDs in JSON. SXL keeps native structure; PSD preserves supported layers; ordinary raster output is the visible composition. JPEG has no alpha and remaining transparent pixels become black. The flatten action step is a no-op for a single-layer document, so it alone does not guarantee a white JPEG matte.

## Cancel, errors and validation

Cancel waits for the current action to finish before the batch stops; it does not undo completed edits or files. A failed file is logged and later inputs can still be processed. Check the log rather than treating the done count as proof of a particular visual result.

For unexpected results, check active target, selection, visibility, required effect IDs, input dimensions, optional fields, final-output choice and actual destination. A native copy is needed if you want editable layers/graphs after a raster delivery.

## Command reference

[Image Size](#command:image_size)
[Fit and Center Crop](#command:fit_crop)
[Canvas Size](#command:canvas_size)
[Rotate or Mirror](#command:rotate)
[Arbitrary Rotation](#command:rotate_arbitrary)
[Crop to Selection](#command:crop_selection)
[Apply Filter](#command:filter)
[New Pixel Layer](#command:new_layer)
[Duplicate Layer](#command:duplicate_layer)
[Bake to Pixel Layer](#command:rasterize_layer)
[Merge Down](#command:merge_down)
[Merge Visible](#command:merge_visible)
[Flatten Action](#command:flatten)
[Selection Action](#command:select)
[Fill Action](#command:fill)
[Erase Action](#command:erase)
[Layer Properties](#command:layer_props)
[Export Action](#command:export)
[Photo RAW Action](#command:photo_raw)
[Registered Operation](#command:op)

[Graph functions and events](#automation)
[Layer versus graph baking](#tools)
[Export and delivery](#export)
