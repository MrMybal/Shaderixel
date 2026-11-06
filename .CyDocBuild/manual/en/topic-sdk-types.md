---
id: sdk-types
title: Native SDK types and ownership
description: Interpret tool events, pixel buffers, callbacks and versioned host fields.
section: Develop / Plugins
order: 100
parent: sdk-abi
revision: 30fe2201368a564a
---

## Tables and lifetime {#tables}

`SxHost` is the host function table. `api_version` identifies the supported ABI, `app_version` identifies Shaderixel, and `ui` points to `SxUi`. `imgui_version` describes the optional direct ImGui integration. `SxPlugin` contains an optional `unload` callback and your opaque `user` pointer.

Keep plugin-owned state until unload; release it from the cleanup callback before the DLL is freed. Host strings are UTF-8. Passed strings are copied; returned strings expire at the next host call. The callback `user` pointer is opaque to Shaderixel.

## Tool event {#tool-event}

`SxToolEvent` is passed to `SxToolFn`:

| Field | Meaning |
| --- | --- |
| `type` | PRESS=1, DRAG=2, RELEASE=3, HOVER=4, DRAW=5 using `SX_TOOL_*`. |
| `x`, `y` | Position in document pixels. |
| `pressure` | Zero to one for tablet input; one for a mouse. |
| `modifiers` | Bit mask: `SX_MOD_SHIFT=1`, `SX_MOD_CTRL=2`, `SX_MOD_ALT=4`. |
| `zoom` | Screen pixels per document pixel. |
| `view` | API 2: zero for canvas; one for 3D. In 3D, x/y identify the texture pixel, or -1 off the model; no tool overlay. |

Draw previews with host drawing functions during `SX_TOOL_DRAW`. They do not paint pixels into the document.

## Pixel and document events {#pixels-events}

`SX_PIXELS_RGBA8=0` is four bytes per pixel, sRGB, straight alpha. `SX_PIXELS_RGBA32F=1` is four floats per pixel, linear, premultiplied alpha. A `SxPixelsFn` buffer is valid during that callback only; copy it before retaining the data.

`SX_EVENT_DOCUMENT_ACTIVATED=1` includes switching or closing the last document. `SX_EVENT_DOCUMENT_EDITED=2` reports pixel or layer edits at most once per frame. `SX_EVENT_DOCUMENT_SAVED=3` supplies the saved filename as text. Register with `on_event` in API 3.

## Callback signatures {#callbacks}

```c
void SxCommandFn(void* user);
void SxPanelFn(void* user);
void SxResultFn(void* user, int ok, const char* result_json);
const char* SxOperationFn(void* user, const char* params_json);
void SxToolFn(void* user, const SxToolEvent* e);
void SxPixelsFn(void* user, int ok, int width, int height, const void* pixels);
void SxEventFn(void* user, int event, const char* text);
```

These are callback function signatures; the SDK typedefs are function pointers. `ok=1` means success. A failed operation result carries JSON error information. Plugin operation result strings are owned by the plugin and remain valid until its next operation call.

[Host functions](doc:sdk-host) · [Panel controls](doc:sdk-ui)
