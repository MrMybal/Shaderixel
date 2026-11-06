"""Build, validate and stage Shaderixel's CyDoc manual."""
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

STAMP=WORK/'.cache/build-state.json'


def digest(path):
    data=path.read_bytes()
    if path.suffix.lower() in ('.md','.json','.py','.mjs','.html','.h','.cpp','.slang','.sxfn','.svg','.css'):
        data=data.replace(b'\r\n',b'\n')
    return hashlib.sha256(data).hexdigest()


def inputs():
    paths={ROOT/'VERSION',ROOT/'cmake/dependencies.json'}
    for folder,subdirs,filenames in os.walk(WORK):
        subdirs[:]=[name for name in subdirs if name not in ('.cache','node_modules','__pycache__','internal')]
        paths.update(Path(folder)/name for name in filenames if not name.startswith('generated-'))
    for folder in ('sdk/include','shaders/effects','shaders/brushes','library/functions','library/lang','src/ui','src/doc'):
        paths.update(p for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix.lower() in ('.json','.h','.cpp','.slang','.sxfn'))
    paths.update(ROOT/'sdk/examples/script_plugin'/name for name in ('shaderixel_client.py','shaderixel_panel.py'))
    paths.discard(ROUTES)
    return {p.relative_to(ROOT).as_posix():digest(p) for p in sorted(paths)}


def validate_generated():
    if not STAMP.is_file():raise ValueError('Run python .CyDocBuild/build_manual.py --bootstrap first.')
    state=json.loads(STAMP.read_text(encoding='utf-8'))
    if state['inputs']!=inputs():raise ValueError('Documentation sources changed. Run python .CyDocBuild/build_manual.py.')
    for name,sha in state['outputs'].items():
        p=ROOT/name
        if not p.is_file() or digest(p)!=sha:raise ValueError('Missing or changed CyDoc output: '+name)
    return pages()


def export_files():
    state=json.loads(STAMP.read_text(encoding='utf-8'))
    return [ROOT/name for name in state['outputs'] if (ROOT/name).is_relative_to(EXPORT)]


def copy_manual(destination):
    validate_generated()
    destination=Path(destination).resolve()
    for source in export_files():
        target=destination/'Documentation'/source.relative_to(EXPORT)
        if source.resolve()==target.resolve():continue
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    # Preserve old addresses without making them the new help entry point.
    for source,relative in [('guide.html','Guide.html'),('manual.html','docs/manual/index.html')]:
        target=destination/relative;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(WORK/'compatibility'/source,target)


def generate():
    data,coverage=prepare()
    package=WORK/'node_modules/cydoc';node=shutil.which('node')
    if not node or not (package/'dist/types.d.ts').is_file():raise SystemExit('Run python .CyDocBuild/build_manual.py --bootstrap (Node.js 22+).')
    # Remove only files registered by our previous generation. Preserve other files.
    previous=json.loads(STAMP.read_text(encoding='utf-8')) if STAMP.exists() else {'outputs':{}}
    registered={ROOT/p for p in previous['outputs'] if (ROOT/p).is_relative_to(EXPORT)}
    unrelated={p for p in EXPORT.rglob('*') if p.is_file()}-registered
    if any(p.name in ('Documentation.html','DocAI.md') for p in unrelated):raise ValueError('Unrecognized existing documentation; preserve it before generating.')
    for p in registered:
        if p.is_file():p.unlink()
    cli=json.loads((package/'package.json').read_text(encoding='utf-8'))['bin']['cydoc']
    subprocess.run([node,str(package/cli),'check',str(WORK/'cydoc.config.json')],cwd=ROOT,check=True)
    subprocess.run([node,str(WORK/'build_reader.mjs')],cwd=ROOT,check=True)
    output=[p for p in EXPORT.rglob('*') if p.is_file() and p not in unrelated]
    output += [ROUTES,WORK/'internal/coverage.json']
    output += list(MANUAL.glob('*/generated-*.md'))
    STAMP.parent.mkdir(parents=True,exist_ok=True)
    STAMP.write_text(json.dumps({'inputs':inputs(),'outputs':{p.relative_to(ROOT).as_posix():digest(p) for p in sorted(output)},'coverage':coverage},indent=2)+'\n',encoding='utf-8')
    return data['en']


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bootstrap',action='store_true');p.add_argument('--check',action='store_true')
    p.add_argument('--ensure',action='store_true');p.add_argument('--output-dir',type=Path)
    p.add_argument('--accept-translations',nargs='+',metavar='ID')
    a=p.parse_args()
    if a.check and (a.bootstrap or a.accept_translations):p.error('--check is read-only')
    if a.bootstrap:subprocess.run([sys.executable,str(WORK/'fetch_cydoc.py')],cwd=ROOT,check=True)
    if a.accept_translations:accept_translations(a.accept_translations)
    if a.check:items=validate_generated()
    elif a.ensure:
        try:items=validate_generated()
        except ValueError:items=generate()
    else:items=generate()
    if a.output_dir:copy_manual(a.output_dir)
    print('CyDoc:',len(items),'application reference routes per language;',EXPORT/'Documentation.html')


if __name__=='__main__':main()
