# Multilingual illustrated documentation

## Delivered coverage

44 handbook chapters in English and French share stable chapter IDs. They cover
installation, workspace, documents/templates, layers/snapshots, painting,
selections/masks/paths, dimensions, blending, both graph editors, libraries,
photo/PBR, UI UV/Toolkit, Paint 3D, AI/providers/ComfyUI, export, automation,
color/data, layout/artboards, smart objects, photo tools, authoring, shortcuts,
troubleshooting, text/shapes, brushes/tablets, resources, animation, rendering,
UV/baking, projection/fluid paint, trim sheets, plugins, interoperability, worked
projects and a glossary.

Each edition includes the generated reference for 449 shipped effects, 162
functions, 46 toolbar tools, 20 action commands, 53 programmable brushes and 168 built-in nodes: 942 pages per language. Function descriptions and variable notes are
translated. Stable IDs and actual library labels remain recognizable; some English
control names are quoted as they appear in the library. User-added presets and
document overrides are outside the shipped defaults.

## Languages

The native reader follows the editor language by default. Its independent
selector retains the current chapter/history; its override lasts for the session.
Regional variants resolve to their base language, unavailable languages fall back
to English, and search ignores French accents and case.

The reader temporarily bypasses the editor translation hook while drawing, so a
French manual stays French with an English interface. Controls, captions, diagrams
and browser navigation follow the manual language.

## Illustrations and offline use

Eight application captures, twelve authored diagrams and a brush contact sheet are supplied in each language:
42 PNG illustrations total, plus 24 editable diagram SVGs. Captures use isolated
scripted examples and minimized processes, without OS input.

Prose and illustrations are compiled into the executable. Pictures load from
Windows resources and can be enlarged. Missing external help files disable only
the browser-opening button.

HTML embeds images/catalogs and makes no remote requests. It supports language
selection, chapter links/history, zoom and printing a chapter or the whole guide.
Printing decodes included images before opening the print dialog.

## Contextual access and reading controls

F1 opens the selected shipped effect/function reference from Graph, Properties
or Layer Preview, or the handbook chapter for the focused tool/panel. Custom
effect overrides open authoring; unknown/renamed functions open libraries. F1
inside the reader and Help > Documentation retain the current article.
Properties and graph node menus also expose Selection help.

Native and HTML readers offer a section selector and 85–150% article text size,
without changing the editor scale. Reading size is session-only. Browser section
jumps preserve the chapter URL and language.

## Guided practice

Help > Guided projects opens seven detailed exercises in both languages, with
starting settings, expected output, diagnostic outputs and troubleshooting.
They cover nine-slice, paintable bake, PBR, color key, upscale, photo variants
and ComfyUI. Six deterministic SXL practice documents contain procedural sources
and embedded copies of shipped functions. No external images are needed, and
opening a tool does not trigger its event.

The native reader opens examples as unsaved working copies; Save prompts for a
new path. The HTML edition embeds identical downloadable SXL bytes and remains
self-contained. Missing example files disable native opening while the embedded
guide remains available. Only example targets declared in the handbook can open.

## Individual tool reference

All 46 Tool enum/toolbar entries have bilingual reference pages with prerequisites,
gestures, controls, limits and related chapters. Tools is a separate search
category. Help > Tool reference opens a grouped index and comparison of editing
commands. F1 from Canvas, 3D View, toolbar or Options and the Tool help button
open the active tool's page; an active free-transform session keeps its session
chapter. Graph selection references and reader focus behavior remain contextual.

tools/manual_tools.py checks the actual enum order, toolbar membership, labels and
key hints against docs/manual/locales/tools.json. Missing or extra profiles fail
generation, preventing a new tool from silently shipping without documentation.

## Action commands and batch execution

The Commands category includes all 20 registered recorded-action command types.
Each page documents target, parameters, factory defaults, expected output and
replay limits in English/French. Help > Action reference opens the guide to
recording, JSON import, disabled/partial steps and batch inputs, names, collisions,
cancellation and logs. F1 from a selected Action Editor step and Command help
open the exact command page; Actions/Batch panels open the guide.

tools/manual_commands.py checks kCmds membership and extracts the restricted
literal defaults from ActionDefaultParams in src/ui/Actions.cpp. Unrecognized
factory syntax or missing translations fail generation. Native fixtures compare
all defaults to the compiled factory, rather than relying only on source parsing.

## Maintenance

- English chapters: docs/manual/01_*.md through 44_*.md.
- French chapters: corresponding files in docs/manual/fr/.
- Registry, UI strings and reference translations: docs/manual/locales/.
- Referenced figures: docs/manual/media/; manifest records dimensions/hashes.
- python tools/build_manual.py regenerates C++ data, HTML, resource declarations,
  the media manifest and six docs/manual/examples/*.sxl practice files. Their
  source recipes are tools/manual_examples.py. Standalone [label](example:id)
  lines create native opening buttons and offline HTML downloads.
  Missing translations or changed source descriptions
  fail validation instead of silently shipping an obsolete French description.
- python tools/build_manual_diagrams.py and node tools/render_manual_diagrams.cjs
  regenerate figures. Rasterization needs Playwright and a browser; ordinary
  application builds use the checked-in images.
- python tools/capture_manual_illustrations.py regenerates examples.
  --only shader_editor limits recapture to the source editor.

CMake copies the browser edition beside development executables. Both package
builders validate freshness and stage the same registered sources, translations
and figures. Unrelated captures are excluded.

## Verification

The Brushes category covers all 53 shipped programmable tips with bilingual usage
notes, source-derived parameter defaults, spacing, timed airbrush, extent, dynamics,
self-flow and texture slots. Help > Brushes opens the index. F1 / Brush help in
the panel use the selected painting or eraser tip; standard/unknown tips fall back
to the guide and custom code/graphs to authoring. The twelve-sample contact sheet
uses actual GPU preview readbacks with simulated pressure, not hardware input.
Run tools/test_documentation_native.py --case brushes-en --case brushes-fr to
compile/render every brush and compare compiled defaults with the documentation on
D3D12 and Vulkan. Then tools/build_manual_brush_atlas.py <scenario-folder>
recomposes both contact sheets before regenerating the manual and rebuilding.

python tools/build_manual.py --check validates generated freshness.
python tools/test_manual.py checks both editions, all declared effect parameters,
media resources/hashes, offline HTML and package staging.

Headless C++ tests cover search, history, automatic/manual locale, fallback,
translation-hook isolation, F1, numeric entry and illustration enlargement.
tools/test_manual_browser.cjs covers both languages, accented search, navigation,
zoom, narrow screens, print coverage and offline images.

python tools/test_documentation_native.py covers 42 minimized D3D12/Vulkan
scenarios: opposite interface/manual languages, 150% scale, pictures with external
help removed, unavailable-language fallback, and contextual F1 from a graph and
shape tool in English/French, including custom/reference fallbacks and 150% text.
Practice cases open all six documents as unsaved copies, reject unknown targets
and ensure exports are not triggered on opening. --case selects scenarios;
--render-examples adds twelve GPU renders checking dimensions, transparency,
non-empty output and normal routing on both backends.
Tool-reference cases verify all 46 application context mappings in each backend,
English/French reference rendering, missing external help and 150% UI scale.
Command cases verify all 20 selected-step context mappings, F1, compiled defaults,
disabled-step semantics, stopping on error without rollback, a PNG export, batch
dimension-based names and numbered collision copies. All files stay in isolated
build test directories; no real mouse/keyboard input is used.

## Complete current-build coverage

The final pass adds settings/storage/update states, practical shader authoring,
asset handoff, recovery/performance and a checked panel directory. Existing chapters
now include full workflows for Paint 3D, geometry baking, Photo RAW/HDR/panorama,
animation, rendering, masks, reusable assets and color-managed delivery.

All 96 EffectGraphOps, 45 MathOps, 11 ExecActions and 16 NodeKind entries have
bilingual behavior notes and compiled pin/default signatures. Help > Node reference,
selected-node F1 and Effect Editor > Node help expose them. The 34-panel Window menu
and directly named windows/modal dialogs are checked against the 68-entry help registry; translated
dialog titles can resolve via stable IDs. Unknown windows retain reader history.

tools/update_manual_catalog.py refreshes node_catalog.json only from matching
D3D12/Vulkan exports. Canonical source hashes reject stale data across semantic
source edits while tolerating LF/CRLF conversion. Node tests compare the final build's
export to that snapshot, verify all 152 operation context mappings, F1 and units/UTF-8
semantics. docs/manual/coverage.json records counts and source provenance and is staged
with both package formats. External AI servers/tablet hardware/receiving engines are
configuration-dependent; the documented local tests do not claim to validate them.

This is a local source/build update. Publishing the next installer and GitHub
release remains a separate distribution action.
