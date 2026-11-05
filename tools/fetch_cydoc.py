"""Build the pinned private CyDoc checkout and install its local package archive.

Run explicitly during developer setup. Ordinary builds never fetch dependencies.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'external/cydoc'
LOCK = json.loads((ROOT/'cmake/dependencies.json').read_text())['cydoc']


def git(*args):
    return subprocess.check_output(['git', '-c', 'safe.directory='+DEST.as_posix(),
                                    '-C', str(DEST), *args], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--refresh', action='store_true', help='Rebuild the same locked dependency')
    args = parser.parse_args()
    os.environ.setdefault('npm_config_cache',str(ROOT/'build/npm-cache'))
    created=not DEST.exists()
    if created:
        subprocess.run(['git', 'clone', '--no-checkout', LOCK['repository'], str(DEST)], check=True)
    if not created and git('status', '--porcelain', '--untracked-files=no'):
        raise SystemExit('CyDoc has local changes; preserve them before updating the dependency.')
    if created or git('rev-parse', 'HEAD') != LOCK['revision']:
        git('fetch', 'origin', LOCK['revision'], '--depth=1')
        git('checkout', '--detach', LOCK['revision'])
    npm = shutil.which('npm.cmd' if os.name=='nt' else 'npm')
    if not npm:
        raise SystemExit('Node.js 22+ and npm are required only to generate documentation.')
    cache = ROOT/'build/CyDocBootstrap'
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache/('cydoc-'+LOCK['version']+'.tgz')
    marker = cache/'revision.txt'
    if args.refresh or not archive.exists() or not marker.exists() or marker.read_text().strip()!=LOCK['revision']:
        subprocess.run([npm, 'ci', '--no-audit', '--no-fund'], cwd=DEST, check=True)
        subprocess.run([npm, 'run', 'build'], cwd=DEST, check=True)
        subprocess.run([npm, 'pack', '--ignore-scripts', '--pack-destination', str(cache)], cwd=DEST, check=True)
        marker.write_text(LOCK['revision']+'\n')
    subprocess.run([npm, 'ci', '--omit=optional', '--no-audit', '--no-fund'], cwd=ROOT/'tools/cydoc', check=True)
    print('CyDoc '+LOCK['version']+' pinned to '+LOCK['revision'])


if __name__=='__main__':
    main()
