# Shaderixel — version notes

[English](VERSION.md) · [Français](VERSION.fr.md) · [README](README.md)

## 0.1.0-alpha.20261120 · Alpha

Windows x64 installer and portable ZIP with D3D12, Vulkan and `ShaderixelCli.exe`.

- Updated to CyMCPSDK 0.7.2 at commit `e562c73`.
- Free is the default for settings without a saved profile. When MCP is enabled, local clients can use the address alone to call every exposed operation. Arguments are still validated.
- Local controlled applies Shaderixel's permissions. Token only requires a Bearer token. Protected combines both controls. The app preserves saved profile choices.
- The README, assistant guide and offline manual explain the four profiles and how to connect the CLI and external MCP clients.

[Release and SHA-256 files](https://github.com/MrMybal/Shaderixel/releases/tag/v0.1.0-alpha.20261120)

## 0.1.0-alpha.20261119.1 · Alpha

Windows x64 installer and portable ZIP with D3D12, Vulkan and `ShaderixelCli.exe`.

- The in-app assistant, CLI and local MCP server share the CyMCPSDK 0.7.0 operation registry.
- Paint 3D operations cover projects, material channels, camera, brush strokes, particle scripts and replay, baking and PBR texture export.
- The assistant can inspect visible controls, capture the main Shaderixel window, navigate menus and send input within that window. Editing and file access use separate permissions.
- Particle emission now uses the correct ray range in orthographic views.
- The offline manual and the assistant guide describe setup, permissions and CLI examples.

[Release and SHA-256 files](https://github.com/MrMybal/Shaderixel/releases/tag/v0.1.0-alpha.20261119.1)

## 0.1.0-alpha.20261119 · Alpha

Windows x64 installer and portable ZIP with D3D12 and Vulkan executables.

- Paint 3D adds surface painting and retouching tools, with updates to UDIM handling and texture export.
- The new particle brushes include rust, mud, rain and dust. The Simulation tab can edit scripts for emission, evolution and impact and replay the last stroke.
- Scripts can define controls, conditional groups and defaults. Wandering Drops demonstrates the impact modes, bounce strength and group resets.
- The Windows minimize, maximize and close controls are integrated into the Shaderixel menu bar.

## 0.1.0-alpha.20261106 · Alpha

Published on **2026-11-06** for **Windows x64**. This experimental release is
intended for testing and feedback.

[Release and SHA-256 files](https://github.com/MrMybal/Shaderixel/releases/tag/v0.1.0-alpha.20261106)
· [Installer](https://github.com/MrMybal/Shaderixel/releases/download/v0.1.0-alpha.20261106/Shaderixel-0.1.0-alpha.20261106-windows-x64-setup.exe)
· [Portable ZIP](https://github.com/MrMybal/Shaderixel/releases/download/v0.1.0-alpha.20261106/Shaderixel-0.1.0-alpha.20261106-windows-x64.zip)

### Changes in this alpha

The 2026-11-06 release updates the in-app and online CyDoc documentation:

- Complete English/French editions with common page and section identifiers.
- Task-based navigation, top search, themes and retained reading positions.
- Local illustrations, workspace galleries and interactive saved function graphs.
- Public C/Python API signatures, parameters and examples.
- Contextual F1 and help for selected tools, brushes, commands and nodes.
- Practice projects, compatible legacy links and the complete offline reader.

Earlier alpha improvements remain included: typed numeric controls, fractional
pixel/percentage dimensions, editable layer baking, UI UV/UI Toolkit libraries
and verified GitHub installer updates. They predate this documentation release.
The repository README and website may evolve independently of the published
installer; layout changes do not create a new binary version.

### Known issues and documented limitations

The release notes do not provide a bug-by-bug list. Consult the
[issue tracker](https://github.com/MrMybal/Shaderixel/issues) for reported defects.
The following documented constraints are not an exhaustive bug list:

- **Platforms:** Windows x64, Windows 10 version 2004 or later. D3D12 and Vulkan
  each need a compatible GPU and driver. No Linux or macOS package is published.
- **Alpha projects:** features and native file formats may change. Keep editable
  originals and separate delivery exports.
- **Photo development:** camera-file support depends on the available decoder.
  Lens XML profiles are external and not bundled. Photo-to-PBR maps estimate
  surface properties from an image.
- **AI:** ComfyUI needs a running server, installed models and compatible API
  workflows. Specialized depth/PBR presets need a supplied workflow; presets
  do not install models or custom nodes.
- **UV/SDF data:** use 32-bit float documents and the proper data interpretation.
  UI texture states use explicit values; widget events belong to the host UI.
- **Animation:** frames share layer content. Separate painted poses need separate
  layers or independent smart objects. Video export samples at 30 fps; GIF has
  additional timing and color constraints.
- **Unreal Engine:** connector sources must be built for the target Unreal
  Editor. Engine-specific prebuilt plugins are not included.
- **Offline help:** keep `Documentation.html` and the complete `DocData/` together
  inside the application's `Documentation/` directory.

See [troubleshooting](https://mrmybal.github.io/Shaderixel/Documentation.html#/en/troubleshooting),
[AI setup](https://mrmybal.github.io/Shaderixel/Documentation.html#/en/ai),
[animation](https://mrmybal.github.io/Shaderixel/Documentation.html#/en/animation)
and [Unreal integration](https://mrmybal.github.io/Shaderixel/Documentation.html#/en/unreal-link).

### Updating and reporting problems

Use **Help > Check for updates...** with alpha releases enabled. Startup checks
follow the saved preference; downloading and installing remain explicit actions.
The first 2026-11-03 installer needs a one-time manual upgrade to enable updates.

Report problems through the [Shaderixel issue tracker](https://github.com/MrMybal/Shaderixel/issues)
with the version, D3D12/Vulkan backend, GPU/driver, reproduction steps and a minimal
project when possible. Review shared files and logs before attaching them.
