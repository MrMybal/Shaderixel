"""Converts tools/cytexture/presets.json (from extract_presets.py) into Shaderixel
library effects: shaders/effects/cy_<category>_<name>.slang.

Every texture input of a CyTexture preset becomes an input of the effect (one
pin per texture on graph nodes: CyInput = Image, CyBackground = Fond,
CyForeground = Calque, CyMask = Masque, other textures keep their name).
Scalar parameters keep their name, default and range; vector parameters become
colors (or 2D values when the code only reads .x/.y). The Custom-node HLSL is
kept as written: it runs inside a function receiving UV, CyCanvasSize and the
parameters with the types Unreal gives them (float, float3), and
Texture2DSample(T, S, uv) reads Shaderixel input T.

Every effect also carries its visual graph (//@graph): one "HLSL personnalisé"
node holding the Custom code, with typed pins wired to texture inputs, UV, canvas
size and parameter nodes, so a copy can be edited around the code in the effect
editor (the library file itself runs the code as written).

Brush presets are skipped here (tools/cytexture/convert_brushes.py).

The Custom-node code comes compacted on one line: lay it out afterwards with
    build/Release/Shaderixel.exe --format-library shaders/effects
(whitespace only; the test --test-effects compiles every effect and graph).

    python tools/cytexture/convert_presets.py
"""
import json
import os
import re

UNREAL_MARKER = 'Imported from Unreal material'  # files written by tools/unreal/convert_materials.py (kept)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(ROOT, 'shaders', 'effects')

# Categories shown in Shaderixel (no reference to the source tool once imported).
CATEGORY = {'Draw': 'Dessin', 'Effect': 'Traitements', 'Blend': 'Fusions', 'Mask': 'Masques'}
INPUT_LABEL = {'CyInput': 'Image', 'CyBackground': 'Fond', 'CyForeground': 'Calque', 'CyMask': 'Masque'}
SYSTEM = {'CyCanvasSize', 'CyBrushPosition', 'CyBrushSize', 'CyBrushHardness', 'CyBrushFlow', 'CyBrushColor', 'CyWriteChannels'}
POSITION_HINTS = ('center', 'position', 'offset', 'direction', 'halfsize', 'size', 'origin', 'pivot', 'point', 'handle', 'scale', 'tiling', 'axis', 'start', 'end', 'focus')


def words(name):
    s = re.sub(r'(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])', ' ', name)
    return s.replace('_', ' ').strip()


def snake(name):
    return re.sub(r'[^a-z0-9]+', '_', words(name).lower()).strip('_')


def num(v):
    s = ('%.6g' % v)
    return s


# Unreal's HLSL silently truncates vectors where Slang does not; the few presets
# relying on it get the intended per-channel form.
FIXUPS = {
    'CRTDisplay': [('float triad=', 'float3 triad=')],
}


def graph_for(p, code, textures, params):
    """Visual graph of the preset: textures, UV, canvas size and parameters wired
    to one custom HLSL node holding the Custom code (EffectGraph JSON)."""
    nodes, pins, links = [], [], []
    nid = [1]

    def node(op, pos, **kw):
        n = {'id': nid[0], 'op': op, 'pos': pos, 'in': [], 'value': kw.pop('value', [0, 0, 0, 0])}
        n.update(kw)
        nid[0] += 1
        nodes.append(n)
        return n

    y = 40
    for i, t in enumerate(textures):
        n = node('texture', [40, y], index=i, name=t)
        pins.append([t, 5])
        links.append([n['id'], 0])
        y += 110
    n = node('uv', [40, y])
    pins.append(['UV', 2])
    links.append([n['id'], 0])
    y += 90
    n = node('canvassize', [40, y])
    pins.append(['CyCanvasSize', 4])
    links.append([n['id'], 0])
    y += 90
    for op, pid, value, lo, hi, pin_type in params:
        extra = {'name': pid, 'text': words(pid), 'index': 1, 'value': [float(x) for x in value]}
        if lo != 0 or hi != 1:
            extra['min'], extra['max'] = float(lo), float(hi)
        n = node(op, [40, y], **extra)
        pins.append([pid, pin_type])
        links.append([n['id'], 0])
        y += 110
    custom = node('custom', [420, 40], name=words(p['name']), text='    ' + code, pins=pins, outs=[['Resultat', 4]])
    custom['in'] = links
    out = node('output', [780, 40])
    out['in'] = [[custom['id'], 0], [0, 0], [0, 0]]
    blend = p['category'] == 'Blend'
    g = {'name': words(p['name']), 'category': CATEGORY[p['category']], 'inputs': 3 if blend else len(textures),
         'premultiplied': False, 'nextId': nid[0], 'nodes': nodes}
    if blend:
        g['kind'] = 1
    return g


def convert(p):
    code = p['code']
    for old, new in FIXUPS.get(p['name'], []):
        code = code.replace(old, new)
    textures = p['textures']
    lines = []
    lines.append('//@name ' + words(p['name']))
    lines.append('//@category ' + CATEGORY[p['category']])
    if p['category'] == 'Blend':
        lines.append('//@blend')
    if textures:
        for t in textures:
            lines.append('//@input ' + INPUT_LABEL.get(t, words(t.replace('Texture', '')) or t))
    else:
        lines.append('//@generator')
    # Neighborhood: only plain reads at UV stay local.
    reads = re.findall(r'Texture2DSample\s*\(\s*\w+\s*,\s*\w+\s*,\s*([^;]*?)\)', code)
    local = all(r.strip() == 'UV' for r in reads) and '[loop]' not in code
    lines.append('//@halo 0' if local else '//@halo full')
    fn_params, call_args = ['float2 UV', 'float4 CyCanvasSize'], ['ctx.uv', 'float4(ctx.size, ctx.texel)']
    graph_params = []  # (op, id, value[4], min, max, pin type) for the visual graph
    for s in p['scalars']:
        lines.append(f'//@param {s["name"]} float {num(s["default"])} {num(s["min"])} {num(s["max"])} "{words(s["name"])}"')
        fn_params.append('float ' + s['name'])
        call_args.append(s['name'])
        graph_params.append(('param', s['name'], [s['default'], 0, 0, 0], s['min'], s['max'], 1))
    for v in p['vectors']:
        n = v['name']
        if n in SYSTEM:
            continue  # provided by the host (canvas size...), not a user parameter
        uses = set(re.findall(re.escape(n) + r'\b(\.[a-zA-Z]+)?', code))
        # Used whole inside dot(): Unreal truncates float4 -> float3 silently, Slang does not.
        in_dot = re.search(r'dot\s*\([^;]*?\b' + re.escape(n) + r'\b(?!\.)', code) is not None
        if in_dot and not uses & {'.rgb', '.xyz'}:
            d = v['default']
            lines.append(f'//@param {n} color {num(d[0])} {num(d[1])} {num(d[2])} {num(d[3])} "{words(n)}"')
            call_args.append(n)
            fn_params.append('float4 ' + n)
            graph_params.append(('paramcolor', n, list(d[:4]), 0, 1, 4))
            continue
        planar = uses and uses <= {'.xy', '.x', '.y'} and any(h in n.lower() for h in POSITION_HINTS)
        d = v['default']
        if planar:
            lo, hi = (0, 1) if all(0 <= x <= 1 for x in d[:2]) else (-2, 2)
            lines.append(f'//@param {n} float2 {num(d[0])} {num(d[1])} {lo} {hi} "{words(n)}"')
            call_args.append(f'float3({n}, 0.0)')
            graph_params.append(('param2', n, [d[0], d[1], 0, 0], lo, hi, 3))
        else:
            lines.append(f'//@param {n} color {num(d[0])} {num(d[1])} {num(d[2])} {num(d[3])} "{words(n)}"')
            call_args.append(f'{n}.rgb')
            graph_params.append(('paramcolor', n, list(d[:4]), 0, 1, 3))
        fn_params.append('float3 ' + n)
    lines.insert(2, '//@graph ' + json.dumps(graph_for(p, code, textures, graph_params), ensure_ascii=False, separators=(',', ':')))
    lines.append('')
    lines.append(f'// Imported preset ({p["category"]} / {p["group"]}): {p["description"]}')
    lines.append('// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.')
    # Unconnected textures read as Unreal's default texture (white), like in CyTexture.
    lines.append('float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }')
    lines.append('#define Texture2DSample(T, S, UVS) CyTex(T, UVS)')
    for i, t in enumerate(textures):
        lines.append(f'static const uint {t} = {i};')
    lines.append('')
    lines.append('float4 CyPreset(' + ', '.join(fn_params) + ')')
    lines.append('{')
    lines.append('    ' + code)
    lines.append('}')
    lines.append('')
    lines.append('float4 Effect(EffectContext ctx)')
    lines.append('{')
    lines.append('    return CyPreset(' + ', '.join(call_args) + ');')
    lines.append('}')
    return '\n'.join(lines) + '\n'


def main():
    presets = json.load(open(os.path.join(HERE, 'presets.json'), encoding='utf-8'))
    written, skipped = 0, []
    for name in os.listdir(OUT):
        if name.startswith('cy_') and name.endswith('.slang'):
            path = os.path.join(OUT, name)
            if UNREAL_MARKER not in open(path, encoding='utf-8').read():
                os.remove(path)
    for p in presets:
        if p['category'] not in CATEGORY:
            skipped.append(p['category'] + '/' + p['name'])
            continue
        fname = f'cy_{p["category"].lower()}_{snake(p["name"])}.slang'
        open(os.path.join(OUT, fname), 'w', encoding='utf-8', newline='\n').write(convert(p))
        written += 1
    print(f'{written} effects written to {OUT}; skipped {len(skipped)} brush presets')
    print('Lay out the compacted code: Shaderixel.exe --format-library shaders/effects')


if __name__ == '__main__':
    main()
