"""Local end-to-end test against a real Shaderixel executable and a mock chat provider."""

import argparse
import contextlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import tempfile
import threading
import time
import urllib.error
import urllib.request
import zlib

ROOT = Path(__file__).resolve().parents[2]
MODEL = 'shaderixel-test-model'
LAYER = 'CyMCP integration layer'


def wait_for(predicate, timeout=45):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        value = predicate()
        if value:
            return value
        time.sleep(0.12)
    raise RuntimeError('Timed out waiting for Shaderixel')


def make_png(path, side):
    rng = random.Random(414)
    pixels = b''.join(b'\0' + rng.randbytes(side * 4) for _ in range(side))

    def chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload
                + struct.pack('>I', zlib.crc32(kind + payload)))

    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', side, side, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(pixels, 1)) + chunk(b'IEND', b''))


class Provider(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        if self.path != '/v1/models':
            self.send_error(404)
            return
        data = json.dumps({'object': 'list', 'data': [{'id': MODEL, 'object': 'model'}]}).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        if self.path != '/v1/chat/completions':
            self.send_error(404)
            return
        request = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        messages = request['messages']
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.end_headers()

        def delta(data, reason=None):
            envelope = {'id': 'fixture', 'object': 'chat.completion.chunk', 'model': MODEL,
                        'choices': [{'index': 0, 'delta': data, 'finish_reason': reason}]}
            self.wfile.write(('data: ' + json.dumps(envelope) + '\n\n').encode())
            self.wfile.flush()

        try:
            if 'fixture-cancel-turn' in str(messages[-1].get('content', '')):
                delta({'role': 'assistant', 'content': 'Interrompre'})
                time.sleep(3)
                delta({'content': ' suite'})
            elif messages[-1]['role'] == 'tool':
                self.server.tool_answer = messages[-1]['content']
                delta({'role': 'assistant', 'content': 'Contrôle'})
                time.sleep(0.75)
                delta({'content': ' réussi'})
            else:
                tools = {entry['function']['name'] for entry in request.get('tools', [])}
                if 'cy_tools_call' not in tools:
                    raise AssertionError(f'Compact tools not present: {tools}')
                args = json.dumps({'name': 'shaderixel.layer_add', 'arguments': {'kind': 'pixel', 'name': LAYER}})
                delta({'role': 'assistant', 'tool_calls': [
                    {'index': 0, 'id': 'call_shaderixel_layer', 'type': 'function',
                     'function': {'name': 'cy_tools_call', 'arguments': args}}
                ]})
                delta({}, 'tool_calls')
                self.wfile.write(b'data: [DONE]\n\n')
                return
            delta({}, 'stop')
            self.wfile.write(b'data: [DONE]\n\n')
        except (BrokenPipeError, ConnectionResetError):
            return


@contextlib.contextmanager
def provider():
    server = ThreadingHTTPServer(('127.0.0.1', 0), Provider)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


class Mcp:
    def __init__(self, connection):
        self.url = connection['url'] + '/mcp'
        self.headers = {'Accept': 'application/json, text/event-stream', 'Content-Type': 'application/json'}
        if 'token' in connection:
            self.headers['Authorization'] = 'Bearer ' + connection['token']
        self.sequence = 0
        reply, headers = self.call('initialize', {'protocolVersion': '2025-06-18',
                                                 'capabilities': {},
                                                 'clientInfo': {'name': 'shaderixel-test', 'version': '1'}})
        self.headers['Mcp-Session-Id'] = headers['Mcp-Session-Id']
        self.headers['MCP-Protocol-Version'] = reply['result']['protocolVersion']
        self.call('notifications/initialized', {}, notification=True)

    def call(self, method, params=None, notification=False):
        self.sequence += 1
        payload = {'jsonrpc': '2.0', 'method': method, 'params': params or {}}
        if not notification:
            payload['id'] = self.sequence
        req = urllib.request.Request(self.url, data=json.dumps(payload).encode(), headers=self.headers)
        with urllib.request.urlopen(req, timeout=35) as response:
            body = response.read()
            return (json.loads(body) if body else None), response.headers

    def tool(self, operation, **arguments):
        value = self.call('tools/call', {'name': operation, 'arguments': arguments})[0]
        return value['result']


def phase_fixture(build, test_root, fixture, mock, backend, *, access_profile=None, chat=False):
    profile = test_root / (access_profile or 'legacy-free')
    profile.mkdir()
    source_name = 'ShaderixelVk.exe' if backend == 'vulkan' else 'Shaderixel.exe'
    # The executable keeps its actual filename, settings and resources in this isolated copy.
    for filename in build.glob('*.dll'):
        shutil.copy2(filename, profile / filename.name)
    exe = profile / source_name
    shutil.copy2(build / source_name, exe)
    shutil.copytree(ROOT / 'library/lang', profile / 'lang')
    shutil.copytree(build / 'plugins', profile / 'plugins')
    shader_folder = profile / 'shaders'
    shutil.copytree(ROOT / 'shaders', shader_folder)
    state = profile / 'state'
    report = profile / 'chat-report.json'
    settings = {
        'updates': {'checkAtStartup': False},
        'ai': {
            'allowWrites': False,
            'allowFiles': False,
            'assistantConnector': 'openai-compatible',
            'assistantModel': MODEL,
            'assistantEndpoint': f'http://127.0.0.1:{mock.server_port}/v1',
            'compactTools': True,
            'keepChatHistory': True,
            'contextKiB': 8,
            'measureCalls': True,
            'spoolResults': True,
        },
    }
    if access_profile is not None:
        settings['ai']['mcpProfile'] = access_profile
    (profile / 'shaderixel_settings.json').write_text(json.dumps(settings), encoding='utf-8')
    env = dict(os.environ, SHADERIXEL_MCP_STATE_DIR=str(state))
    arguments = [str(exe), '--mcp', str(fixture)]
    if chat:
        env['SX_MCP_CHAT_RESULT'] = str(report)
        arguments += ['--ai-test', '--focus', 'Assistant IA']
    return profile, state, report, env, arguments


def check_instance(build, test_root, fixture, mock, backend, access_profile):
    chat_enabled = access_profile is None
    require_token = access_profile in ('protected', 'token_only')
    controlled = access_profile in ('protected', 'local_controlled')
    profile, state, report, env, argv = phase_fixture(
        build, test_root, fixture, mock, backend, access_profile=access_profile, chat=chat_enabled)
    cli = build / 'ShaderixelCli.exe'
    with (profile / 'process.log').open('wb') as log:
        proc = subprocess.Popen(argv, cwd=profile, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            def command(*args, input=None, error=None):
                result = subprocess.run([str(cli), *args], input=input, capture_output=True,
                                        text=True, encoding='utf-8', env=env, timeout=90)
                if error:
                    assert result.returncode != 0 and not result.stdout, result
                    assert json.loads(result.stderr)['error']['code'] == error, result
                    return None
                assert result.returncode == 0 and not result.stderr, result
                return json.loads(result.stdout)

            def connection_ready():
                file = state / f'instance-{proc.pid}.json'
                if proc.poll() is not None:
                    raise RuntimeError(f'Shaderixel exited early ({proc.returncode}): {profile}')
                if not file.is_file():
                    return None
                try:
                    return json.loads(file.read_text(encoding='utf-8'))
                except (OSError, ValueError):
                    return None

            connection = wait_for(connection_ready)
            assert connection['url'].startswith('http://127.0.0.1:')
            assert ('token' in connection) == require_token
            client = Mcp(connection)
            assert ('Authorization' in client.headers) == require_token
            catalog = client.call('tools/list')[0]['result']['tools']
            assert 'shaderixel.document_info' in {t['name'] for t in catalog}, catalog
            listed = command('list')['operations']
            assert {t['name'] for t in listed} == {t['name'] for t in catalog}
            schema = command('describe', 'shaderixel.document_info')
            assert schema['outputSchema']['properties']['layers']['type'] == 'array'
            assert command('instances')[0]['pid'] == proc.pid
            if require_token:
                try:
                    Mcp({'url': connection['url']})
                    raise AssertionError('MCP initialized without a token')
                except urllib.error.HTTPError as e:
                    assert e.code == 401
                request = urllib.request.Request(connection['url'] + '/v1', data=b'{"method":"list"}',
                                                 headers={'Content-Type': 'application/json'})
                try:
                    with urllib.request.urlopen(request, timeout=15):
                        raise AssertionError('The CLI endpoint accepted a request without a token')
                except urllib.error.HTTPError as e:
                    assert e.code == 401

            def opened():
                reply = client.tool('shaderixel.document_info')
                value = reply.get('structuredContent', {})
                return value if not reply.get('isError') and value.get('width') == 768 else None

            doc = wait_for(opened)
            info = command('--instance', str(proc.pid), 'invoke', 'shaderixel.document_info', '-', input='{}')
            assert doc['name'] == info['name'] and info['width'] == 768
            if controlled:
                forbidden = client.tool('shaderixel.layer_add', kind='pixel', name='forbidden')
                assert forbidden.get('isError') and 'AccessDenied' in json.dumps(forbidden), forbidden
                command('invoke', 'shaderixel.layer_add', '-', input='{"kind":"pixel"}', error='AccessDenied')
                if 'shaderixel.ui_input' in {t['name'] for t in catalog}:
                    command('invoke', 'shaderixel.ui_input', '-', input='{"actions":[{"action":"key","key":"z"}]}',
                            error='AccessDenied')
            else:
                invalid = client.tool('shaderixel.layer_add', kind=42)
                assert invalid.get('isError') and 'InvalidParameters' in json.dumps(invalid), invalid
                response = client.tool('shaderixel.layer_add', kind='pixel', name='external-layer')
                assert 'id' in response.get('structuredContent', {}), response
                command('invoke', 'shaderixel.layer_update', '-', input=json.dumps(
                    {'id': response['structuredContent']['id'], 'name': 'cli-layer'}))
                edited = client.tool('shaderixel.document_info')['structuredContent']
                assert 'cli-layer' in {layer['name'] for layer in edited['layers']}
            if access_profile == 'protected':
                effects = command('invoke', 'shaderixel.effects_list', '-', input='{"limit":3}')
                assert len(effects['effects']) == 3 and effects['nextOffset'] == 3
                assert command('grep', 'calques')['matches'] == []  # the guide uses English
                assert command('grep', 'document')['matches']
                render = command('invoke', 'shaderixel.render')
                uri = render['reference']['uri']
                assert render['reference']['chunked'] and render['reference']['bytes'] > 1024 * 1024, render
                downloaded = profile / 'composite.png'
                stats = command('download', uri, str(downloaded))
                assert stats['bytes'] == render['reference']['bytes'] == downloaded.stat().st_size
                with downloaded.open('rb') as file:
                    assert file.read(8) == b'\x89PNG\r\n\x1a\n'
                    file.seek(16)
                    assert struct.unpack('>II', file.read(8)) == (768, 768)
                other = client.call('resources/read', {'uri': uri})[0]['result']
                assert json.loads(other['contents'][0]['text'])['size'] == stats['bytes']
                assert any(state.glob('results-*/cymcp-results-*/*')), 'Large result did not spool to disk'
            if chat_enabled:
                chat = wait_for(lambda: json.loads(report.read_text(encoding='utf-8')) if report.is_file() else None, 140)
                assert chat['ok'] and chat['streamed'], chat
                assert getattr(mock, 'tool_answer', None), 'The local provider did not receive a tool result'
                info = command('invoke', 'shaderixel.document_info')
                names = {layer['name'] for layer in info['layers']}
                assert {LAYER, 'cli-layer'} <= names, names
                path = profile / 'saved.sxl'
                save = json.dumps({'path': str(path)})
                assert command('invoke', 'shaderixel.document_save', '-', input=save)['path'] == str(path)
                assert path.is_file() and path.stat().st_size > 0
                command('invoke', 'shaderixel.document_save', '-', input=save, error='ConfirmationRequired')
                command('invoke', 'shaderixel.document_save', '-', input=json.dumps({'path': str(profile / 'missing' / 'failed.sxl')}),
                        error='WriteFailed')
                files = list((state / 'conversations').glob('assistant-*.json'))
                assert files and 'fixture-tool-turn' in files[0].read_text(encoding='utf-8')
                metrics = wait_for(lambda: next((p for p in state.glob('performance-*.jsonl')
                                                  if p.stat().st_size > 0), None))
                contents = metrics.read_text(encoding='utf-8')
                assert 'shaderixel.layer_add' in contents and 'fixture-tool-turn' not in contents
                assert LAYER not in contents
                (test_root / 'cli-example.json').write_text(json.dumps(info, ensure_ascii=False), encoding='utf-8')
        finally:
            if proc.poll() is None:
                proc.terminate()  # Only the isolated test copy is affected.
                proc.wait(timeout=45)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('build', type=Path)
    parser.add_argument('--backend', choices=('d3d12', 'vulkan'), default='d3d12')
    args = parser.parse_args()
    build = args.build.resolve()
    test_root = Path(tempfile.mkdtemp(prefix='mcp-e2e-', dir=build))
    print(f'Test files: {test_root}', flush=True)
    cli = build / 'ShaderixelCli.exe'
    help_result = subprocess.run([str(cli), '--help'], check=True, capture_output=True, text=True)
    assert 'invoke NAME' in help_result.stdout
    version = subprocess.run([str(cli), 'version'], check=True, capture_output=True, text=True)
    print(version.stdout.strip(), flush=True)
    fixture = test_root / 'mcp-fixture.png'
    make_png(fixture, 768)
    with provider() as mock:
        for access_profile, checks in (
            ('protected', 'HTTP 401 without token, authenticated read, permissions and chunked PNG'),
            ('local_controlled', 'tokenless read and permission checks'),
            ('token_only', 'HTTP 401 without token, authenticated read and write'),
            (None, 'legacy settings use Free, tokenless read and write, validation, CLI and chat'),
        ):
            check_instance(build, test_root, fixture, mock, args.backend, access_profile)
            print(f'{args.backend} / {access_profile or "Free"}: {checks}', flush=True)


if __name__ == '__main__':
    main()
