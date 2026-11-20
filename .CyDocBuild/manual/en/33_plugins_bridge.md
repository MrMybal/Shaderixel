---
id: plugins-bridge
title: Plugins, scripts and the local bridge
description: Plugins > Plugin Manager lists installed plugins and their enabled state.
section: Handbook
order: 32
parent: automation
revision: 716c20052d4e2b8f
---

## Plugin Manager {#plugin-manager}

Plugins > Plugin Manager lists installed plugins and their enabled state. It can install a ZIP, reload a plugin, rescan folders or open the user folder. Installing a ZIP replaces an installed version with the same destination.

Plugins live in plugins beside the application or %LOCALAPPDATA%\Shaderixel\Plugins. Each has a plugin.json manifest. A native plugin runs in Shaderixel; a script plugin runs a separate program. Use plugins from sources you intend to trust with your editor environment.

Create a Plugin generates a script/native skeleton. Development auto-reload helps iterate on rebuilt DLLs without restarting. User workflows generally need installation/configuration rather than editing the SDK.

## Assistant and bridge {#assistant-and-bridge}

In AI Assistant, start the local MCP server and copy its configuration for an HTTP client. The default Free profile accepts all exposed operations at the local address without a token. [Settings](doc:settings) describes the other profiles. Chat and the server share the same operations for documents, layers, graphs and resources. Run `ShaderixelCli.exe list` to inspect the catalogue of the open instance.

Paint 3D operations can create a project, adjust the camera and materials, paint a stroke, change a particle script and replay an emission, bake maps and export texture sets. Run `ShaderixelCli.exe describe shaderixel.paint3d_stroke` for the normalized view coordinates and brush parameters.

`shaderixel.ui_state` finds visible controls and indexed menus. `shaderixel.ui_activate` navigates to an indexed item. `shaderixel.ui_input` sends clicks, drags, scrolling, keys or text within the main window. `shaderixel.ui_capture` returns a private picture of that window. The controlled profiles require editing and file access permissions for generic menu and input control. Native Windows dialogs and detached panels are outside window capture and input.

An assistant connection, an image-generation provider and a ComfyUI server are separate configurations. A running bridge does not imply an external model/checkpoint is installed.

## Developer references {#developer-references}

See [plugin manifests](doc:plugin-manifests), the [native ABI](doc:sdk-abi) and the [Python interface](doc:script-api). Reusable graph tools can automate image work without a native plugin.

[AI and ComfyUI](doc:ai)
[Actions and graph tools](doc:automation)
