"""Generate, validate and stage Shaderixel's complete offline CyDoc manual.

Setup: python tools/build_manual.py --bootstrap
Build: python tools/build_manual.py
Check: python tools/build_manual.py --check
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from manual_content import ROOT, MANUAL, pages, languages, blocks, ui_strings, media_assets
from cydoc_content import WORK, EXPORT, ROUTES, prepare, accept_translations

STAMP = WORK.parent/'build-state.json'


def digest(path):
    data=path.read_bytes()
    if path.suffix.lower() in ('.md','.json','.py','.mjs','.html','.h','.cpp','.slang','.sxfn','.svg','.css'):
        data=data.replace(b'\r\n',b'\n')
    return hashlib.sha256(data).hexdigest()


def inputs():
    paths={ROOT/'VERSION',ROOT/'cydoc.config.json',ROOT/'cmake/dependencies.json',ROOT/'README.md'}
    for folder in ('docs','sdk','shaders/effects','library/functions','library/lang','.claude/skills','src/ui','src/doc'):
        paths.update(p for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix.lower() in ('.md','.json','.png','.svg','.h','.cpp','.py','.slang','.sxfn'))
    paths.difference_update([MANUAL/'coverage.json',ROUTES])
    paths.update((ROOT/'tools').glob('manual*.py'))
    paths.update([ROOT/'tools/build_manual.py',ROOT/'tools/cydoc_content.py'])
    paths.update(p for p in (ROOT/'tools/cydoc').iterdir() if p.is_file())
    return {p.relative_to(ROOT).as_posix():digest(p) for p in sorted(paths) if '__pycache__' not in p.parts}


def validate_generated():
    if not STAMP.is_file(): raise ValueError('CyDoc manual is not built. Run python tools/build_manual.py --bootstrap.')
    state=json.loads(STAMP.read_text(encoding='utf-8'))
    if state['inputs']!=inputs(): raise ValueError('Stale CyDoc sources. Run python tools/build_manual.py.')
    for relative,checksum in state['outputs'].items():
        path=ROOT/relative
        if not path.is_file() or digest(path)!=checksum: raise ValueError('Missing/stale CyDoc resource: '+relative)
    return pages()


def copy_manual(destination):
    validate_generated()
    target=Path(destination)/'docs/manual'
    target.mkdir(parents=True,exist_ok=True)
    # Only registered export files are distributable; ad-hoc captures/notes in a
    # developer's build directory must not enter an installer.
    state=json.loads(STAMP.read_text(encoding='utf-8'))
    for relative in state['outputs']:
        source=ROOT/relative
        if not source.is_relative_to(EXPORT):continue
        out=target/source.relative_to(EXPORT)
        out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,out)
    shutil.copy2(ROOT/'tools/cydoc/legacy-guide.html',Path(destination)/'Guide.html')


def generate():
    data,coverage=prepare()
    node=shutil.which('node')
    package=ROOT/'tools/cydoc/node_modules/cydoc'
    if not node or not (package/'dist/index.d.ts').is_file():
        raise SystemExit('Build dependencies unavailable. Run python tools/build_manual.py --bootstrap (Node.js 22+ required).')
    cli=json.loads((package/'package.json').read_text())['bin']['cydoc']
    if EXPORT.exists():
        assert EXPORT.resolve().is_relative_to(WORK.resolve())
        shutil.rmtree(EXPORT)
    subprocess.run([node,str(package/cli),'check',str(WORK/'cydoc.config.json')],cwd=ROOT,check=True)
    subprocess.run([node,str(ROOT/'tools/cydoc/build.mjs'),str(WORK)],cwd=ROOT,check=True)
    for name in ('examples',):
        shutil.copytree(MANUAL/name,EXPORT/name,dirs_exist_ok=True)
    shutil.copy2(MANUAL/'coverage.json',EXPORT/'coverage.json')
    output_files=[p for p in EXPORT.rglob('*') if p.is_file()]+[ROUTES,MANUAL/'coverage.json']
    STAMP.parent.mkdir(exist_ok=True)
    STAMP.write_text(json.dumps({'inputs':inputs(),'outputs':{p.relative_to(ROOT).as_posix():digest(p) for p in sorted(output_files)},'coverage':coverage},indent=2)+'\n',encoding='utf-8')
    return data['en']


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bootstrap',action='store_true')
    parser.add_argument('--check',action='store_true')
    parser.add_argument('--ensure',action='store_true',help='Regenerate only when inputs changed')
    parser.add_argument('--accept-translations',nargs='+',metavar='ID',help='Explicitly acknowledge reviewed French translations by legacy ID, or all')
    parser.add_argument('--output-dir',type=Path,help='Stage the complete reader beside an executable')
    args=parser.parse_args()
    if args.check and (args.bootstrap or args.accept_translations):
        parser.error('--check cannot change dependencies or translation acknowledgements')
    if args.bootstrap: subprocess.run([sys.executable,str(ROOT/'tools/fetch_cydoc.py')],cwd=ROOT,check=True)
    if args.accept_translations: accept_translations(args.accept_translations)
    if args.check:items=validate_generated()
    elif args.ensure:
        try:items=validate_generated()
        except ValueError:items=generate()
    else:items=generate()
    if args.output_dir:copy_manual(args.output_dir)
    print('CyDoc: '+str(len(items))+' original pages per language; complete export: '+str(EXPORT))


if __name__=='__main__':main()
