---
id: smart-objects
title: Smart objects
description: A smart object separates source content from an instance's placement and appearance.
section: Handbook
order: 19
parent: layers
revision: 39d2045db7712cf9
---

A smart object separates source content from an instance's placement and appearance. Sources can be embedded or linked to external files.

## Sources and instances {#sources-and-instances}

File offers embedded and linked import. Embedded content travels with the document. Linked content can refresh from a source that must remain available.

Instances can share a source while retaining separate placement. Source edits can affect all instances; transforming an instance does not necessarily resize its source.

Use Properties to inspect/edit content and refresh a link. Keep external sources when moving projects. Filters remain editable on an instance.

## Reuse a logo or decal safely {#reuse-a-logo-or-decal-safely}

1. Import the source embedded when it must travel with the project, or linked when an external master should remain authoritative.
2. Duplicate/place instances and change their corners/transforms independently. Inspect which instances share the source before editing its content.
3. Refresh a linked source after external edits. If it moved, restore/relink the correct source rather than baking an unintended missing preview.
4. Keep effects/masks on the intended instance. Bake only when independent pixels are wanted; transforming an instance alone does not rewrite its master file.

## Paintable conversion {#paintable-conversion}

Bake to Pixel Layer replaces the selected instance's rendered content and filters with independent document-sized pixels. Mask, opacity and blend settings stay separate.

Other instances and the source remain intact. Ctrl+Z restores the smart object. Save the editable native project for recovery beyond the undo session.

Smart objects differ from smart materials, which belong to the 3D material-and-mask workflow.

[Layers, filters and pixel baking](doc:layers)
[Documents, saving and templates](doc:documents)
