"""Deterministic, pixel-free practice documents for the offline handbook.

All functions are copied into the documents; only shipped shaders are required.
Tools remain unexecuted when a document is opened. No external image is needed.
"""
import copy
import json
from pathlib import Path
import shlex
import struct

ROOT = Path(__file__).resolve().parents[1]


def effect(identifier, **settings):
    params = []
    seen = set()
    for line in (ROOT / 'shaders/effects' / (identifier + '.slang')).read_text(encoding='utf-8-sig').splitlines():
        if not line.startswith('//@param '):
            continue
        name, typ, *values = shlex.split(line[9:])
        size = {'color': 4, 'float2': 2, 'float3': 3, 'float4': 4}.get(typ, 1)
        value = settings.get(name, [float(x) for x in values[:size]])
        value = list(value) if isinstance(value, (list, tuple)) else [value]
        if len(value) != size:
            raise ValueError(identifier + ': wrong parameter size: ' + name)
        params.append(value + [0] * (4 - size))
        seen.add(name)
    if settings.keys() - seen:
        raise ValueError(identifier + ': unknown parameters: ' + str(settings.keys() - seen))
    return dict(effect=identifier, params=params, enabled=True)


def function(identifier, inputs, values=None, variables=None):
    source = json.loads((ROOT / 'library/functions' / (identifier + '.sxfn')).read_text(encoding='utf-8-sig'))
    graph = copy.deepcopy(source['graph'])
    ports = sorted((n for n in graph['nodes'] if n['kind'] == 'subinput'), key=lambda n: n['port'])
    defaults = [copy.deepcopy(n.get('value', {'type': 'Image'})) for n in ports]
    for name, value in (values or {}).items():
        port = next(n['port'] for n in ports if n['name'] == name)
        defaults[port] = dict(defaults[port], **({'s': value} if isinstance(value, str) else {'f': value}))
    exposed = {v['name']: v['value'] for v in graph.get('variables', []) if v.get('exposed')}
    overrides = {}
    for name, value in (variables or {}).items():
        overrides[name] = dict(exposed[name], **({'s': value} if isinstance(value, str) else {'f': value}))
    return dict(kind='function', name=source['name'], funcKind=source['kind'], sub=graph,
                inputs=list(inputs) + [0] * (len(ports) - len(inputs)), pinValues=defaults,
                varValues=overrides, applyMode=False)


def document(name, nodes, output, width=512, height=256, output_port=0):
    nodes = copy.deepcopy(nodes)
    for i, node in enumerate(nodes, 1):
        node.update(id=i, pos=[60 + (i - 1) * 310, 80])
    nodes.append(dict(id=len(nodes)+1, kind='output', name='Output', inputs=[output],
                      inputChannels=[output_port*8], pos=[60+len(nodes)*310,80]))
    return dict(format='shaderixel', version=2, name=name, width=width, height=height,
                precision='float32', mode='nodes', blendInGamma=False,
                nextId=len(nodes)+1, activeNode=output, layers=[], graph=dict(nodes=nodes, output=len(nodes)))


def examples():
    def fx(identifier, inputs=(), **settings):
        return dict(kind='effect', name=identifier, inputs=list(inputs), effect=effect(identifier, **settings))
    result = {}
    result['ui_nine_slice'] = document('Practice - Nine slice', [
        fx('ui_draw_slice_test'),
        function('UI UV Nine Slice Panel', [1], {'Widget Size (px)': [375.5,75.25]},
                 {'Center mode':1, 'Logical source size (px; 0 = region texels)':[128,128]})], 2)
    result['color_key'] = document('Practice - Color key', [
        fx('solid_color', color=[0,1,0,1]),
        fx('ui_draw_panel', size=[280,140], radius=32, color_top=[.8,.08,.02,1],
           color_bottom=[.2,.01,.03,1], border=0, bevel=0, shadow_color=[0,0,0,0]),
        dict(kind='blend',name='Opaque source on green',inputs=[1,2],blend=0,opacity=1),
        function('Masks Color Key and Despill',[3])], 4)
    result['pbr_maps'] = document('Practice - PBR map inspection', [
        dict(fx('noise', scale=12),name='Practice pattern - replace with your photo'),
        function('PBR Photo to Maps',[1])], 2, width=256, height=256, output_port=2)
    result['upscale'] = document('Practice - Local upscale', [
        fx('ui_draw_slice_test'), function('Textures Upscale and Export',[1],
          {'Scale Factor':2,'PNG File':'practice_upscaled.png'}, {'Use ComfyUI':0})],2,width=256,height=128)
    result['photo_variants'] = document('Practice - Photo variants', [
        dict(fx('ui_draw_slice_test'), name='Practice colors - replace with your photo'),
        function('Photo Essentials Develop',[1], {'Exposure (Stops)':.5}),
        function('Photo Essentials Exposure Bracket Export',[2], {'File Prefix':'practice_photo'})],3)
    result['bake_background'] = dict(format='shaderixel',version=2,name='Practice - Paintable background',
        width=512,height=256,precision='float32',mode='layers',blendInGamma=False,nextId=2,activeLayer=1,
        layers=[dict(id=1,name='Editable gradient',kind='fill',effect=effect('gradient_fill',
                     colorA=[.025,.015,.08,1],colorB=[.06,.4,.7,1],angle=25),
                     blend=0,opacity=1,visible=True,locked=False)],
        graph=dict(output=2,nodes=[dict(id=2,kind='output',name='Output',inputs=[0],pos=[380,80])]))
    return result


def files():
    result = {}
    for identifier, doc in examples().items():
        def node_ids(graph):
            for node in graph.get('nodes', []):
                yield node['id']
                if isinstance(node.get('sub'), dict): yield from node_ids(node['sub'])
        doc['nextId']=max([doc['nextId']-1,*node_ids(doc['graph'])])+1
        metadata = json.dumps(doc, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
        result[identifier] = b'SXL1' + struct.pack('<IQ',2,len(metadata)) + metadata
    return result
