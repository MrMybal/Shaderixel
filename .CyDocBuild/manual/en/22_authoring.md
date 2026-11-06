---
id: authoring
title: Authoring effects and reusable presets
description: An effect is a .slang shader; a reusable image-graph function is .sxfn JSON.
section: Create / Custom effects
order: 21
revision: 39222f681dd674a9
---

An effect is a .slang shader; a reusable image-graph function is .sxfn JSON. The formats differ even when edited visually.

```image
{"id": "shader-editor", "src": "assets/manual/shader_editor_en.png", "alt": "Source editor and exposed shader parameters.", "controls": "minimal"}
```

## Shaders {#shaders}

The .slang filename is the stable ID. Use lowercase letters, digits and underscores. Translated labels must not change that identity.

Define float4 Effect(EffectContext ctx). Metadata //@name, //@category and //@param supplies names and controls. //@generator declares Draw; //@blend declares a blend. Neighbor reads need appropriate //@halo metadata for tiled rendering.

## Functions {#functions}

subinput/suboutput nodes define ports; exposed variables become instance controls. kind is subgraph for live calculation or tool for event/action workflows.

All referenced effects must exist. Inputs follow actual pin order; parameter pins follow image inputs. Multi-output links need the right source port. Avoid cycles.

Save an editor-created example before hand-editing JSON. This preserves current field names and value formats.

## Locations and validation {#locations-and-validation}

Source functions live in library/functions; installed ones in functions beside the executable; user functions in %LOCALAPPDATA%\Shaderixel\Functions. Effects live in shaders/effects. Modify a copy of protected supplied effects.

Shaderixel.exe --test-effects compiles effects, then exits. Inspect [test-effects] in shaderixel.log. Compilation and graph validation are separate: a valid shader does not prove a tool's wiring/export is correct.

Use the [preset format reference](doc:preset-format) for metadata and function ports, and the [plugin API](doc:sdk-abi) for extensions. The catalog describes shipped defaults rather than user overrides.

[Visual effect graph and shader editor](doc:effect-editor)
[Functions, execution and automation](doc:automation)
