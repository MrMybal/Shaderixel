"""UI Toolkit structural and numerical GPU checks; no synthesized desktop input.
python tools/test_ui_toolkit.py --gpu --backend both
"""
from __future__ import annotations
import argparse,copy,json,math,re,shlex,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
IDS="""ui_sdf_shape ui_sdf_boolean ui_sdf_modify ui_sdf_mask ui_sdf_render ui_sdf_stroke
ui_sdf_from_alpha ui_motion_flow ui_uv_parallax ui_motion_transition
ui_draw_interface_pattern ui_uv_pixel_snap ui_alpha_refine ui_alpha_apply_sdf ui_skin_state""".split()

def meta(id):
    text=(ROOT/'shaders/effects'/(id+'.slang')).read_text(encoding='utf-8');params={};inputs=[]
    for line in text.splitlines():
        if line.startswith('//@input '):inputs.append(line[9:])
        if line.startswith('//@param '):
            t=shlex.split(line[9:]);params[t[0]]=t[1]
    return text,params,[] if '//@generator' in text else inputs or ['Image']

def static_checks():
    for id in IDS:
        text,params,inputs=meta(id)
        assert '//@category UI Toolkit/' in text and text.isascii(),id
        assert len(params)<=32 and len(inputs)<=4,id
        assert 'float4 Effect(EffectContext ctx)' in text,id
        if re.search(r'UILinear\(|UIAddressed\(|Prev\(|ctx\.px\s*[+-]',text):assert '//@halo full' in text,id
    paths=sorted((ROOT/'library/functions').glob('UI Toolkit *.sxfn'));assert len(paths)==41,len(paths)
    def check(g,context):
        nodes={n['id']:n for n in g['nodes']};assert len(nodes)==len(g['nodes']) and 0 not in nodes,context
        assert nodes[g['output']]['kind']=='suboutput',context
        for kind in ('subinput','suboutput'):
            ports=sorted(n['port'] for n in nodes.values() if n['kind']==kind);assert ports==list(range(len(ports))),(context,ports)
        variables={v['name']:v for v in g.get('variables',[])};assert len(variables)==len(g.get('variables',[])),context
        assert all(v.get('exposed') and v.get('desc') for v in variables.values()),context
        done=set();stack=set()
        def walk(id):
            if not id or id in done:return
            assert id in nodes and id not in stack,(context,id)
            stack.add(id)
            for src in nodes[id].get('inputs',[]):walk(src)
            stack.remove(id);done.add(id)
        def outputs(n):
            kind=n['kind']
            if kind in ('subinput','value','varget'):return [n['value']['type']]
            if kind=='function':return [p['value']['type'] for p in sorted((p for p in n['sub']['nodes'] if p['kind']=='suboutput'),key=lambda p:p['port'])]
            if kind=='math':return ['Float','Float'] if n['op']=='break_vec2' else [{'vec2':'Vec2','equal':'Bool','concat':'Text'}.get(n['op'],'Float')]
            if kind in ('event','action'):return ['Exec']
            if kind=='split':return ['Image']
            return ['Image']
        def typ(src,ch):
            if not src:return None
            if nodes[src]['kind']=='split':assert 1<=ch<=4,(context,src,ch);return 'Canal'
            assert ch%8==0,(context,src,ch)
            ports=outputs(nodes[src]);assert ch//8<len(ports),(context,src,ch)
            return ports[ch//8]
        def compatible(actual,expected):
            return actual is None or actual==expected or actual in {'Image','Canal'} and expected in {'Image','Canal'} or actual in {'Float','Int','Bool'} and expected in {'Float','Int','Bool'}
        for n in nodes.values():
            walk(n['id']);ins=n.get('inputs',[]);channels=n.get('inputChannels',[0]*len(ins));assert len(ins)==len(channels),(context,n['id'])
            actual=[typ(src,ch) for src,ch in zip(ins,channels)];expected=None
            if n['kind']=='varget':assert n['op'] in variables and variables[n['op']]['value']['type']==n['value']['type'],(context,n)
            if n['kind']=='effect':
                _,ps,images=meta(n['effect']['effect']);pins=n.get('paramPinNames',[])
                assert len(set(pins))==len(pins) and all(p in ps for p in pins),(context,n)
                assert len(ins)==len(images)+len(pins),(context,n)
                assert all(k in ps for k in n['effect'].get('set',{})),(context,n)
                expected=['Image']*len(images)+[{'float2':'Vec2','color':'Color','enum':'Int','bool':'Bool','int':'Int'}.get(ps[p],'Float') for p in pins]
            elif n['kind']=='function':
                check(n['sub'],context+'/'+n['name']);expected=[p['value']['type'] for p in sorted((p for p in n['sub']['nodes'] if p['kind']=='subinput'),key=lambda p:p['port'])]
            elif n['kind']=='suboutput':expected=[n['value']['type']]
            elif n['kind']=='blend':expected=['Image','Image','Canal']
            elif n['kind']=='action':assert n['op']=='export_png';expected=['Exec','Image','Text']
            if expected is not None:
                assert len(actual)==len(expected),(context,n['id'],actual,expected)
                assert all(compatible(a,b) for a,b in zip(actual,expected)),(context,n['id'],actual,expected)
        assert done==nodes.keys(),context
    for path in paths:
        d=json.loads(path.read_text(encoding='utf-8'));assert d['name']==path.stem and d['description'] and d['format']=='shaderixel-function',path
        check(d['graph'],path.stem)
    print(f'PASS static: {len(IDS)} effects, {len(paths)} graphs; nested graphs, types, ports, references and cycles',flush=True)
    return paths

def save_doc(path,nodes,output,width=256,height=192):
    ref,ch=output if isinstance(output,tuple) else (output,0)
    last=max([n['id'] for n in nodes]+[0])+1;out=dict(id=last,kind='output',name='Output',inputs=[ref],inputChannels=[ch],pos=[2400,0])
    d=dict(format='shaderixel',version=2,name=path.stem,width=width,height=height,precision='float32',mode='nodes',blendInGamma=False,nextId=last+1,activeNode=ref,layers=[],graph=dict(output=last,variables=[],nodes=nodes+[out]))
    data=json.dumps(d,separators=(',',':')).encode('utf-8');path.write_bytes(b'SXL1'+struct.pack('<IQ',2,len(data))+data)

class Fixture:
    def __init__(self):self.nodes=[]
    def fx(self,id,inputs=(),**sets):
        refs=[p if isinstance(p,tuple) else (p,0) for p in inputs];i=len(self.nodes)+1
        self.nodes.append(dict(id=i,kind='effect',name=id,inputs=[p[0] for p in refs],inputChannels=[p[1] for p in refs],pos=[i*300,0],effect={'effect':id,'set':sets},paramPinNames=[]));return i
    def function(self,doc,inputs=(),defaults=None):
        sub=copy.deepcopy(doc['graph'])
        for n in sub['nodes']:
            if n['kind']=='subinput' and n['name'] in (defaults or {}):n['value']['f']=defaults[n['name']]
        refs=[p if isinstance(p,tuple) else (p,0) for p in inputs];i=len(self.nodes)+1
        self.nodes.append(dict(id=i,kind='function',name=doc['name'],sub=sub,funcKind=doc['kind'],applyMode=False,inputs=[p[0] for p in refs],inputChannels=[p[1] for p in refs],pos=[i*300,0]));return i
    def solid(self,color):return self.fx('ui_draw_panel',size=[256,192],radius=0,border=0,bevel=0,shadow_color=[0,0,0,0],color_top=color,color_bottom=color)
    def box(self,color=(.2,.4,.6,1)):
        field=self.fx('ui_sdf_shape',shape=1,size=[128,80],radius=0)
        image=self.fx('ui_sdf_render',[field],color_a=list(color),color_b=list(color),border=0,bevel=0,shadow_color=[0,0,0,0],softness=0)
        return image,field

def gpu_checks(paths,backend):
    from PIL import Image,ImageChops
    out=ROOT/'build/ui_toolkit_tests'/backend;out.mkdir(parents=True,exist_ok=True)
    exe=ROOT/'build/Release'/('ShaderixelVk.exe' if backend=='vulkan' else 'Shaderixel.exe')
    si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=7;count=0
    def render(name,f,output):
        nonlocal count
        count+=1;p=out/(f'{count:02}_'+name);doc=p.with_suffix('.sxl');png=p.with_suffix('.png');screen=p.with_suffix('.screen.png');save_doc(doc,f.nodes,output)
        if png.exists():png.unlink()
        subprocess.run([str(exe),str(doc),'--frames','90','--dump-image',str(png),'--capture',str(screen)],cwd=out,startupinfo=si,timeout=180,check=True)
        log=(out/'shaderixel.log').read_text(encoding='utf-8',errors='replace');p.with_suffix('.log').write_text(log,encoding='utf-8')
        assert 'Dumped document image' in log and not re.search(r'FAILED|error\[E|unknown effect|cycle',log,re.I),(name,log)
        im=Image.open(png).convert('RGBA');im.load();return im
    def linear(pixel):
        def dec(v):
            v/=255;return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
        return tuple(dec(v) for v in pixel[:3])+(pixel[3]/255,)
    def near(actual,expected,tol=.009):assert all(abs(a-b)<=tol for a,b in zip(actual,expected)),(actual,expected)
    f=Fixture();field=f.fx('ui_sdf_shape',shape=0,size=[64,64]);view=f.fx('ui_sdf_mask',[field],view=4,range=64);im=render('circle_distance',f,view)
    for xy in [(128,96),(160,96),(180,96)]:
        d=math.hypot(xy[0]+.5-128,xy[1]+.5-96)-32;near(linear(im.getpixel(xy))[0:1],(.5+d/128,))
    f=Fixture();field=f.fx('ui_sdf_shape',shape=1,size=[128,80],radius=12);view=f.fx('ui_sdf_mask',[field],view=5);im=render('rounded_contour',f,view);length=2*(128+80-48)+2*math.pi*12
    near(linear(im.getpixel((128,56)))[0:1],(52.5/length,));near(linear(im.getpixel((191,96)))[0:1],((104+math.pi*6+28.5)/length,))
    for shape in range(7):
        f=Fixture();field=f.fx('ui_sdf_shape',shape=shape,size=[100,80]);mask=f.fx('ui_sdf_mask',[field],softness=0);im=render(f'shape_{shape}',f,mask)
        assert im.getpixel((128,96))[0]==255 and im.getpixel((10,10))[0]==0,shape
    for op in range(7):
        f=Fixture();a=f.fx('ui_sdf_shape',shape=0,size=[80,80],position=[.375,.5]);b=f.fx('ui_sdf_shape',shape=0,size=[80,80],position=[.625,.5]);field=f.fx('ui_sdf_boolean',[a,b],operation=op,smoothness=12);mask=f.fx('ui_sdf_mask',[field],softness=0);im=render(f'boolean_{op}',f,mask)
        assert im.getpixel((80,96))[0]==(0 if op in (1,5) else 255),(op,'A only')
        assert im.getpixel((128,96))[0]==(0 if op in (2,3,6) else 255),(op,'overlap')
    for pat in (1,2):
        f=Fixture();field=f.fx('ui_sdf_shape',shape=1,size=[128,80],radius=0);stroke=f.fx('ui_sdf_stroke',[field],pattern=pat,width=4,fit=0,dash=12,gap=8,softness=0);im=render(f'stroke_{pat}',f,stroke)
        assert im.getpixel((64,55))[3]==255 and im.getpixel((74,55))[3]==0,pat
    f=Fixture();field=f.fx('ui_sdf_shape',shape=1,size=[128,80],radius=0);stroke=f.fx('ui_sdf_stroke',[field],double_line=1,width=2,double_gap=4,softness=0);im=render('double_stroke',f,stroke)
    assert im.getpixel((128,53))[3]==255 and im.getpixel((128,59))[3]==255 and im.getpixel((128,56))[3]==0
    f=Fixture();a=f.fx('ui_draw_slice_test',grid_width=0);neutral=f.solid([.5,.5,.5,1]);flow=f.fx('ui_motion_flow',[a,neutral,0,0],time=.25);im=render('neutral_flow',f,flow);near(linear(im.getpixel((100,75))),(.04,.07,.1,1))
    flow_images=[]
    for t in [0,.25,1]:
        f=Fixture();a=f.fx('ui_draw_slice_test',grid_width=0);map=f.solid([.8,.2,0,1]);flow=f.fx('ui_motion_flow',[a,map,0,0],time=t);flow_images.append(render('flow_'+str(t),f,flow))
    assert ImageChops.difference(flow_images[0],flow_images[2]).convert('RGB').getbbox() is None
    assert ImageChops.difference(flow_images[0],flow_images[1]).convert('RGB').getbbox() is not None
    f=Fixture();field=f.fx('ui_uv_parallax',[0,0,0],pointer=[1,0],strength=[20,12],plane_depth=.75);im=render('parallax',f,field);near(linear(im.getpixel((128,96))),(128.5/256-20*.75/256,96.5/192+12*.75/192,1,1))
    for mode in range(6):
        for progress in [0,1]:
            f=Fixture();a=f.solid([.2,.4,.6,.75]);b=f.solid([.8,.2,.1,.5]);o=f.fx('ui_motion_transition',[a,b,0,0],mode=mode,progress=progress);im=render(f'transition_{mode}_{progress}',f,o);near(linear(im.getpixel((128,96))),(.8,.2,.1,.5) if progress else (.2,.4,.6,.75))
    f=Fixture();a=f.solid([1,0,0,1]);b=f.solid([0,0,1,1]);o=f.fx('ui_motion_transition',[a,b,0,0],mode=2,progress=.5,softness=0);im=render('wipe_direction',f,o)
    assert im.getpixel((64,96))==(0,0,255,255) and im.getpixel((192,96))==(255,0,0,255)
    for step,expected in [(1,128.5),(.25,128.75)]:
        f=Fixture();uv=f.fx('ui_uv_affine',[0],offset=[.2/256,0]);uv=f.fx('ui_uv_pixel_snap',[uv],grid=[step,step]);im=render('pixel_snap_'+str(step),f,uv);near(linear(im.getpixel((128,96)))[0:1],(expected/256,))
    layout=json.loads((ROOT/'library/functions/UI Toolkit Layout Pixel Snap.sxfn').read_text(encoding='utf-8'));f=Fixture();node=f.function(layout,defaults={'Position (UV)':[.2342,.6789],'Size (px)':[100.3,60.7],'Canvas Size (px)':[256,192],'Grid (px)':[.25,.5]})
    uv=f.fx('ui_uv_coordinates');n=f.nodes[-1];n['paramPinNames']=['position','target_size'];n['inputs']=[node,node];n['inputChannels']=[0,8];im=render('layout_value_ports',f,uv)
    posx=round(.2342*256/.25)*.25;posy=round(.6789*192/.5)*.5;near(linear(im.getpixel((60,130))),((60.5-posx)/100.25+.5,(130.5-posy)/60.5+.5,1,1))
    f=Fixture();a=f.solid([.6,.7,.8,.5]);o=f.fx('ui_alpha_refine',[a,0],remove_matte=1);im=render('matte_removal',f,o);near(linear(im.getpixel((128,96))),(.2,.4,.6,.5))
    f=Fixture();image,_=f.box();field=f.fx('ui_sdf_from_alpha',[image]);view=f.fx('ui_sdf_mask',[field],view=4,range=64);im=render('alpha_distance',f,view)
    near(linear(im.getpixel((60,96)))[0:1],(.5+3.5/128,));near(linear(im.getpixel((80,96)))[0:1],(.5-16.5/128,))
    for grow in [3,-3,0]:
        f=Fixture();image,_=f.box();field=f.fx('ui_sdf_from_alpha',[image]);outimage=f.fx('ui_alpha_apply_sdf',[image,field,0],grow=grow,softness=4 if grow==0 else 1);im=render('alpha_boundary_'+str(grow),f,outimage)
        if grow==3:near(linear(im.getpixel((61,96))),(.2,.4,.6,1));assert im.getpixel((60,96))[3]==0
        elif grow==-3:assert im.getpixel((66,96))[3]==0 and im.getpixel((67,96))[3]==255
        else:near((im.getpixel((63,96))[3]/255,im.getpixel((64,96))[3]/255),(.375,.625))
    skin=json.loads((ROOT/'library/functions/UI Toolkit Skin Image Variants.sxfn').read_text(encoding='utf-8'));skin_images=[]
    for port in range(5):
        f=Fixture();source,_=f.box();node=f.function(skin,[source]);im=render('skin_port_'+str(port),f,(node,port*8));skin_images.append(im)
    near((skin_images[0].getpixel((128,96))[3]/255,skin_images[3].getpixel((128,96))[3]/255),(1,.45))
    assert skin_images[0].getpixel((193,96))[3]==0 and skin_images[4].getpixel((193,96))[3]>200
    assert len({im.getpixel((128,96)) for im in skin_images})>=4
    print(f'PASS {backend}: numerical shape, contour, Boolean, border, flow, transition, snap, nested value, alpha and state checks',flush=True)
    for path in paths:
        if path.stem=='UI Toolkit Layout Pixel Snap':continue # exercised through typed value ports above
        doc=json.loads(path.read_text(encoding='utf-8'));f=Fixture();source,field=f.box();other=f.solid([.8,.2,.1,1]);flow=f.solid([.8,.2,0,1]);ports=sorted((n for n in doc['graph']['nodes'] if n['kind']=='subinput'),key=lambda n:n['port']);connections=[]
        for n in ports:
            name=n['name'];typ=n['value']['type'];ref=0
            if typ=='Image':
                if 'SDF' in name:ref=field
                elif 'UV Map' in name or 'Depth' in name:ref=0
                elif 'Flow Map' in name:ref=flow
                elif name=='Image B' or name=='Background':ref=other
                else:ref=source
            connections.append(ref)
        defaults={'Widget Size (px)':[200,120],'Canvas Size (px)':[256,192],'Size (px)':[180.5,90.25]}
        node=f.function(doc,connections,defaults);im=render(path.stem.replace(' ','_'),f,node)
        assert im.getchannel('A').getbbox() is not None,(path,'empty output')
        assert max(hi for lo,hi in im.convert('RGB').getextrema())>10,(path,'black output')
        print(f'PASS {backend}: {path.stem}',flush=True)
    print(f'PASS {backend}: {count} minimized GPU fixtures',flush=True)
    return count

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--gpu',action='store_true');p.add_argument('--backend',choices=['d3d12','vulkan','both'],default='d3d12');args=p.parse_args();paths=static_checks()
    if args.gpu:
        counts={b:gpu_checks(paths,b) for b in (['d3d12','vulkan'] if args.backend=='both' else [args.backend])}
        out=ROOT/'build/ui_toolkit_tests/results.json';out.write_text(json.dumps(counts,indent=2),encoding='utf-8')
if __name__=='__main__':main()
