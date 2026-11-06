---
id: script-api
title: Python script interface
description: Call editor operations and describe script panels without a native DLL.
section: Develop / Plugins
order: 100
parent: plugins-bridge
revision: 959e2a6275002fc8
---

## Launch from Shaderixel {#launch}

Create a script plugin in Plugin Manager. The app launches it in its plugin folder and supplies `SHADERIXEL_URL`, `SHADERIXEL_TOKEN` and `SHADERIXEL_WORKSPACE` through its environment. Keep those values out of manifests and shared files.

Import `shaderixel_client` and `shaderixel_panel` from the supplied SDK helpers. They use the Python standard library. `ShaderixelError` reports a missing bridge, an operation error or an unanswered call.

## Read the active document {#example}

```python
from shaderixel_client import call
info = call("shaderixel.document_info")
print(info)
```

`describe()` returns operation identifiers, descriptions and input schemas. `call(operation, parameters, workspace)` invokes one operation on the editor’s main thread. Its result is the operation’s JSON value; inspect that operation’s schema before using fields.

For several edits in one undo step, bracket your calls with `shaderixel.history_group`, using `action: begin` and `action: end`. Close the group even when an operation fails.

## Script panels {#panels}

`Panel` constructs widget dictionaries and sends one JSON message per output line. A `panels` entry in `plugin.json` gives the script an application-drawn dockable window.

- `text`, `note`, `separator` and `same_line` arrange explanatory content.
- `button`, `slider`, `checkbox`, `input`, `color` and `combo` describe controls with stable IDs.
- `on_click` receives an ID; `on_change` receives an ID and a value. They also work as decorators.
- `run(ui)` sends the initial interface and processes input until closure. The `ui` function returns the current widget list.
- `set_ui`, `status` and `log` send interface updates and messages. Use `send` only for a protocol dictionary, not arbitrary printed text.

Shaderixel sends `open`, `click`, `change` and `close` events on standard input. The script sends `ui`, `status` or `log` messages on standard output. Reserve standard output for this protocol while a panel is running.

The child pages list the current helper signatures and their parameters.
