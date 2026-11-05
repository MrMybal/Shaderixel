# CyDoc integration and maintenance

Shaderixel's current help reader is CyDoc 0.1.0, locked to the Git revision in
`cmake/dependencies.json`. Shaderixel is a Windows C++20 application with CyUI,
Direct3D 12 and Vulkan. Help opens the complete exported reader using a Windows
file URL and CyDoc's native route helper. No server, CDN, WebView runtime, or
network request is required to read it. A modern local browser is required.

## Content and modules

The original 44 handbook chapters and all 898 reference pages are retained in
English and French. Shader metadata, function ports/variables, toolbar tools,
recorded commands, brushes and compiled node catalogs remain the authoritative
reference inputs. The six procedural practice documents and all 42 illustrations
are preserved. Repository guides, examples, resources and SDK sources are included
as additional references rather than silently discarded.

Only `image` and `graph` are selected. Image provides enlargement, keyboard/touch
interaction and local annotations for the existing figures. Graph displays the
actual saved root-level function connections, including output/channel selectors;
it does not execute shaders or tool events and does not expand nested subgraphs.
Static figures retain their editable SVG sources and are not converted into
interactive node graphs. There are no existing playable videos, controllable GIFs,
or embedded mesh assets to justify those readers. Gallery is unnecessary for the
existing chapter illustrations. Three.js is absent from the installed application
tool package and the exported reader. Building CyDoc itself uses its own upstream
development dependencies, which are not application runtime dependencies.

The host uses the public `cydoc/viewer` API with the Midnight theme, a stable
`shaderixel-help` reading-data namespace, and the classic local module/asset
providers produced by the CyDoc exporter. Theme choices remain editable. Source
Markdown is compiled by CyDoc; the host does not maintain a second renderer.

## Generate and distribute

Requirements: Python 3.9+, Node.js 22+, npm, and Git access to the pinned CyDoc
repository through the developer's existing credential manager.

```powershell
python tools/build_manual.py --bootstrap
python tools/build_manual.py
python tools/build_manual.py --check
```

Setup clones the locked dependency under `external/cydoc`, builds it, packs a local
archive and installs the archive with the checked npm lock. It preserves local
dependency edits. Ordinary builds do not clone or install dependencies.

The old `Guide.html` entry point forwards to the same CyDoc reader; the former test-guide prose is preserved as a historical reference.

`cydoc.config.json` is the application configuration template. Preparation writes
the effective version, aliases, Markdown, approved assets and SDK sources under
`build/CyDoc/project/`. CyDoc's `check` command compiles, validates and writes a
fresh `help/` export there. It is not a read-only check. Shaderixel's Python
`--check` instead verifies the recorded input/output hashes without regenerating.

CMake's `ShaderixelDocumentation` target regenerates stale documentation and
copies the full export to `<build>/docs/manual`. Both package builders call the
same validation/copy code. They ship scripts, modules, data, assets, examples,
translation reports and third-party notices together. Node and Python are not
needed by users reading the installed manual. Do not copy only `index.html`.

For direct inspection, open `build/CyDoc/project/help/index.html` through `file://`.
The former source-tree `docs/manual/index.html` forwards to that generated export.

## Public reader on GitHub Pages

[Documentation in English](https://mrmybal.github.io/Shaderixel/#/en/getting-started)
and [documentation en français](https://mrmybal.github.io/Shaderixel/#/fr/getting-started)
use the same reader and export as the application. HTTP hosting does not change
the bundled `file://` reader, modules, resource providers or content.

`.github/workflows/documentation.yml` generates the export on pushes to `main`
and manual dispatches, tests content and the offline browser reader, then deploys
an official Pages artifact. Only the deployment job receives Pages/OIDC write
permissions. CyDoc is fetched from its public repository at the locked revision;
no private repository token is required.

To stage a site locally after generation, choose a new empty output directory:

```powershell
python tools/stage_documentation.py --output build/pages-site
```

Staging copies only the registered export files, byte-for-byte, plus `.nojekyll`.
It never copies the application folder, dependency checkout, logs, user settings
or arbitrary files left in a developer's export directory. Existing destination
files cause an error instead of being overwritten. The repository's Pages build
type must be **GitHub Actions**. The README links select English explicitly; the
reader's locale selector and French link retain the French edition.

## Author pages, translations and routes

English handbook sources are `docs/manual/*.md`; French counterparts are in `fr/`.
Existing reference translation catalogs remain in `locales/`. The preparation
adapter produces CyDoc frontmatter and common explicit anchors. Five translated
chapters place a section earlier than English; their explicit section mapping is
kept in `tools/cydoc_content.py`, so an anchor follows meaning rather than ordinal
position. A heading-count mismatch fails generation.

Old identifiers such as `ui_textures`, `effect:ui_uv_nine_slice` and function names
are aliases of stable CyDoc slugs. The reader supports both previous
`#en/getting_started` links and CyDoc `#/en/getting-started` routes. Native help uses
Windows `UrlCreateFromPathW` and CyDoc's UTF-8 route encoder, including spaces,
non-ASCII paths, explicit sections and media IDs. Relevant command-line options:

```powershell
build/Release/Shaderixel.exe --documentation-page ui_textures --documentation-language fr --documentation-section nine-slice-and-eight-slice --documentation-media nine-slice
```

F1, menu entries and Selection/Tool/Brush/Command/Node help use the same native
catalog and the same file reader. Unknown/custom resources retain handbook
fallbacks. Help > Practice projects retains the application-side unsaved-copy
workflow; browser practice pages offer local downloads.

For the original bilingual pages, English revisions are content fingerprints and
French `sourceRevision` values record the reviewed source fingerprint in
`docs/manual/locales/revisions.json`. Generation never silently advances them.
After reviewing a changed translation, acknowledge its legacy page ID explicitly:

```powershell
python tools/build_manual.py --accept-translations getting_started
```

An English edit leaves an old translation outdated; an unreviewed French edit
becomes untracked. `--accept-translations all` establishes a reviewed migration
baseline and should not be used to dismiss unreviewed content changes. Existing checks
continue to reject stale French function/variable descriptions. Each generation
writes CyDoc's translation report. Additional repository appendices that never had
translations explicitly retain their original language; they are not silently
presented as reviewed translations. French technical originals remain accessible
with an English notice. Source annotations currently have no revision tags in
CyDoc's extraction contract and are consequently reported as untracked.

## Code documentation and boundaries

CyDoc extracts the bilingual `@doc sdk-abi` annotations from the public C SDK
header. Its lexical source index includes only the SDK header and shipped C++/
Python plugin examples. Complete internal editor sources, dependency checkouts,
build output, local settings and private configuration are not indexed. Include/
import links and line anchors are generated by CyDoc. The result is not a C++
compiler index; macro expansion and overload inference are not claimed. No
Doxygen or TypeDoc dependency is needed for these explicit annotations and sources.

Shipped shader/function files linked by the references remain downloadable and
are not executed by the documentation. Existing code examples are highlighted at
generation time. Roadmap and former manual milestone records are labeled historical;
their planned features and obsolete reader implementation details are not treated
as current application behavior.

## Verification and limitations

```powershell
python tools/build_manual.py --check
python tools/test_manual.py
node tools/test_manual_browser.cjs
python tools/test_documentation_native.py
```

The browser test uses Playwright with an installed Chrome/Chromium, an offline
context and `file://`. Browser-internal DOM events exercise navigation, language,
anchors, media, keyboard/touch behavior, download links and reading positions.
Native tests launch minimized, self-exiting D3D12/Vulkan fixtures, exercise F1
through internal ImGui events, and capture the exact generated URLs without
opening uncontrolled browser windows. No real mouse or keyboard is used.

The export can be staged through either package path and compared file-for-file.
Native OS default-browser association and persistent file-origin storage depend on
the user's browser configuration. Linux/macOS/Android application layers do not
exist in Shaderixel and are not claimed as tested targets. External AI models,
tablets, video codecs and 3D engine integrations are unaffected by this migration.
CyDoc currently loads the full manual/search data together; very large libraries
may eventually need upstream content segmentation. The translation report exposes
remaining supplementary-reference translation work.
