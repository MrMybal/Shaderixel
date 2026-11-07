"""Refresh real EN/FR showcase captures from an already built Windows package.

Optional authoring tool: requires Pillow. No real mouse or keyboard input.
Each app process starts minimized in an isolated temporary working directory,
uses the application's own demo/capture flags and exits on its own.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MEDIA = ROOT/'website/media'


def capture(package):
    from PIL import Image
    if os.name != 'nt':
        raise SystemExit('Native screenshot generation requires Windows.')
    package = package.resolve()
    if not (package/'Shaderixel.exe').is_file():
        raise ValueError('Pass an already built portable package with Shaderixel.exe.')
    work = ROOT/'build/WebsiteCaptures'
    work.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='run-', dir=work))
    app = run/'app'
    app.mkdir()
    for name in ('Shaderixel.exe', 'dxcompiler.dll', 'dxil.dll', 'slang.dll', 'slang-compiler.dll', 'slang-glslang.dll', 'slang-rt.dll'):
        shutil.copy2(package/name, app/name)
    for name in ('shaders', 'functions', 'lang', 'smartmaterials', 'assets', 'scenes'):
        if (package/name).is_dir():
            shutil.copytree(package/name, app/name)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 7
    provenance = []
    demos = [('image-graph', 'nodes'), ('shapes', 'booleans'), ('curves', 'curves'), ('precision', 'blendif'), ('actions', 'actions'), ('material', 'render')]
    for language in ('en', 'fr'):
        for name, demo in demos:
            case = run/(name+'-'+language)
            case.mkdir()
            target = case/'capture.png'
            frames = '280' if name == 'material' else '160'
            illustration = 'render_view' if name == 'material' else 'website'
            args = [str(app/'Shaderixel.exe'), '--demo', demo, '--frames', frames, '--capture', str(target)]
            if name == 'material':
                args.append(str(package/'Exemples/16_Tasse_Paint3D.sx3d'))
            result = subprocess.run(args, cwd=case, startupinfo=startup,
                                    env=dict(os.environ, SX_LANG=language, SX_THEME='violet', SX_DPI='1.25', SX_MANUAL_ILLUSTRATION=illustration),
                                    capture_output=True, timeout=180)
            log = (case/'shaderixel.log').read_text(encoding='utf-8', errors='replace')
            if result.returncode or '[ui-error]' in log:
                raise RuntimeError('Capture failed; inspect ' + str(case/'shaderixel.log'))
            image = Image.open(target).convert('RGB')
            if name == 'precision':
                if image.size != (2544, 1401):
                    raise ValueError('Review the crop coordinates after changing capture resolution.')
                image = image.crop((1930, 660, 2544, 1320))
            dest = MEDIA/(name+'-'+language+'.webp')
            image.save(dest, quality=95 if name == 'precision' else 92, method=6)
            provenance.append({'file':dest.relative_to(ROOT).as_posix(), 'demo':demo, 'language':language, 'size':image.size})
            print('Captured', dest.name, flush=True)
    (run/'provenance.json').write_text(json.dumps(provenance, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', required=True, type=Path)
    capture(parser.parse_args().package)
