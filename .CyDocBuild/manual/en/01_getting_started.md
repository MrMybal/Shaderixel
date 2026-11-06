---
id: getting-started
title: Getting started
description: Shaderixel is a GPU image editor with a layer stack and an image node graph.
section: Start here
order: 0
revision: 1b867e0fd375fb13
---

Shaderixel is a GPU image editor with a layer stack and an image node graph. The separate visual effect graph builds the calculation inside one effect.

```image
{"id": "layers-view", "src": "assets/manual/layers_view_en.png", "alt": "Layer workflow: canvas, generated graph and editable properties.", "controls": "minimal"}
```

Help > Documentation or F1 opens this manual without an Internet connection. Search covers the handbook, shipped effects/functions, toolbar tools, brushes, recorded commands and all built-in node operations. User-installed presets may not be listed.

F1 uses the focused panel: a selected effect opens its library reference, while a painting tool or editor opens its handbook chapter. Properties also has Selection help; the graph node context menu offers the same command. Unknown or renamed functions open the libraries chapter, and custom effect overrides open effect authoring. Help > Documentation reopens the last page requested from the application. Use browser history and My space to resume browsing inside the reader.

CyDoc opens the complete local manual in your browser, without a server or Internet connection. Use the page outline for sections, the language selector for English/French, and browser zoom for reading size. Search at the top finds pages as you type; Ctrl+K opens the search dialog. Image controls enlarge illustrations inside the reader; Escape reduces them. My space and Resume reading retain reading data when local storage is available.

## Install and launch {#install-and-launch}

Use the Windows x64 installer or extract the portable ZIP into a writable folder. Windows 10 version 2004 or later and a D3D12-capable GPU are required for Shaderixel.exe; ShaderixelVk.exe uses a compatible Vulkan driver. The per-user installer includes the runtime libraries and does not need administrator rights.

Use Help > Check for updates for the public GitHub channel. Startup and prerelease checking are configurable there. Save your editable projects separately from delivery images and keep external linked assets with the project.

## First image in five minutes {#first-image-in-five-minutes}

1. Choose File > New, set canvas width, height and precision, and start in Layers mode. Alternatively open an image with Ctrl+O.
2. Select a pixel layer in Layers, choose Brush (B), pick a foreground color and paint.
3. Apply an effect as a layer filter. Change its parameters in Properties; disable the filter to compare.
4. Add a layer mask to hide part of the layer. Select the image thumbnail again before painting color.
5. Save with Ctrl+S as a native document. Use File > Export or Export As for a delivery image.

A checkerboard means transparency. An empty pixel layer needs paint or a fill before it becomes visible.

## Choose a starting point {#choose-a-starting-point}

- Photo editing: open the photo and try Photo Essentials Develop.
- Material extraction: connect an image to PBR Photo to Maps in the main graph.
- UI panels: use a 32-bit float graph document and UI UV Nine Slice Panel.
- Model painting: create a Paint 3D project and choose or import a mesh.

[Workspace and navigation](doc:workspace)
[Main image graph](doc:image-graph)
[Photo to PBR maps](doc:pbr)
[UI textures and slicing](doc:ui-textures)

## Find a complete procedure or reference {#find-a-complete-procedure-or-reference}

Guided projects includes seven worked exercises and six openable practice files. The built-in node index distinguishes image graphs, per-pixel shaders and explicit execution. The panel directory explains every registered panel/dialog. Settings covers interface/tablet/history/AI preferences and the update flow; Delivery and Recovery cover the final checks and common failure paths.

[Complete built-in nodes](doc:builtin-nodes)
[Panel and dialog directory](doc:panels)
[Settings and updates](doc:settings)
[Asset delivery](doc:delivery)
[Diagnosis and recovery](doc:recovery)
