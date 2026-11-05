"""Stage only the validated CyDoc export at a GitHub Pages site root."""
import argparse
import json
from pathlib import Path
import shutil

from build_manual import ROOT, STAMP, EXPORT, validate_generated


def stage_documentation(destination):
    validate_generated()
    destination = Path(destination).resolve()
    if destination == ROOT or destination.is_relative_to(EXPORT.resolve()):
        raise ValueError('Choose a separate, empty site output directory.')
    if destination.exists() and any(destination.iterdir()):
        raise ValueError('Site output must be empty; existing files are preserved.')
    state = json.loads(STAMP.read_text(encoding='utf-8'))
    copied = []
    for relative in state['outputs']:
        source = ROOT / relative
        if not source.is_relative_to(EXPORT):
            continue
        if source.is_symlink():
            raise ValueError('Symlinks are not distributable documentation: ' + relative)
        target = destination / source.relative_to(EXPORT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        copied.append(target)
    (destination / '.nojekyll').write_text('', encoding='utf-8')
    return copied


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    print('Staged', len(stage_documentation(args.output)), 'CyDoc files:', args.output)
