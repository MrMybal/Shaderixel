---
id: documents
title: Documents, saving and templates
description: A native .sxl file stores an editable image document.
section: Handbook
order: 2
parent: getting-started
revision: 68f1807994532397
---

A native .sxl file stores an editable image document. Paint 3D projects use .sx3d. A flattened export is a delivery image; it does not replace the editable project.

## Create and open {#create-and-open}

File > New chooses size, precision and Layers or Node mode. Templates provide a configured starting stack or graph. File > New Paint 3D Project starts model painting.

Ctrl+O opens a supported image or document. Ctrl+Shift+O imports an image as a layer. Dropping a file can open a document, import an image or insert a resource according to its type and drop target. Use File > Open for a supplied .sxl example when you want the whole example composition.

## Save editable work {#save-editable-work}

Use one native master for continued editing and separate, clearly named exports for delivery. Save As creates a new working path; it is also the base for later relative graph exports. Opening a bundled practice example starts an unsaved copy, so Save asks for a new destination.

Layers mode builds a stack; Node mode gives direct image connections. Converting a layer stack exposes its generated graph. Returning an arbitrary custom graph to Layers is only offered when the structure can be represented; do not assume every graph is reversible to a layer stack. Save a version before changing modes.

Ctrl+S saves; Ctrl+Shift+S chooses another path. A changed document has an unsaved marker. Closing it offers save, discard and cancel.

Save before running export tools with relative filenames: they resolve beside the saved document. An unsaved document uses exports/ beside the executable.

A linked smart object still depends on its external source. Moving only the parent document does not relocate that source. Embed content for a self-contained handoff.

## Templates and precision {#templates-and-precision}

File > Save as Template creates a reusable starting document and metadata. Templates start whole documents; functions insert reusable nodes. User templates are in %LOCALAPPDATA%\Shaderixel\Templates. Shipped templates are in templates/ beside the installed executable.

Use 32-bit float for UV coordinate fields, signed distances and calculations outside 0 to 1. Preview colors may clip while the underlying values remain valid. A high-precision document does not make an 8-bit PNG export high precision.

[Export and delivery](doc:export)
[Smart objects](doc:smart-objects)
