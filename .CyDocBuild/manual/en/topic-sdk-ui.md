---
id: sdk-ui
title: Native panel UI
description: Draw plugin controls with the SxUi table supplied by the host.
section: Develop / Plugins
order: 100
parent: sdk-abi
revision: 9b176194d6026ce6
---

## Draw in a panel callback {#drawing}

`SxUi` is an immediate-mode widget table available as `host->ui`. Shaderixel creates the panel window; your `SxPanelFn` callback draws its contents. Call these functions only from the native UI callback.

Boolean and numeric controls edit the supplied value and report changes. Pair `push_id` and `pop_id` for repeated controls. `same_line`, `separator` and `set_next_width` arrange the next widgets; `tooltip` describes the previous widget.

```c
static float strength = 0.5f;
host->ui->slider_float("Strength", &strength, 0.0f, 1.0f, "%.2f");
```

## Version and values {#versions}

Use `SX_PLUGIN_API_VERSION` to check the host ABI. The color swatch button requires API 3. Direct ImGui use is optional and requires the same ImGui version as the host; the SxUi table avoids that direct dependency.

Each child page documents the public callback signature. UTF-8 buffers must include space for a terminating null character.
