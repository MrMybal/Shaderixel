"""Builds the Shaderixel Link Unreal plugin for every installed engine (4.25, 4.27 and 5.x).

    python tools/build_unreal_plugin.py                 # every engine found under the engines root
    python tools/build_unreal_plugin.py 4.27 5.5         # some versions only
    python tools/build_unreal_plugin.py --root "C:/Program Files/Epic Games"

Each build lands in build/ue/<version>/ShaderixelLink (binaries + sources, ready to copy into a
project's Plugins folder or the engine's Plugins/Marketplace folder). A summary is printed at the end.
"""
import os
import re
import subprocess
import sys

import local_paths

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLUGIN = os.path.join(ROOT, 'unreal', 'ShaderixelLink', 'ShaderixelLink.uplugin')
# Engines roots: tools/local_paths.json "unreal_engines" (or UNREAL_ENGINES), else the usual install folder.
DEFAULT_ROOTS = local_paths.get('unreal_engines', []) + [r'C:\Program Files\Epic Games']


def engines(root_dirs):
    found = {}
    for r in root_dirs:
        if not os.path.isdir(r):
            continue
        for d in os.listdir(r):
            m = re.match(r'UE_(\d+)\.(\d+)$', d)
            if not m:
                continue
            ver = (int(m.group(1)), int(m.group(2)))
            if ver < (4, 25):
                continue
            uat = os.path.join(r, d, 'Engine', 'Build', 'BatchFiles', 'RunUAT.bat')
            if os.path.exists(uat):
                found['%d.%d' % ver] = uat
    return found


def main():
    args = sys.argv[1:]
    roots = DEFAULT_ROOTS
    if '--root' in args:
        i = args.index('--root')
        roots = [args[i + 1]]
        del args[i:i + 2]
    found = engines(roots)
    wanted = args or sorted(found, key=lambda v: tuple(map(int, v.split('.'))))
    results = []
    for ver in wanted:
        if ver not in found:
            results.append((ver, 'moteur introuvable'))
            continue
        out = os.path.join(ROOT, 'build', 'ue', ver, 'ShaderixelLink')
        log = os.path.join(ROOT, 'build', 'ue', ver + '.log')
        os.makedirs(os.path.dirname(out), exist_ok=True)
        print('Unreal %s...' % ver, flush=True)
        cmd = ['cmd', '/c', found[ver], 'BuildPlugin', '-Plugin=' + PLUGIN, '-Package=' + out, '-TargetPlatforms=Win64', '-Rocket']
        # Older RunUAT.bat run AutomationTool.exe from the current folder: not with this variable set.
        env = {k: v for k, v in os.environ.items() if k.lower() != 'nodefaultcurrentdirectoryinexepath'}
        with open(log, 'w', encoding='utf-8', errors='replace') as f:
            code = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, env=env).returncode
        results.append((ver, 'OK' if code == 0 else 'ÉCHEC (code %d, voir %s)' % (code, log)))
        print('  ' + results[-1][1], flush=True)
    print()
    for ver, status in results:
        print('UE %-5s %s' % (ver, status))
    return 0 if all(s == 'OK' for _, s in results) else 1


if __name__ == '__main__':
    sys.exit(main())
