---
id: panels
title: Panel and dialog directory
description: This directory covers every panel in Window > Panels and the application's directly named user dialogs.
section: Handbook
order: 43
parent: workspace
revision: 0c5d1a28c8ce267a
---

This directory covers every panel in Window > Panels and the application's directly named user dialogs. Each entry explains its role and links to the corresponding workflow. F1 follows the focused panel, with more specific references for selected tools, nodes, brushes and action steps.

Use Window to reopen a panel, select its right-side group, and clear its filter if controls appear missing. A modal operation may need to finish or be canceled before returning to the rest of the editor. Help > Documentation reopens the last page requested from the application, without starting an operation.

Choose a panel below to find its role and the workflow it belongs to.

[Workspace and navigation](doc:workspace)
[Settings](doc:settings)

## Layers {#layers}

Layer order, visibility, locks, groups, masks and pixel conversion.

[Open the workflow guide](doc:layers)

## Graph {#graph}

Image connections, typed values, reusable functions and execution links. Select a node for its exact F1 reference.

[Open the workflow guide](doc:image-graph)

## Layer preview {#layer-preview}

Inspect the selected layer/node separately from the final composition; verify the intended view when diagnosing transparency.

[Open the workflow guide](doc:layers)

## Effect editor {#effect-editor}

Edit per-pixel shader graphs or code, compile and preview. A selected shader node has its own F1 reference.

[Open the workflow guide](doc:effect-editor)

## 3D view {#3d-view}

Inspect and paint a UV-mapped model, its material channel and lighting. F1 also follows the active tool.

[Open the workflow guide](doc:paint3d)

## Properties {#properties}

Parameters of the selected layer or node, dimensions, masks and exposed function variables.

[Open the workflow guide](doc:layers)

## History {#history}

Undo states and snapshots; session history does not replace saving an editable project.

[Open the workflow guide](doc:layers)

## Navigator {#navigator}

A small document view for navigating a zoomed canvas without changing output dimensions.

[Open the workflow guide](doc:workspace)

## Histogram {#histogram}

Distribution of displayed image/channel values; an empty transparent result can have no useful sample population.

[Open the workflow guide](doc:photo)

## Image analysis {#image-analysis}

Exposure, clipping and comparison overlays. These change inspection, not the exported image content.

[Open the workflow guide](doc:photo)

## Options {#options}

Controls for the active tool, including numerical input and Tool help.

[Open the workflow guide](doc:painting)

## Paths {#paths}

Edit paths and convert between path geometry, selection, fill and stroke.

[Open the workflow guide](doc:masks)

## Actions {#actions}

Choose, record and run supported editor-command sequences.

[Open the workflow guide](doc:actions)

## Layer comps {#layer-comps}

Save layer-state variants and restore/export them; these are not independent pixel documents.

[Open the workflow guide](doc:layers)

## Text styles {#text-styles}

Store and reuse typography settings, while keeping text content editable.

[Open the workflow guide](doc:text-shapes)

## Effects {#effects}

Search shipped/user effects and generators, inspect previews and insert them into an appropriate target.

[Open the workflow guide](doc:libraries)

## Node palette {#node-palette}

Find main image-graph nodes by category/name; shader operations belong to Effect Editor.

[Open the workflow guide](doc:builtin-nodes)

## Blends {#blends}

Blend mode, opacity, coverage and conditional Blend If controls for the current layer.

[Open the workflow guide](doc:blending)

## Color {#color}

Foreground/background color and component editing; distinguish display color from numeric material data.

[Open the workflow guide](doc:color)

## Swatches {#swatches}

Choose/import reusable colors; a swatch does not change a document profile.

[Open the workflow guide](doc:resources)

## Brushes {#brushes}

Choose procedural tips, textures and dynamics. Brush help follows the independent brush or eraser selection.

[Open the workflow guide](doc:brushes-tablet)

## Patterns {#patterns}

Choose, define and scale repeating image patterns for fills.

[Open the workflow guide](doc:resources)

## Materials {#materials}

Inspect and choose PBR materials; check channel interpretation and normal convention.

[Open the workflow guide](doc:pbr)

## 3D layers {#3d-layers}

Material fills, paint layers, channel enablement, masks and smart material assemblies.

[Open the workflow guide](doc:paint3d)

## Brush material {#brush-material}

Choose the material channels and values painted onto the 3D model.

[Open the workflow guide](doc:paint3d)

## Render {#render}

Accumulating presentation render, pause/restart, camera and image/pass export.

[Open the workflow guide](doc:rendering)

## Render scene {#render-scene}

Scene objects, transforms, material overrides, lights, environment, camera and quality.

[Open the workflow guide](doc:rendering)

## AI Assistant {#ai-assistant}

Assistant connection and local bridge; separate from the image-generation provider.

[Open the workflow guide](doc:ai)

## AI Generation {#ai-generation}

Provider, endpoint, model/workflow, prompt and generated image requests.

[Open the workflow guide](doc:ai)

## Trim sheet {#trim-sheet}

Named texture regions, strip layouts, guide conversion, ID layers and layout export.

[Open the workflow guide](doc:trims)

## AI Inpainting {#ai-inpainting}

Generate replacement content using a selected image region/mask and provider configuration.

[Open the workflow guide](doc:ai)

## Assets {#assets}

Browse supported images, materials and libraries; opening a document differs from inserting a function.

[Open the workflow guide](doc:resources)

## Action editor {#action-editor}

Edit/reorder steps, parameters and disabled states; F1 follows the selected command.

[Open the workflow guide](doc:actions)

## Animation {#animation}

Layer-state frames, duration, playback, tweening and GIF/video export.

[Open the workflow guide](doc:animation)

## Tools {#tools}

Grouped toolbar and active-tool selection; F1 follows the selected tool.

[Open the workflow guide](doc:tools)

## Canvas {#canvas}

Document result, painting and navigation. F1 follows the active tool or transform session.

[Open the workflow guide](doc:workspace)

## Panels {#panels}

Right-side groups and docking; show the correct group when a panel is hidden.

[Open the workflow guide](doc:workspace)

## Shader errors {#shader-errors}

Compiler diagnostics for failed programs, with source messages to correct before judging output.

[Open the workflow guide](doc:shader-authoring)

## Apply Image {#apply-image}

Combine a chosen source image/channel into the active target; verify source, destination and selection.

[Open the workflow guide](doc:masks)

## Calculations {#calculations}

Calculate channel combinations for masks/selections with explicit source and blend settings.

[Open the workflow guide](doc:masks)

## Color range {#color-range}

Build a selection from a color range; inspect the matte before confirming.

[Open the workflow guide](doc:masks)

## Select and Mask {#select-and-mask}

Refine selection edges and inspect the resulting coverage.

[Open the workflow guide](doc:masks)

## Focus Area {#focus-area}

Estimate a focus-based selection and inspect its limits around low-detail regions.

[Open the workflow guide](doc:masks)

## Paste in perspective {#paste-in-perspective}

Place pasted pixel content using a perspective frame before committing.

[Open the workflow guide](doc:dimensions)

## Contact Sheet {#contact-sheet}

Arrange several source images into a contact sheet; check labels, page size and destination.

[Open the workflow guide](doc:photo)

## Map baking (baker) {#map-baking-baker}

Geometry-derived maps, optional high-poly source, cage, rays, padding and output channels/files.

[Open the workflow guide](doc:uv-baker)

## UV automatiques (xatlas) {#uv-automatiques-xatlas}

Unwrap and pack islands, preview and apply with an editable source saved first.

[Open the workflow guide](doc:uv-baker)

## Export As {#export-as}

Image format, area, scale, transparency and layer/selection variants.

[Open the workflow guide](doc:export)

## Export textures {#export-textures}

PBR channel packing presets, normal convention, output size, padding and filenames.

[Open the workflow guide](doc:delivery)

## Export for Unreal {#export-for-unreal}

Model/material delivery to Unreal, distinct from a shader Custom-node export.

[Open the workflow guide](doc:delivery)

## New Paint 3D project {#new-paint-3d-project}

Create the model, texture resolution and initial material targets.

[Open the workflow guide](doc:paint3d)

## Plugins {#plugins}

Install, enable, reload, rescan and create plugin skeletons.

[Open the workflow guide](doc:plugins-bridge)

## Turntable {#turntable}

Presentation rotation output for a model; it does not animate the painted UV texture itself.

[Open the workflow guide](doc:rendering)

## Batch processing {#batch-processing}

Apply a validated action chain to folders or open documents with naming/conflict controls.

[Open the workflow guide](doc:actions)

## Photo operations {#photo-operations}

Panorama, layer alignment, automatic blending and HDR source/options dialog.

[Open the workflow guide](doc:photo)

## Software updates {#software-updates}

Startup checking, alpha channel preference, verified download and installer launch.

[Open the workflow guide](doc:settings)

## Photo RAW {#photo-raw}

Photo RAW development, before/after, local corrections, curves, color and lens settings. Confirm or cancel before returning to the document.

[Open the workflow guide](doc:photo)

## Custom node code {#custom-node-code}

Large custom-node code editor; compile and inspect diagnostics before keeping shader changes.

[Open the workflow guide](doc:shader-authoring)

## About Shaderixel {#about-shaderixel}

Application version and build/dependency information, useful when checking an update or reporting a problem.

[Open the workflow guide](doc:settings)

## Close unsaved documents {#close-unsaved-documents}

Save, discard or cancel when closing modified documents. Cancel keeps the document open, including during an update request.

[Open the workflow guide](doc:documents)

## Color management {#color-management}

Choose the relevant color-management operation/profile; assignment, conversion and proofing have different purposes.

[Open the workflow guide](doc:color)

## Spot channel {#spot-channel}

Create a named spot-color channel and verify its coverage separately from ordinary RGB delivery.

[Open the workflow guide](doc:color)

## Operation progress {#operation-progress}

Long-running operation progress and cancellation. Let the operation finish or cancel before judging its completed output.

[Open the workflow guide](doc:recovery)

## Image geometry {#image-geometry}

Image-size, canvas-size and related geometry operation options. Check absolute/relative units and anchor before confirming.

[Open the workflow guide](doc:dimensions)

## New document {#new-document}

Choose dimensions, precision, Layers/Nodes mode or a template before creating a document.

[Open the workflow guide](doc:documents)

## Save as template {#save-as-template}

Save a reusable starting document with metadata and thumbnail; this differs from saving an image-graph function.

[Open the workflow guide](doc:documents)

## Modify selection {#modify-selection}

Selection modification such as feather, grow/shrink or border. Check the active selection and radius before applying.

[Open the workflow guide](doc:masks)

## Save layout {#save-layout}

Name the current panel workspace for later reuse; it stores layout rather than document pixels.

[Open the workflow guide](doc:workspace)
