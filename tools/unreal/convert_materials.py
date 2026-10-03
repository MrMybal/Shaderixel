"""Converts an Unreal material export (tools/unreal/export_materials.py) into
Shaderixel library effects (shaders/effects/<prefix>_<name>.slang).

    python tools/unreal/convert_materials.py export.json [--prefix ue] [--category "Unreal"]

The material graph is translated node by node into HLSL, starting from the
Emissive (or Base Color) and Opacity outputs. Texture object / texture sample
parameters become effect inputs (one pin per texture on graph nodes), scalar and
vector parameters become effect parameters, Custom nodes keep their code (as a
function receiving their inputs). CyTexture system values (CyCanvasSize,
CyInvCanvasSize, CyCanvasDimensions) are provided by Shaderixel. CyTexture
preset contracts found in the export give names, categories, descriptions and
parameter ranges.

Materials using nodes not handled here are listed with the missing node types
(nothing is written for them): send that list, the importer grows from it.
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(ROOT, 'shaders', 'effects')

SYSTEM = {
    'CyCanvasSize': ('float4(ctx.size, ctx.texel)', 4),
    'CyCanvasDimensions': ('ctx.size', 2),
    'CyInvCanvasSize': ('ctx.texel', 2),
}
INPUT_LABEL = {'CyInput': 'Image', 'CyInputTexture': 'Image', 'CyBackground': 'Fond', 'CyForeground': 'Calque', 'CyMask': 'Masque'}
CY_CATEGORY = {'Draw': 'CyTexture Dessin', 'Effect': 'CyTexture Effets', 'Blend': 'CyTexture Fusions', 'Mask': 'CyTexture Masques'}


class Unsupported(Exception):
    pass


def words(name):
    s = re.sub(r'(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])', ' ', name)
    return s.replace('_', ' ').strip()


def ident(s):
    s = re.sub(r'\W', '_', s)
    return s if s and not s[0].isdigit() else '_' + s


def F(v):
    s = '%.7g' % float(v)
    return s if any(c in s for c in '.en') else s + '.0'


def T(n):
    return 'float' if n == 1 else 'float%d' % n


def conv(v, n):
    """Value (expr, comps) as n components (HLSL rules: broadcast scalars, truncate, pad)."""
    e, k = v
    if k == n:
        return e
    if k == 1:
        return '((%s)(%s))' % (T(n), e)
    if k > n:
        return '(%s).%s' % (e, 'xyzw'[:n])
    pad = {(2, 3): ', 0.0', (2, 4): ', 0.0, 1.0', (3, 4): ', 1.0'}[(k, n)]
    return '%s(%s%s)' % (T(n), e, pad)


def prop(node, *names, default=None):
    p = node.get('props', {})
    for n in names:
        if n in p and p[n] is not None:
            return p[n]
    return default


def pname(node):
    v = prop(node, 'parameter_name', default='')
    return str(v)


class Gen:
    def __init__(self, mat, functions):
        self.mat = mat
        self.nodes = mat['nodes']
        self.functions = functions
        self.lines = []
        self.memo = {}
        self.textures = []      # texture parameter names (effect inputs, in order)
        self.params = {}        # name -> ('float'|'color', default, min, max)
        self.customs = []       # helper functions
        self.missing = set()
        self.global_reads = False
        self.base = {}          # node -> full value of multi-output nodes (computed once)

    def once(self, nid, make):
        """Full value of a node computed a single time (shared by its outputs)."""
        if nid not in self.base:
            e, k = make()
            var = 'b_%s' % nid
            self.lines.append('    %s %s = %s;' % (T(k), var, e))
            self.base[nid] = (var, k)
        return self.base[nid]

    def tex_index(self, name):
        if name not in self.textures:
            self.textures.append(name)
        return self.textures.index(name)

    def input(self, node, idx_or_name, default=None):
        """Value connected to an input (by index or name), else default."""
        ins = node.get('inputs', [])
        entry = None
        if isinstance(idx_or_name, int):
            entry = ins[idx_or_name] if idx_or_name < len(ins) else None
        else:
            for e in ins:
                if e.get('name', '').lower() == idx_or_name.lower():
                    entry = e
                    break
        if entry and 'node' in entry:
            return self.value(entry['node'], entry.get('output', ''))
        return default

    def connected(self, node, name):
        return any(e.get('name', '').lower() == name.lower() and 'node' in e for e in node.get('inputs', []))

    def value(self, nid, output=''):
        key = (nid, output)
        if key in self.memo:
            return self.memo[key]
        node = self.nodes[nid]
        v = self.translate(nid, node, output or '')
        # Materialize non-trivial values once.
        if v[0] and not re.fullmatch(r'[\w.]+|\d[\d.e+-]*', v[0]) and not v[0].startswith('TEX:'):
            var = 'v_%s_%s' % (nid, ident(output or 'o'))
            self.lines.append('    %s %s = %s;' % (T(v[1]), var, v[0]))
            v = (var, v[1])
        self.memo[key] = v
        return v

    def channel_out(self, value, output):
        e, k = value
        o = output.upper()
        if o in ('R', 'X'):
            return ('(%s).x' % e, 1)
        if o in ('G', 'Y'):
            return ('(%s).y' % e, 1)
        if o in ('B', 'Z'):
            return ('(%s).z' % e, 1)
        if o in ('A', 'W'):
            return ('(%s).w' % conv(value, 4), 1) if k < 4 else ('(%s).w' % e, 1)
        if o == 'RGB':
            return (conv(value, 3), 3)
        if o == 'RGBA':
            return (conv(value, 4), 4)
        if o == 'RG':
            return (conv(value, 2), 2)
        return value

    def translate(self, nid, n, output):
        c = n['class'].replace('MaterialExpression', '')
        A = lambda name, d=0.0: self.input(n, name, (F(d), 1))
        if c in ('Reroute', 'NamedRerouteUsage', 'NamedRerouteDeclaration'):
            return self.input(n, 0, ('0.0', 1))
        if c == 'TextureCoordinate':
            ut, vt = prop(n, 'u_tiling', default=1.0), prop(n, 'v_tiling', default=1.0)
            return ('UV' if ut == 1 and vt == 1 else '(UV * float2(%s, %s))' % (F(ut), F(vt)), 2)
        if c in ('ScreenPosition', 'ViewSize') or c == 'SceneTexelSize':
            if c == 'ScreenPosition':
                return ('UV', 2)
            return ('ctx.size', 2) if c == 'ViewSize' else ('ctx.texel', 2)
        if c == 'Time':
            return ('0.0', 1)
        if c == 'Constant':
            return (F(prop(n, 'r', default=0)), 1)
        if c == 'Constant2Vector':
            return ('float2(%s, %s)' % (F(prop(n, 'r', default=0)), F(prop(n, 'g', default=0))), 2)
        if c in ('Constant3Vector', 'Constant4Vector'):
            col = prop(n, 'constant', default=[0, 0, 0, 1])
            k = 3 if c == 'Constant3Vector' else 4
            return self.channel_out(('%s(%s)' % (T(k), ', '.join(F(x) for x in col[:k])), k), output)
        if c in ('ScalarParameter', 'StaticBoolParameter'):
            name = pname(n)
            if name in SYSTEM:
                return SYSTEM[name]
            d = prop(n, 'default_value', default=0.0)
            lo, hi = prop(n, 'slider_min', default=0.0), prop(n, 'slider_max', default=0.0)
            self.params.setdefault(name, ('float', [float(d)], float(lo), float(hi)))
            return (ident(name), 1)
        if c == 'VectorParameter':
            name = pname(n)
            if name in SYSTEM:
                return self.channel_out(SYSTEM[name], output)
            d = prop(n, 'default_value', default=[0, 0, 0, 1])
            self.params.setdefault(name, ('color', d, 0, 1))
            v = (ident(name), 4)
            return self.channel_out(v, output) if output else (conv(v, 3), 3)
        if c in ('TextureObjectParameter', 'TextureObject'):
            name = pname(n) or 'Texture%s' % nid
            return ('TEX:%d' % self.tex_index(name), 0)
        if c in ('TextureSample', 'TextureSampleParameter2D'):
            if c == 'TextureSampleParameter2D':
                idx = self.tex_index(pname(n) or 'Texture%s' % nid)
            else:
                t = self.input(n, 'Tex')
                if not t or not t[0].startswith('TEX:'):
                    raise Unsupported('TextureSample sans texture objet')
                idx = int(t[0][4:])
            uvv = self.input(n, 'Coordinates') or self.input(n, 'UVs')
            if uvv:
                self.global_reads = True
            uv = conv(uvv, 2) if uvv else 'UV'
            return self.channel_out(self.once(nid, lambda: ('CyTex(%d, %s)' % (idx, uv), 4)), output or 'RGB')
        binops = {'Add': '+', 'Subtract': '-', 'Multiply': '*', 'Divide': '/'}
        if c in binops:
            a = self.input(n, 'A', (F(prop(n, 'const_a', default=0 if c in ('Add', 'Subtract') else 1)), 1))
            b = self.input(n, 'B', (F(prop(n, 'const_b', default=0 if c in ('Add', 'Subtract') else 1)), 1))
            k = max(a[1], b[1])
            return ('(%s %s %s)' % (conv(a, k), binops[c], conv(b, k)), k)
        if c in ('Min', 'Max'):
            a = self.input(n, 'A', (F(prop(n, 'const_a', default=0)), 1))
            b = self.input(n, 'B', (F(prop(n, 'const_b', default=0)), 1))
            k = max(a[1], b[1])
            return ('%s(%s, %s)' % (c.lower(), conv(a, k), conv(b, k)), k)
        if c == 'Power':
            a = self.input(n, 'Base', ('1.0', 1))
            b = self.input(n, 'Exponent', (F(prop(n, 'const_exponent', default=2)), 1))
            return ('pow(max(%s, 0.0), %s)' % (a[0], conv(b, a[1])), a[1])
        if c == 'LinearInterpolate':
            a = self.input(n, 'A', (F(prop(n, 'const_a', default=0)), 1))
            b = self.input(n, 'B', (F(prop(n, 'const_b', default=1)), 1))
            t = self.input(n, 'Alpha', (F(prop(n, 'const_alpha', default=0.5)), 1))
            k = max(a[1], b[1])
            return ('lerp(%s, %s, %s)' % (conv(a, k), conv(b, k), t[0] if t[1] == 1 else conv(t, k)), k)
        if c == 'Clamp':
            x = self.input(n, 'Input', ('0.0', 1))
            lo = self.input(n, 'Min', (F(prop(n, 'min_default', default=0)), 1))
            hi = self.input(n, 'Max', (F(prop(n, 'max_default', default=1)), 1))
            return ('clamp(%s, %s, %s)' % (x[0], conv(lo, x[1]), conv(hi, x[1])), x[1])
        unary = {'Saturate': 'saturate', 'Abs': 'abs', 'Floor': 'floor', 'Ceil': 'ceil', 'Frac': 'frac', 'Normalize': 'normalize',
                 'SquareRoot': 'sqrt', 'Round': 'round', 'Truncate': 'trunc', 'Sign': 'sign', 'Exponential': 'exp', 'Logarithm2': 'log2'}
        if c in unary:
            x = self.input(n, 0, ('0.0', 1))
            return ('%s(%s)' % (unary[c], x[0]), x[1])
        if c == 'OneMinus':
            x = self.input(n, 0, ('0.0', 1))
            return ('(1.0 - %s)' % x[0], x[1])
        if c in ('Sine', 'Cosine'):
            x = self.input(n, 0, ('0.0', 1))
            period = prop(n, 'period', default=1.0) or 0
            arg = x[0] if not period else '(%s * %s)' % (x[0], F(6.283185307 / period))
            return ('%s(%s)' % ('sin' if c == 'Sine' else 'cos', arg), x[1])
        if c == 'Fmod':
            a, b = self.input(n, 'A', ('0.0', 1)), self.input(n, 'B', ('1.0', 1))
            k = max(a[1], b[1])
            return ('fmod(%s, %s)' % (conv(a, k), conv(b, k)), k)
        if c in ('DotProduct', 'Distance'):
            a, b = self.input(n, 'A', ('0.0', 1)), self.input(n, 'B', ('0.0', 1))
            k = max(a[1], b[1])
            return ('%s(%s, %s)' % ('dot' if c == 'DotProduct' else 'distance', conv(a, k), conv(b, k)), 1)
        if c == 'Length':
            x = self.input(n, 0, ('0.0', 1))
            return ('length(%s)' % x[0], 1)
        if c == 'Step':
            y, x = self.input(n, 'Y', (F(prop(n, 'const_y', default=0)), 1)), self.input(n, 'X', (F(prop(n, 'const_x', default=0)), 1))
            k = max(y[1], x[1])
            return ('step(%s, %s)' % (conv(y, k), conv(x, k)), k)
        if c == 'SmoothStep':
            lo, hi = self.input(n, 'Min', (F(prop(n, 'const_min', default=0)), 1)), self.input(n, 'Max', (F(prop(n, 'const_max', default=1)), 1))
            x = self.input(n, 'Value', (F(prop(n, 'const_value', default=0)), 1))
            k = max(lo[1], hi[1], x[1])
            return ('smoothstep(%s, %s, %s)' % (conv(lo, k), conv(hi, k), conv(x, k)), k)
        if c == 'ComponentMask':
            x = self.input(n, 0, ('0.0', 1))
            sw = ''.join(ch for ch, flag in zip('xyzw', [prop(n, 'r'), prop(n, 'g'), prop(n, 'b'), prop(n, 'a')]) if flag) or 'x'
            return ('(%s).%s' % (conv(x, 4), sw), len(sw))
        if c == 'AppendVector':
            a, b = self.input(n, 'A', ('0.0', 1)), self.input(n, 'B', ('0.0', 1))
            k = min(a[1] + b[1], 4)
            return ('%s(%s, %s)' % (T(k), a[0], conv(b, k - a[1])), k)
        if c == 'If':
            a, b = self.input(n, 'A', ('0.0', 1)), self.input(n, 'B', (F(prop(n, 'const_b', default=0)), 1))
            gt, eq, lt = self.input(n, 'A > B', ('0.0', 1)), self.input(n, 'A == B', None), self.input(n, 'A < B', ('0.0', 1))
            k = max(gt[1], lt[1], eq[1] if eq else 1)
            s = '((%s > %s) ? %s : %s)' % (conv(a, 1), conv(b, 1), conv(gt, k), conv(lt, k))
            if eq:
                s = '((%s == %s) ? %s : %s)' % (conv(a, 1), conv(b, 1), conv(eq, k), s)
            return (s, k)
        if c == 'Desaturation':
            x = self.input(n, 0, ('0.0', 1))
            frac = self.input(n, 'Fraction', ('1.0', 1))
            col = conv(x, 3)
            return ('lerp(%s, (float3)dot(%s, float3(0.3, 0.59, 0.11)), %s)' % (col, col, conv(frac, 1)), 3)
        if c == 'Panner':
            return self.input(n, 'Coordinate', ('UV', 2))
        if c == 'MaterialFunctionCall':
            f = n.get('function', '') or str(prop(n, 'material_function', default=''))
            base = f.rsplit('.', 1)[-1]
            if base == 'ScreenAlignedPixelToPixelUVs':
                return ('UV', 2)
            if base == 'DitherTemporalAA':
                return self.input(n, 0, ('0.0', 1))
            raise Unsupported('fonction de matériau ' + base)
        if c == 'Custom':
            return self.channel_out(self.once(nid, lambda: self.custom(nid, n, output)), output)
        raise Unsupported(c)

    def custom(self, nid, n, output):
        code = str(prop(n, 'code', default=''))
        out_type = str(prop(n, 'output_type', default='CMOT_Float4'))
        k_out = {'CMOT_FLOAT1': 1, 'CMOT_FLOAT2': 2, 'CMOT_FLOAT3': 3, 'CMOT_FLOAT4': 4}.get(out_type.upper().replace('ECUSTOMMATERIALOUTPUTTYPE.', ''), 4)
        params, args = [], []
        names = [e.get('name', '') for e in n.get('inputs', [])]
        custom_inputs = prop(n, 'inputs', default=[]) or []
        for ci in custom_inputs:
            nm = str(ci.get('input_name', '')) if isinstance(ci, dict) else ''
            if nm and nm not in names:
                names.append(nm)
        for nm in names:
            if not nm:
                continue
            if nm in SYSTEM:
                e, k = SYSTEM[nm]
            else:
                v = self.input(n, nm)
                if v is None:
                    if nm == 'UV':
                        v = ('UV', 2)
                    else:
                        v = ('0.0', 1)
                e, k = v
            if e.startswith('TEX:'):
                params.append('uint %s' % ident(nm))
                args.append(e[4:])
                self.global_reads = True
                continue
            params.append('%s %s' % (T(k), ident(nm)))
            args.append(e)
        fname = 'Custom_%s' % nid
        self.customs.append('%s %s(%s)\n{\n%s\n}\n' % (T(k_out), fname, ', '.join(params), code.replace('\r\n', '\n')))
        self.global_reads = True
        return ('%s(%s)' % (fname, ', '.join(args)), k_out)


def contracts_by_material(data):
    """CyTexture preset contracts: material path -> contract asset."""
    out = {}
    for a in data.get('assets', []):
        mat = a.get('props', {}).get('material')
        if isinstance(mat, dict) and 'object' in mat:
            out[mat['object']] = a
    return out


def convert(mat, contract, prefix, category, functions):
    g = Gen(mat, functions)
    outs = mat.get('outputs', {})
    color_key = 'emissive' if 'emissive' in outs else 'base_color' if 'base_color' in outs else None
    if not color_key:
        raise Unsupported('aucune sortie Emissive / Base Color reliée')
    c = outs[color_key]
    color = g.value(c['node'], c.get('output', ''))
    alpha = None
    for k in ('opacity', 'opacity_mask'):
        if k in outs:
            alpha = g.value(outs[k]['node'], outs[k].get('output', ''))
            break
    if g.missing:
        raise Unsupported(', '.join(sorted(g.missing)))
    result = 'float4(%s, %s)' % (conv(color, 3), conv(alpha, 1) if alpha else '1.0')

    name = mat['name']
    cat, desc = category, ''
    ranges = {}
    if contract:
        p = contract.get('props', {})
        name = contract['name']
        for pre in ('CT_Cy', 'CT_'):
            if name.startswith(pre):
                name = name[len(pre):]
        if '_' in name:
            kind, name = name.split('_', 1)
            cat = CY_CATEGORY.get(kind, cat)
        desc = str(p.get('description', '') or '')
        for e in p.get('exposed_parameters', []) or []:
            if isinstance(e, dict) and e.get('name'):
                ranges[str(e['name'])] = (e.get('minimum'), e.get('maximum'))
    for pre in ('M_Cy', 'M_'):
        if name.startswith(pre):
            name = name[len(pre):]
    name = name.replace('Advanced', 'Advanced ', 1) if name.startswith('Advanced') else name

    L = ['//@name ' + words(name).replace('  ', ' '), '//@category ' + cat]
    if g.textures:
        for t in g.textures:
            L.append('//@input ' + INPUT_LABEL.get(t, words(t.replace('Texture', '')) or t))
    else:
        L.append('//@generator')
    L.append('//@halo full' if g.global_reads else '//@halo 0')
    for pn, (kind, d, lo, hi) in g.params.items():
        if pn in ranges and ranges[pn][0] is not None and ranges[pn][1] is not None and ranges[pn][1] > ranges[pn][0]:
            lo, hi = ranges[pn]
        if kind == 'float':
            if not hi > lo:
                lo, hi = (min(0.0, d[0]), max(1.0, d[0] * 2 if d[0] > 0 else 1.0))
            L.append('//@param %s float %s %s %s "%s"' % (ident(pn), F(d[0]), F(lo), F(hi), words(pn)))
        else:
            d = (list(d) + [0, 0, 0, 1])[:4]
            L.append('//@param %s color %s "%s"' % (ident(pn), ' '.join(F(x) for x in d), words(pn)))
    L.append('')
    L.append('// Imported from Unreal material %s%s' % (mat['path'], (': ' + desc) if desc else ''))
    L.append('float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }  // unconnected = white, as Unreal')
    L.append('#define Texture2DSample(T, S, UVS) CyTex(T, UVS)')
    L.append('#define Texture2DSampleLevel(T, S, UVS, LOD) CyTex(T, UVS)')
    L.append('')
    for f in g.customs:
        L.append(f)
    L.append('float4 Effect(EffectContext ctx)')
    L.append('{')
    L.append('    float2 UV = ctx.uv;')
    L.extend(g.lines)
    L.append('    return %s;' % result)
    L.append('}')
    return name, '\n'.join(L) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('export')
    ap.add_argument('--prefix', default='ue')
    ap.add_argument('--category', default='Unreal')
    ap.add_argument('--out', default=OUT)
    args = ap.parse_args()
    data = json.load(open(args.export, encoding='utf-8'))
    contracts = contracts_by_material(data)
    functions = {f['path']: f for f in data.get('functions', [])}
    written, failed = [], []
    for mat in data.get('assets', []):
        if mat.get('type') != 'Material':
            continue
        contract = contracts.get(mat['path'])
        prefix = 'cy' if contract or mat['path'].startswith('/CyTexture/') else args.prefix
        try:
            name, text = convert(mat, contract, prefix, args.category, functions)
        except Unsupported as e:
            failed.append((mat['path'], str(e)))
            continue
        fname = '%s_%s.slang' % (prefix, re.sub(r'[^a-z0-9]+', '_', words(name).lower()).strip('_'))
        path = os.path.join(args.out, fname)
        if os.path.exists(path) and prefix == 'cy' and not name.startswith('Advanced'):
            continue  # presets already imported from CyTexture's generator source
        open(path, 'w', encoding='utf-8', newline='\n').write(text)
        written.append(fname)
    print('%d effects written to %s' % (len(written), args.out))
    if failed:
        print('%d materials not converted:' % len(failed))
        reasons = {}
        for p, r in failed:
            reasons.setdefault(r, []).append(p.rsplit('/', 1)[-1])
        for r, ps in sorted(reasons.items(), key=lambda x: -len(x[1])):
            print('  %s (%d): %s' % (r, len(ps), ', '.join(ps[:6]) + (' ...' if len(ps) > 6 else '')))


if __name__ == '__main__':
    main()
