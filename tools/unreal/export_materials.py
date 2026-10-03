"""Unreal Editor script: exports materials to JSON for Shaderixel (read-only, saves nothing).

Run it in the Unreal Editor (Python Editor Script Plugin enabled), from the
Output Log console (Cmd: Python) or Tools > Execute Python Script:

    py "<Shaderixel>/tools/unreal/export_materials.py" --path /CyTexture/Editor/LayerPresets --out "<Shaderixel>/tools/unreal/export.json"

Without --path, the assets selected in the Content Browser are exported.
Each material is walked from its outputs (Emissive, Base Color, Opacity, Opacity
Mask): every reached expression is written with its class, editable properties
(read through the class documentation, so any expression type is covered), its
connected inputs (source node + output name) and its position. Material
function calls also carry the function's own graph. Other assets found under
--path (CyTexture preset contracts, material instances) are written with their
properties so the importer gets names, categories and parameter ranges.
Then, in Shaderixel:

    python tools/unreal/convert_materials.py tools/unreal/export.json
"""
import argparse
import json
import re
import sys

import unreal

MEL = unreal.MaterialEditingLibrary
OUTPUTS = [
    ('emissive', unreal.MaterialProperty.MP_EMISSIVE_COLOR),
    ('base_color', unreal.MaterialProperty.MP_BASE_COLOR),
    ('opacity', unreal.MaterialProperty.MP_OPACITY),
    ('opacity_mask', unreal.MaterialProperty.MP_OPACITY_MASK),
]


def property_names(obj):
    """Editor property names listed in the generated class documentation."""
    names = []
    for cls in type(obj).__mro__:
        doc = cls.__doc__ or ''
        for m in re.finditer(r'^\s*-\s*``(\w+)``\s*\(', doc, re.M):
            if m.group(1) not in names:
                names.append(m.group(1))
    return names


def to_json(v, depth=0):
    if depth > 6:
        return str(v)
    if v is None or isinstance(v, (bool, int, float, str)):
        return v
    if isinstance(v, (unreal.Name, unreal.Text)):
        return str(v)
    if isinstance(v, unreal.LinearColor):
        return [v.r, v.g, v.b, v.a]
    if isinstance(v, unreal.Color):
        return [v.r, v.g, v.b, v.a]
    if isinstance(v, unreal.Vector2D):
        return [v.x, v.y]
    if isinstance(v, unreal.Vector):
        return [v.x, v.y, v.z]
    if isinstance(v, unreal.Vector4):
        return [v.x, v.y, v.z, v.w]
    if isinstance(v, unreal.Object):
        return {'object': v.get_path_name(), 'class': v.get_class().get_name()}
    if isinstance(v, unreal.EnumBase):
        return str(v.name) if hasattr(v, 'name') else str(v)
    if isinstance(v, (list, tuple, unreal.Array)):
        return [to_json(x, depth + 1) for x in v]
    if isinstance(v, unreal.Map):
        return {str(k): to_json(x, depth + 1) for k, x in v.items()}
    if isinstance(v, unreal.StructBase):
        out = {}
        for n in property_names(v):
            try:
                out[n] = to_json(v.get_editor_property(n), depth + 1)
            except Exception:
                pass
        if not out:
            try:
                out['text'] = v.export_text()
            except Exception:
                out['text'] = str(v)
        return out
    return str(v)


def dump_properties(obj, skip=()):
    props = {}
    for n in property_names(obj):
        if n in skip:
            continue
        try:
            props[n] = to_json(obj.get_editor_property(n))
        except Exception:
            pass
    return props


class Walker:
    def __init__(self, material, host=None):
        self.material = material
        # Material handed to the editing library for connections: it must be a
        # Material, also for expressions inside material functions.
        self.host = host if host is not None else (material if isinstance(material, unreal.Material) else None)
        self.nodes = {}
        self.ids = {}

    def node_id(self, expr):
        key = expr.get_path_name()
        if key not in self.ids:
            self.ids[key] = 'n%d' % len(self.ids)
        return self.ids[key]

    def visit(self, expr):
        nid = self.node_id(expr)
        if nid in self.nodes:
            return nid
        node = {'class': expr.get_class().get_name(), 'name': expr.get_name()}
        self.nodes[nid] = node
        try:
            x, y = MEL.get_material_expression_node_position(expr)
            node['pos'] = [x, y]
        except Exception:
            pass
        node['props'] = dump_properties(expr, skip=('material_expression_editor_x', 'material_expression_editor_y'))
        # Inputs: names and connected sources, slot by slot.
        names = []
        try:
            names = list(MEL.get_material_expression_input_names(expr))
        except Exception:
            pass
        sources = []
        for owner in (self.host, self.material, None):
            try:
                sources = list(MEL.get_inputs_for_material_expression(owner, expr))
                break
            except Exception:
                pass
        inputs = []
        for i in range(max(len(names), len(sources))):
            src = sources[i] if i < len(sources) else None
            entry = {'name': str(names[i]) if i < len(names) else 'Input%d' % i}
            if src is not None:
                entry['node'] = self.visit(src)
                try:
                    entry['output'] = str(MEL.get_input_node_output_name_for_material_expression(expr, src))
                except Exception:
                    entry['output'] = ''
            inputs.append(entry)
        node['inputs'] = inputs
        # Material function calls: the function graph too.
        if isinstance(expr, unreal.MaterialExpressionMaterialFunctionCall):
            func = expr.get_editor_property('material_function')
            if func:
                node['function'] = func.get_path_name()
        return nid

    def run(self):
        outputs = {}
        for key, prop in OUTPUTS:
            try:
                expr = MEL.get_material_property_input_node(self.material, prop)
            except Exception:
                expr = None
            if expr is None:
                continue
            outputs[key] = {'node': self.visit(expr)}
            try:
                outputs[key]['output'] = str(MEL.get_material_property_input_node_output_name(self.material, prop))
            except Exception:
                outputs[key]['output'] = ''
        return outputs


def export_material(mat):
    w = Walker(mat)
    entry = {
        'path': mat.get_path_name(),
        'name': mat.get_name(),
        'type': 'Material',
        'props': dump_properties(mat, skip=('expressions',)),
    }
    entry['outputs'] = w.run()
    entry['nodes'] = w.nodes
    params = {}
    for getter, default, kind in ((MEL.get_scalar_parameter_names, MEL.get_material_default_scalar_parameter_value, 'scalar'),
                                  (MEL.get_vector_parameter_names, MEL.get_material_default_vector_parameter_value, 'vector'),
                                  (MEL.get_texture_parameter_names, MEL.get_material_default_texture_parameter_value, 'texture')):
        try:
            for n in getter(mat):
                try:
                    params[str(n)] = {'kind': kind, 'default': to_json(default(mat, n))}
                except Exception:
                    params[str(n)] = {'kind': kind}
        except Exception:
            pass
    entry['parameters'] = params
    return entry


def package_expressions(asset):
    """Expressions stored in an asset's package (material functions expose no
    expression list to Python: loaded objects of the package are scanned)."""
    pkg = asset.get_outermost()
    found = []
    for expr in unreal.ObjectIterator(unreal.MaterialExpression):
        try:
            if expr.get_outermost() == pkg:
                found.append(expr)
        except Exception:
            pass
    return found


def export_function(func, host=None):
    """A material function: inputs, outputs and the graph between them."""
    w = Walker(func, host)
    entry = {'path': func.get_path_name(), 'name': func.get_name(), 'type': 'MaterialFunction', 'props': dump_properties(func)}
    inputs, outputs = [], []
    for expr in package_expressions(func):
        if isinstance(expr, unreal.MaterialExpressionFunctionOutput):
            outputs.append(w.visit(expr))
        elif isinstance(expr, unreal.MaterialExpressionFunctionInput):
            inputs.append(w.visit(expr))
    entry['outputs'] = outputs
    entry['inputs'] = inputs
    entry['nodes'] = w.nodes
    return entry


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument('--path', default='', help='content folder to export (recursive), e.g. /CyTexture/Editor/LayerPresets')
    ap.add_argument('--out', default='', help='output .json file')
    args = ap.parse_args(argv)
    if args.path:
        reg = unreal.AssetRegistryHelpers.get_asset_registry()
        datas = reg.get_assets_by_path(args.path, recursive=True)
        assets = [d.get_asset() for d in datas]
    else:
        assets = list(unreal.EditorUtilityLibrary.get_selected_assets())
    out = args.out or (unreal.Paths.project_saved_dir() + 'ShaderixelMaterialExport.json')
    result = {'unreal': unreal.SystemLibrary.get_engine_version(), 'assets': []}
    functions = {}
    with unreal.ScopedSlowTask(len(assets), 'Export Shaderixel') as task:
        task.make_dialog(True)
        for a in assets:
            if task.should_cancel():
                break
            task.enter_progress_frame(1, a.get_name() if a else '')
            if a is None:
                continue
            try:
                if isinstance(a, unreal.Material):
                    e = export_material(a)
                    for n in e['nodes'].values():
                        if 'function' in n and n['function'] not in functions:
                            f = unreal.load_asset(n['function'])
                            if f:
                                functions[n['function']] = export_function(f, a)
                    result['assets'].append(e)
                elif isinstance(a, unreal.MaterialFunction):
                    functions[a.get_path_name()] = export_function(a)
                else:
                    result['assets'].append({'path': a.get_path_name(), 'name': a.get_name(), 'type': a.get_class().get_name(),
                                             'props': dump_properties(a)})
            except Exception as ex:
                result['assets'].append({'path': a.get_path_name(), 'name': a.get_name(), 'error': str(ex)})
    # Functions called from functions (any material serves as host for the editing library).
    any_material = next((x for x in assets if isinstance(x, unreal.Material)), None)
    pending = True
    while pending:
        pending = False
        for f in list(functions.values()):
            for n in f['nodes'].values():
                path = n.get('function')
                if path and path not in functions:
                    fa = unreal.load_asset(path)
                    if fa:
                        functions[path] = export_function(fa, any_material)
                        pending = True
    result['functions'] = list(functions.values())
    with open(out, 'w', encoding='utf-8') as f:
        json.dump(result, f, indent=1, ensure_ascii=False)
    n = sum(1 for e in result['assets'] if e.get('type') == 'Material')
    unreal.log('Shaderixel export: %d materials, %d other assets, %d functions -> %s' % (n, len(result['assets']) - n, len(functions), out))
    # Diagnostic: function graphs must come with their connections.
    unlinked = [f['name'] for f in functions.values()
                if f.get('outputs') and not any('node' in e for o in f['outputs'] for e in f['nodes'][o].get('inputs', []))]
    if unlinked:
        unreal.log_warning('Shaderixel export: connections not read inside %d functions (%s)' % (len(unlinked), ', '.join(unlinked[:8])))
    else:
        unreal.log('Shaderixel export: function graphs complete')


main(sys.argv[1:])
