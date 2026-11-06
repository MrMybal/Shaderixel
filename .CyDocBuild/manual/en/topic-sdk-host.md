---
id: sdk-host
title: Native host API
description: Register editor extensions and access operations, pixels, settings and events.
section: Develop / Plugins
order: 100
parent: sdk-abi
revision: 0722495e85e948ef
---

## Use the host table {#host-table}

Include `shaderixel_plugin.h` and retain the `SxHost` pointer only while your plugin is loaded. All native plugin callbacks run on the main thread. Check `host->api_version` before accessing fields added in API 2 or API 3.

Strings passed to the host are copied. Returned strings are valid until the next host call. Pixel-read buffers are valid only during their callback. Copy a buffer before retaining its contents.

## Choose a service {#services}

- Register commands, panels, tools or operations with `add_command`, `add_panel`, `add_tool` and `add_operation`.
- Obtain supported operation identifiers and input schemas with `operations_json`, then call an operation with `call`. Its completion may be delayed.
- Use `active_layer`, `pixels_read` and `pixels_write` for a paintable pixel target. Choose the correct color space and alpha convention.
- Store plugin preferences with `setting_get` and `setting_set`; respond to document changes with `on_event`.
- Draw tool previews only during `SX_TOOL_DRAW`. Overlay coordinates are document pixels and colors use `0xAABBGGRR`.

Select a function below for its actual public signature, parameters and behavior.

[Native ABI and formats](doc:sdk-abi) · [Panel UI](doc:sdk-ui)
