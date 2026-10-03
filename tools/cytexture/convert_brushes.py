"""Converts the CyTexture brush presets of tools/cytexture/presets.json (from
extract_presets.py) into Shaderixel programmable brushes:
shaders/brushes/cy_<name>.slang.

The Custom-node HLSL is kept as written and runs inside Tip(): the CyTexture
brush contract maps onto the Shaderixel brush (UV = local tip space,
CyBrushHardness / CyBrushFlow = brush settings, CyBrushPosition.z = pressure,
CyBrushColor = brush color). CyTexture brushes apply flow and pressure
themselves, hence //@selfflow.

Additions on top of the presets:
  - presets with a Seed parameter get "Graine par touche": every dab draws a
    different random pattern;
  - presets with a Rotation parameter get "Suivre le trait": the tip turns
    with the stroke direction.

Brushes reading a texture (TextureBrush) get a //@texture slot per texture,
read with Texture2DSample as in CyTexture; the default image comes from
shaders/brushes/textures (the user picks another one in the brush settings).

    python tools/cytexture/convert_brushes.py
"""
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(ROOT, 'shaders', 'brushes')

CONTRACT = {'CyBrushSize', 'CyBrushHardness', 'CyBrushFlow', 'CyBrushPosition', 'CyBrushColor', 'CyWriteChannels', 'CyCanvasSize'}
GROUPS = {'Shape Tips': 'Formes', 'Particles': 'Particules', 'Natural Media': 'Matières', 'Surface Detail': 'Détails'}
# Dab spacing per group (fraction of the diameter): scattered patterns read
# better with spaced dabs, solid tips with dense ones.
SPACING = {'Shape Tips': 0.1, 'Particles': 0.3, 'Natural Media': 0.12, 'Surface Detail': 0.45}


def words(name):
    s = re.sub(r'(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])', ' ', name)
    return s.replace('_', ' ').strip()


def snake(name):
    return re.sub(r'[^a-z0-9]+', '_', words(name).lower()).strip('_')


def num(v):
    return '%.6g' % v


def convert(p):
    code = p['code']
    scalars = [s for s in p['scalars'] if s['name'] not in CONTRACT]
    vectors = [v for v in p['vectors'] if v['name'] not in CONTRACT]
    names = {s['name'] for s in scalars}
    seeded, rotating = 'Seed' in names, 'Rotation' in names
    title = words(p['name'][:-5] if p['name'].endswith('Brush') else p['name'])
    lines = ['//@name ' + title,
             '//@category ' + GROUPS.get(p['group'], 'Pinceaux'),
             '//@selfflow',
             '//@spacing ' + num(0.35 if p['textures'] else SPACING.get(p['group'], 0.15))]
    for t in p['textures']:
        lines.append(f'//@texture {t} "{words(t)}" textures/splatter.png')
    textured = bool(p['textures'])
    fn_params = ['float2 UV', 'float CyBrushHardness', 'float CyBrushFlow', 'float4 CyBrushPosition',
                 'float3 CyBrushColor', 'float4 CyCanvasSize']
    call_args = ['b.uv', 'b.hardness', 'b.flow', 'float4(b.px, b.pressure, 0.0)', 'b.color',
                 'float4(b.docSize, 1.0 / b.docSize)']
    for s in scalars:
        lines.append(f'//@param {s["name"]} float {num(s["default"])} {num(s["min"])} {num(s["max"])} "{words(s["name"])}"')
        fn_params.append('float ' + s['name'])
        call_args.append('Seed + floor(b.seed * 997.0) * RandomSeed' if s['name'] == 'Seed' else s['name'])
    for v in vectors:
        d = v['default']
        lines.append(f'//@param {v["name"]} color {num(d[0])} {num(d[1])} {num(d[2])} {num(d[3])} "{words(v["name"])}"')
        fn_params.append('float4 ' + v['name'])
        call_args.append(v['name'])
    if seeded:
        lines.append('//@param RandomSeed float 1 0 1 "Graine par touche"')
    if rotating:
        lines.append('//@param FollowStroke float 0 0 1 "Suivre le trait"')
    if textured:
        # Stamped image: random turn of each dab by default (natural texture), or along the stroke.
        lines.append('//@param RandomAngle float 1 0 1 "Rotation aléatoire"')
        lines.append('//@param FollowStroke float 0 0 1 "Suivre le trait"')
    lines.append('')
    lines.append(f'// Imported preset (Brush / {p["group"]}): {p["description"]}')
    lines.append('// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).')
    lines.append('float4 CyPreset(' + ', '.join(fn_params) + ')')
    lines.append('{')
    lines.append('    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);')
    lines.append('    ' + code)
    lines.append('}')
    lines.append('')
    lines.append('float4 Tip(Brush b)')
    lines.append('{')
    lines.append('    return CyPreset(' + ', '.join(call_args) + ');')
    lines.append('}')
    if textured:
        lines.append('')
        lines.append('void Dynamics(inout Dab d)')
        lines.append('{')
        lines.append('    d.angle = Rand(d.seed, 4.0) * 6.2831853 * RandomAngle + FollowStroke * atan2(d.direction.y, d.direction.x);')
        lines.append('}')
    if rotating:
        lines.append('')
        lines.append('void Dynamics(inout Dab d)')
        lines.append('{')
        lines.append('    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);')
        lines.append('}')
    return '\n'.join(lines) + '\n'


def main():
    presets = json.load(open(os.path.join(HERE, 'presets.json'), encoding='utf-8'))
    os.makedirs(OUT, exist_ok=True)
    for name in os.listdir(OUT):
        if name.startswith('cy_') and name.endswith('.slang'):
            os.remove(os.path.join(OUT, name))
    written, skipped = 0, []
    for p in presets:
        if p['category'] != 'Brush':
            continue
        fname = 'cy_' + snake(p['name'][:-5] if p['name'].endswith('Brush') else p['name']) + '.slang'
        open(os.path.join(OUT, fname), 'w', encoding='utf-8', newline='\n').write(convert(p))
        written += 1
    print(f'{written} brushes written to {OUT}')


if __name__ == '__main__':
    main()
