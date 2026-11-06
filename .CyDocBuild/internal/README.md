# Shaderixel documentation maintenance

This directory is private working material. It is outside CyDoc's public content
roots and is never copied into the application manual or the Pages artifact.
Older `CYDOC_MIGRATION.md`, `MANUAL_MILESTONES.md`, `legacy_test_guide.fr.md` and
`previous-*` files are historical records of the replaced integration.

## Sources and distribution

`.CyDocBuild/manual/en/` is the original English edition; `manual/fr/` contains
the reviewed French edition. Numbered chapters and `topic-*.md` are authored
sources. `generated-*.md` is derived reference content and is ignored by Git.
Assets, locale catalogs, adapters and build dependencies belong in `.CyDocBuild/`.
Only `Documentation/Documentation.html`, `DocAI.md` and all of `DocData/` are
distributed. Never add internal reports or AI instructions to public content.

CyDoc 0.1.0 is pinned to `6ddf9a582defb2f11fbb3f992afef6f98dcadc93` in
`cmake/dependencies.json`. Use the documented Git checkout, build and local
`npm pack` distribution. There is no dependency on an npm registry publication
of CyDoc. The lockfile verifies the resulting archive. A fresh bootstrap needs
Git access to CyDoc, Node.js 22+, npm and Python 3.9+; reading the manual needs
none of these tools.

The current public viewer supplies navigation, top search, keyboard/touch media,
tooltips, themes and reading positions. The small application bootstrap uses
`mountViewer` and the exported module/packed-asset providers. Its only additions
are legacy bare-hash handling, F1 suppression, media targeting after lazy mounting
and reviewed local project/license download links. It is bundled from owned
sources with esbuild on each generation.
Do not edit `Documentation/` by hand.

Modules: `image` for the 42 bilingual illustrations, `gallery` plus its image
reader for three actual workspaces, and `graph` for saved function connections.
No video, GIF or mesh media is present; Three.js is not installed in the host
toolchain or shipped with the export. Graphs do not execute presets.

## Commands from the repository root

```powershell
python .CyDocBuild/build_manual.py --bootstrap
python .CyDocBuild/build_manual.py
python .CyDocBuild/build_manual.py --check
python .CyDocBuild/test_manual.py
node --test .CyDocBuild/test_reader_dom.mjs
cmd /c "build.bat Release"
python .CyDocBuild/test_documentation_native.py
```

`--bootstrap` builds the pinned library in `.cache/cydoc`, packs its local archive
and runs `npm ci --omit=optional` in `.CyDocBuild/`. Ordinary generation does not
fetch dependencies. CMake calls `--ensure --output-dir <binary-dir>`; package.py
and build_installer.py validate the same inputs and copy only registered exports.
The wrapper's `--check` is read-only. The underlying **CyDoc CLI `check` writes an
export** and is intentionally invoked during generation, not freshness checks.

The browser suite for an environment allowing local browser navigation is
`.CyDocBuild/test_manual_browser.cjs` (Playwright on Windows). It opens the
generated reader offline and uses DOM events, never real input devices. Do not
run it to circumvent a browser/tool security refusal of `file://`.

Pages staging, when requested, is:

```powershell
python .CyDocBuild/stage_documentation.py --output build/pages-site
```

The destination must be fresh. GitHub Actions builds/stages the same export and
keeps a small root index redirect for existing public URLs. If CyDoc is private,
the workflow needs a `CYDOC_READ_TOKEN` Actions secret with read access to that
repository; the application's default GitHub token cannot read another private
repository. This local update does not push, run Actions or change secrets.

## Translations and public APIs

Keep common stable page IDs and explicit anchors in EN/FR. Review both editions
before acknowledging a chapter with
`python .CyDocBuild/build_manual.py --accept-translations <chapter-id>`.
Source and translation fingerprints intentionally do not advance automatically.
Descriptions/variable notes use reviewed paired catalogs. `fr_labels.txt` records
French library labels; unknown public labels stop generation. Composite controls
keep their scope visibly separate from their setting. Filenames, serialized
types and public symbols retain their actual technical spelling.

`public_api.py` extracts C callback declarations from the public SDK header and
Python signatures with `ast` from the two public helper modules. `prepare_api.mjs`
uses the public CyDoc `extractComments` and neutral `importApi` APIs. It emits
signatures, curated bilingual parameters/behavior and short examples. This is not
a full C++ or Python semantic analyzer. Header annotations supply the ABI page;
authored guides cover structs, events, manifests and the Unreal connector.
No SourceFile pages, implementation bodies or source trees are exported.

## Local verification on 2026-11-06

The current export has 1,094 pages per locale, including the 942 existing native
routes, 65 public API pages, authored developer guides and navigation hubs.
Common anchors, aliases, translation states, local resources, module selection
and portable/installer staging are checked by the content suite. Native scenarios
run both renderers minimized, self-exit and capture URLs without launching the OS
browser. DOM fixtures evaluate no scripts and fetch no resources.

The integrated browser explicitly refused `file://` under its security policy.
No alternate browser, server or navigation workaround was used. Real browser
rendering, geometry/overflow and file-origin persistence require a manual test
or a future permitted CI run. Do not describe the DOM checks as visual/browser
verification. Generated reports and captures stay in this internal directory.
