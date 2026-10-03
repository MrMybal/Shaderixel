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
CY_CATEGORY = {'Draw': 'Dessin', 'Effect': 'Traitements', 'Blend': 'Fusions', 'Mask': 'Masques'}


# Implicit float4 -> float3 truncations Unreal accepts and Slang does not (exact code fragments).
CODE_FIXUPS = [
    ('OffsetCoord + (Dir*DistanceRadial*(0.5*Chroma)) ) * saturate(1.0-abs(3*(u/OffsetStep)-float3(0.0,1.0,2.0)))',
     'OffsetCoord + (Dir*DistanceRadial*(0.5*Chroma)) ).rgb * saturate(1.0-abs(3*(u/OffsetStep)-float3(0.0,1.0,2.0)))'),
    ('RandomSamp += Texture2DSample(Tex,TexSampler,ScenePixels);', 'RandomSamp += Texture2DSample(Tex,TexSampler,ScenePixels).xy;'),
    ('float TempAARotation = RandomSamp;', 'float TempAARotation = RandomSamp.x;'),
    ('float TempAADistance = Spiral*RandomSamp;', 'float TempAADistance = Spiral*RandomSamp.x;'),
    # Engine functions SpiralBlur-Texture and AbberatedBlur-Texture.
    ('float3 newSample=Texture2DSample(Tex,TexSampler,NewUV);', 'float3 newSample=Texture2DSample(Tex,TexSampler,NewUV).rgb;'),
    ('float3 CurrentAbberationValue=saturate(1.0-abs(3*(i/Steps)-float4(0.0,1.0,2.0,3.0)));',
     'float3 CurrentAbberationValue=saturate(1.0-abs(3*(i/Steps)-float4(0.0,1.0,2.0,3.0))).xyz;'),
    ('CurColor += Texture2DSample(Tex,TexSampler,NewUV)*distpow;', 'CurColor += Texture2DSample(Tex,TexSampler,NewUV).rgb*distpow;'),
]


class Unsupported(Exception):
    pass


def words(name):
    s = re.sub(r'(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])', ' ', name)
    return s.replace('_', ' ').strip()


def ident(s):
    s = re.sub(r'\W', '_', s)
    return s if s and not s[0].isdigit() else '_' + s


RESERVED = set("""abs acos all any asin atan atan2 ceil clamp cos cosh cross ddx ddy degrees determinant distance dot exp exp2 floor fmod frac
length lerp log log2 max min mul normalize pow radians reflect refract round rsqrt saturate sign sin sinh smoothstep sqrt step tan tanh
trunc float float2 float3 float4 int uint bool half true false return in out inout sample Sample Input InputUV Prev PrevUV HasInput ctx UV
View CyTex Effect""".split())


def pident(s):
    """HLSL name of a material parameter (prefixed when it would shadow an intrinsic)."""
    i = ident(s)
    return 'p_' + i if i in RESERVED else i


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


class Shared:
    """State shared by a material and the function graphs inlined into it."""
    def __init__(self):
        self.lines = []
        self.textures = []      # texture parameter names (effect inputs, in order)
        self.params = {}        # name -> ('float'|'color', default, min, max)
        self.customs = []       # helper functions
        self.missing = set()
        self.global_reads = False
        self.counter = 0
        self.read = set()       # id() of input entries consumed by the translation
        self.visited = []       # (generator, node id) translated


class Gen:
    def __init__(self, mat, functions, shared=None, bindings=None, prefix=''):
        self.mat = mat
        self.nodes = mat['nodes']
        self.functions = functions
        self.S = shared or Shared()
        self.bindings = bindings or {}   # function inputs: name -> value thunk (inlined function graphs)
        self.prefix = prefix
        self.memo = {}
        self.base = {}          # node -> full value of multi-output nodes (computed once)

    # Shared containers under their historical names.
    lines = property(lambda self: self.S.lines)
    textures = property(lambda self: self.S.textures)
    params = property(lambda self: self.S.params)
    customs = property(lambda self: self.S.customs)
    missing = property(lambda self: self.S.missing)

    @property
    def global_reads(self):
        return self.S.global_reads

    @global_reads.setter
    def global_reads(self, v):
        self.S.global_reads = v

    def once(self, nid, make):
        """Full value of a node computed a single time (shared by its outputs)."""
        if nid not in self.base:
            e, k = make()
            var = 'b_%s%s' % (self.prefix, nid)
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
            wanted = [x.lower() for x in idx_or_name.split('|')]  # "Exponent|Exp": aliases
            for e in ins:
                if e.get('name', '').lower() in wanted:
                    entry = e
                    break
            # Unnamed main input (exported as "None"): the first slot.
            if entry is None and ins and ins[0].get('name', '') in ('None', '') and wanted[0] in ('input', 'value', 'x', 'coordinate', 'in'):
                entry = ins[0]
        if entry and 'node' in entry:
            self.S.read.add(id(entry))
            return self.value(entry['node'], entry.get('output', ''))
        return default

    def check_inputs(self):
        """Every connection of a translated node must have been read (no silent drop)."""
        # Inputs deliberately not reproduced: mip bias, noise filter width (anti-aliasing), depth fade distance (no scene depth in 2D).
        ignored = {'apply view mipbias', 'filterwidth', 'fadedistance'}
        for gen, nid in self.S.visited:
            node = gen.nodes[nid]
            for e in node.get('inputs', []):
                if 'node' in e and id(e) not in self.S.read and e.get('name', '').lower() not in ignored:
                    raise Unsupported('entrée non traduite : %s.%s' % (node['class'].replace('MaterialExpression', ''), e.get('name', '')))

    def connected(self, node, name):
        return any(e.get('name', '').lower() == name.lower() and 'node' in e for e in node.get('inputs', []))

    def value(self, nid, output=''):
        key = (nid, output)
        if key in self.memo:
            return self.memo[key]
        node = self.nodes[nid]
        self.S.visited.append((self, nid))
        v = self.translate(nid, node, output or '')
        # Materialize non-trivial values once.
        if v[0] and not re.fullmatch(r'[\w.]+|\d[\d.e+-]*', v[0]) and not v[0].startswith('TEX:'):
            var = 'v_%s%s_%s' % (self.prefix, nid, ident(output or 'o'))
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
        if c == 'FunctionInput':
            return self.function_input(n)
        if c == 'FunctionOutput':
            return self.input(n, 0, ('0.0', 1))
        if c == 'Noise':
            return self.noise(n)
        if c == 'DepthFade':
            # Scene depth does not exist in 2D: the opacity passes through.
            return self.input(n, 'InOpacity') or self.input(n, 'Opacity') or (F(prop(n, 'opacity_default', default=1.0)), 1)
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
        if c in ('StaticSwitch', 'StaticSwitchParameter'):
            # Static switch: a runtime branch on the switch value (parameter or input).
            tv, fv = self.input(n, 'True', ('0.0', 1)), self.input(n, 'False', ('0.0', 1))
            k = max(tv[1], fv[1])
            if c == 'StaticSwitchParameter':
                name = pname(n)
                self.params.setdefault(name, ('float', [1.0 if prop(n, 'default_value', default=False) else 0.0], 0.0, 1.0))
                cond = pident(name)
            else:
                cv = self.input(n, 'Value', ('1.0' if prop(n, 'default_value', default=False) else '0.0', 1))
                cond = conv(cv, 1)
            return ('((%s > 0.5) ? %s : %s)' % (cond, conv(tv, k), conv(fv, k)), k)
        if c == 'StaticBool':
            return ('1.0' if prop(n, 'value', default=False) else '0.0', 1)
        if c in ('ScalarParameter', 'StaticBoolParameter'):
            name = pname(n)
            if name in SYSTEM:
                return SYSTEM[name]
            d = prop(n, 'default_value', default=0.0)
            lo, hi = prop(n, 'slider_min', default=0.0), prop(n, 'slider_max', default=0.0)
            self.params.setdefault(name, ('float', [float(d)], float(lo), float(hi)))
            return (pident(name), 1)
        if c == 'VectorParameter':
            name = pname(n)
            if name in SYSTEM:
                return self.channel_out(SYSTEM[name], output)
            d = prop(n, 'default_value', default=[0, 0, 0, 1])
            self.params.setdefault(name, ('color', d, 0, 1))
            v = (pident(name), 4)
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
            b = self.input(n, 'Exponent|Exp', (F(prop(n, 'const_exponent', default=2)), 1))
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
            if a[1] >= 4:
                return a  # nothing left to append to
            k = min(a[1] + b[1], 4)
            return ('%s(%s, %s)' % (T(k), a[0], conv(b, k - a[1])), k)
        if c == 'SphereMask':
            a, b = self.input(n, 'A', ('UV', 2)), self.input(n, 'B', ('float2(0.5, 0.5)', 2))
            r = self.input(n, 'Radius', (F(prop(n, 'attenuation_radius', default=256.0)), 1))
            h = self.input(n, 'Hardness', (F(prop(n, 'hardness_percent', default=100.0) / 100.0), 1))
            k = max(a[1], b[1])
            return ('saturate((1.0 - distance(%s, %s) / max(%s, 1e-5)) / max(1.0 - %s, 1e-5))' % (conv(a, k), conv(b, k), conv(r, 1), conv(h, 1)), 1)
        if c == 'RotateAboutAxis':
            # Offset from Position to Position rotated around the axis (angle in turns / Period).
            axis = conv(self.input(n, 'NormalizedRotationAxis', ('float3(0.0, 0.0, 1.0)', 3)), 3)
            ang = conv(self.input(n, 'RotationAngle', ('0.0', 1)), 1)
            pivot = conv(self.input(n, 'PivotPoint', ('float3(0.0, 0.0, 0.0)', 3)), 3)
            pos = conv(self.input(n, 'Position', ('float3(UV, 0.0)', 3)), 3)
            period = prop(n, 'period', default=1.0) or 1.0
            self.S.counter += 1
            fn = 'RotateAboutAxis_%d' % self.S.counter
            self.customs.append('float3 %s(float3 axis, float angle, float3 pivot, float3 pos)\n{\n'
                                '    float3 c = pivot + axis * dot(axis, pos - pivot);\n'
                                '    float3 u = pos - c, v = cross(axis, u);\n'
                                '    float s, co;\n    sincos(angle * %s, s, co);\n'
                                '    return c + u * co + v * s - pos;\n}\n' % (fn, F(6.283185307 / period)))
            return ('%s(%s, %s, %s, %s)' % (fn, axis, ang, pivot, pos), 3)
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
            if isinstance(f, dict):
                f = f.get('object', '')
            base = f.rsplit('.', 1)[-1]
            # Screen-space helpers: a document pixel is a screen pixel here.
            if base == 'ScreenAlignedPixelToPixelUVs':
                return ('UV', 2)
            if base == 'DitherTemporalAA':
                return self.input(n, 0, ('0.0', 1))
            return self.call_function(nid, n, f, base, output)
        if c == 'Custom':
            return self.channel_out(self.once(nid, lambda: self.custom(nid, n, output)), output)
        raise Unsupported(c)

    def call_function(self, nid, n, path, base, output):
        """Inlines a material function graph (exported with its nodes)."""
        func = self.functions.get(path)
        if not func or not func.get('nodes'):
            raise Unsupported('fonction de matériau ' + base + ' (graphe non exporté : relancez l\'export)')
        fnodes = func['nodes']
        outs = [o for o in func.get('outputs', []) if o in fnodes]
        if not outs:
            raise Unsupported('fonction de matériau ' + base + ' sans sortie')
        def out_name(o):
            return str(prop(fnodes[o], 'output_name', default='') or '')
        outs.sort(key=lambda o: (prop(fnodes[o], 'sort_priority', default=0) or 0, out_name(o)))
        target = outs[0]
        if output:
            for o in outs:
                if out_name(o).lower() == output.lower():
                    target = o
        # Inputs of the call, by function input name (lazy: evaluated in this graph).
        bindings = {}
        for e in n.get('inputs', []):
            if 'node' in e:
                self.S.read.add(id(e))  # handed to the function (which may not use every input)
                bindings[e.get('name', '').lower()] = (lambda e=e: self.value(e['node'], e.get('output', '')))
        self.S.counter += 1
        child = Gen(func, self.functions, self.S, bindings, '%sf%d_' % (self.prefix, self.S.counter))
        out_node = fnodes[target]
        if not any('node' in e for e in out_node.get('inputs', [])):
            # Function graphs exported without their connections would silently give 0.
            raise Unsupported('fonction de matériau ' + base + ' : graphe interne non exporté (relancez l\'export)')
        return child.input(out_node, 0, ('0.0', 1))

    def function_input(self, n):
        name = str(prop(n, 'input_name', default='') or '').lower()
        for e in n.get('inputs', []):
            if e.get('name', '').lower() == 'preview':
                self.S.read.add(id(e))  # used only when the call leaves the input unconnected
        if name in self.bindings:
            return self.bindings[name]()
        # Unconnected: the function's preview value (graph or constant).
        pv = self.input(n, 'Preview')
        if pv is not None:
            return pv
        kind = str(prop(n, 'input_type', default='FUNCTION_INPUT_SCALAR')).upper()
        v = prop(n, 'preview_value', default=[0, 0, 0, 0])
        if isinstance(v, dict):
            v = [v.get('x', 0), v.get('y', 0), v.get('z', 0), v.get('w', 0)]
        elif not isinstance(v, list):
            v = [v, 0, 0, 0]
        k = 4 if 'VECTOR4' in kind else 3 if 'VECTOR3' in kind else 2 if 'VECTOR2' in kind else 1
        if 'TEXTURE' in kind:
            raise Unsupported('entrée texture de fonction non reliée')
        return ('%s(%s)' % (T(k), ', '.join(F(x) for x in v[:k])) if k > 1 else F(v[0]), k)

    def noise(self, n):
        """Unreal's Noise node approximated by fractal gradient noise (same parameters)."""
        pos = self.input(n, 'Position|World Position')
        p = conv(pos, 3) if pos else 'float3(UV * ctx.size / 64.0, 0.0)'
        scale = F(prop(n, 'scale', default=1.0))
        levels = int(prop(n, 'levels', default=6) or 1)
        lo, hi = F(prop(n, 'output_min', default=-1.0)), F(prop(n, 'output_max', default=1.0))
        lscale = F(prop(n, 'level_scale', default=2.0))
        turb = 1.0 if prop(n, 'turbulence', default=True) else 0.0
        fn = str(prop(n, 'noise_function', default='')).upper()
        cell = 'VORONOI' in fn
        self.S.counter += 1
        name = 'Noise_%s%d' % (self.prefix, self.S.counter)
        body = (
            'float h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }\n'
            if False else '')
        self.customs.append("""float %(name)s_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float %(name)s_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
%(core)s
}
float %(name)s(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < %(levels)d; ++l)
    {
        float v = %(name)s_one(p);
        v = %(turb)s ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= %(lscale)s; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}
""" % {'name': name, 'levels': max(1, min(levels, 10)), 'turb': 'true' if turb else 'false', 'lscale': lscale,
       'core': ('    float d = 8.0;\n    [unroll] for (int z = -1; z <= 1; ++z) [unroll] for (int y = -1; y <= 1; ++y) [unroll] for (int x = -1; x <= 1; ++x)\n'
                '    { float3 o = float3(x, y, z); float3 r = o + float3(%(n)s_h(i + o), %(n)s_h(i + o + 17.0), %(n)s_h(i + o + 31.0)) - f; d = min(d, dot(r, r)); }\n'
                '    return sqrt(d) * 2.0 - 1.0;' % {'n': name}) if cell else
               ('    float3 u = f * f * (3.0 - 2.0 * f);\n'
                '    float n000 = %(n)s_h(i), n100 = %(n)s_h(i + float3(1, 0, 0)), n010 = %(n)s_h(i + float3(0, 1, 0)), n110 = %(n)s_h(i + float3(1, 1, 0));\n'
                '    float n001 = %(n)s_h(i + float3(0, 0, 1)), n101 = %(n)s_h(i + float3(1, 0, 1)), n011 = %(n)s_h(i + float3(0, 1, 1)), n111 = %(n)s_h(i + 1.0);\n'
                '    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);\n'
                '    return v * 2.0 - 1.0;' % {'n': name})})
        return ('lerp(%s, %s, saturate(%s(%s * %s) * 0.5 + 0.5))' % (lo, hi, name, p, scale), 1)

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
                e, k = SYSTEM[nm]  # provided by Shaderixel: the connected parameter is not needed
                for entry in n.get('inputs', []):
                    if entry.get('name') == nm:
                        self.S.read.add(id(entry))
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
        fname = 'Custom_%s%s' % (self.prefix, nid)
        # Unreal pads/truncates returned vectors implicitly: returns go through SXRet
        # (float4); self-referencing declarations ("float a = a = x") are simplified.
        body = code.replace('\r\n', '\n')
        for old, new in CODE_FIXUPS:
            body = body.replace(old, new)
        body = re.sub(r'\b((?:float[234]?|int|half[234]?)\s+(\w+))\s*=\s*\2\s*=', r'\1 =', body)
        body = re.sub(r'\breturn\b([^;]+);', r'return SXRet(\1);', body)
        # Locals declared without a value start at zero (Unreal's compiler leaves them at 0 in
        # practice and some engine functions rely on it; Slang does not initialize them).
        body = re.sub(r'(^|[;{}\n]\s*)((?:float[234]?|int|uint|half[234]?|bool)\s+)(\w+)\s*;', r'\1\2\3 = (\2)0;', body)
        self.customs.append('float4 %s(%s)\n{\n%s\n}\n' % (fname, ', '.join(params), body))
        self.global_reads = True
        return ('(%s(%s))%s' % (fname, ', '.join(args), {1: '.x', 2: '.xy', 3: '.xyz', 4: ''}[k_out]), k_out)


def contracts_by_material(data):
    """CyTexture preset contracts: material path -> contract asset."""
    out = {}
    for a in data.get('assets', []):
        mat = a.get('props', {}).get('material')
        if isinstance(mat, dict) and 'object' in mat:
            out[mat['object']] = a
    return out


def material_and_overrides(target, by_path):
    """A material, or the parent material of an instance with its parameter values."""
    overrides = {}
    seen = 0
    while target and target.get('type') == 'MaterialInstanceConstant' and seen < 8:
        p = target.get('props', {})
        for key in ('scalar_parameter_values', 'vector_parameter_values'):
            for e in p.get(key, []) or []:
                try:
                    name = str(e['parameter_info']['name'])
                    overrides.setdefault(name, e['parameter_value'])
                except Exception:
                    pass
        parent = p.get('parent')
        target = by_path.get(parent.get('object')) if isinstance(parent, dict) else None
        seen += 1
    return target, overrides


def convert(mat, contract, category, functions, overrides, display_name=None):
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
    g.check_inputs()
    if g.missing:
        raise Unsupported(', '.join(sorted(g.missing)))
    result = 'float4(%s, %s)' % (conv(color, 3), conv(alpha, 1) if alpha else '1.0')

    name = display_name or mat['name']
    cat, desc = category, ''
    exposed = {}
    if contract:
        p = contract.get('props', {})
        desc = str(p.get('description', '') or '')
        for e in p.get('exposed_parameters', []) or []:
            if isinstance(e, dict) and e.get('name'):
                exposed[str(e['name'])] = e
    for pre in ('M_Cy', 'MI_Cy', 'M_', 'MI_'):
        if name.startswith(pre):
            name = name[len(pre):]
    if '_' in name and name.split('_', 1)[0] in CY_CATEGORY:
        kind, name = name.split('_', 1)
        cat = CY_CATEGORY[kind]
    if name.startswith('Advanced'):
        name = 'Advanced ' + name[len('Advanced'):]

    L = ['//@name ' + words(name).replace('  ', ' '), '//@category ' + cat]
    if g.textures:
        for t in g.textures:
            L.append('//@input ' + INPUT_LABEL.get(t, words(t.replace('Texture', '')) or t))
    else:
        L.append('//@generator')
    L.append('//@halo full' if g.global_reads else '//@halo 0')
    for pn, (kind, d, lo, hi) in g.params.items():
        e = exposed.get(pn)
        if kind == 'float':
            dv = float(overrides.get(pn, e['scalar'] if e and isinstance(e.get('scalar'), (int, float)) else d[0]))
            if e and e.get('maximum') is not None and e.get('minimum') is not None and e['maximum'] > e['minimum']:
                lo, hi = e['minimum'], e['maximum']
            if not hi > lo:
                lo, hi = (min(0.0, dv), max(1.0, dv * 2 if dv > 0 else 1.0))
            L.append('//@param %s float %s %s %s "%s"' % (pident(pn), F(dv), F(lo), F(hi), words(pn)))
        else:
            dv = overrides.get(pn) or (e.get('vector') if e and isinstance(e.get('vector'), list) else d)
            if isinstance(dv, dict):
                dv = [dv.get(k, 0) for k in ('r', 'g', 'b', 'a')]
            dv = (list(dv) + [0, 0, 0, 1])[:4]
            L.append('//@param %s color %s "%s"' % (pident(pn), ' '.join(F(x) for x in dv), words(pn)))
    L.append('')
    L.append('// Imported from Unreal material %s%s' % (mat['path'], (': ' + desc) if desc else ''))
    L.append('float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }  // unconnected = white, as Unreal')
    L.append('#define Texture2DSample(T, S, UVS) CyTex(T, UVS)')
    L.append('#define Texture2DSampleLevel(T, S, UVS, LOD) CyTex(T, UVS)')
    L.append('static EffectContext ctx;  // for helper functions (noise) reading the document size')
    L.append('// Unreal view values and helpers used by Custom nodes (a document pixel = a screen pixel).')
    L.append('struct SXView { float4 ViewSizeAndInvSize; float4 BufferSizeAndInvSize; float4 TemporalAAParams; };')
    L.append('static SXView View;')
    L.append('static SXView ResolvedView;')
    L.append('float2 ViewportUVToBufferUV(float2 uv) { return uv; }')
    L.append('float2 BufferUVToViewportUV(float2 uv) { return uv; }')
    L.append('float4 SXRet(float v) { return float4(v, v, v, 1.0); }')
    L.append('float4 SXRet(float2 v) { return float4(v, 0.0, 1.0); }')
    L.append('float4 SXRet(float3 v) { return float4(v, 1.0); }')
    L.append('float4 SXRet(float4 v) { return v; }')
    L.append('')
    for f in g.customs:
        L.append(f)
    L.append('float4 Effect(EffectContext context)')
    L.append('{')
    L.append('    ctx = context;')
    L.append('    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);')
    L.append('    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;')
    L.append('    View.TemporalAAParams = 0;  // no temporal jitter in a document')
    L.append('    ResolvedView = View;')
    L.append('    float2 UV = ctx.uv;')
    L.extend(g.lines)
    L.append('    return %s;' % result)
    L.append('}')
    return name, cat, '\n'.join(L) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('export')
    ap.add_argument('--prefix', default='ue')
    ap.add_argument('--category', default='Unreal')
    ap.add_argument('--out', default=OUT)
    ap.add_argument('--all-cytexture', action='store_true', help='also the CyTexture presets already imported from its source')
    args = ap.parse_args()
    data = json.load(open(args.export, encoding='utf-8'))
    by_path = {a['path']: a for a in data.get('assets', [])}
    functions = {f['path']: f for f in data.get('functions', [])}
    jobs = []  # (target asset, contract)
    targeted = set()
    for a in data.get('assets', []):
        m = a.get('props', {}).get('material')
        if isinstance(m, dict) and m.get('object') in by_path:
            # CyTexture contract. Presets without "Advanced" come from CyTexture's generator
            # source and are already imported (tools/cytexture), unless --all-cytexture.
            cname = a['name'].split('_', 2)[-1]
            if not cname.startswith('Advanced') and not args.all_cytexture:
                targeted.add(m['object'])
                continue
            jobs.append((by_path[m['object']], a))
            targeted.add(m['object'])
    for a in data.get('assets', []):
        if a.get('type') == 'Material' and a['path'] not in targeted:
            jobs.append((a, None))
    # Previous outputs of this converter are replaced (other effects are left alone).
    for fn in os.listdir(args.out):
        fp = os.path.join(args.out, fn)
        if fn.endswith('.slang') and 'Imported from Unreal material' in open(fp, encoding='utf-8', errors='ignore').read(4000):
            os.remove(fp)
    written, failed = [], []
    for target, contract in jobs:
        mat, overrides = material_and_overrides(target, by_path)
        if not mat or not mat.get('nodes') and not mat.get('outputs'):
            failed.append((target['path'], 'matériau parent introuvable'))
            continue
        try:
            name, cat, text = convert(mat, contract, args.category, functions, overrides, target['name'])
        except Unsupported as e:
            failed.append((target['path'], str(e)))
            continue
        kind = {v: k for k, v in CY_CATEGORY.items()}.get(cat)
        prefix = ('cy_' + kind.lower()) if kind else args.prefix
        fname = '%s_%s.slang' % (prefix, re.sub(r'[^a-z0-9]+', '_', words(name).lower()).strip('_'))
        open(os.path.join(args.out, fname), 'w', encoding='utf-8', newline='\n').write(text)
        written.append(fname)
    print('%d effects written to %s' % (len(written), args.out))
    if failed:
        print('%d not converted:' % len(failed))
        reasons = {}
        for p, r in failed:
            reasons.setdefault(r, []).append(p.rsplit('/', 1)[-1].split('.')[0])
        for r, ps in sorted(reasons.items(), key=lambda x: -len(x[1])):
            print('  %s (%d): %s' % (r, len(ps), ', '.join(ps[:6]) + (' ...' if len(ps) > 6 else '')))


if __name__ == '__main__':
    main()
