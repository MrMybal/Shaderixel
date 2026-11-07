<p align="center">
  <a href="https://mrmybal.github.io/Shaderixel/">
    <img src="assets/logo.png" alt="Shaderixel app icon" width="112" height="112">
  </a>
</p>

<h1 align="center">Shaderixel</h1>

<p align="center">
  <strong><a href="https://mrmybal.github.io/Shaderixel/">Website</a> · <a href="https://mrmybal.github.io/Shaderixel/Documentation.html#/en/getting-started">Documentation</a></strong><br>
  <a href="https://mrmybal.github.io/Shaderixel/?lang=fr">Site en français</a> · <a href="https://mrmybal.github.io/Shaderixel/Documentation.html#/fr/getting-started">Documentation française</a>
</p>

GPU image editor with a Photoshop-style **layer** workflow and a **node graph**
workflow. Effects are written in HLSL/Slang and compiled at runtime.

## Install (Windows x64)

[Download the alpha installer](https://github.com/MrMybal/Shaderixel/releases/download/v0.1.0-alpha.20261106/Shaderixel-0.1.0-alpha.20261106-windows-x64-setup.exe)
and run it. Windows 10 version 2004 or later and a D3D12-capable GPU are required;
the Vulkan executable requires a compatible Vulkan driver.

The installer defaults to `%LOCALAPPDATA%\Programs\Shaderixel Alpha` and does not
require administrator rights. It adds Start menu shortcuts for D3D12 and Vulkan,
with an optional desktop shortcut. Visual C++ runtime libraries, effects, graph
functions, examples and license notices are included. The installer interface
starts in English; French is also available.

Uninstall from **Windows Settings > Apps**. User-created files and settings are
retained. The portable ZIP remains available on the
[release page](https://github.com/MrMybal/Shaderixel/releases/tag/v0.1.0-alpha.20261106).

## Documentation

[**Documentation**](https://mrmybal.github.io/Shaderixel/Documentation.html#/en/getting-started)
opens the complete visual manual online: navigation, search, images, diagrams and
interactive function graphs. [Lire en français](https://mrmybal.github.io/Shaderixel/Documentation.html#/fr/getting-started).

**Help > Documentation** and **F1** open `Documentation/Documentation.html` through
a local `file://` URL. Keep the complete `Documentation/` directory beside the
executable. The reader needs no web server, Internet connection or Node.js.

English is the original and default language; the complete French edition uses
the same page and section identifiers. Search is at the top, with **Ctrl+K** for
the search dialog. The navigation tree groups guides, preset families, tools,
brushes and public API references. Themes, bookmarks, notes and reading positions
are available in the reader.

The manual retains 44 handbook chapters and the shipped references: 449 effects,
162 functions, 46 tools, 20 recorded commands, 53 brushes and 168 built-in node
entries. Contextual F1 and Selection/Tool/Brush/Command/Node help open the matching
page. Public C and Python APIs describe signatures and behavior without browsing
implementation files.

The **image**, **gallery** and **graph** readers provide local illustrations,
workspace comparisons and saved function connections. Enlargement stays inside
the documentation window; **Escape** reduces it. Graph views do not execute
shaders, exports or AI events. **Help > Practice projects** opens one of six
editable examples as an unsaved copy; their manual pages also offer downloads.

`DocAI.md` is a separate assistant sidecar. Maintainer inputs and working records
live in `.CyDocBuild/` and are excluded from the installed manual.

## Updates

Shaderixel checks GitHub releases in the background at startup. Open
**Help > Check for updates...** to check manually, change the startup preference
or choose whether to include alpha/prerelease versions.

When a newer Windows installer is available, the update window shows the release
notes and a download button. The application verifies the download size and
SHA-256 before offering **Install and restart**. Unsaved documents go through the
normal save/discard/cancel prompts; cancelling keeps the application open. The
setup wizard preserves the installed folder and the current D3D12/Vulkan backend.
A failed or offline check leaves the editor usable and can be retried manually.

The first installer dated 2026-11-03 has no built-in updater. Install the
2026-11-04 alpha or a newer version once to enable subsequent checks. No GitHub account is required
for this public release channel. Development capture/effect-test runs skip the
startup check; `--updates` opens the panel and `--check-updates` explicitly runs a
check, including during an automated capture.

## Type values in sliders

**Double-click a slider or drag field** to type its value; **Ctrl+click** works
too. Press **Enter** to confirm, **Escape** to cancel, or **Tab** to move on.
Floating-point controls accept a decimal dot or comma. The text field keeps the
actual precision even when the normal display rounds values or adds units.

This shared behavior covers numeric controls throughout the application,
including effect parameters, brush settings, transforms, vector components,
logarithmic sliders and vertical sliders. Blend If also exposes four numeric
threshold fields below each bar; their values remain ordered within 0–255.
Integer parameters remain integers.

## Precise layer and shape dimensions

Layer transformation properties and the Free Transform options support
**Percentage (%)** and **Pixels (px)**. Shape properties expose position, width
and height directly, including boolean components. Disable **Constrain proportions**
to enter independent dimensions such as **375.5 × 75.25 px**. Decimal input accepts
either a dot or a comma; switching units does not resize the object.

Layer percentages use the source dimensions; shape-frame percentages use the
document width and height. Shape-frame dimensions precede rotation, stroke and
layer filters. Pixel dimensions preserve existing flips; negative percentages
can still mirror a transform.

## Bake a layer to editable pixels

Select a layer and click **Bake to Pixel Layer** in **Properties**, or choose
**Layer > Bake to Pixel Layer** (also available from the layer's context menu).
This replaces a fill/generator, text, shape or smart-object layer with a pixel
layer at the document size. Existing layer filters and transforms are baked into
the pixels, so you can paint on the result immediately without merging another
layer. A pixel layer with filters can be baked in the same way.

The layer keeps its name, position in the stack, visibility, mask, opacity,
clipping and blending settings. Text, shape, smart-object and filter controls are
replaced by the rendered pixels; **Ctrl+Z** restores the original editable layer.
Linked smart-object instances keep their original content. Unlock a locked layer
first. Groups and adjustment layers use the existing merge commands instead.
The action can also be recorded and replayed in **Actions**.

## UI texture and widget libraries

**37 effects and 65 reusable graphs** cover nine-slice and tiled-center panels,
eight-slice borders, sprite atlases, flipbooks, UV transforms, progress indicators,
SDF shapes and Boolean operations, styled borders, flow maps, parallax,
transitions, interface patterns, pixel snapping, alpha cleanup and skin states.
Inputs and variables expose the main controls, including fractional pixel sizes.

Open the graph node menu > **Library > UI UV ...** or **UI Toolkit ...** to add a
function. See the [UI UV guide](https://mrmybal.github.io/Shaderixel/#/en/ui-textures) and
[UI Toolkit guide](https://mrmybal.github.io/Shaderixel/#/en/ui-textures) for field formats, controls and examples.
The installer and portable package include both libraries and the self-contained
[UI UV demo](examples/ui_uv_library_demo.sxl) and
[UI Toolkit demo](examples/ui_toolkit_demo.sxl). Open these documents with
**File > Open** or drop them on the canvas. Use a **32-bit float document** for UV
coordinate and signed-distance fields.

![UI Toolkit shapes, borders, motion, patterns and coordinated skin states](docs/images/ui_toolkit.png)

## Screenshots

**Layers and non-destructive effects** — combine painting, gradients, masks and
adjustments, with the layer stack and generated graph visible together.

![Shaderixel canvas with painting, glow, adjustment layers and the layer stack](docs/screenshots/layers.png)

**Visual effect graphs** — connect image inputs, math operations and parameters,
with live node thumbnails and an effect-output preview.

![Shaderixel visual effect editor with connected nodes and live image previews](docs/screenshots/effect-graph.png)

**HLSL/Slang editing** — edit shader code with syntax highlighting and expose
parameters in the same workspace as the graph and layer preview.

![Shaderixel HLSL and Slang source editor with exposed parameters and a layer preview](docs/screenshots/shader-editor.png)

## Interface language

The interface defaults to **English** on a fresh installation. Select another
language from **View > Language**; existing language preferences are preserved.
Interface dictionaries are loaded from `lang/` next to the executable, or from
`library/lang/` when running a development build. An unavailable saved language
falls back to English.

## Build (Windows, Visual Studio 2022)

```bat
python tools/fetch_cyui.py
build.bat            (Release)
build.bat Debug
```

The Release executables are generated in `build/Release/`: `Shaderixel.exe`
(D3D12) and, when the Vulkan headers are available, `ShaderixelVk.exe` (Vulkan).
Use `build_target.bat <target>` to build a single target. The build copies the
interface dictionaries into `lang/` alongside the executables.

## Build the Windows installer

Install [Inno Setup 7](https://jrsoftware.org/isdl.php), then generate the example
package and compile the setup executable with the Visual Studio x64 CRT:

```bat
python tools/package.py --source-only-unreal --name Shaderixel-0.1.0-alpha.20261105-windows-x64
python tools/build_installer.py --package dist/Shaderixel-0.1.0-alpha.20261105-windows-x64 --version 0.1.0-alpha.20261105 --iscc "C:\Program Files\Inno Setup 7\ISCC.exe"
```

Use a fresh version/package name for each build. The installer tool rebuilds both
executables, stages selected package content and writes a setup EXE plus a SHA-256
sidecar to `dist/`. Use `--crt-dir` to select another Visual Studio x64
`Microsoft.VC143.CRT` redistributable directory. Machine-dependent demo guide
captures and runtime caches are excluded; the reviewed README screenshots are included.

`python tools/test_installer.py --installer <setup.exe> --manifest <manifest.json>`
checks installation, reinstall, both installed executables and uninstall while
preserving a user-created file. The manifest is generated under
`build/InstallerPayload/<version>/manifest.json`. The check refuses to replace an
existing Shaderixel Alpha registration and uses silent installers and internal
application demos without operating system input devices.

## Dependencies (`external/`, not tracked by Git)

- CyUI 1.2, locked to `8367401fe45fc22bfed5e227cb3de343177f676b`: run `python tools/fetch_cyui.py`. The build uses the application's existing ImGui target and only Core, Canvas, Nodes, Curves, Plots, Images, Code and CodeEditor.
- Dear ImGui, docking branch: `git clone -b docking https://github.com/ocornut/imgui external/imgui`
- stb: `git clone https://github.com/nothings/stb external/stb`
- xatlas (automatic UV unwrapping, MIT): `git clone --depth 1 https://github.com/jpcy/xatlas external/xatlas`
- ufbx (FBX loading, MIT): `git clone --depth 1 https://github.com/ufbx/ufbx external/ufbx`
- LibRaw (camera RAW decoding, LGPL 2.1 / CDDL 1.0, optional; WIC is used when unavailable): `git clone --depth 1 --branch 0.22.2 https://github.com/LibRaw/LibRaw external/LibRaw`
- Lens profiles: the [lensfun](https://github.com/lensfun/lensfun) database (`data/db`, CC BY-SA 3.0) is not bundled. Place its XML files in `%LOCALAPPDATA%\Shaderixel\lensfun` or select a folder in the application. Installed darktable and RawTherapee databases are also detected.
- Unreal textures (`.uasset`) are read by `src/io/uasset`, adapted from CyImgView by the same author. Meshes stored in `.uasset` files are exported through an installed Unreal Editor using a commandlet and glTF.
- Slang: extract the Windows x64 release into `external/slang`.
- DXC (`dxcompiler.dll` / `dxil.dll`): copied automatically from the Windows SDK.
- Vulkan (optional): `git clone --depth 1 https://github.com/KhronosGroup/Vulkan-Headers external/vulkan-headers`
  and `git clone --depth 1 https://github.com/zeux/volk external/volk`. No Vulkan SDK is required; a compatible driver is needed at runtime.

## Update checks

Run `tools\test_updates.bat` for release selection, version ordering, trusted URL
validation and installer integrity checks. Add `--live` to download and verify a
published installer from GitHub without launching it. For installer upgrade
checks, pass `--upgrade-from <previous-setup.exe>` to `tools/test_installer.py`.

## UI integration checks

Run `tools\test_cyui.bat` (or add `--clean` for a complete rebuild) to configure a separate `build/CyUiCheck` directory,
build both Windows backends and run the headless UI interaction tests. Tests feed
ImGui events directly and never operate the system mouse or keyboard. Run
`python tools/test_cyui_native.py` for native GPU captures and effect compilation. Audio,
Gallery, the CyUI demo, Web/CEF and the optional ImGuizmo adapter are disabled.
See [docs/CYUI_MIGRATION.md](docs/CYUI_MIGRATION.md) for the integration boundary,
retained application adapters and verification details.

See also [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md),
[docs/ROADMAP.md](docs/ROADMAP.md) and the
[Unreal Engine plugin README](unreal/ShaderixelLink/README.md).

## License

Copyright (c) 2026 MrMybal and Shaderixel contributors.

Except where a separate license is specified, Shaderixel is free software:
you can redistribute it and/or modify it under the terms of the GNU General
Public License as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version (`GPL-3.0-or-later`).
See [LICENSE](LICENSE) for the full terms.

Shaderixel is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

The standalone Unreal Engine connector in `unreal/ShaderixelLink/` is separately
licensed under the [MIT License](unreal/ShaderixelLink/LICENSE). It communicates
with Shaderixel through files and a separate process. This license does not
cover Unreal Engine itself, which remains subject to Epic's terms.

[CyUI](https://github.com/MrMybal/CyUI) is licensed under the
[MIT License](https://github.com/MrMybal/CyUI/blob/8367401fe45fc22bfed5e227cb3de343177f676b/LICENSE).
Its repository is currently private, so viewing that link requires repository access.
Distributions include the license text at `licences/cyui/LICENSE`, together with
notices for the selected CyUI modules and their dependencies.

Third-party dependencies retain their own licenses and copyright notices.
Using Shaderixel does not, by itself, place your images, textures or models
under the GPL.
