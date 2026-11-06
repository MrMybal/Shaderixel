---
id: plugins-bridge
title: Plugins, scripts and the local bridge
description: Plugins > Plugin Manager lists installed plugins and their enabled state.
section: Handbook
order: 32
parent: automation
revision: f874ad773198fa6d
---

## Plugin Manager {#plugin-manager}

Plugins > Plugin Manager lists installed plugins and their enabled state. It can install a ZIP, reload a plugin, rescan folders or open the user folder. Installing a ZIP replaces an installed version with the same destination.

Plugins live in plugins beside the application or %LOCALAPPDATA%\Shaderixel\Plugins. Each has a plugin.json manifest. A native plugin runs in Shaderixel; a script plugin runs a separate program. Use plugins from sources you intend to trust with your editor environment.

Create a Plugin generates a script/native skeleton. Development auto-reload helps iterate on rebuilt DLLs without restarting. User workflows generally need installation/configuration rather than editing the SDK.

## Assistant and bridge {#assistant-and-bridge}

The AI panel exposes the local bridge and connection configuration for supported clients. The bridge provides operations for documents, layers, graphs and resources. A client operation can modify a document, so inspect the active target and save work as appropriate.

An assistant connection, an image-generation provider and a ComfyUI server are separate configurations. A running bridge does not imply an external model/checkpoint is installed.

## Developer references {#developer-references}

See [plugin manifests](doc:plugin-manifests), the [native ABI](doc:sdk-abi) and the [Python interface](doc:script-api). Reusable graph tools can automate image work without a native plugin.

[AI and ComfyUI](doc:ai)
[Actions and graph tools](doc:automation)
