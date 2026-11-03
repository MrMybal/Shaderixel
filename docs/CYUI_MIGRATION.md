# CyUI modular integration

## Dependency and build boundary

Shaderixel previously used Dear ImGui directly, with its own theme, font loader,
HLSL syntax painter, gradient editor, curve painter and histogram drawing. It had
no CyUI dependency, umbrella target or vendored CyUI implementation to update.

`cmake/dependencies.json` locks CyUI 1.2 to
`8367401fe45fc22bfed5e227cb3de343177f676b`. Run
`python tools/fetch_cyui.py` before configuring. The updater preserves a matching
checkout and refuses to switch a checkout with local changes. CMake verifies the
revision and fails with an actionable message if it is missing or different.
No source is downloaded during the production configure/build itself.

Ninja builds also probe the exact MSVC `/showIncludes` prefix using a real UTF-8
compilation. This avoids stale objects caused by a misdecoded localized compiler
prefix. The same encoding is used for C and C++ sources; C++ module scanning is
disabled because this project has no C++ modules.

CyUI links the existing `imgui` target through `CYUI_IMGUI_TARGET`. Shaderixel
retains its compatible docking ImGui (1.93 WIP, API version 19297) and C++20.
There is no second bundled ImGui or added rendering backend. Shaderixel's own
D3D12/Vulkan backends still own texture uploads and rendering.

| Target | Used for |
| --- | --- |
| CyUI::Core | Themes, fonts, palette, buttons, tooltips, filter fields/matching, notifications, locale |
| CyUI::Canvas | Shared double-precision graph transforms and grid; dependency of image/plot views |
| CyUI::Nodes | Deterministic graph layout proposals, validated and applied as one host history operation |
| CyUI::Curves | Tone-curve rendering, using Shaderixel's interpolation callback |
| CyUI::Plots | Both histogram panels, including logarithmic normalization and existing statistics |
| CyUI::Images | Gradient stops, effect previews and active layer/node previews with fit/pan/zoom/checkerboard |
| CyUI::Code | Shared HLSL syntax and palette; dependency of CodeEditor |
| CyUI::CodeEditor | Editable/read-only shader sources, native editing Undo/Redo and compiler line markers |

CMake selects modules before `add_subdirectory` and links explicit targets.
Audio, AudioViews, Gallery, the demo, Web/CEF and ImGuizmo are not built.
Timeline, Inspector, Assets, Viewport, Diff, Commands, Diagnostics and DocumentViews
are not selected by this application build.

## Ownership and interaction

Shaderixel retains shader compilation, HLSL formatting and compiler-message
parsing, GPU image processing, histogram calculation, color management, documents,
project formats, persistence and Undo/Redo. CyUI receives prepared models and
borrowed GPU texture IDs; it never opens or saves Shaderixel documents.

The source-editor adapter keeps a separate editor per context/widget and resets
it only when the host replaces its source. Compiler marks and semantic pin names
remain host annotations over the common syntax engine. Context shutdown releases
the editor caches. Native text Undo is separate from Shaderixel's effect-graph
transactions, as with the former text field.

Gradient stops have stable view IDs independent of their positions or indices.
Foreground/background followers, reverse, dithering, presets and sRGB versus
linear-light interpolation remain host policies. CyUI owns the drag preview;
only a validated commit replaces the stop list. Revisions cancel stale previews
when a preset or another host control changes the model. The adapter preserves
single-click insertion in addition to CyUI's double-click insertion.

Tone curves use the shared renderer in read-only mode with an application
interaction adapter. This preserves single-click insertion, bounded keys,
endpoint protection, right-click/drag-out deletion and one history operation
per gesture. Escape restores the preview and point IDs without adding a history operation.
The shared evaluation callback renders the application's actual interpolation.
Pointer-to-value conversion follows the common view axes when navigating the curve.

The graph uses shared transforms/grid and the common node-layout algorithm.
Arrange nodes is available from the empty-canvas context menu and records one
host Undo operation. Existing graph gestures and all model/evaluation semantics
remain in Shaderixel.

Themes and compact/comfortable density are selectable from View and persisted in
the existing settings file. Violet/comfortable preserve the application's
initial appearance. Monitor DPI changes update CyUI and host geometry together.
`SX_THEME`, `SX_DENSITY` and `SX_DPI` provide isolated scripted checks.
`SX_UI_FOCUS=properties|code|gradient|analysis` can reveal the relevant production panel
in a scripted demo before capture. English remains the fresh-install default; the existing French-source translation
adapter is preserved for the wider application and CyUI's locale follows the
selected language. New controls use stable `trLabel` identities.

## Required capabilities not supplied by this revision

These are retained application adapters, not copied implementations of CyUI:

- The graph's full card renderer needs variable card dimensions, editable typed
  input values, RGBA split outputs, additional execution connections, reroutes,
  frame comments, bake/live badges, exact thumbnail anchors, its minimap and
  insertion on an existing wire. `NodeOptions` supplies one global card width and
  row size plus a body drawing callback; it has no card-layout/hit-test extension
  contract for those interactions. Replacing it wholesale would lose features.
- Tone curves require endpoint protection, [0,1] constraints, single-click insert
  and drag-out deletion. `CurveOptions` has no host constraint/delete-policy
  callbacks, hence the gesture adapter around its shared renderer.
- Main painting and 3D views include document rotation, rulers/guides, tablet
  samples, selection/painting overlays, mesh hit tests and multiple view modes.
  They retain those host interactions rather than adopting a generic viewport
  that would change their input ownership. Their shared palette references now
  resolve through CyUI.
- Frame animation stores ordered layer-state snapshots with thumbnails, tweening,
  onion skins and frame reordering; it is not a free-positioned clip model.
  Its existing presentation is retained instead of adding an unused Timeline.
- Window filtering and global search currently intercept labeled ImGui items and
  crawl menus/tabs. CyUI's opt-in filters do not intercept raw ImGui calls or
  provide this application-wide search index. The existing hooks remain, with
  common filter field UI and matching replacing their duplicated controls.
- Relative numeric dragging and translation of the existing French-source UI use
  three application ImGui patches. None is removed while its behavior is in use.
- The Unreal connector uses native Unreal menus/settings and a file-based link to
  the separate executable. It has no embedded ImGui panel. CyUI's Unreal backend
  targets UE5.5+, while Shaderixel Link also targets UE4.25 onward; it is not added
  as a new dependency or a replacement for those native menus.

No browser is present, so Web/CEF is deliberately absent. All app-specific GPU,
format, plugin and engine dependencies remain necessary; no such dependency is
removed merely to reduce the UI build.

## Verification

`tools/test_cyui.bat --clean` configures `build/CyUiCheck` independently of the normal
Release output, builds both Windows backends and runs
`shaderixel_cyui_interactions`. The tests create real ImGui frames and send events
to ImGui's queue, without creating a native window or operating input devices.
They cover five themes, both densities and 100%/150% DPI, context isolation,
filtering, source round trips, read-only editing, text Undo/Redo and tone-curve
insert/move/delete/cancel gestures. Run `python tools/test_cyui_native.py` after that build for the native D3D12 and
Vulkan checks. It starts each process minimized, waits for its autonomous exit,
checks for ImGui recovery errors, captures the migrated views and runs
`--test-effects` on both backends. Results and captures are written under
`build/CyUiCheck/validation`, outside Git. Automated application runs now return a
failure status on an ImGui recovery error rather than accepting a broken capture.

Validated on Windows x64 with MSVC:

- Both D3D12 and Vulkan builds succeed, including the ordinary `build/Release` output.
- The headless interaction suite passes across 20 theme/density/DPI combinations.
- Seven native UI scenarios exit successfully with zero ImGui recovery errors.
- The HLSL editor, tone curves, gradient editor and preview captures were inspected.
- `--test-effects`: 412/412 on D3D12 and 412/412 on Vulkan; no reported graph/operator failures.
- Ninja records `App.h` for independent UI translation units after the localized-prefix fix.
- Selected module license/notice copies match their dependency sources byte for byte.

Shaderixel currently targets Windows. Linux/macOS platform layers are planned,
as before this integration; no new platform restriction is introduced and no
Linux/macOS runtime validation is claimed.

## Distribution

The application remains GPL-3.0-or-later; the Unreal connector remains MIT.
CyUI and TextEditor retain their MIT notices. The packager preserves CyUI's root
license, THIRD_PARTY_NOTICES and the selected syntax-engine/data notices, alongside
the existing notices for Shaderixel's other dependencies.
