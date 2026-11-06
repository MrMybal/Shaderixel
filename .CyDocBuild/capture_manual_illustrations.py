"""Capture bilingual manual examples minimized, with isolated settings and no OS input."""
import json
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--only',action='append',choices=('layers_view','effect_graph','shader_editor','pbr_view','paint3d_view','render_view','animation_view','photo_view'))
    args=parser.parse_args()
    output = ROOT / 'build/ManualIllustrations'
    output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='run-', dir=output))
    app = run / 'app'
    app.mkdir()
    for name in ('Shaderixel.exe', 'dxcompiler.dll', 'dxil.dll', 'slang.dll',
                 'slang-compiler.dll', 'slang-glslang.dll', 'slang-rt.dll'):
        shutil.copy2(ROOT / 'build/Release' / name, app / name)
    for source, target in [('shaders', 'shaders'), ('library/functions', 'functions'), ('library/lang', 'lang')]:
        shutil.copytree(ROOT / source, app / target)
    media = ROOT / '.CyDocBuild/assets/manual'
    media.mkdir(exist_ok=True)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 7
    results = []
    for language in ('en', 'fr'):
        for name, demo in [('layers_view', 'image'), ('effect_graph', 'effectgraph'), ('shader_editor', 'codefx'), ('pbr_view', 'pbr'),
                           ('paint3d_view','paint3d'),('render_view','paint3d'),('animation_view','anim'),('photo_view','image')]:
            if args.only and name not in args.only:
                continue
            work = run / (name + '-' + language)
            work.mkdir()
            image = media / (name + '_' + language + '.png')
            # The ordinary layers demo has no image-file dependency.
            mode = [] if name == 'layers_view' else [demo]
            command = [str(app / 'Shaderixel.exe'), '--demo'] + mode + ['--frames', '150', '--capture', str(image)]
            environment=dict(os.environ,SX_LANG=language,SX_THEME='violet',SX_DPI='1.25',SX_MANUAL_ILLUSTRATION=name)
            if name=='shader_editor':
                environment['SX_UI_FOCUS']='code'
            completed = subprocess.run(command, cwd=work, env=environment,
                                       startupinfo=startup, capture_output=True, timeout=180)
            log = (work / 'shaderixel.log').read_text(encoding='utf-8', errors='replace')
            if completed.returncode or '[ui-error]' in log:
                raise RuntimeError(log[-5000:])
            results.append(dict(image=image.relative_to(ROOT).as_posix(), language=language, demo=demo))
            print('Captured', image.name, flush=True)
    (output / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
