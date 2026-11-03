"""Run Shaderixel's real GPU UI scenarios without operating system input devices."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/CyUiCheck')
    args = parser.parse_args()
    build = args.build_dir.resolve()
    output = build / 'validation'
    output.mkdir(exist_ok=True)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 7  # minimized, no focus; automated rendering stays behind other windows
    cases = [
        ('graph-violet', 'Shaderixel.exe', 'nodeui', 'violet', 'comfortable', '1', 'en', {}),
        ('curves-light', 'Shaderixel.exe', 'curves', 'light-grey', 'compact', '1', 'en', {'SX_UI_FOCUS': 'properties'}),
        ('shader-navy', 'Shaderixel.exe', 'codefx', 'navy', 'comfortable', '1', 'en', {'SX_UI_FOCUS': 'code'}),
        ('analysis-amber', 'Shaderixel.exe', 'analysis', 'amber-night', 'compact', '1', 'fr', {'SX_FILE': str(ROOT/'assets/logo.png'), 'SX_ANQUAD':'1', 'SX_UI_FOCUS':'analysis'}),
        ('graph-midgrey-hidpi', 'Shaderixel.exe', 'nodes', 'mid-grey', 'comfortable', '1.5', 'en', {}),
        ('gradient-violet', 'Shaderixel.exe', 'curves', 'violet', 'comfortable', '1', 'en', {'SX_UI_FOCUS':'gradient'}),
        ('vulkan-light', 'ShaderixelVk.exe', 'curves', 'light-grey', 'comfortable', '1', 'en', {'SX_UI_FOCUS':'properties'}),
    ]
    results = []
    for name, binary, demo, theme, density, dpi, language, extra in cases:
        work = output/name
        work.mkdir(exist_ok=True)
        capture = work/'capture.png'
        if capture.exists():
            capture.unlink()
        environment = dict(os.environ, SX_THEME=theme, SX_DENSITY=density, SX_DPI=dpi, SX_LANG=language, **extra)
        result = subprocess.run([str(build/binary), '--demo', demo, '--frames', '100', '--capture', str(capture)],
                                cwd=work, env=environment, startupinfo=startup, capture_output=True, timeout=180)
        log = (work/'shaderixel.log').read_text(encoding='utf-8', errors='replace')
        record = {'case': name, 'exit': result.returncode, 'capture': capture.exists(),
                  'ui_errors': log.count('[ui-error]')}
        results.append(record)
        print(json.dumps(record), flush=True)
        if result.returncode or not capture.exists() or record['ui_errors']:
            raise SystemExit('Native UI scenario failed: '+name)
    for binary in ('Shaderixel.exe', 'ShaderixelVk.exe'):
        name = 'effects-'+binary.removesuffix('.exe')
        work = output/name
        work.mkdir(exist_ok=True)
        result = subprocess.run([str(build/binary), '--test-effects', '--frames', '3'], cwd=work,
                                startupinfo=startup, capture_output=True, timeout=240)
        log = (work/'shaderixel.log').read_text(encoding='utf-8', errors='replace')
        lines = [line for line in log.splitlines() if '[test-effects]' in line]
        failed = [line for line in log.splitlines() if '[test-' in line and 'FAILED' in line]
        summary = next((line for line in reversed(lines) if 'effects compiled' in line), '')
        counts = re.search(r'(\d+) / (\d+) effects compiled', summary)
        record = {'case':name, 'exit':result.returncode, 'summary':summary, 'failed':len(failed), 'ui_errors':log.count('[ui-error]')}
        results.append(record)
        print(json.dumps(record),flush=True)
        if result.returncode or failed or record['ui_errors'] or not counts or counts[1] != counts[2]:
            raise SystemExit('Effect compilation failed: '+name)
    (output/'native-results.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    main()
