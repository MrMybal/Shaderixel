"""Exercise native actions, UI access and Paint 3D on an isolated Shaderixel instance."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time

from test_shaderixel_mcp import Mcp, make_png, phase_fixture, provider, wait_for


def check(build, backend):
    root = Path(tempfile.mkdtemp(prefix='control-e2e-', dir=build))
    print(root, flush=True)
    image = root / 'input.png'
    make_png(image, 128)
    with provider() as mock:
        profile, state, _, env, argv = phase_fixture(build, root, image, mock, backend)
        with (profile / 'process.log').open('wb') as log:
            process = subprocess.Popen(argv, cwd=profile, env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                path = state / f'instance-{process.pid}.json'
                wait_for(path.is_file)
                client = Mcp(json.loads(path.read_text(encoding='utf-8')))
                cli = build / 'ShaderixelCli.exe'

                def run(*args, value=None, error=None):
                    data = json.dumps(value) if value is not None else None
                    result = subprocess.run([str(cli), *args], input=data, capture_output=True,
                                            text=True, encoding='utf-8', env=env, timeout=100)
                    if error is not None:
                        assert result.returncode and json.loads(result.stderr)['error']['code'] == error, result
                        return None
                    assert result.returncode == 0 and not result.stderr, result
                    return json.loads(result.stdout)

                def invoke(tool, value=None, error=None):
                    return run('invoke', 'shaderixel.' + tool, *(['-'] if value is not None else []),
                               value=value, error=error)

                def tool_ready(name, params):
                    reply = client.tool('shaderixel.' + name, **params)
                    if not reply.get('isError'):
                        return reply.get('structuredContent')
                    detail = json.loads(reply['content'][0]['text'])
                    assert detail['code'] in ('NoDocument', 'Busy', 'NotReady'), detail
                    return None

                catalogue = run('list')['operations']
                names = {op['name'] for op in catalogue}
                assert {'shaderixel.ui_capture', 'shaderixel.paint3d_stroke',
                        'shaderixel.particles_configure', 'shaderixel.paint3d_export'} <= names, names
                wait_for(lambda: tool_ready('document_info', {}))
                invoke('action_run', {'command': 'canvas_size', 'arguments': {'w': 192, 'h': 128, 'relative': False}})
                assert invoke('document_info')['width'] == 192
                assert invoke('actions_list')['commands']
                wait_for((profile / 'shaderixel_search.json').is_file, timeout=60)
                snapshot = wait_for(lambda: (v if not (v := invoke('ui_state'))['indexing'] and v['controls'] else None))
                assert snapshot['width'] > 0 and snapshot['height'] > 0 and snapshot['panels']
                invoke('ui_input', {'actions': [{'action': 'key', 'key': 'z', 'ctrl': True}]})
                assert invoke('document_info')['width'] == 128
                def click_file_menu():
                    menu = invoke('ui_state', {'query': 'Fichier'})
                    if menu['indexing']:
                        return None
                    button = next((item for item in menu['controls'] if item['label'] in ('Fichier', 'File')), None)
                    if button is None:
                        return None
                    return tool_ready('ui_input', {'actions': [{'action': 'click', 'id': button['id'], 'frame': menu['frame']}]})

                wait_for(click_file_menu)
                wait_for(lambda: any(item['label'].startswith(('Nouveau', 'New'))
                                     for item in invoke('ui_state', {'query': 'Nouveau'})['controls']), timeout=8)
                wait_for(lambda: tool_ready('ui_input', {'actions': [{'action': 'key', 'key': 'escape'}]}))
                shot = invoke('ui_capture')
                screenshot = root / 'view.png'
                assert run('download', shot['reference']['uri'], str(screenshot))['bytes'] == screenshot.stat().st_size
                assert screenshot.read_bytes().startswith(b'\x89PNG\r\n\x1a\n')
                print(backend + ': catalogue, macro, clic dans Fichier, annulation clavier et capture PNG', flush=True)

                project = invoke('paint3d_new', {'mesh': 'sphere', 'resolution': 1024, 'name': 'Sphere MCP'})
                assert project['width'] == 1024
                info = invoke('paint3d_info')
                assert info['triangles'] > 100 and info['textureSets'] and info['materials']
                assert invoke('paint3d_camera', {'preset': 'front', 'distance': 2.5})['ortho']
                material = invoke('paint3d_material', {'action': 'paint', 'name': 'MCP Paint'})
                assert material['id'] > 0
                invoke('paint3d_brush', {'size': 96, 'flow': 0.95, 'opacity': 1,
                                         'baseColor': [0.92, 0.07, 0.05], 'channels': ['base_color'], 'tip': False})
                presets = invoke('paint3d_export_presets')['presets']
                assert len(presets) > 1
                export_args = {'name': 'probe', 'size': '512', 'preset': 0, 'padding': 0}

                def export(folder):
                    output = invoke('paint3d_export', dict(export_args, folder=str(folder)))
                    return {Path(p).name: hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in output['files']}

                time.sleep(0.2)
                before = export(root / 'before')
                stroke = wait_for(lambda: tool_ready('paint3d_stroke', {'points': [[0.43, 0.5], [0.57, 0.5]]}))
                assert stroke['dabs'] > 1 and stroke['surfaces'] > 0
                after_folder = root / 'after'
                after = export(after_folder)
                assert before and before.keys() == after.keys() and before != after, (before, after)
                invoke('paint3d_export', dict(export_args, folder=str(after_folder)), error='ConfirmationRequired')

                info = invoke('particles_info')
                preset = next(row['index'] for row in info['presets'] if 'Gouttes vagabondes' in row['name'])
                configured = invoke('particles_configure', {'preset': preset, 'settings': {'life': 0.3},
                    'values': {'impact_mode': 'Rebondir', 'bounce_strength': 0.8}, 'groupOpen': {'bounce': True}})
                bounce = next(g for g in configured['groups'] if g['id'] == 'bounce')
                assert bounce['visible'] and bounce['open']
                stored = invoke('particles_save', {'name': 'Brosse MCP'})
                assert stored['brush']['name'] == 'Brosse MCP'
                reset = invoke('particles_configure', {'resetAll': True})
                bounce = next(g for g in reset['groups'] if g['id'] == 'bounce')
                assert not bounce['visible'] and not bounce['open']
                assert abs(reset['brush']['particleScript']['values']['bounce_strength'] - 0.35) < 0.001
                invoke('particles_configure', {'saved': 'Brosse MCP'})
                assert invoke('particles_play', {'action': 'emit', 'points': [[0.5, 0.43], [0.5, 0.55]], 'ticksPerPoint': 4})['preview']
                gesture = invoke('particles_gesture')
                assert gesture['inputs'] and abs(gesture['brush']['particleScript']['values']['bounce_strength'] - 0.8) < 0.001
                source = stored['brush']['particleScript']['source']
                updated = source.replace('sin(age * 9) * 0.95', 'sin(age * 9) * 0.25')
                assert updated != source
                invoke('particles_configure', {'script': updated, 'values': {'impact_force': 1.5}})
                replay = invoke('particles_play', {'action': 'replay'})
                assert replay['preview'] and replay['ticks'] > 0
                replay_script = invoke('particles_gesture')['brush']['particleScript']
                assert replay_script['values']['impact_force'] == 1.5 and replay_script['source'] == updated
                assert invoke('particles_play', {'action': 'commit'})['applied']
                saved = root / 'project.sx3d'
                result = invoke('document_save', {'path': str(saved)})
                assert Path(result['path']).stat().st_size > 0
                opened = invoke('document_open', {'path': result['path']})
                assert 0 <= opened['index'] < len(invoke('documents')['documents'])
                print(backend + ': caméra, brosse multicanal, export PBR différent, saisie et rejeu des particules', flush=True)
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=20)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('build', type=Path)
    parser.add_argument('--backend', default='d3d12', choices=('d3d12', 'vulkan'))
    options = parser.parse_args()
    check(options.build.resolve(), options.backend)
