---
id: plugin-manifests
title: Plugin installation and manifests
description: Install or create native and script extensions with Plugin Manager.
section: Develop / Plugins
order: 100
parent: plugins-bridge
revision: 868939c73daf9697
---

## Install or reload a plugin {#install}

1. Open Plugins > Plugin Manager.
2. Install a ZIP containing a plugin folder and `plugin.json`, or copy the folder to `%LOCALAPPDATA%\Shaderixel\Plugins`.
3. Rescan, enable the plugin and check its commands or panels.
4. Reload after changing the plugin. Development auto-reload watches rebuilt native DLLs.

The app also scans `plugins` beside the executable. Installing a ZIP replaces an installed version at the same destination. A native plugin runs inside Shaderixel on the main thread; a script runs in a separate process. Install extensions you trust with your editing environment.

## Manifest fields {#manifest}

```json
{
  "id": "studio.example",
  "name": "Example tools",
  "version": "1.0",
  "dll": "example.dll"
}
```

- `id`, `name`, `version`, `author` and `description` identify the extension.
- `dll` names a native library relative to the plugin folder.
- `commands` is a list of objects with `menu`, `run` and optional `description`. Slash separates Plugins submenus; `run` executes in the plugin folder.
- `panels` is a list with `title` and `run`; the process describes an application-drawn panel.

Native and script entries may coexist. Use Create a Plugin in Plugin Manager to obtain a skeleton matching the current format. For native callbacks, follow the [ABI reference](doc:sdk-abi); for scripts, follow the [Python interface](doc:script-api).
