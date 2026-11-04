"""Exercise install, repair and uninstall without operating input devices.

Refuses to run while Shaderixel Alpha is already registered for this user.
Uses a unique folder under build/ and a unique Start menu group.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import uuid
import winreg

ROOT = Path(__file__).resolve().parents[1]
UNINSTALL_KEY = r'Software\Microsoft\Windows\CurrentVersion\Uninstall\{D68E8140-342F-4A36-9767-9FAE9BD4F085}_is1'


def registered():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, UNINSTALL_KEY) as key:
            return winreg.QueryValueEx(key, 'InstallLocation')[0]
    except FileNotFoundError:
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installer', required=True, type=Path)
    parser.add_argument('--manifest', required=True, type=Path)
    args = parser.parse_args()
    if registered() is not None:
        parser.error('An existing Shaderixel Alpha registration must not be changed by this test.')
    installer = args.installer.resolve()
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    work = ROOT / 'build/InstallerCheck' / uuid.uuid4().hex
    app = work / 'Shaderixel test é'
    work.mkdir(parents=True)
    group = 'Shaderixel Installer Test ' + work.name
    start_menu = Path(os.environ['APPDATA']) / 'Microsoft/Windows/Start Menu/Programs' / group
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 7  # minimized, no focus
    results = []

    def run(command, **kwargs):
        result = subprocess.run(command, startupinfo=startup, capture_output=True, timeout=240, **kwargs)
        if result.returncode:
            raise RuntimeError('Command failed with exit code ' + str(result.returncode))
        return result

    def verify_payload():
        for relative, checksum in manifest.items():
            installed = app / relative
            assert installed.is_file(), 'Missing installed file: ' + relative
            assert hashlib.sha256(installed.read_bytes()).hexdigest() == checksum, 'Changed installed file: ' + relative

    try:
        command = [str(installer), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-',
                   '/LANG=english', '/DIR=' + str(app), '/GROUP=' + group, '/TASKS=',
                   '/LOG=' + str(work / 'install.log')]
        run(command)
        assert registered() and Path(registered()).resolve() == app.resolve()
        verify_payload()
        assert (start_menu / 'Shaderixel Alpha.lnk').is_file()
        assert (start_menu / 'Shaderixel Alpha (Vulkan).lnk').is_file()
        results.append({'check': 'install and payload hashes', 'files': len(manifest), 'passed': True})
        # Files created by the user must survive repair and uninstall.
        project = app / 'user-project.sxl'
        project.write_bytes(b'Installer preservation fixture\n')
        run(command)
        verify_payload()
        assert project.read_bytes() == b'Installer preservation fixture\n'
        results.append({'check': 'reinstall preserves user files', 'passed': True})
        for binary, demo in [('Shaderixel.exe', 'codefx'), ('ShaderixelVk.exe', 'curves')]:
            case = work / binary.removesuffix('.exe')
            case.mkdir()
            capture = case / 'capture.png'
            run([str(app / binary), '--demo', demo, '--frames', '100', '--capture', str(capture)],
                cwd=case, env=dict(os.environ, SX_LANG='en', SX_UI_FOCUS='code' if demo == 'codefx' else 'properties'))
            log = (case / 'shaderixel.log').read_text(encoding='utf-8', errors='replace')
            assert capture.is_file() and '[ui-error]' not in log
            assert 'Interface language: en' in log
            results.append({'check': 'installed ' + binary, 'passed': True})
    finally:
        # Never uninstall a different registration, even if the test failed.
        current = registered()
        uninstaller = app / 'unins000.exe'
        if current and Path(current).resolve() == app.resolve() and uninstaller.is_file():
            run([str(uninstaller), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART',
                 '/LOG=' + str(work / 'uninstall.log')])
    assert registered() is None
    assert not start_menu.exists()
    assert not (app / 'Shaderixel.exe').exists() and not (app / 'ShaderixelVk.exe').exists()
    assert (app / 'user-project.sxl').read_bytes() == b'Installer preservation fixture\n'
    results.append({'check': 'uninstall removes registration and shortcuts, preserves user files', 'passed': True})
    (work / 'results.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(results, indent=2), flush=True)
    print('Validation: ' + str(work), flush=True)


if __name__ == '__main__':
    main()
