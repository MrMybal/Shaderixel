# Shaderixel showcase

The static EN/FR showcase uses actual application captures, the published Windows
alpha downloads, and the complete existing CyDoc export. English is the default;
`?lang=fr` selects French, including screenshot UI and manual links.

## Build and preview

From the repository root, with Python 3.12+ and the documentation dependencies
already bootstrapped:

```powershell
python .CyDocBuild/build_manual.py --check
python website/build.py --output build/website-preview
python website/test_site.py --output build/website-preview
python -m http.server 5812 --bind 127.0.0.1 --directory build/website-preview
```

Open <http://127.0.0.1:5812/> or <http://127.0.0.1:5812/?lang=fr>.
Use a **fresh empty output directory** for each build. The builder preserves
unrecognized existing files by refusing a nonempty destination. It only copies
the registered CyDoc export and explicit showcase assets, never application
settings, local projects, source code, caches or credentials.

If documentation inputs changed, first run
`python .CyDocBuild/build_manual.py --bootstrap` once, then
`python .CyDocBuild/build_manual.py`. See the existing `.CyDocBuild` guides.

There is no site-specific Node dependency, runtime fetch, CDN, remote font or web
framework. The initial English page is static HTML; language switching, workspace
tabs and the accessible image dialog are progressive enhancements. CSS, JavaScript,
locale data and images have content hashes in their filenames. The staged export
also works from a local folder; browser automation verification uses loopback HTTP.

## Documentation and future Pages deployment

`Documentation.html` and `DocData/` are copied byte for byte by the existing
`.CyDocBuild/stage_documentation.py` mechanism. The application continues to open
its existing offline reader, with no build/package/help changes. The showcase
links directly to `Documentation.html#/en/<page>` or `#/fr/<page>`.
Old root `#/en/<page>`, `#en/<page>` and French equivalents redirect to that reader.

The existing Pages workflow stages this combined site, then tests the output. A
future push to `main` will use that workflow. **Creating or previewing the local
site does not publish anything.** No Pages deployment was requested for this change.

`release.json` pins the actual published alpha, installer and portable ZIP. Update
it deliberately when the application release changes; the static check checks it
against `VERSION`. Keep alpha/platform limitations accurate in both locales.

## Captures and content

`content.json` contains paired EN/FR strings, including alt text. `media.json`
declares dimensions and the language variants. Sources of the committed WebP files:

- `layers`, `effect-lab`, `shader`, `paint3d`, `render`, `raw`, `animation`:
  existing `.CyDocBuild/assets/manual/*_{en,fr}.png` captures. RGB WebP quality 92.
- `image-graph`, `shapes`, `curves`, `actions`: real `nodes`, `booleans`, `curves`
  and `actions` native demos at 2544 × 1401, violet theme, 125% UI scale.
- `material`: supplied `Exemples/16_Tasse_Paint3D.sx3d` in the native render demo.
- `precision`: unretouched crop `(1930, 660, 2544, 1320)` from the real `blendif`
  demo properties panel, showing numeric transforms and pixel baking. Quality 95.
- `ui-toolkit` and `uv-library`: actual graph output sheets from
  `docs/images/ui_toolkit.png` and `docs/images/ui_uv_library.png`, quality 94.
- `logo`: lossless WebP of `assets/logo.png`.

The photo-development example is procedural and is explicitly captioned as such.
The material view selector shows two workspaces of the same sphere, not a claimed
before/after comparison. No generated marketing image or invented user data is used.

Optional refresh of the six additional captures, on Windows with Pillow installed:

```powershell
python website/capture.py --package dist/Shaderixel-0.1.0-alpha.20261106-windows-x64
```

The tool uses minimized app processes, isolated working directories and scripted
capture flags; it never sends real mouse or keyboard input. Review newly captured
images for privacy and accuracy before committing them. Refresh `media.json` if
dimensions change. For the existing manual captures use its established capture
tool (`.CyDocBuild/capture_manual_illustrations.py`).

## Verification

`test_site.py` verifies translation coverage, real manual page IDs, local assets,
anchors, hashed filenames, download metadata and exact export contents. It also
compares every staged CyDoc file against the generated reader. No application
rebuild is needed because the app itself is unchanged.

Browser QA should cover 1280 × 720 and 1440 × 900 desktop, 390 × 844 and 320px
mobile, both locales, tabs and arrow navigation, material views, image enlargement
and Escape/focus restoration, FAQ, menu, and the actual CyDoc reader. Check that
the entire main screenshot is visible in the initial desktop viewport and that
mobile GitHub access remains visible. Respect reduced motion and browser zoom.

Verified locally on 2026-11-07 in the Codex Chromium browser: EN/FR at 1280 × 720,
FR at 1440 × 900, 390 × 844 and 320 × 700. The desktop hero is entirely visible;
mobile has no page overflow and keeps GitHub visible. Language-specific images
and manual routes, workspace tabs/arrow keys, material views, mobile menu, FAQ,
image dialog navigation/Escape/focus restoration and chapter tracking were checked.
All 28 locale media resources returned HTTP 200. The five static checks passed,
including byte equality for the 90 CyDoc export files. Download metadata matches
the public, non-draft alpha release. No browser console errors were observed.

The existing public CyDoc reader was visually checked; the unchanged bundled
reader was verified by hashes. Local website browser QA used loopback HTTP, not
`file://`. Real phone hardware, Safari and Firefox were not tested. No push or
deployment is part of this local change.
