"""Validate native CyDoc routes and contextual F1 with minimized self-exiting apps.

Uses internal ImGui events only. The OS browser launch is intercepted and logged;
the companion browser test opens those exact file URLs with offline Chromium.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from urllib.parse import urlsplit,unquote
from build_manual import ROOT,copy_manual


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',type=Path,default=ROOT/'build/Release')
    parser.add_argument('--case',action='append',dest='cases')
    args=parser.parse_args();build=args.build_dir.resolve()
    output=ROOT/'build/DocumentationNative';output.mkdir(exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix='cydoc-',dir=output));app=run/'app été #1';app.mkdir()
    for name in ('Shaderixel.exe','ShaderixelVk.exe','dxcompiler.dll','dxil.dll','slang.dll','slang-compiler.dll','slang-glslang.dll','slang-rt.dll'):
        shutil.copy2(build/name,app/name)
    for source,target in [('shaders','shaders'),('library/functions','functions'),('library/lang','lang')]:
        shutil.copytree(ROOT/source,app/target)
    copy_manual(app)
    cases=[('home-en','getting_started','en','','','',{}),
           ('home-fr','getting_started','fr','','','',{}),
           ('fallback','getting_started','de','','','',{}),
           ('deep-image','ui_textures','fr','nine-slice-and-eight-slice','nine-slice','',{}),
           ('deep-graph','function:UI UV Nine Slice Panel','en','graph','function-graph','',{}),
           ('context','getting_started','en','','','nodes',{'SX_MANUAL_CONTEXT_TEST':'1'}),
           ('tools','tool:clone','fr','','','nodes',{'SX_MANUAL_TOOLS_TEST':'tool:clone'}),
           ('nodes','builtin_nodes','en','','','effectgraph',{'SX_MANUAL_NODES_TEST':'1'}),
           ('commands','command:export','fr','','','nodes',{'SX_MANUAL_COMMANDS_TEST':'1'}),
           ('practice','tutorials','en','','','nodes',{'SX_MANUAL_EXAMPLES_TEST':'1'}),
           ('brushes','brush:cy_texture','fr','','','nodes',{'SX_MANUAL_BRUSHES_TEST':'1'})]
    if args.cases:cases=[c for c in cases if c[0] in args.cases]
    assert cases,'No matching cases'
    si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=7
    results=[]
    for case,topic,language,anchor,media,demo,extra in cases:
        for backend,exe in [('d3d12','Shaderixel.exe'),('vulkan','ShaderixelVk.exe')]:
            work=run/(backend+'-'+case);work.mkdir()
            frames='1800' if case=='brushes' else '240' if case=='commands' else '100'
            command=[str(app/exe),'--documentation-page',topic,'--documentation-language',language,
                     '--frames',frames,'--capture',str(work/'app.png')]
            if anchor:command+=['--documentation-section',anchor]
            if media:command+=['--documentation-media',media]
            if demo:command+=['--demo',demo]
            completed=subprocess.run(command,cwd=work,startupinfo=si,timeout=240,capture_output=True,
                                     env=dict(os.environ,SX_LANG='en',SX_CYDOC_CAPTURE_LINKS='1',**extra))
            log=(work/'shaderixel.log').read_text(encoding='utf-8',errors='replace')
            assert completed.returncode==0 and '[ui-error]' not in log,(case,backend,log[-8000:])
            urls=re.findall(r'\[cydoc\] open (.+)',log)
            assert urls and not re.search(r'\[manual-(?:context|nodes|tools|commands|practice|brushes)\] FAIL',log),(case,backend,log[-8000:])
            for url in urls:
                parsed=urlsplit(url);assert parsed.scheme=='file' and parsed.fragment.startswith(('/en/','/fr/'))
                location=unquote(parsed.path).lstrip('/')
                assert Path(location).is_file(),url
                assert '%20' in url and '%23' in url and '%C3%A9' in url,url
            if case=='fallback':assert '#/en/getting-started' in urls[0]
            if anchor:assert '/'+anchor in urls[0]
            if media:assert 'media='+media in urls[0]
            expected={'context':8,'tools':47,'nodes':156,'practice':7,'brushes':112}.get(case)
            if expected:assert log.count('[manual-'+case+'] PASS')==expected,(case,backend,'context count',log[-8000:])
            if case=='commands':assert '[manual-commands] PASS batch names and numbered copy' in log
            results.append({'case':case,'backend':backend,'urls':urls,'passed':True,'capture':str(work/'app.png')})
            print('PASS',backend,case,len(urls),'local CyDoc links',flush=True)
    (output/'cydoc-results.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
    print('PASS',len(results),'native CyDoc scenarios;',run,flush=True)


if __name__=='__main__':main()
