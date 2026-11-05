"""Capture both GPU backends with isolated settings and no OS input.

Runs the embedded manual with/without help files, without touching an installed app.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import ctypes
from ctypes import wintypes
import hashlib
from manual_examples import examples as practice_documents
from manual_commands import catalog as command_catalog
from manual_brushes import catalog as brush_catalog
from manual_nodes import catalog as node_catalog

from build_manual import ROOT, copy_manual


def verify_embedded_images(executable):
    """Inspect PE resources as data; never execute the DLL entry point."""
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.LoadLibraryExW.argtypes=[wintypes.LPCWSTR,ctypes.c_void_p,wintypes.DWORD]
    kernel.LoadLibraryExW.restype=ctypes.c_void_p
    kernel.FindResourceW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p]
    kernel.FindResourceW.restype=ctypes.c_void_p
    kernel.LoadResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p]
    kernel.LoadResource.restype=ctypes.c_void_p
    kernel.SizeofResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p]
    kernel.SizeofResource.restype=wintypes.DWORD
    kernel.LockResource.argtypes=[ctypes.c_void_p]
    kernel.LockResource.restype=ctypes.c_void_p
    kernel.FreeLibrary.argtypes=[ctypes.c_void_p]
    module=kernel.LoadLibraryExW(str(executable),None,0x02|0x20)
    if not module:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        manifest=json.loads((ROOT/'docs/manual/media/manifest.json').read_text(encoding='utf-8'))
        for asset in manifest:
            resource=kernel.FindResourceW(module,asset['resource'],10)
            assert resource, (executable.name,asset['path'],'resource missing')
            memory=kernel.LoadResource(module,resource)
            data=ctypes.string_at(kernel.LockResource(memory),kernel.SizeofResource(module,resource))
            assert hashlib.sha256(data).hexdigest()==asset['sha256'],(executable.name,asset['path'],'stale embedded image')
        print('PASS',executable.name,len(manifest),'embedded image hashes',flush=True)
        return len(manifest)
    finally:
        kernel.FreeLibrary(module)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/Release')
    parser.add_argument('--case',action='append',dest='selected_cases',help='Run only this named scenario; repeat for several.')
    parser.add_argument('--render-examples',action='store_true',help='Also render the six practice graphs on both backends.')
    args = parser.parse_args()
    args.build_dir=args.build_dir.resolve()
    for executable in ('Shaderixel.exe','ShaderixelVk.exe'):
        verify_embedded_images(args.build_dir/executable)
    output = ROOT / 'build/DocumentationNative'
    output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='run-', dir=output))
    app = run / 'app'
    app.mkdir()
    for name in ('Shaderixel.exe', 'ShaderixelVk.exe', 'dxcompiler.dll', 'dxil.dll',
                 'slang.dll', 'slang-compiler.dll', 'slang-glslang.dll', 'slang-rt.dll'):
        shutil.copy2(args.build_dir / name, app / name)
    shutil.copytree(ROOT / 'shaders', app / 'shaders')
    shutil.copytree(ROOT / 'library/functions', app / 'functions')
    shutil.copytree(ROOT / 'library/lang', app / 'lang')
    copy_manual(app)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 7  # SW_SHOWMINNOACTIVE
    results = []
    cases = [('home-en', 'getting_started', False, 'violet', '1', 'en', '', 'layers_view_en'),
             ('home-fr', 'getting_started', False, 'violet', '1', 'fr', '', 'layers_view_fr'),
             ('function-fr', 'function:UI UV Nine Slice Panel', False, 'light-grey', '1', 'fr', '', ''),
             ('fr-on-english', 'image_graph', False, 'violet', '1', 'en', 'fr', 'graphs_fr'),
             ('en-on-french', 'layers', False, 'light-grey', '1', 'fr', 'en', 'layers_en'),
             ('embedded-fr', 'ui_textures', True, 'violet', '1.5', 'fr', '', 'nine_slice_fr'),
             ('fallback', 'ui_textures', True, 'violet', '1', 'en', 'de', 'nine_slice_en'),
             ('context-en', 'getting_started', True, 'violet', '1', 'en', '', 'nine_slice_en'),
             ('context-fr', 'getting_started', True, 'light-grey', '1', 'fr', '', 'nine_slice_fr')]
    cases += [('examples-en','tutorials',False,'violet','1','en','','matte_en'),
              ('examples-fr','tutorials',False,'light-grey','1','fr','','matte_fr')]
    cases += [('tools-en','tool:clone',True,'violet','1','en','',''),
              ('tools-fr','tool:background_eraser',True,'light-grey','1.5','fr','','')]
    cases += [('commands-en','command:flatten',True,'violet','1','en','',''),
              ('commands-fr','command:filter',True,'light-grey','1','fr','','')]
    cases += [('brushes-en','brush:cy_texture',True,'violet','1','en','',''),
              ('brushes-fr','brush:cy_texture',True,'light-grey','1','fr','','')]
    cases += [('nodes-en','builtin_nodes',True,'violet','1','en','',''),
              ('nodes-fr','builtin_nodes',True,'light-grey','1.5','fr','',''),
              ('delivery-fr','delivery',True,'violet','1','fr','','delivery_fr'),
              ('settings-en','settings',True,'violet','1','en','','')]
    if args.selected_cases:
        unknown=set(args.selected_cases)-{case[0] for case in cases}
        if unknown: parser.error('Unknown cases: '+', '.join(sorted(unknown)))
        cases=[case for case in cases if case[0] in args.selected_cases]
    for case, chapter, missing, theme, scale, interface_language, manual_language, picture in cases:
        if not missing and not (app/'docs').exists():
            (app/'docs-not-loaded').rename(app/'docs')
        if missing and (app / 'docs').exists():
            (app / 'docs').rename(app / 'docs-not-loaded')
        for backend, exe in [('d3d12', 'Shaderixel.exe'), ('vulkan', 'ShaderixelVk.exe')]:
            folder = run / (backend + '-' + case)
            folder.mkdir()
            command = [str(app / exe), '--documentation-page', chapter, '--frames', '80', '--capture', str(folder / 'app.png')]
            if manual_language:
                command += ['--documentation-language',manual_language]
            env = dict(os.environ, SX_LANG=interface_language, SX_THEME=theme, SX_DPI=scale)
            if case.startswith('context-'):
                command += ['--demo','nodes']
                env['SX_MANUAL_CONTEXT_TEST']='1'
            if case.startswith('examples-'):
                command += ['--demo','nodes']
                env['SX_MANUAL_EXAMPLES_TEST']='1'
            if case.startswith('tools-'):
                command += ['--demo','nodes']
                env['SX_MANUAL_TOOLS_TEST']=chapter
            if case.startswith('commands-'):
                command += ['--demo','nodes']
                command[command.index('--frames')+1]='200'
                env['SX_MANUAL_COMMANDS_TEST']='1'
            if case.startswith('brushes-'):
                command += ['--demo','nodes']
                command[command.index('--frames')+1]='1800'
                env['SX_MANUAL_BRUSHES_TEST']='1'
            if case.startswith('nodes-'):
                command += ['--demo','effectgraph']
                env['SX_MANUAL_NODES_TEST']='1'
            completed = subprocess.run(command, cwd=folder, env=env, startupinfo=startup,
                                       capture_output=True, timeout=180)
            log = (folder / 'shaderixel.log').read_text(encoding='utf-8', errors='replace')
            assert completed.returncode == 0, (case, backend, log[-5000:])
            assert '[ui-error]' not in log, (case, backend, log[-5000:])
            if case.startswith('context-'):
                assert '[manual-context] FAIL' not in log and log.count('[manual-context] PASS')==8,(case,backend,log[-8000:])
            if case.startswith('examples-'):
                assert '[manual-practice] FAIL' not in log and log.count('[manual-practice] PASS')==7,(case,backend,log[-8000:])
                assert log.count('[manual] opened practice copy ')==6,(case,backend,log[-8000:])
                assert not list(folder.rglob('practice_*.png')),(case,backend,'opening a tool must not export files')
                assert not list((app/'exports').glob('practice_*.png')),(case,backend,'opening a tool must not export files')
            if case.startswith('tools-'):
                assert '[manual-tools] FAIL' not in log and log.count('[manual-tools] PASS')==47,(case,backend,log[-8000:])
            if case.startswith('nodes-'):
                assert '[manual-nodes] FAIL' not in log and log.count('[manual-nodes] PASS')==156,(case,backend,log[-12000:])
                actual=json.loads((folder/'manual-runtime-catalog.json').read_text(encoding='utf-8'))
                assert actual==node_catalog(),(case,backend,'compiled node defaults/signatures differ from reference')
            if case.startswith('brushes-'):
                from PIL import Image
                brushes=brush_catalog()
                assert '[manual-brushes] FAIL' not in log and log.count('[manual-brushes] PASS')==len(brushes)*2+6,(case,backend,log[-16000:])
                actual=json.loads((folder/'manual-brush-defaults.json').read_text(encoding='utf-8'))
                assert set(actual)=={item['brush'] for item in brushes}
                for item in brushes:
                    compiled=actual[item['brush']]
                    for key in ('spacing','airbrush','extent'):
                        assert abs(compiled[key]-item[key])<0.00001,(item['brush'],key)
                    for key in ('selfFlow','dynamics'): assert compiled[key]==item[key],(item['brush'],key)
                    assert len(compiled['params'])==len(item['params'])
                    for values,expected in zip(compiled['params'],item['params']):
                        for value,want in zip(values,expected['default']): assert abs(value-want)<0.00001,(item['brush'],expected['name'])
                    image=Image.open(folder/('brush-'+item['brush']+'.png')).convert('RGBA')
                    assert image.size==(480,160) and image.getchannel('A').getextrema()[1]>0,(item['brush'],'empty preview')
            if case.startswith('commands-'):
                from PIL import Image
                assert '[manual-commands] FAIL' not in log and log.count('[manual-commands] PASS')==27,(case,backend,log[-10000:])
                actual=json.loads((folder/'manual-command-defaults.json').read_text(encoding='utf-8'))
                assert actual=={item['command']:item['defaults'] for item in command_catalog()},(case,backend,'documented defaults differ from the compiled factory')
                for file,size in [('Action practice_proof.png',(32,32)),('batch-output/sample_001_16x8.png',(32,32)),('batch-output/sample_001_16x8 (2).png',(16,8))]:
                    with Image.open(folder/file) as png: assert png.size==size,(case,backend,file,png.size)
            assert (folder / 'app.png').stat().st_size > 10000
            assert not missing or not (app / 'docs/manual/index.html').exists()
            if picture:
                assert '[manual] embedded image media/'+picture+'.png ' in log, (case,backend,'embedded picture not decoded',log[-5000:])
            record = dict(case=case, backend=backend, chapter=chapter, externalHelp=not missing,
                          screenshot=str((folder / 'app.png').relative_to(ROOT)), dpi=scale,
                          interfaceLanguage=interface_language,manualLanguage=manual_language or 'automatic',embeddedImage=picture)
            results.append(record)
            print('PASS', backend, case, flush=True)
    (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    if args.render_examples:
        from PIL import Image
        if not (app/'docs').exists(): (app/'docs-not-loaded').rename(app/'docs')
        rendered=[]
        for name,document in practice_documents().items():
            for backend,exe in [('d3d12','Shaderixel.exe'),('vulkan','ShaderixelVk.exe')]:
                folder=run/('render-'+backend+'-'+name);folder.mkdir()
                image=folder/'result.png'
                command=[str(app/exe),str(app/'docs/manual/examples'/(name+'.sxl')),'--frames','100',
                         '--dump-image',str(image),'--capture',str(folder/'app.png')]
                completed=subprocess.run(command,cwd=folder,startupinfo=startup,timeout=180,capture_output=True,
                                         env=dict(os.environ,SX_LANG='en',SX_THEME='violet',SX_DPI='1'))
                log=(folder/'shaderixel.log').read_text(encoding='utf-8',errors='replace')
                assert completed.returncode==0 and '[ui-error]' not in log and 'FAILED' not in log,(name,backend,log[-8000:])
                pixels=Image.open(image).convert('RGBA');pixels.load()
                assert pixels.size==(document['width'],document['height']),(name,pixels.size)
                assert len(pixels.getcolors(pixels.width*pixels.height))>10,(name,'empty or constant output')
                center=pixels.getpixel((pixels.width//2,pixels.height//2))
                if name in ('ui_nine_slice','color_key'):
                    assert pixels.getpixel((0,0))[3]==0 and center[3]>250,(name,'foreground/coverage mismatch',center)
                if name=='pbr_maps': assert center[2]>center[0] and center[2]>center[1],(name,'normal output mismatch',center)
                assert not list(folder.rglob('practice_*.png')),(name,'tool unexpectedly ran on open')
                rendered.append(dict(example=name,backend=backend,dimensions=pixels.size))
                print('PASS render',backend,name,flush=True)
        (output/'practice-results.json').write_text(json.dumps(rendered,indent=2)+'\n',encoding='utf-8')
    print('PASS', len(results), 'native documentation scenarios; isolated files retained in', run, flush=True)


if __name__ == '__main__':
    main()
